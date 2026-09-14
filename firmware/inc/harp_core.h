#ifndef HARP_CORE_H
#define HARP_CORE_H
#include <stdint.h>
#include <harp_message.h>
#include <reg_spec.h>
#include <core_registers.h>
#include <harp_synchronizer.h>
#include <arm_regs.h>
#include <cstring> // for memcpy
#include <tusb.h>
#include <utility> // for std::to_underlying

// Pico-specific includes.
#include <hardware/structs/timer.h>
#include <pico/divider.h> // for fast hardware division with remainder.
#include <hardware/timer.h>
#include <pico/unique_id.h>
#include <pico/bootrom.h>

// Project version
inline constexpr semver_t PICO_CORE_VERSION = {1, 1, 0};

// Version of the Harp Protocol that this library most closely implements.
inline constexpr semver_t HARP_PROTOCOL = {2, 0, 0};

inline constexpr uint8_t RPI_CORE_ID[] = {'r', 'p', 'i'};

#define NO_PC_INTERVAL_US (3'000'000UL) // Threshold duration. If the connection
                                        // with the PC has been inactive for
                                        // this duration, op mode should switch
                                        // to IDLE.
#define HEARTBEAT_ACTIVE_INTERVAL_US (1'000'000UL)
#define HEARTBEAT_STANDBY_INTERVAL_US (3'000'000UL)

/**
 * \brief enum for easier interpretation of the OP_MODE bitfield in the
 *  R_OPERATION_CTRL register.
 */
enum op_mode_t: uint8_t
{
    STANDBY = 0,
    ACTIVE = 1,
    RESERVED = 2,
    SPEED = 3
};


/**
 * \brief Harp Core that handles management of common bank registers.
*       Implemented as a singleton to simplify attaching interrupt callbacks
*       (and since you can only have one per device.)
 */
class HarpCore
{
using enum reg_type_t;


// Make constructor protected to prevent creating instances outside of init().
protected: // protected, but not private, to enable derived class usage.
    HarpCore(uint16_t who_am_i, semver_t firmware, semver_t hardware,
             const char name[],
             const uint8_t tag[],
             const uint8_t interface_hash[]);

    ~HarpCore();

public:

    static inline constexpr size_t APP_REG_START_ADDRESS = 32;

    HarpCore() = delete;  // Disable default constructor.
    HarpCore(HarpCore& other) = delete; // Disable copy constructor.
    void operator=(const HarpCore& other) = delete; // Disable assignment operator.

/**
 * \brief initialize the harp core singleton with parameters and init Tinyusb.
 * \note default constructor, copy constructor, and assignment operator have
 *  been disabled.
 */
    static HarpCore& init(uint16_t who_am_i, semver_t firmware, semver_t hardware,
                          const char name[],
                          const uint8_t tag[],
                          const uint8_t interface_hash[]);

    static inline HarpCore* self = nullptr; // pointer to the singleton instance.
    static HarpCore& instance() {return *self;} ///< returns the singleton.


/**
 * \brief Periodically handle tasks based on the current time, state,
 *      and inputs. Should be called in a loop. Calls tud_task() and
 *      process_cdc_input().
 */
    void run();

/**
 * \brief return a reference to the message header in the #rx_buffer_.
 * \warning this should only be accessed if new_msg() is true.
 */
    msg_header_t& get_buffered_msg_header()
    {return *((msg_header_t*)(&rx_buffer_));}

/**
 * \brief return a reference to the message in the #rx_buffer_. Inline.
 * \warning this should only be accessed if new_msg() is true.
 */
    msg_t get_buffered_msg();

/**
 * \brief flag indicating whether or not a new message is in the #rx_buffer_.
 */
    bool new_msg()
    {return new_msg_;}

/**
 * \brief generic handler function to write a message payload to a core or
 *      app register and issue a harp reply (unless is_muted()).
 * \note this function may be used in cases where no actions must trigger from
        writing to this register.
 * \note since the struct is byte-aligned, writing more data than the size of
 *      the register will sequentially write to the next register within the
 *      app range and core range. In this way,
 *      you can write to multiple sequential registers starting from the
 *      msg.address.
 */
    static void write_reg_generic(msg_t& msg);

/**
 * \brief generic handler function to read a message payload to a core or
 *      app register and issue a harp reply (unless is_muted()).
 * \note this function may be used in cases where (1) the register value is
 *      up-to-date and (2) no actions must trigger from reading this register.
 */
    static void read_reg_generic(uint8_t reg_name);


/**
 * \brief read-error handler function. Send a zero-length payload harp reply
 *  from the specified register with the reply type set as  `READ_ERROR` to
 *  indicate a read error.
 * \note This function is provided for convenience where no payload is
 *  necessary. For alternate circumstances where a specific payload must be
 *  included, invoke send_harp_reply() directly instead.
 */
    static void read_reg_error(uint8_t reg_name);

/**
 * \brief write-error handler function. Send a zero-length payload harp reply
 *  from the specified register with the reply type set as `WRITE_ERROR` to
 *  indicate a write error.
 * \note This function is provided for convenience where no payload is
 *  necessary. For alternate circumstances where a specific payload must be
 *  included, invoke send_harp_reply() directly instead.
 */
    static void write_reg_error(msg_t& msg);


/**
 * \brief update local (app or core) register data with the payload provided in
 *  the input msg.
 */
    static inline void copy_msg_payload_to_register(msg_t& msg)
    {
        const RegSpec& spec = reg_address_to_spec(msg.header.address);
        memcpy((void*)spec.base_ptr, msg.payload, spec.num_bytes);
    }

/**
 * \brief Construct and send a Harp-compliant timestamped reply message from
 *  provided arguments.
 * \note this function is static such that we can write functions that invoke it
 *  before instantiating the HarpCore singleton.
 * \note Calls `tud_task()`.
 * \note will update the timestamp registers.
 * \param reply_type `READ`, `WRITE`, `EVENT`, `READ_ERROR`, or `WRITE_ERROR` enum.
 * \param reg_name address to mark the origin point of the data.
 * \param data pointer to payload content of the data.
 * \param num_bytes `sizeof(data)`
 * \param payload_type `U8`, `S8`, `U16`, `U32`, `U64`, `S64`, or `Float` enum.
 * \param harp_time_us the harp time (in microseconds) to timestamp onto the
 *  outgoing message.
 */
    static void send_harp_reply(msg_type_t reply_type, uint8_t reg_name,
                                const volatile void* data, uint8_t num_bytes,
                                reg_type_t payload_type, uint64_t harp_time_us);

/**
 * \brief Construct and send a Harp-compliant timestamped reply message from
 *  provided arguments. Timestamp is generated automatically at the time this
 *  function is called.
 * \note this function is static such that we can write functions that invoke it
 *  before instantiating the HarpCore singleton.
 * \note Calls `tud_task()`.
 * \param reply_type `READ`, `WRITE`, `EVENT`, `READ_ERROR`, or `WRITE_ERROR` enum.
 * \param reg_name address to mark the origin point of the data.
 * \param data pointer to payload content of the data.
 * \param num_bytes `sizeof(data)`
 * \param payload_type `U8`, `S8`, `U16`, `U32`, `U64`, `S64`, or `Float` enum.
 */
    static inline void send_harp_reply(msg_type_t reply_type, uint8_t reg_name,
                                       const volatile void* data,
                                       uint8_t num_bytes,
                                       reg_type_t payload_type)
    {return send_harp_reply(reply_type, reg_name, data, num_bytes, payload_type,
                            harp_time_us_64());}

/**
 * \brief Construct and send a Harp-compliant timestamped reply message where
 *  payload data is written from the specified register.
 * \details this function will lookup the particular core-or-app register's
 *  specs for the provided address and construct a reply based on those specs.
 * \note this function is static such that we can write functions that invoke it
 *  before instantiating the HarpCore singleton.
 * \note Calls `tud_task()`.
 * \param reply_type `READ`, `WRITE`, `EVENT`, `READ_ERROR`, or `WRITE_ERROR` enum.
 * \param reg_name address to mark the origin point of the data.
 */
    static inline void send_harp_reply(msg_type_t reply_type, uint8_t reg_name)
    {
        const RegSpec& spec = reg_address_to_spec(reg_name);
        send_harp_reply(reply_type, reg_name, spec.base_ptr, spec.num_bytes,
                        spec.payload_type);
    }

/**
 * \brief Send a Harp-compliant reply with a specific timestamp.
 * \note this function is static such that we can write functions that invoke it
 *  before instantiating the HarpCore singleton.
 * \note Calls `tud_task()`.
 * \param reply_type `READ`, `WRITE`, `EVENT`, `READ_ERROR`, or `WRITE_ERROR` enum.
 * \param reg_name address to mark the origin point of the data.
 * \param harp_time_us the harp time (in microseconds) to timestamp onto the
 *  outgoing message.
 */
    static inline void send_harp_reply(msg_type_t reply_type, uint8_t reg_name,
                                       uint64_t harp_time_us)
    {
        const RegSpec& spec = reg_address_to_spec(reg_name);
        send_harp_reply(reply_type, reg_name, spec.base_ptr, spec.num_bytes,
                        spec.payload_type, harp_time_us);
    }

/**
 * \brief true if the mute flag has been set in the R_OPERATION_CTRL register.
 */
    static inline bool is_muted()
    {return bool(self->regs_.r_operation_ctrl_bits.MUTE_RPL);}

/**
 * \brief true if the device is synchronized via external CLKIN input.
 * \details true if the device has received and handled at least one
 *  synchronization signal from its external CLKIN input. As implemented, this
 *  function will never return false after synchronizing at least once, but
 *  that may change later.
 */
    static inline bool is_synced()
    {
        return (self->sync_ == nullptr)?
            false:
            self->sync_->is_synced();
    }

/**
 * \brief true if the "events enabled" flag has been set in the
 *  R_OPERATION_CTRL register. Equivalent to the device Op Mode being ACTIVE.
 */
    static inline bool events_enabled()
    {return self->get_op_mode() == ACTIVE;}

/**
 * \brief get the total elapsed microseconds (64-bit) in "Harp" time.
 * \details  Internally, an offset is tracked and updated where
 *  \f$t_{Harp} = t_{local} - t_{offset} \f$
 * \warning this value is not monotonic and can change at any time if (1) an
 *  external synchronizer is physically connected and operating and (2) this
 *  class instance has configured a synchronizer with set_synchronizer().
 */
    static inline uint64_t harp_time_us_64()
    {return system_to_harp_us_64(time_us_64());}

/**
 * \brief get the current elapsed seconds in "Harp" time.
 * \note the returned seconds are rounded down to the most recent second that
 *  has elapsed.
 */
    static inline uint32_t harp_time_s()
    {
        self->update_timestamp_regs(); // calls harp_time_us_64() internally.
        return self->regs_.R_TIMESTAMP_SECOND;
    }

/**
 * \brief convert harp time (in 64-bit microseconds) to local system time
 *  (in 64-bit microseconds).
 * \details this utility function is useful for setting alarms in the device's
 *  local time domain, which is monotonic and unchanged by adjustments to
 *  the harp time.
 * \note if synchronizer is attached, the conversion will be in reference to
 *  the externally synchronized time.
 * \param harp_time_us the current time in microseconds
 */
    static inline uint64_t harp_to_system_us_64(uint64_t harp_time_us)
    {return (self->sync_ == nullptr)?
                harp_time_us + self->offset_us_64_:
                self->sync_->harp_to_system_us_64(harp_time_us);}

/**
 * \brief convert harp time (in 32-bit microseconds) to local system time
 *  (in 32-bit microseconds).
 * \details this utility function is useful for setting alarms in the device's
 *  local time domain, which is monotonic and unchanged by adjustments to
 *  the harp time.
 * \note if synchronizer is attached, the conversion will be in reference to
 *  the externally synchronized time.
 * \param harp_time_us the current time in microseconds
 */
    static inline uint32_t harp_to_system_us_32(uint64_t harp_time_us)
    {return uint32_t(harp_to_system_us_64(harp_time_us));}

/**
 * \brief convert system time (in 64-bit microseconds) to local system time
 *  (in 64-bit microseconds).
 * \details this utility function is useful for timestamping events in the
 *  local time domain and then calculating when they happened in Harp time.
 * \note If the synchronizer is attached, the conversion will be in referenced
 *  to the synchronized time.
 * \note A `system_to_harp_us_32()` command does not exist because Harp time
 *  is only available in 64-bit time.
 * \param system_time_us the current system time in microseconds
 */
    static inline uint64_t system_to_harp_us_64(uint64_t system_time_us)
    {return (self->sync_ == nullptr)?
                system_time_us - self->offset_us_64_:
                self->sync_->system_to_harp_us_64(system_time_us);}

/**
 * \brief Override the current Harp time with a specific time.
 * \note useful if a separate entity besides the synchronizer input jack
 *  needs to set the time (i.e: specifying the time over Harp protocol by
 *  writing to timestamp registers).
 * \note If a synchronizer is attached, this function will override the
 *  synchronizer's time also.
 */
    static inline void set_harp_time_us_64(uint64_t harp_time_us)
    {if (self->sync_ != nullptr)
        self->sync_->set_harp_time_us_64(harp_time_us);
     self->offset_us_64_ = time_us_64() - harp_time_us;}

/**
 * \brief attach a synchronizer. If the synchronizer is attached, then calls to
 *  harp_time_us_64() and harp_time_us_32() will reflect the synchronizer's
 *  time.
 */
    static void set_synchronizer(HarpSynchronizer* sync)
    {self->sync_ = sync;}

/**
 * \brief attach a callback function to control external visual indicators
 *  (i.e: LEDs).
 */
    static void set_visual_indicators_fn(void (*func)(bool))
    {self->set_visual_indicators_fn_ = func;}

/**
 * \brief assign functions that control the external OP_LED.
 */
    inline void set_op_led_fns(void (*set_led_fn)(bool), bool (*get_led_fn)())
    {
        set_led_fn_ = set_led_fn;
        get_led_fn_ = get_led_fn;
    }

/**
 * \brief attach a handler function for dealing with writes to the
 * r_clock_config register.
 * \warning like all other write handler functions, this function must send
 * a harp reply at the end of the function only if the device is not muted.
 */
    static void set_r_clock_config_write_handler(void (*func)(msg_t&))
    {self->handle_r_clock_config_write_fn_ = func;}

/**
 * \brief force the op mode state. Useful to put the core in an error state.
 */
    static void force_state(op_mode_t next_state)
    {self->update_state(true, next_state);}

/**
 * \brief return the Operaion Mode (STANDBY, ACTIVE, SPEED)
 */
    static inline op_mode_t get_op_mode()
    {return op_mode_t(self->regs_.r_operation_ctrl_bits.OP_MODE);}

/**
 * \brief return a reference to the specified core or app register's specs used
 *  for issuing a harp reply for that register.
 * \details address	is the full address range where 0 is the first core
 *  register, and APP_REG_START_ADDRESS is the first app register.
 */
    static inline const RegSpec& reg_address_to_spec(uint8_t address)
    {
        if (address < CORE_REG_COUNT)
            return self->core_reg_specs_[address];
        return self->address_to_app_reg_spec(address); // virtual. Implemented by app.
    }

protected:
/**
 * \brief flag that new message has been handled. Inline.
 * \note Does not affect internal behavior.
 */
    void clear_msg()
    {new_msg_ = false;}

/**
 * \brief entry point for handling incoming harp messages to core registers.
 *      Dispatches message to the appropriate handler.
 */
    void handle_buffered_core_message();

/**
 * \brief Handle incoming messages for the derived class. Does nothing here,
 *  but not pure virtual since we need to be able to instantiate a standalone
 *  harp core.
 */
    virtual void handle_buffered_app_message(){};

/**
 * \brief update state of the derived class. Does nothing in the base class,
 *  but not pure virtual since we need to be able to instantiate a standalone
 *  harp core.
 */
    virtual void update_app_state(){};

/**
 * \brief reset the app. Called when the writing to the RESET_DEF register.
 *  Base class implementation restore non-read-only registers.
 *  Child class implementation calls the user-specified reset function.
 */
    virtual void reset_app();


/**
 * \brief Enable or disable external virtual indicators.
 */
    inline void set_visual_indicators(bool enabled)
    {
        if (set_visual_indicators_fn_ != nullptr)
        set_visual_indicators_fn_(enabled);
    }

    inline void set_led(bool enabled)
    {
        if (set_led_fn_ != nullptr)
            set_led_fn_(enabled);
    }

    inline bool get_led()
    {
        if (get_led_fn_ != nullptr)
            return get_led_fn_();
        return 0;
    }

/**
 * \brief send one harp reply read message per app register.
 *  Called when the writing to the R_OPERATION_CTRL's DUMP bit.
 *  Does nothing in the base class, but not pure virtual since we need to be
 *  able to instantiate a standalone harp core.
 */
    virtual void dump_app_registers(){};

    virtual const RegSpec& address_to_app_reg_spec(uint8_t address)
    {return core_reg_specs_[0];} // should never happen.

/**
 * \brief flag indicating whether or not a new message is in the #rx_buffer_.
 */
    bool new_msg_;

/**
 * \brief function pointer to function that enables/disables visual indicators.
 */
    void (* set_visual_indicators_fn_)(bool);

/**
 * \brief function pointer to function that enables/disables OP_LED.
 */
    void (* set_led_fn_)(bool);

/**
 * \brief function pointer to function that reads the state of the OP_LED.
 */
    bool (* get_led_fn_)();

/**
 * \brief function pointer. if not null, call this function when writing to
 * the `R_CLOCK_CONFIG` register.
 */
    void (* handle_r_clock_config_write_fn_)(msg_t&);

/**
 * \brief function pointer to synchronizer if configured.
 */
    HarpSynchronizer* sync_;

private:
/**
 * \brief recompute the next heartbeat event time based on the current time.
 */
    static inline void update_next_heartbeat_from_curr_harp_time_us(
        uint64_t curr_harp_time_us)
    {
        // Recompute next whole second (in [us]) based on synchronized time.
        // Round *up* to the nearest whole second.
#if defined(PICO_RP2040) // Use 2040-specific integer hardware divider.
        uint64_t remainder;
        uint64_t quotient = divmod_u64u64_rem(curr_harp_time_us, 1'000'000ULL,
                                              &remainder);
 #else
        uint64_t remainder = curr_harp_time_us % 1'000'000ULL;
 #endif
        self->next_heartbeat_time_us_ =
            harp_to_system_us_32(curr_harp_time_us - remainder)
            + self->heartbeat_interval_us_;
    }
/**
 * \brief the total number of bytes read into the the msg receive buffer.
 *  This is implemented as a read-only reference to the #rx_buffer_index_.
 */
    const uint8_t& total_bytes_read_;

/**
 * \brief buffer to contain data read from the serial port.
 */
    uint8_t rx_buffer_[MAX_PACKET_SIZE];

/**
 * \brief #rx_buffer_ index where the next incoming byte will be written.
 */
    uint8_t rx_buffer_index_;

/**
 * \brief local offset from "Harp time" to device hardware timer tracing
 *  elapsed microseconds since boot, where
 *  \f$t_{offset} = t_{local} - t_{Harp} \f$
 * \note if a synchronizer is attached with set_synchronizer(), then
 * this value is not used.
 */
    uint64_t offset_us_64_;

/**
 * \brief next time a heartbeat message is scheduled to issue.
 * \note only valid if Op Mode is in the ACTIVE state.
 * \note this value is currently specified in 32-bit local system time
 *  since it is a short interval.
 */
    uint32_t next_heartbeat_time_us_;

/**
 * \brief the current interval at which the \p next_neartbeat_time_us_ is being
 * updated.
 */
    uint32_t heartbeat_interval_us_;

/**
 * \brief last time device detects no connection with the PC in microseconds.
 * \note only valid if Op Mode is not in STANDBY mode.
 * \note this value is currently specified in 32-bit local system time
 *  since it is a short interval.
 */
    uint32_t disconnect_start_time_us_;

/**
 * \brief flag to indicate the the device was disconnected and the event has
 *  been handled.
 */
    bool disconnect_handled_;

/**
 * \brief flag to indicate the the device was connected and the event has
 *  been handled.
 */
    bool connect_handled_;

/**
 * \brief true if the device has synchronized and all consequential activity
 *  has been handled.
 */
    bool sync_handled_;

/**
 * \brief Read incoming bytes from the USB serial port. Does not block.
 *  \warning If called again before handling previous message in the buffer, the
 *      buffered message may be be overwritten if a new message has arrived.
 */
    void process_cdc_input();

/**
 * \brief update internal state machine.
 * \param force. If true, the state will change to the #forced_next_state.
 *  Otherwise, the #forced_next_state is ignored.
 * \param forced_next_state if #force then this is the next state that the
 *  op mode state machine will enter.
 */
    static void update_state(bool force = false,
                             op_mode_t forced_next_state = STANDBY);


/**
 * \brief Write the current Harp time to the timestamp registers.
 * \warning must be called before timestamp registers are read.
 */
    static inline void update_timestamp_regs()
    {return set_timestamp_regs(harp_time_us_64());}


/**
 * \brief Write the a specified Harp time to the timestamp registers.
 */
    static void set_timestamp_regs(uint64_t harp_time_us);

    // core register read handler functions. Handles read operations on those
    // registers. One-per-harp-register where necessary, but read_reg_generic()
    // can be used in most cases.
    // Note: these all need to have the same function signature.
    static void read_uuid(uint8_t reg_name);


    static inline void update_heartbeat_register()
    {
        const uint8_t& state = self->regs_.r_operation_ctrl_bits.OP_MODE;
        self->regs_.R_HEARTBEAT = ((state == ACTIVE? 1: 0) << 1) | (is_synced()? 1: 0);
    }

/**
 * \brief read the [Heartbeat][https://github.com/harp-tech/protocol/blob/main/Device.md#r_heartbeat-u16--device-status-information]
 * register.
 */
    static void read_heartbeat(uint8_t reg_name);

/**
 * \brief identify (via underlying register) whether this device is a clock
 * generator (false by default).
 */
    static inline void set_is_clock_generator(bool is_clock_gen)
    {self->regs_.r_clock_config_bits.CLK_GEN = is_clock_gen;}

    // write handler function per core register. Handles write
    // operations to that register.
    // Note: these all need to have the same function signature.

/**
 * \brief Handle writing to the `R_TIMESTAMP_SECOND` register and update the
 *  device's Harp time to reflect the seconds written to this register.
 */
    static void write_timestamp_second(msg_t& msg);


    static void write_operation_ctrl(msg_t& msg);
    static void write_reset_dev(msg_t& msg);

    static void write_r_clock_config_default(msg_t& msg);

    CoreRegValues regs_; ///< struct of Harp core register values.

    // Lookup table. Necessary because register values are not of equal size,
    //  so we can't index into them directly via enum value.
    const RegSpec core_reg_specs_[CORE_REG_COUNT] =
    {RegSpec::U16((void*)&regs_.R_WHO_AM_I,
                  read_reg_generic, write_reg_error),
     RegSpec::U8((void*)&regs_.R_HW_VERSION_H,
                 read_reg_generic, write_reg_error),
     RegSpec::U8((void*)&regs_.R_HW_VERSION_L,
                 read_reg_generic, write_reg_error),
     RegSpec::U8((void*)&regs_.R_ASSEMBLY_VERSION,
                 read_reg_generic, write_reg_error),
     RegSpec::U8((void*)&regs_.R_HARP_VERSION_H,
                 read_reg_generic, write_reg_error),
     RegSpec::U8((void*)&regs_.R_HARP_VERSION_L,
                 read_reg_generic, write_reg_error),
     RegSpec::U8((void*)&regs_.R_FW_VERSION_H,
                 read_reg_generic, write_reg_error),
     RegSpec::U8((void*)&regs_.R_FW_VERSION_L,
                 read_reg_generic, write_reg_error),
     RegSpec::U32(&regs_.R_TIMESTAMP_SECOND,
                  read_reg_generic, write_timestamp_second),
     RegSpec::U16(&regs_.R_TIMESTAMP_MICRO,
                  read_reg_generic, write_reg_error),
     RegSpec::U8(&regs_.R_OPERATION_CTRL,
                  read_reg_generic, write_operation_ctrl),
     RegSpec::U8(&regs_.R_RESET_DEV,
                 read_reg_generic, write_reset_dev),
     RegSpec::U8Array(&regs_.R_DEVICE_NAME,  sizeof(regs_.R_DEVICE_NAME),
                      read_reg_generic, write_reg_generic),
     RegSpec::U16(&regs_.R_SERIAL_NUMBER,
                  read_reg_generic, write_reg_generic),
     RegSpec::U8(&regs_.R_CLOCK_CONFIG,
                 read_reg_generic, write_r_clock_config_default),
     RegSpec::U8(&regs_.R_TIMESTAMP_OFFSET,
                 read_reg_generic, write_reg_error),
     RegSpec::U8Array(&regs_.R_UUID, sizeof(regs_.R_UUID),
                      read_uuid, write_reg_error),
     RegSpec::U8Array(&regs_.R_TAG, sizeof(regs_.R_TAG),
                      read_reg_generic, write_reg_error),
     RegSpec::U16(&regs_.R_HEARTBEAT,
                  read_heartbeat, write_reg_error),
     RegSpec::U8Array(&regs_.R_VERSION, sizeof(regs_.R_VERSION),
                      read_reg_generic, write_reg_error),
    };
};

#endif //HARP_CORE_H
