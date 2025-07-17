#include <harp_core.h>

HarpCore& HarpCore::init(uint16_t who_am_i,
                         uint8_t hw_version_major, uint8_t hw_version_minor,
                         uint8_t assembly_version,
                         uint8_t harp_version_major, uint8_t harp_version_minor,
                         uint8_t fw_version_major, uint8_t fw_version_minor,
                         uint16_t serial_number, const char name[],
                         const uint8_t tag[])
{
    // Create the singleton instance using the private constructor.
    static HarpCore core(who_am_i, hw_version_major, hw_version_minor,
                         assembly_version,
                         harp_version_major, harp_version_minor,
                         fw_version_major, fw_version_minor, serial_number,
                         name, tag);
    return core;
}

HarpCore::HarpCore(uint16_t who_am_i,
                   uint8_t hw_version_major, uint8_t hw_version_minor,
                   uint8_t assembly_version,
                   uint8_t harp_version_major, uint8_t harp_version_minor,
                   uint8_t fw_version_major, uint8_t fw_version_minor,
                   uint16_t serial_number, const char name[],
                   const uint8_t tag[])
:regs_{who_am_i, hw_version_major, hw_version_minor, assembly_version,
       harp_version_major, harp_version_minor,
       fw_version_major, fw_version_minor, serial_number, name, tag},
 rx_buffer_index_{0}, total_bytes_read_{rx_buffer_index_}, new_msg_{false},
 set_visual_indicators_fn_{nullptr}, sync_{nullptr}, offset_us_64_{0},
 disconnect_handled_{false}, connect_handled_{false}, sync_handled_{false},
 heartbeat_interval_us_{HEARTBEAT_STANDBY_INTERVAL_US}
{
    // Create a pointer to the first (and one-and-only) instance created.
    if (self == nullptr)
        self = this;
    tusb_init();
#if defined(PICO_RP2040)
    // Populate Harp Core R_UUID with unique id from QSPI Flash.
    uint8_t flash_id[16]; // flash id is 8 bytes long, so zero out the rest.
    for (size_t i = 0; i < 16; ++i)
        flash_id[i] = 0;
    flash_get_unique_id(flash_id);
    memcpy((void*)(&regs.R_UUID[0]), (void*)&(flash_id), sizeof(flash_id));
#else
#pragma warning("Harp Core Register UUID not autodetected for this board.")
#endif
    // Initialize next heartbeat.
    update_next_heartbeat_from_curr_harp_time_us(harp_time_us_64());
}

HarpCore::~HarpCore(){self = nullptr;}

void HarpCore::run()
{
    tud_task();
    update_state();
    update_app_state(); // Does nothing unless a derived class implements it.
    process_cdc_input();
    if (not new_msg_)
        return;
#ifdef DEBUG_HARP_MSG_IN
    msg_t msg = get_buffered_msg();
    printf("Msg data: \r\n");
    printf("  type: %d\r\n", msg.header.type);
    printf("  addr: %d\r\n", msg.header.address);
    printf("  raw len: %d\r\n", msg.header.raw_length);
    printf("  port: %d\r\n", msg.header.port);
    printf("  payload type: %d\r\n", msg.header.payload_type);
    printf("  payload len: %d\r\n", msg.header.payload_length());
    uint8_t payload_len = msg.header.payload_length();
    if (payload_len > 0)
    {
        printf("  payload: ");
        for (auto i = 0; i < msg.payload_length(); ++i)
            printf("%d, ", ((uint8_t*)(msg.payload))[i]);
    }
    printf("\r\n\r\n");
#endif
    // Handle in-range register msgs and clear them. Ignore out-of-range msgs.
    handle_buffered_core_message(); // Handle msg. Clear it if handled.
    if (not new_msg_)
        return;
    handle_buffered_app_message(); // Handle msg. Clear it if handled.
    // Always clear any unhandled messages, so we don't lock up.
    if (new_msg_)
    {
#ifdef DEBUG_HARP_MSG_IN
    printf("Ignoring out-of-range msg!\r\n");
#endif
        clear_msg();
    }
}

void HarpCore::process_cdc_input()
{
    // TODO: Consider a timeout if we never receive a fully formed message.
    // TODO: scan for partial messages.
    // Fetch all data in the serial port. If it's at least a header's worth,
    // check the payload size and keep reading up to the end of the packet.
    if (not tud_cdc_available())
        return;
    // If the header has arrived, only read up to the full payload so we can
    // process one message at a time.
    uint32_t max_bytes_to_read = sizeof(rx_buffer_) - rx_buffer_index_;
    if (total_bytes_read_ >= sizeof(msg_header_t))
    {
        // Reinterpret contents of the rx buffer as a message header.
        msg_header_t& header = get_buffered_msg_header();
        // Read only the remainder of a single harp message.
        max_bytes_to_read = header.msg_size() - total_bytes_read_;
    }
    uint32_t bytes_read = tud_cdc_read(&(rx_buffer_[rx_buffer_index_]),
                                       max_bytes_to_read);
    rx_buffer_index_ += bytes_read;
    // See if we have a message header's worth of data yet. Baily early if not.
    if (total_bytes_read_ < sizeof(msg_header_t))
        return;
    // Reinterpret contents of the rx buffer as a message header.
    msg_header_t& header = get_buffered_msg_header();
    // Bail early if the full message (with payload) has not fully arrived.
    if (total_bytes_read_ < header.msg_size())
        return;
    rx_buffer_index_ = 0; // Reset buffer index for the next message.
    new_msg_ = true;
    return;
}

msg_t HarpCore::get_buffered_msg()
{
    // Reinterpret (i.e: type pun) contents of the uart rx buffer as a message.
    // Use references and ptrs to existing data so we don't make any copies.
    msg_header_t& header = get_buffered_msg_header();
    void* payload = rx_buffer_ + header.payload_base_index_offset();
    uint8_t& checksum = *(rx_buffer_ + header.checksum_index_offset());
    return msg_t{header, payload, checksum};
}

void HarpCore::handle_buffered_core_message()
{
    msg_t msg = get_buffered_msg();
    // TODO: check checksum.
    // Note: PC-to-Harp msgs don't have timestamps, so we don't check for them.
    // Ignore out-of-range messages. Expect them to be handled by derived class.
    if (msg.header.address > CORE_REG_COUNT)
        return;
    // Handle read-or-write behavior.
    switch (msg.header.type)
    {
        case READ:
            reg_func_table_[msg.header.address].read_fn_ptr(msg.header.address);
            break;
        case WRITE:
            reg_func_table_[msg.header.address].write_fn_ptr(msg);
            break;
    }
    clear_msg();
}

void HarpCore::update_state(bool force, op_mode_t forced_next_state)
{
    // Update internal logic.
    // Use 32-bit time representation since we are updating short intervals.
    uint64_t harp_time_us = harp_time_us_64();
    uint32_t time_us = harp_to_system_us_32(harp_time_us);
    bool tud_cdc_is_connected = tud_cdc_connected(); // Compute this once.
    bool is_synced = self->is_synced(); // Compute this once
    // Preserve flag value when synced; clear flag if we unsync.
    self->sync_handled_ = is_synced?
                              self->sync_handled_: false;
    // Extra logic so that we only act on the connection/disconnection event
    // changes once. State changes are connection/disconnection dependendent,
    // but after connecting, the PC is able to override the state at any point.
    self->disconnect_handled_ = tud_cdc_is_connected?
                                    false: self->disconnect_handled_;
    self->connect_handled_ = tud_cdc_is_connected? self->connect_handled_: false;
    if (!tud_cdc_is_connected && !self->disconnect_handled_)
    {
        self->disconnect_handled_ = true;
        self->disconnect_start_time_us_ = time_us;
    }
    // Extra logic to handle behavior that happens upon synchronizing.
    if (is_synced && !self->sync_handled_)
    {
        update_next_heartbeat_from_curr_harp_time_us(harp_time_us);
        self->sync_handled_ = true;
    }
    // Update state machine "next-state" logic.
    const uint8_t& state = self->regs_.r_operation_ctrl_bits.OP_MODE;
    uint8_t next_state = force? forced_next_state: state;
    if (!force)
    {
        switch (state)
        {
            case STANDBY:
                if (tud_cdc_is_connected && !self->connect_handled_)
                    next_state = ACTIVE;
            case ACTIVE:
                // Drop to STANDBY if we've lost the PC connection for too long.
                if (!tud_cdc_is_connected && self->disconnect_handled_
                    && (time_us - self->disconnect_start_time_us_)
                       >= NO_PC_INTERVAL_US)
                    next_state = STANDBY;
                break;
            case RESERVED:
                break;
            case SPEED:
                break;
            default:
                break;
        }
    }
    // Handle state edge output logic.
    // Update Heartbeat interval.
    if ((state != ACTIVE) && (next_state == ACTIVE))
    {
        self->connect_handled_ = true;
        self->heartbeat_interval_us_ = HEARTBEAT_ACTIVE_INTERVAL_US;
    }
    if (state == ACTIVE && next_state == STANDBY)
    {
        self->heartbeat_interval_us_ = HEARTBEAT_STANDBY_INTERVAL_US;
    }
    // Handle OPERATION_CTRL behavior.
    if (int32_t(time_us - self->next_heartbeat_time_us_) >= 0)
    {
        self->next_heartbeat_time_us_ += self->heartbeat_interval_us_;
        // Dispatch heartbeat msg and Blink LED.
        if (self->regs_.r_operation_ctrl_bits.ALIVE_EN)
        {
            //if (self->regs_.r_operation_ctrl_bits.VISUALEN)
            //    set_led(!get_led);
            if ((state == ACTIVE) & !is_muted()) // i.e: events enabled
                send_harp_reply(EVENT, TIMESTAMP_SECOND);
        }
    }
    // Handle in-state dependent output logic.
    // Do the state transition.
    self->regs_.r_operation_ctrl_bits.OP_MODE = next_state;
}

const RegSpecs& HarpCore::reg_address_to_specs(uint8_t address)
{
    if (address < CORE_REG_COUNT)
        return regs_.address_to_specs[address];
    return address_to_app_reg_specs(address); // virtual. Implemented by app.
}

void HarpCore::send_harp_reply(msg_type_t reply_type, uint8_t reg_name,
                               const volatile uint8_t* data, uint8_t num_bytes,
                               reg_type_t payload_type, uint64_t harp_time_us)
{
    // FIXME: implementation as-is cannot send more than 64 bytes of data
    //  because of underlying usb implementation.
    // Dispatch timestamped Harp reply.
    // Note: This fn implementation assumes little-endian architecture.
    uint8_t raw_length = num_bytes + 10;
    uint8_t checksum = 0;
    msg_header_t header{reply_type, raw_length, reg_name, 255,
                        (reg_type_t)(HAS_TIMESTAMP | payload_type)};
#ifdef DEBUG_HARP_MSG_OUT
    printf("Sending msg: \r\n");
    printf("  type: %d\r\n", header.type);
    printf("  addr: %d\r\n", header.address);
    printf("  raw len: %d\r\n", header.raw_length);
    printf("  port: %d\r\n", header.port);
    printf("  payload type: %d\r\n", header.payload_type);
    printf("  payload len: %d\r\n", header.payload_length());
    uint8_t payload_len = header.payload_length();
    if (payload_len > 0)
    {
        printf("  payload: ");
        for (auto i = 0; i < header.payload_length(); ++i)
            printf("%d, ", data[i]);
    }
    printf("\r\n\r\n");
#endif
    // Push data into usb packet and send it.
    for (uint8_t i = 0; i < sizeof(header); ++i) // push the header.
    {
        uint8_t& byte = *(((uint8_t*)(&header))+i);
        checksum += byte;
        tud_cdc_write_char(byte);
    }
    self->set_timestamp_regs(harp_time_us); // update and push timestamp.
    for (uint8_t i = 0; i < sizeof(self->regs.R_TIMESTAMP_SECOND); ++i)
    {
        uint8_t& byte = *(((uint8_t*)(&self->regs.R_TIMESTAMP_SECOND)) + i);
        checksum += byte;
        tud_cdc_write_char(byte);
    }
    for (uint8_t i = 0; i < sizeof(self->regs.R_TIMESTAMP_MICRO); ++i)
    {
        uint8_t& byte = *(((uint8_t*)(&self->regs.R_TIMESTAMP_MICRO)) + i);
        checksum += byte;
        tud_cdc_write_char(byte);
    }
    // TODO: should we lockout global interrupts to prevent reg data from
    //  changing underneath us?
    for (uint8_t i = 0; i < num_bytes; ++i) // push the payload data.
    {
        const volatile uint8_t& byte = *(data + i);
        checksum += byte;
        tud_cdc_write_char(byte);
    }
    tud_cdc_write_char(checksum); // push the checksum.
    tud_cdc_write_flush();  // Send usb packet, even if not full.
    // Call tud_task to handle case we issue multiple harp replies in a row.
    // FIXME: a better way might be to check tinyusb's internal buffer's
    // remaining space.
    tud_task();
}

void HarpCore::read_reg_generic(uint8_t reg_name)
{
    send_harp_reply(READ, reg_name);
}


void HarpCore::write_reg_generic(msg_t& msg)
{
    copy_msg_payload_to_register(msg);
    if (self->is_muted())
        return;
    const uint8_t& reg_name = msg.header.address;
    const RegSpecs& specs = self->reg_address_to_specs(msg.header.address);
    send_harp_reply(WRITE, reg_name, specs.base_ptr, specs.num_bytes,
                    specs.payload_type);
}

void HarpCore::write_to_read_only_reg_error(msg_t& msg)
{
#ifdef DEBUG_HARP_MSG_IN
    printf("Error: Reg address %d is read-only.\r\n", msg.header.address);
#endif
    send_harp_reply(WRITE_ERROR, msg.header.address);
}

void HarpCore::set_timestamp_regs(uint64_t harp_time_us)
{
    // RP2040 implementation:
    // Harp Time is computed as an offset relative to the RP2040's main
    // timer register, which ticks every 1[us].
    // Note: R_TIMESTAMP_MICRO can only represent values up to 31249.
    // Note: Update microseconds first.
#if defined(PICO_RP2040)
    uint64_t leftover_microseconds;
    uint64_t curr_seconds = divmod_u64u64_rem(harp_time_us, 1'000'000UL,
                                              &leftover_microseconds);
    self->regs.R_TIMESTAMP_SECOND = uint32_t(curr_seconds); // will not overflow.
    self->regs.R_TIMESTAMP_MICRO = uint16_t(leftover_microseconds >> 5);
#else
    uint64_t& curr_microseconds = harp_time_us;
    self->regs.R_TIMESTAMP_SECOND = curr_microseconds / 1'000'000ULL;
    self->regs.R_TIMESTAMP_MICRO = uint16_t((curr_microseconds % 1'000'000UL)>>5);
#endif
}

void HarpCore::read_timestamp_second(uint8_t reg_name)
{
    self->update_timestamp_regs();
    read_reg_generic(reg_name);
}

void HarpCore::write_timestamp_second(msg_t& msg)
{
    uint32_t seconds;
    // We cannot reinterpret-cast bc of alignment?
    memcpy((void*)(&seconds), msg.payload, sizeof(seconds));
    // Replace the current number of elapsed seconds (in harp time) without
    // altering the number of elapsed microseconds.
    uint64_t set_time_microseconds = uint64_t(seconds) * 1'000'000ULL;
#if defined(PICO_RP2040)
    uint64_t curr_microseconds;
    uint64_t curr_seconds = divmod_u64u64_rem(harp_time_us_64(), 1'000'000ULL,
                                              &curr_microseconds);
#else
    uint64_t curr_microseconds = harp_time_us_64() % 1'000'000ULL;
#endif
    uint64_t new_harp_time_us = set_time_microseconds + curr_microseconds;
    set_harp_time_us_64(new_harp_time_us);
    // Update time-dependent behavior. Take harp time from this function such
    // that external synchronizer takes priority.
    update_next_heartbeat_from_curr_harp_time_us(harp_time_us_64());
    // Send harp reply.
    // Note: harp timestamp registers will be updated before being dispatched.
    send_harp_reply(WRITE, msg.header.address);
}

void HarpCore::read_timestamp_microsecond(uint8_t reg_name)
{
    // Update register. Then trigger a generic register read.
    self->update_timestamp_regs();
    read_reg_generic(reg_name);
}

void HarpCore::write_timestamp_microsecond(msg_t& msg)
{
    const uint32_t msg_us = ((uint32_t)(*((uint16_t*)msg.payload))) << 5;
    // PICO implementation: replace the current number of elapsed microseconds
    // in harp time with the value received from the message.
#if defined(PICO_RP2040)
    uint64_t curr_total_s  = div_u64u64(harp_time_us_64(), 1'000'000ULL);
#else
    uint64_t curr_total_s  = harp_time_us_64() / 1'000'000ULL;
#endif
    uint64_t new_harp_time_us = curr_total_s + msg_us;
    set_harp_time_us_64(new_harp_time_us);
    // Update time-dependent behavior. Take harp time from this function such
    // that external synchronizer takes priority.
    update_next_heartbeat_from_curr_harp_time_us(harp_time_us_64());
    // Send harp reply.
    // Note: Harp timestamp registers will be updated before dispatching reply.
    send_harp_reply(WRITE, msg.header.address);
}

void HarpCore::write_operation_ctrl(msg_t& msg)
{
    uint8_t& write_byte = *((uint8_t*)msg.payload);
    // Handle OP Mode state-edge logic here since we can force a state change
    // directly.
    const uint8_t& state = self->regs_.r_operation_ctrl_bits.OP_MODE;
    const uint8_t& next_state = (*((OperationCtrlBits*)(&write_byte))).OP_MODE;
    if (state != next_state)
        self->force_state((op_mode_t)next_state);
    // Update register state. Note: DUMP bit always reads as zero.
    self->regs.R_OPERATION_CTRL = write_byte & ~(0x01 << DUMP_OFFSET);
    self->set_visual_indicators(bool((write_byte >> VISUAL_EN_OFFSET) & 0x01));
    // Bail early if we are muted.
    if (self->is_muted())
        return;
    // Tease out flags.
    bool DUMP = bool((write_byte >> DUMP_OFFSET) & 0x01);
    // Send WRITE reply.
    send_harp_reply(WRITE, msg.header.address);
    // DUMP-bit-specific behavior: if set, dispatch one READ reply per register.
    // Apps must also dump their registers.
    if (DUMP)
    {
        for (uint8_t address = 0; address < CORE_REG_COUNT; ++address)
        {
            send_harp_reply(READ, address);
        }
        self->dump_app_registers();
    }
}

void HarpCore::write_reset_dev(msg_t& msg)
{
    uint8_t& write_byte = *((uint8_t*)msg.payload);
    // R_RESET_DEV Register state does not need to be updated since writing to
    // it only triggers behavior.
    // Tease out relevant flags.
    const bool& rst_dev_bit = bool((write_byte >> RST_DEV_OFFSET) & 1u);
    const bool& reset_dfu_bit = bool((write_byte >> RST_DFU_OFFSET) & 1u);
    // Issue a harp reply only if we aren't resetting.
    // TODO: unclear if this is the appropriate behavior.
    // Reset if specified to do so.
#if defined(PICO_RP2040)
    if (reset_dfu_bit)
        reset_usb_boot(0,0);
#else
#pragma warning("Boot-to-DFU-mode via Harp Protocol not supported for this device.")
#endif
    if (rst_dev_bit)
    {
        // Reset core state machine and app.
        self->regs_.r_operation_ctrl_bits.OP_MODE = STANDBY;
        self->reset_app();
    }
    else
        send_harp_reply(WRITE, msg.header.address);
    // TODO: handle the other bit-specific operations.
}

void HarpCore::write_device_name(msg_t& msg)
{
    // TODO.
    // PICO implementation. Write to allocated flash memory
    // since we have no eeprom.
// https://github.com/raspberrypi/pico-examples/blob/master/flash/program/flash_program.c
    write_reg_generic(msg);
}

void HarpCore::write_serial_number(msg_t& msg)
{
    // TODO.
    write_reg_generic(msg);
}

void HarpCore::write_clock_config(msg_t& msg)
{
    // TODO.
    write_reg_generic(msg);
}

void HarpCore::write_timestamp_offset(msg_t& msg)
{
    // TODO.
    write_reg_generic(msg);
}

