#ifndef HARP_C_APP_H
#define HARP_C_APP_H
#include <harp_core.h>
#include <core_registers.h>
#include <reg_types.h>

/**
 * \brief Harp C-style App that handles core behaviors in addition to
*   reads/writes to app-specific registers.
*   Implemented as a singleton to simplify attaching interrupt callbacks
*   (and since you can only have one per device).
 */
class HarpCApp: public HarpCore
{

// Make constructor private to prevent creating instances outside of init().
private:
/**
 * \brief constructor
 * \param app_reg_specs array of reg specs, indexed by app register address.
 * \param app_register_count number of app registers
 * \param app_reg_count number of app registers.
 * \param update_fn pointer to function that will be called periodically to
 *  update the app state.
 * \param reset_fn pointer to function that will reset the app state.
 */
    HarpCApp(uint16_t who_am_i, semver_t firmware, semver_t hardware,
             const char name[],
             const uint8_t tag[],
             const uint8_t interface_hash[],
             RegSpec* app_reg_specs, size_t app_reg_count,
             void (* update_fn)(void), void (* reset_fn)(void));

    ~HarpCApp();

public:
    HarpCApp() = delete;  // Disable default constructor.
    HarpCApp(HarpCApp& other) = delete; // Disable copy constructor.
    void operator=(const HarpCApp& other) = delete; // Disable assignment operator.

/**
 * \brief initialize the harp core app singleton with parameters and init Tinyusb.
 */
    static HarpCApp& init(uint16_t who_am_i, semver_t firmware, semver_t hardware,
                          const char name[],
                          const uint8_t tag[],
                          const uint8_t interface_hash[],
                          RegSpec* app_reg_specs, size_t app_reg_count,
                          void (* update_fn)(void), void (*reset_fn)(void));

    static inline HarpCApp* self = nullptr; // pointer to the singleton instance.
    static HarpCApp& instance() {return *self;} ///< returns the singleton.

private:
/**
 * \brief entry point for handling incoming harp messages to core registers.
 *  Dispatches message to the appropriate handler.
 */
    void handle_buffered_app_message();

/**
 * \brief update app state. Readable registers can be updated here.
 *  Implements virtual member fn in base class of the same name.
 */
    void update_app_state()
    {update_fn_();}

/**
 * \brief Reset the app state.
 *  Implements virtual member fn in base class of the same name.
 */
    void reset_app()
    {reset_fn_();}

/**
 * \brief send one harp reply read message per app register.
 *  Implements virtual member fn in base class of the same name.
 */
    void dump_app_registers();

/**
 * \brief return app address's specs from the specified register address.
 * \param address is the full address range where 0 is the first core register,
 *  and APP_REG_START_ADDRESS is the first app register.
 * \details used in Harp Core to extract specs for a particular app register.
 */
    inline const RegSpec& address_to_app_reg_spec(uint8_t address)
    {return app_reg_specs_[address - APP_REG_START_ADDRESS];}

// Private Members
    RegSpec* app_reg_specs_;
    size_t app_reg_count_;
    void (* update_fn_)(void);
    void (* reset_fn_)(void);
};

#endif //HARP_C_APP_H
