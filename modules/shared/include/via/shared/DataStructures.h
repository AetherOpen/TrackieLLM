/**
 * @file DataStructures.h
 * @author TrackieLLM Core Team
 * @brief Defines common data structures used across all TrackieLLM modules.
 *
 * @copyright Copyright (c) 2024
 *
 * This header provides the definitions for fundamental data types that are
 * created, passed between, and consumed by different parts of the application.
 *
 * By centralizing these definitions, we ensure consistency and prevent code
 * duplication. The structures are designed to be simple Plain Old Data (POD)
 * types for performance and ease of serialization if needed in the future.
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>

// Include the HAL header for hal_frame_t, as it's the raw input source.
#include "via/hal/hal.h"

namespace via::shared {

/**
 * @struct BoundingBox
 * @brief Represents a single detected object in an image.
 *
 * Coordinates are typically normalized to the range [0.0, 1.0] relative to
 * the image dimensions, but can also be in absolute pixel values depending
 * on the context.
 */
struct BoundingBox {
    float x1 = 0.0f;          // Top-left corner, x-coordinate
    float y1 = 0.0f;          // Top-left corner, y-coordinate
    float x2 = 0.0f;          // Bottom-right corner, x-coordinate
    float y2 = 0.0f;          // Bottom-right corner, y-coordinate
    float score = 0.0f;       // The confidence score of the detection [0.0, 1.0]
    int class_id = -1;        // The integer ID of the detected class
    std::string class_name;   // The human-readable name of the class (e.g., "car", "person")
};

/**
 * @struct DepthData
 * @brief Holds the result of a depth estimation process.
 */
struct DepthData {
    // A vector representing the depth map, typically in row-major order.
    // Values could be relative or in metric units (e.g., meters) depending
    // on the model and post-processing.
    std::vector<float> depth_map;
    int width = 0;
    int height = 0;
};

/**
 * @struct FaceRecognitionResult
 * @brief Represents a recognized face.
 */
struct FaceRecognitionResult {
    BoundingBox box;          // The location of the face in the image.
    std::string name;         // The name of the recognized person (e.g., "Joao").
                              // Could be "Unknown" if no match is found.
    float confidence = 0.0f;  // Confidence of the match against the known face database.
};

/**
 * @struct SceneData
 * @brief A comprehensive container for all data related to a single captured frame.
 *
 * This object is the primary data packet that flows through the perception pipeline.
 * It is created by the PerceptionEngine, populated by various IProcessors, and
 * eventually consumed by the core orchestrator to make decisions.
 */
struct SceneData {
    // The original frame from the camera HAL.
    // This is the raw input for all perception processing.
    hal_frame_t frame;

    // A flag to indicate if the frame data is valid and has been properly acquired.
    bool is_frame_valid = false;

    // --- Results from Perception Processors ---

    // A list of all objects detected in the scene.
    // Populated by a processor like YOLOv8.
    std::vector<BoundingBox> detections;

    // The estimated depth map of the scene.
    // Populated by a processor like MiDaS.
    DepthData depth;

    // A list of all faces recognized in the scene.
    // Populated by a face recognition processor.
    std::vector<FaceRecognitionResult> recognized_faces;

    // --- Additional Context ---

    // A high-level textual description of the scene, potentially generated
    // by a Vision-Language Model (VLM) in a future version.
    std::string scene_description;
};

} // namespace via::shared
