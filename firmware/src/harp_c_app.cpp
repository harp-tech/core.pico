#include "harp_message.h"
#include "harp_c_app.h"

HarpCApp& HarpCApp::init(uint16_t who_am_i,
                         uint8_t hw_version_major, uint8_t hw_version_minor,
                         uint8_t assembly_version,
                         uint8_t fw_version_major, uint8_t fw_version_minor,
                         uint16_t serial_number, const char name[],
                         const uint8_t tag[],
                         RegSpec* app_reg_specs, size_t app_reg_count,
                         void (* update_fn)(void), void (* reset_fn)(void))
{
    static HarpCApp app(who_am_i, hw_version_major, hw_version_minor,
                        assembly_version,
                        fw_version_major, fw_version_minor, serial_number,
                        name, tag, app_reg_specs, app_reg_count, update_fn,
                        reset_fn);
    return app;
}

HarpCApp::HarpCApp(uint16_t who_am_i,
                   uint8_t hw_version_major, uint8_t hw_version_minor,
                   uint8_t assembly_version,
                   uint8_t fw_version_major, uint8_t fw_version_minor,
                   uint16_t serial_number, const char name[],
                   const uint8_t tag[],
                   RegSpec* app_reg_specs, size_t app_reg_count,
                   void (*update_fn)(void), void (* reset_fn)(void))
:app_reg_specs_{app_reg_specs},
 app_reg_count_{app_reg_count},
 update_fn_{update_fn},
 reset_fn_{reset_fn},
 HarpCore(who_am_i, hw_version_major, hw_version_minor, assembly_version,
          fw_version_major, fw_version_minor, serial_number, name, tag)
{
    // Call base class constructor.
    // Create a ptr to the first (and only) derived class instance created.
    if (self == nullptr)
        self = this;
}

HarpCApp::~HarpCApp(){self = nullptr;}

void HarpCApp::handle_buffered_app_message()
{
    msg_t msg = get_buffered_msg();
    // Assume that this function deals with register ranges that start at
    // APP_REG_START_ADDRESS.
    uint8_t app_reg_address = msg.header.address - APP_REG_START_ADDRESS;
    if (msg.header.address >= APP_REG_START_ADDRESS + app_reg_count_)
        HarpCore::send_harp_reply(msg_type_t(msg.header.type | ERROR_MASK),
            msg.header.address, nullptr, 0, msg.header.payload_type);
    else
    {
        switch (msg.header.type)
        {
            case READ:
                app_reg_specs_[app_reg_address].read_fn_ptr(msg.header.address);
                break;
            case WRITE:
                app_reg_specs_[app_reg_address].write_fn_ptr(msg);
                break;
        }
    }
    clear_msg();
}

void HarpCApp::dump_app_registers()
{
    for (uint8_t address = APP_REG_START_ADDRESS;
         address < app_reg_count_ + APP_REG_START_ADDRESS; ++address)
        reg_address_to_spec(address).read_fn_ptr(address);
}
