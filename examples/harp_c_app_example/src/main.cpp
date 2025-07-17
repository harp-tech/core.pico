#include <cstring>
#include <harp_c_app.h>
#include <harp_synchronizer.h>
#include <core_registers.h>
#include <reg_types.h>
#ifdef DEBUG
    #include <pico/stdlib.h> // for uart printing
    #include <cstdio> // for printf
#endif

#include <harp_core.h>
#include <hardware/flash.h>
#include <pico/bootrom.h>

// Create device name array.
const uint16_t who_am_i = 3226;
const uint8_t hw_version_major = 1;
const uint8_t hw_version_minor = 0;
const uint8_t assembly_version = 2;
const uint8_t harp_version_major = 2;
const uint8_t harp_version_minor = 0;
const uint8_t fw_version_major = 5;
const uint8_t fw_version_minor = 0;

// Harp App Register Setup.
const size_t reg_count = 2;

#define FIRMWARE_UPDATE_PICO_BOOTSEL 1

// Define register contents.
#pragma pack(push, 1)
struct app_regs_t
{
    volatile uint32_t firmware_update_capabilities;
    volatile uint32_t firmware_update_start;
} app_regs;
#pragma pack(pop)

static void handle_reset_command(msg_t& msg)
{
    HarpCore::copy_msg_payload_to_register(msg);
    if (app_regs.firmware_update_start == FIRMWARE_UPDATE_PICO_BOOTSEL)
    {
        HarpCore::send_harp_reply(WRITE, msg.header.address);

        // Give controller a chance to gracefully close the serial port
        for (int i = 0; i < 5; i++)
        {
            sleep_ms(100);
            tud_task();
        }

        rom_reset_usb_boot(0, 0);
    }
    else
        HarpCore::send_harp_reply(WRITE_ERROR, msg.header.address);

    app_regs.firmware_update_start = 0;
}

// Define register "specs."
RegSpecs app_reg_specs[reg_count]
{
    {(uint8_t*)&app_regs.firmware_update_capabilities, sizeof(app_regs.firmware_update_capabilities), U32},
    {(uint8_t*)&app_regs.firmware_update_start, sizeof(app_regs.firmware_update_start), U32},
};

// Define register read-and-write handler functions.
RegFnPair reg_handler_fns[reg_count]
{
    {&HarpCore::read_reg_generic, &HarpCore::write_to_read_only_reg_error},
    {&HarpCore::read_reg_generic, &handle_reset_command}, // write-only
};

void app_reset()
{
    app_regs.firmware_update_capabilities = FIRMWARE_UPDATE_PICO_BOOTSEL;
    app_regs.firmware_update_start = 0;
}

void update_app_state()
{
    // update here!
    // If app registers update their states outside the read/write handler
    // functions, update them here.
    // (Called inside run() function.)
    gpio_put(PICO_DEFAULT_LED_PIN, (time_us_64() / 1000000) & 1);
}

//DJM: This should ideally be default behavior within the Pico Core if the user doesn't explicitly provide a serial number
uint16_t get_serial_number()
{
    static_assert(sizeof(pico_unique_board_id_t) == sizeof(uint64_t));
    union
    {
        pico_unique_board_id_t id_array;
        uint64_t id64;
    } board_id;
    //TODO: This isn't working for whatever reason, I just get a bunch of 0's
    // For some reason the constructor isn't running. I noticed multiple copies of unique_id.obj are being built, maybe that has something to do with it?
    //pico_get_unique_board_id(&board_id.id_array);

    // Using flash_get_unique_id directly as a workaround for now. This is not ideal as it doesn't properly handle some edge cases or the RP2350.
    flash_get_unique_id(&board_id.id_array.id[0]);

    //TODO: 2 bytes is super very bad for a unique identifier.
    // Ideally Harp Regulator wants the first or last two bytes of the serial number as it's used to match the serial number seen in PICOBOOT (which matches pico_get_unique_board_id)
    // and Harp Regulator uses a prefix/suffix match to handle the difference in sizes.
    //
    // Be mindful of whether we prefer the upper or lower 16 bits, the Pico SDK used to use the wrong end of the flash ID when it exceeded 8 bytes and it resulted
    // in some flash chips with 16 byte IDs all having the same unique board ID. We likely want to tailor this to whatever flash chips are generally used with the Pico core
    // (or we should support a longer serial number somehow. Maybe if the client sends a READ U64 it replies with the full thing?)
    // https://github.com/raspberrypi/pico-sdk/issues/1641
    // https://github.com/raspberrypi/pico-sdk/issues/1132#issuecomment-2285719484
    //
    // With the official Raspberry Pi Pico boards, it seems my 16 least-significant bits are the ones with the most entropy.
    // (The upper 16 bits are actually identical on the two boards I checked.)
    return (uint16_t)board_id.id_array.id[6] << 8 | (uint16_t)board_id.id_array.id[7];
}

// Create Harp App.
HarpCApp& app = HarpCApp::init(who_am_i, hw_version_major, hw_version_minor,
                               assembly_version,
                               harp_version_major, harp_version_minor,
                               fw_version_major, fw_version_minor,
                               get_serial_number(), "Example C App",
                               (const uint8_t*)GIT_HASH, // in CMakeLists.txt.
                               &app_regs, app_reg_specs,
                               reg_handler_fns, reg_count, update_app_state,
                               app_reset);

// Core0 main.
int main()
{
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    app_reset(); //DJM: Should the core call this to ensure everything starts out in some neutral state?

// Init Synchronizer.
    HarpSynchronizer& sync = HarpSynchronizer::init(uart1, 5);
    app.set_synchronizer(&sync);
#ifdef DEBUG
    stdio_uart_init_full(uart0, 921600, 0, -1); // use uart1 tx only.
    printf("Hello, from an RP2040!\r\n");
#endif
    while(true)
    {
        app.run();
    }
}
