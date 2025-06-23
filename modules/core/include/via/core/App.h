/**
 * @file App.h
 * @author TrackieLLM Core Team
 * @brief Defines the main application class for TrackieLLM.
 *
 * @copyright Copyright (c) 2024
 *
 * The App class acts as the central orchestrator for the entire system.
 * It is responsible for the complete lifecycle of the application, including:
 *  - Loading configuration safely via the Rust FFI.
 *  - Initializing the Hardware Abstraction Layer (HAL).
 *  - Creating, initializing, and managing all functional modules (Perception, Reasoning, etc.).
 *  - Handling system signals (like SIGINT/Ctrl+C) for graceful shutdown.
 *  - Running the main application loop.
 *  - Releasing all resources in the correct order upon termination.
 */

#pragma once // Modern include guard

#include <vector>
#include <memory>
#include <string>
#include <atomic>

// Forward-declare the C-style opaque pointer from the Rust config loader.
// This avoids including the C header in this C++ header, reducing coupling.
struct ViaConfig;

// Forward-declare the module interface.
// All modules will be treated through this interface.
namespace via::core {
    class IModule;
}

namespace via::core {

/**
 * @class App
 * @brief The main application class that orchestrates all components of TrackieLLM.
 *
 * This class is designed to be instantiated once in `main()`. It is non-copyable
 * and non-movable to enforce a single point of control over system resources.
 */
class App final {
public:
    /**
     * @brief Constructs the application instance.
     *
     * @param systemConfigPath Path to the system.default.yml file.
     * @param hardwareConfigPath Path to the hardware.default.yml file.
     * @param profileConfigPath Path to the user profile .yml file.
     */
    App(std::string systemConfigPath,
        std::string hardwareConfigPath,
        std::string profileConfigPath);

    /**
     * @brief Destructor. Ensures all resources are released.
     *
     * Calls the shutdown sequence to guarantee that modules are stopped,
     * the HAL is de-initialized, and configuration memory is freed.
     */
    ~App();

    // --- Rule of Five: Make the class non-copyable and non-movable ---
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    /**
     * @brief Runs the main application lifecycle.
     *
     * This is the primary entry point after the App is constructed. It will
     * execute the initialization sequence, run the main loop, and then
     * perform a graceful shutdown.
     *
     * @return int An exit code (EXIT_SUCCESS or EXIT_FAILURE).
     */
    int run();

private:
    /**
     * @brief Performs the entire initialization sequence.
     * @return true on success, false on any critical failure.
     */
    bool initialize();

    /**
     * @brief Performs the entire shutdown sequence.
     */
    void shutdown();

    /**
     * @brief Loads configuration using the safe Rust library.
     * @return true on success, false on failure.
     */
    bool loadConfiguration();

    /**
     * @brief Initializes the Hardware Abstraction Layer (HAL).
     * @return true on success, false on failure.
     */
    bool initializeHAL();

    /**
     * @brief Instantiates and initializes all functional modules.
     *
     * This method will create instances of PerceptionEngine, LlmInterpreter, etc.,
     * and store them as `IModule` pointers in the `m_modules` vector.
     *
     * @return true on success, false on failure.
     */
    bool initializeModules();

    /**
     * @brief Registers a signal handler for graceful shutdown (e.g., on Ctrl+C).
     */
    void registerSignalHandler();

    /**
     * @brief The static signal handler function.
     * @param signal The signal number received.
     */
    static void handleSignal(int signal);

    // --- Member Variables ---

    // An atomic flag to signal the main loop to terminate.
    // Must be static to be accessible from the static signal handler.
    static std::atomic<bool> s_isRunning;

    // Paths to configuration files provided at construction.
    const std::string m_systemConfigPath;
    const std::string m_hardwareConfigPath;
    const std::string m_profileConfigPath;

    // Opaque pointer to the configuration data managed by the Rust library.
    // We are responsible for calling via_config_free() on this pointer.
    ViaConfig* m_config;

    // A collection of all functional modules. Using unique_ptr ensures that
    // module destructors are called automatically when the App is destroyed (RAII).
    std::vector<std::unique_ptr<IModule>> m_modules;
};

} // namespace via::core
