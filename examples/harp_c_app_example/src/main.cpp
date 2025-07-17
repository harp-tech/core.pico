#include <cstring>
#include <harp_c_app.h>
#include <harp_synchronizer.h>
#include <core_registers.h>
#include <reg_types.h>
#ifdef DEBUG
    #include <pico/stdlib.h> // for uart printing
    #include <cstdio> // for printf
#endif

// Create device name array.
const uint16_t who_am_i = 1234;
const uint8_t hw_version_major = 1;
const uint8_t hw_version_minor = 0;
const uint8_t assembly_version = 2;
const uint8_t harp_version_major = 2;
const uint8_t harp_version_minor = 0;
const uint8_t fw_version_major = 3;
const uint8_t fw_version_minor = 0;
const uint16_t serial_number = 0xCAFE;

// Harp App Register Setup.
const size_t reg_count = 2;

// Define register contents.
#pragma pack(push, 1)
struct app_regs_t
{
    volatile uint8_t test_byte;  // app register 0
    volatile uint32_t test_uint; // app register 1
} app_regs;
#pragma pack(pop)

// Define register "specs."
RegSpecs app_reg_specs[reg_count]
{
    {(uint8_t*)&app_regs.test_byte, sizeof(app_regs.test_byte), U8},
    {(uint8_t*)&app_regs.test_uint, sizeof(app_regs.test_uint), U32}
};

// Define register read-and-write handler functions.
RegFnPair reg_handler_fns[reg_count]
{
    {&HarpCore::read_reg_generic, &HarpCore::write_reg_generic},
    {&HarpCore::read_reg_generic, &HarpCore::write_to_read_only_reg_error}
};

void app_reset()
{
    app_regs.test_byte = 0;
    app_regs.test_uint = 0;
}

void update_app_state()
{
    // update here!
    // If app registers update their states outside the read/write handler
    // functions, update them here.
    // (Called inside run() function.)
}

// Create Harp App.
HarpCApp& app = HarpCApp::init(who_am_i, hw_version_major, hw_version_minor,
                               assembly_version,
                               harp_version_major, harp_version_minor,
                               fw_version_major, fw_version_minor,
                               serial_number, "Example C App",
                               (const uint8_t*)GIT_HASH, // in CMakeLists.txt.
                               &app_regs, app_reg_specs,
                               reg_handler_fns, reg_count, update_app_state,
                               app_reset);

// Core0 main.
int main()
{
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
