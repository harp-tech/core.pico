#ifndef CORE_REGISTERS_H
#define CORE_REGISTERS_H
#include <cstdint>
#include <reg_types.h>
#include <reg_spec.h>
#include <core_reg_bits.h>
#include <cstring>  // for strcpy
#include <array> // for size

/**
 * \brief enum where the name is the name of the register and the
 *        value is the address according to the harp protocol spec.
 */
enum CoreRegName : uint8_t
{
    WHO_AM_I = 0,
    HW_VERSION_H = 1, // major hardware version
    HW_VERSION_L = 2, // minor hardware version
    ASSEMBLY_VERSION = 3,
    HARP_VERSION_H = 4,
    HARP_VERSION_L = 5,
    FW_VERSION_H = 6,
    FW_VERSION_L = 7,
    TIMESTAMP_SECOND = 8,
    TIMESTAMP_MICRO = 9,
    OPERATION_CTRL = 10,
    RESET_DEF = 11,
    DEVICE_NAME = 12,
    SERIAL_NUMBER = 13,
    CLOCK_CONFIG = 14,
    TIMESTAMP_OFFSET = 15,
    UUID = 16,
    TAG = 17,
    HEARTBEAT = 18,
    VERSION = 19
};

inline constexpr size_t CORE_REG_COUNT = CoreRegName::VERSION - CoreRegName::WHO_AM_I + 1;

#pragma pack(push, 1)
/**
 * \brief Packed struct containing major.minor.patch semantic version information.
 */
struct semver_t
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
};
#pragma pack(pop)

#pragma pack(push, 1)
/**
 * \brief Harp Version Core Register packed convenienced struct
 */
struct harp_version_reg_t
{
    semver_t protocol;
    semver_t firmware;
    semver_t hardware;
    char core_id[3];
    char interface_hash[20];
};
#pragma pack(pop)

// Byte-align struct data so we can send it out serially byte-by-byte.
#pragma pack(push, 1)
struct CoreRegValues
{
    const uint16_t R_WHO_AM_I;
    const uint8_t R_HW_VERSION_H;
    const uint8_t R_HW_VERSION_L;
    const uint8_t R_ASSEMBLY_VERSION;
    const uint8_t R_HARP_VERSION_H;
    const uint8_t R_HARP_VERSION_L ;
    const uint8_t R_FW_VERSION_H;
    const uint8_t R_FW_VERSION_L;
    volatile uint32_t R_TIMESTAMP_SECOND;
    volatile uint16_t R_TIMESTAMP_MICRO;
    volatile uint8_t R_OPERATION_CTRL;
    volatile uint8_t R_RESET_DEV;
    volatile char R_DEVICE_NAME[25];
    volatile char default_name[25];
    volatile uint16_t R_SERIAL_NUMBER;
    volatile uint8_t R_CLOCK_CONFIG;
    volatile uint8_t R_TIMESTAMP_OFFSET;  // Deprecated.
    volatile uint8_t R_UUID[16];
    uint8_t R_TAG[8];
    uint16_t R_HEARTBEAT;
    harp_version_reg_t R_VERSION;

    // Custom Constructor to initialize strings.
    CoreRegValues(uint16_t who_am_i,
                  semver_t protocol, semver_t firmware, semver_t hardware,
                  const char name[],
                  const uint8_t tag[],
                  const uint8_t core_id[],
                  const uint8_t interface_hash[])
    :R_WHO_AM_I{who_am_i},
     R_HW_VERSION_H{hardware.major},
     R_HW_VERSION_L{hardware.minor},
     R_ASSEMBLY_VERSION{0},
     R_HARP_VERSION_H{protocol.major},
     R_HARP_VERSION_L{protocol.minor},
     R_FW_VERSION_H{firmware.major},
     R_FW_VERSION_L{firmware.minor},
     R_OPERATION_CTRL{0},
     R_RESET_DEV{0},
     R_DEVICE_NAME{0},
     default_name{0},
     R_SERIAL_NUMBER{0},
     R_CLOCK_CONFIG{0},
     R_TIMESTAMP_OFFSET{0},
     R_UUID{0}, // all zeros.
     R_HEARTBEAT{0},
     R_VERSION{.protocol = protocol,
               .firmware = firmware,
               .hardware = hardware,
               .interface_hash = {0}}
    {
        memcpy(R_TAG, tag, sizeof(R_TAG));
        memcpy(R_VERSION.core_id, core_id, sizeof(harp_version_reg_t::core_id));
        memcpy(R_VERSION.interface_hash, interface_hash,
            sizeof(harp_version_reg_t::interface_hash));
        strcpy((char*)default_name, name);
        reset();
    }

    void reset()
    {
        memcpy((char*)R_DEVICE_NAME, (char*)default_name, sizeof(R_DEVICE_NAME));
        // Flag that we only boot from non-volatile memory
        r_reset_dev_bits.BOOT_DEF = 1;
        // Setup R_OPERATION_CTRL defaults
        r_operation_ctrl_bits.ALIVE_EN = 1;
        r_operation_ctrl_bits.OPLED_EN = 1;
        r_operation_ctrl_bits.VISUAL_EN = 1;
        r_operation_ctrl_bits.HEARTBEAT_EN = 1;
    }

    // Syntactic Sugar. Make bitfields for certain registers easier to access.
    OperationCtrlBits& r_operation_ctrl_bits = *((OperationCtrlBits*)(&R_OPERATION_CTRL));
    ResetDevBits& r_reset_dev_bits = *((ResetDevBits*)(&R_RESET_DEV));
    ClockConfigBits& r_clock_config_bits = *((ClockConfigBits*)(&R_CLOCK_CONFIG));
};
#pragma pack(pop)

#endif //CORE_REGISTERS_H
