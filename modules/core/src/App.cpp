/**
 * @file App.cpp
 * @author TrackieLLM Core Team
 * @brief Implementation of the main application class for TrackieLLM.
 *
 * @copyright Copyright (c) 2024
 *
 * This file contains the logic for orchestrating the entire application lifecycle,
 * from initialization to graceful shutdown.
 */

#include "via/core/App.h"
#include "via/core/IModule.h"

// C-ABI headers for HAL and Config Loader
#include "via/hal/hal.h"
#include "via_config.h"

// Module implementations
#include "via/perception/PerceptionEngine.h"
#include "via/reasoning/LlmInterpreter.h"
// #include "via/audio/AudioEngine.h" // Example for a future module

// C++ Standard Library
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace via::core {

// Initialize the static member for signal handling
std::atomic<bool> App::s_isRunning(false);

// ============================================================================
// Constructor / Destructor
// ============================================================================

App::App(std::string systemConfigPath,
         std::string hardwareConfigPath,
         std::string profileConfigPath)
    : m_systemConfigPath(std::move(systemConfigPath)),
      m_hardwareConfigPath(std::move(hardwareConfigPath)),
      m_profileConfigPath(std::move(profileConfigPath)),
      m_config(nullptr)
{
    std::cout << "TrackieLLM Application instance created." << std::endl;
}

App::~App() {
    // Ensure shutdown is called, even if run() was not explicitly called.
    shutdown();
    std::cout << "TrackieLLM Application instance destroyed." << std::endl;
}

// ============================================================================
// Public Methods
// ============================================================================

int App::run() {
    registerSignalHandler();

    if (!initialize()) {
        std::cerr << "FATAL: Application initialization failed." << std::endl;
        shutdown();
        return EXIT_FAILURE;
    }

    // --- Main Application Loop ---
    // The main thread will block here, keeping the application alive while
    // the modules do their work in their own threads.
    std::cout << "Application running. Press Ctrl+C to exit." << std::endl;
    s_isRunning.store(true);
    while (s_isRunning.load()) {
        // The main thread can perform periodic tasks here if needed,
        // or simply sleep to reduce CPU usage.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutdown signal received. Terminating application..." << std::endl;
    shutdown();

    return EXIT_SUCCESS;
}

// ============================================================================
// Private Lifecycle Methods
// ============================================================================

bool App::initialize() {
    std::cout << "--- Starting Initialization Sequence ---" << std::endl;

    if (!loadConfiguration()) {
        return false;
    }

    if (!initializeHAL()) {
        return false;
    }

    if (!initializeModules()) {
        return false;
    }

    // Start all modules after all have been initialized successfully
    std::cout << "[App] Starting all modules..." << std::endl;
    for (const auto& module : m_modules) {
        std::cout << "  -> Starting module: " << module->getName() << std::endl;
        if (!module->start()) {
            std::cerr << "FATAL: Failed to start module: " << module->getName() << std::endl;
            return false;
        }
    }

    std::cout << "--- Initialization Sequence Complete ---" << std::endl;
    return true;
}

void App::shutdown() {
    std::cout << "--- Starting Shutdown Sequence ---" << std::endl;

    // Stop all modules in reverse order of initialization
    if (!m_modules.empty()) {
        std::cout << "[App] Stopping all modules..." << std::endl;
        for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
            std::cout << "  -> Stopping module: " << (*it)->getName() << std::endl;
            (*it)->stop();
        }
        // The unique_ptrs will automatically deallocate the modules when cleared
        m_modules.clear();
        std::cout << "[App] All modules stopped and deallocated." << std::endl;
    }

    // Shutdown HAL
    hal_shutdown();
    std::cout << "[App] HAL shutdown." << std::endl;

    // Free configuration memory
    if (m_config) {
        via_config_free(m_config);
        m_config = nullptr;
        std::cout << "[App] Configuration memory freed." << std::endl;
    }

    std::cout << "--- Shutdown Sequence Complete ---" << std::endl;
}

bool App::loadConfiguration() {
    std::cout << "[App] Loading configuration..." << std::endl;
    m_config = via_config_load(m_systemConfigPath.c_str(),
                               m_hardwareConfigPath.c_str(),
                               m_profileConfigPath.c_str());
    if (!m_config) {
        std::cerr << "FATAL: Failed to load configuration files." << std::endl;
        return false;
    }
    std::cout << "[App] Configuration loaded successfully." << std::endl;
    return true;
}

bool App::initializeHAL() {
    std::cout << "[App] Initializing Hardware Abstraction Layer (HAL)..." << std::endl;
    if (hal_initialize() != HAL_STATUS_OK) {
        std::cerr << "FATAL: Failed to initialize HAL." << std::endl;
        return false;
    }
    std::cout << "[App] HAL initialized successfully." << std::endl;
    return true;
}

bool App::initializeModules() {
    std::cout << "[App] Initializing all modules..." << std::endl;

    // --- Create Module Instances ---
    // The order of creation can be important if modules depend on each other
    // via an event bus or other mechanism, but here they are independent.
    m_modules.push_back(std::make_unique<perception::PerceptionEngine>());
    m_modules.push_back(std::make_unique<reasoning::LlmInterpreter>());
    // m_modules.push_back(std::make_unique<audio::AudioEngine>());

    // --- Initialize Each Module ---
    for (const auto& module : m_modules) {
        std::cout << "  -> Initializing module: " << module->getName() << std::endl;
        if (!module->initialize(*m_config)) {
            std::cerr << "FATAL: Failed to initialize module: " << module->getName() << std::endl;
            return false;
        }
    }

    std::cout << "[App] All modules initialized successfully." << std::endl;
    return true;
}

// ============================================================================
// Signal Handling
// ============================================================================

void App::registerSignalHandler() {
    signal(SIGINT, App::handleSignal);  // Handle Ctrl+C
    signal(SIGTERM, App::handleSignal); // Handle termination signal
}

void App::handleSignal(int signal) {
    std::cout << "\nSignal " << signal << " received. Requesting shutdown..." << std::endl;
    s_isRunning.store(false);
}

} // namespace via::core
