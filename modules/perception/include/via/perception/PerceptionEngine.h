/**
 * @file PerceptionEngine.h
 * @author TrackieLLM Perception Team
 * @brief Defines the main perception module that runs the vision pipeline.
 *
 * @copyright Copyright (c) 2024
 *
 * The PerceptionEngine class is a core functional module of TrackieLLM. It inherits
 * from IModule to integrate with the main application lifecycle.
 *
 * Its primary responsibilities are:
 *  - Running in a dedicated thread to not block the rest of the application.
 *  - Interacting with the Camera HAL to grab raw video frames.
 *  - Managing and executing a pipeline of IProcessor stages (e.g., for
 *    detection, depth estimation) on each frame.
 *  - Publishing the results (packaged in a SceneData object) to the rest of
 *    the system, typically via an event bus or a shared queue.
 */

#pragma once

#include "via/core/IModule.h"
#include "via/perception/IProcessor.h"
#include "via/shared/DataStructures.h"
#include "via/hal/hal.h"

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

namespace via::perception {

/**
 * @class PerceptionEngine
 * @brief The main perception module, implementing the IModule interface and managing a pipeline of IProcessors.
 */
class PerceptionEngine final : public core::IModule {
public:
    /**
     * @brief Default constructor.
     */
    PerceptionEngine();

    /**
     * @brief Destructor. Ensures the worker thread is stopped and joined.
     */
    ~PerceptionEngine() override;

    // --- Rule of Five: Non-copyable and non-movable ---
    PerceptionEngine(const PerceptionEngine&) = delete;
    PerceptionEngine& operator=(const PerceptionEngine&) = delete;
    PerceptionEngine(PerceptionEngine&&) = delete;
    PerceptionEngine& operator=(PerceptionEngine&&) = delete;

    // --- IModule Interface Implementation ---

    /**
     * @brief Initializes the perception engine.
     *
     * Reads configuration, opens the camera via HAL, and creates the
     * configured IProcessor instances for the pipeline.
     * @param config The global configuration object.
     * @return true on success, false otherwise.
     */
    bool initialize(const ViaConfig& config) override;

    /**
     * @brief Starts the perception worker thread.
     * @return true if the thread was started successfully.
     */
    bool start() override;

    /**
     * @brief Stops the perception worker thread gracefully.
     */
    void stop() override;

    /**
     * @brief Gets the name of this module.
     * @return The string "PerceptionEngine".
     */
    const char* getName() const override;

private:
    /**
     * @brief The main function for the worker thread.
     *
     * This loop continuously grabs frames from the camera, runs them through
     * the processing pipeline, and publishes the results.
     */
    void perceptionLoop();

    // --- Member Variables ---

    // A collection of processors that form the vision pipeline.
    // unique_ptr ensures RAII-style resource management for each processor.
    std::vector<std::unique_ptr<IProcessor>> m_pipeline;

    // Handle to the camera device, obtained from the HAL.
    hal_camera_t* m_camera_handle;

    // --- Threading Members ---

    // The dedicated worker thread for all perception tasks.
    std::thread m_thread;

    // An atomic flag to signal the worker thread to terminate.
    // std::atomic is crucial for safe communication between threads without locks.
    std::atomic<bool> m_isRunning;
};

} // namespace via::perception
