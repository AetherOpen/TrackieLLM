/**
 * @file IModule.h
 * @author TrackieLLM Core Team
 * @brief Defines the interface for all functional modules in the TrackieLLM system.
 *
 * @copyright Copyright (c) 2024
 *
 * This file specifies the abstract base class `IModule`. Any class that represents a
 * major functional block of the application (e.g., perception, reasoning, audio processing)
 * must inherit from this class and implement its pure virtual functions.
 *
 * This polymorphic design allows the main `App` orchestrator to manage a
 * collection of different modules through a common pointer type (`IModule*`),
 * invoking their lifecycle methods (`initialize`, `start`, `stop`) in a
 * standardized way.
 */

#pragma once

// Forward-declare the C-style opaque pointer from the Rust config loader.
// This avoids including the C header in this C++ header, reducing coupling.
// We pass it by const reference to the initialize method.
struct ViaConfig;

namespace via::core {

/**
 * @class IModule
 * @brief An interface (abstract base class) for a functional module.
 *
 * Defines the essential lifecycle methods that the main `App` will call.
 * Modules are non-copyable and non-movable by design to prevent slicing
 * and ownership issues.
 */
class IModule {
public:
    /**
     * @brief Virtual destructor.
     *
     * CRITICAL: A virtual destructor is mandatory for any base class that will be
     * managed polymorphically (i.e., deleted through a base class pointer).
     * This ensures that the derived class's destructor is called correctly,
     * preventing resource leaks.
     */
    virtual ~IModule() = default;

    /**
     * @brief Initializes the module with its specific configuration.
     *
     * This method is called once by the `App` during the startup sequence.
     * Implementations should perform tasks like loading models, allocating memory,
     * and validating settings from the configuration.
     *
     * @param config A constant reference to the global configuration object.
     *               The module can query this object for its required settings.
     * @return `true` if initialization was successful, `false` otherwise.
     */
    virtual bool initialize(const ViaConfig& config) = 0;

    /**
     * @brief Starts the module's main execution logic.
     *
     * This method is called after all modules have been successfully initialized.
     * For modules that run continuously (e.g., in their own thread), this is
     * where the thread should be started.
     *
     * @return `true` if the module started successfully, `false` otherwise.
     */
    virtual bool start() = 0;

    /**
     * @brief Stops the module's execution and cleans up resources.
     *
     * This method is called by the `App` during the graceful shutdown sequence.
     * Implementations must ensure they stop all running threads, release file
     * handles, deallocate memory, etc. This method should block until the
     * module has completely stopped.
     */
    virtual void stop() = 0;

    /**
     * @brief Returns the human-readable name of the module.
     *
     * Used primarily for logging and debugging purposes.
     *
     * @return A constant, null-terminated string with the module's name.
     */
    virtual const char* getName() const = 0;

protected:
    // --- Rule of Five: Make the interface non-copyable and non-movable ---
    // This prevents object slicing when dealing with polymorphic types.
    IModule() = default; // Allow derived classes to default-construct.
    IModule(const IModule&) = delete;
    IModule& operator=(const IModule&) = delete;
    IModule(IModule&&) = delete;
    IModule& operator=(IModule&&) = delete;
};

} // namespace via::core
