/**
 * @file IProcessor.h
 * @author TrackieLLM Perception Team
 * @brief Defines the interface for a single processing unit within the perception pipeline.
 *
 * @copyright Copyright (c) 2024
 *
 * This file specifies the abstract base class `IProcessor`. Any class that performs
 * a discrete computer vision or AI inference task (e.g., object detection,
 * depth estimation, face recognition) must inherit from this class.
 *
 * This design allows the main `PerceptionEngine` to manage a flexible and
 * reconfigurable pipeline of different processing steps. The engine can
 * iterate over a collection of `IProcessor` pointers and execute them in sequence
 * on a given scene, with each processor adding its results to a shared data structure.
 */

#pragma once

// Forward-declare the primary data structure that flows through the pipeline.
// This avoids a direct include dependency on DataStructures.h in this interface file.
namespace via::shared {
    struct SceneData;
}

namespace via::perception {

/**
 * @class IProcessor
 * @brief An interface (abstract base class) for a perception processing stage.
 *
 * Defines the contract for a single, self-contained step in the vision pipeline.
 * Processors are non-copyable and non-movable to prevent ownership issues and
 * object slicing.
 */
class IProcessor {
public:
    /**
     * @brief Virtual destructor.
     *
     * Essential for ensuring that derived class destructors are called when an
     * object is deleted via an `IProcessor*` pointer, preventing resource leaks.
     */
    virtual ~IProcessor() = default;

    /**
     * @brief Executes the processing logic of this stage on the given scene data.
     *
     * This is the core method of the interface. An implementation will read data
     * from the `scene_data` object (e.g., the camera frame) and write its results
     * back into the same object (e.g., a list of detected bounding boxes).
     *
     * @param scene_data A reference to the shared data object for the current frame.
     *                   This object is passed through each stage of the pipeline.
     * @return `true` if the processing was successful, `false` otherwise. A failure
     *         might cause the `PerceptionEngine` to skip subsequent stages.
     */
    virtual bool process(shared::SceneData& scene_data) = 0;

    /**
     * @brief Returns the human-readable name of the processor.
     *
     * Useful for logging, debugging, and profiling the perception pipeline.
     *
     * @return A constant, null-terminated string with the processor's name
     *         (e.g., "YOLOv8_Detector", "MiDaS_Estimator").
     */
    virtual const char* getName() const = 0;

protected:
    // --- Rule of Five: Enforce non-copyable, non-movable semantics ---
    // This is a standard safety measure for polymorphic base classes.
    IProcessor() = default;
    IProcessor(const IProcessor&) = delete;
    IProcessor& operator=(const IProcessor&) = delete;
    IProcessor(IProcessor&&) = delete;
    IProcessor& operator=(IProcessor&&) = delete;
};

} // namespace via::perception
