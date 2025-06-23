/**
 * @file Preprocessor.cpp
 * @author TrackieLLM Perception Team
 * @brief C++ fallback implementation for image preprocessing routines.
 *
 * @copyright Copyright (c) 2024
 *
 * This file contains standard C++ implementations for image preprocessing tasks
 * like resizing and normalization. These functions are used on platforms where
 * hardware-accelerated Assembly versions are not available (e.g., x86 architecture)
 * or as a reference for correctness.
 *
 * The primary function implements a high-quality bilinear interpolation for resizing,
 * followed by normalization to the [0.0, 1.0] range and conversion to the
 * planar CHW (Channels, Height, Width) format required by most neural networks.
 */

#include "via/perception/Preprocessor.h" // Corresponding header (hypothetical)
#include "via/shared/DataStructures.h"
#include "via/hal/hal.h"

#include <vector>
#include <cmath>
#include <cstdint>
#include <iostream> // Should be replaced by a proper logger
#include <algorithm> // For std::min/max

namespace via::perception::utils {

/**
 * @brief Performs image preprocessing using standard C++.
 *
 * Resizes an input frame to target dimensions using bilinear interpolation,
 * normalizes pixel values to [0.0, 1.0], and converts the layout from
 * interleaved HWC (Height, Width, Channels) to planar CHW.
 *
 * @param input_frame The source frame from the HAL. Must be in RGB24 format.
 * @param[out] output_tensor The destination vector to be filled with the float tensor data.
 * @param target_width The desired width of the output tensor.
 * @param target_height The desired height of the output tensor.
 */
void preprocess_image_cpp(const hal_frame_t& input_frame,
                          std::vector<float>& output_tensor,
                          int target_width,
                          int target_height)
{
    // --- 1. Input Validation ---
    if (!input_frame.data || input_frame.width == 0 || input_frame.height == 0) {
        std::cerr << "Error: Preprocessor received invalid input frame." << std::endl;
        return;
    }
    if (input_frame.format != HAL_PIXEL_FORMAT_RGB24) {
        std::cerr << "Error: Preprocessor only supports RGB24 format." << std::endl;
        return;
    }
    if (target_width <= 0 || target_height <= 0) {
        std::cerr << "Error: Preprocessor received invalid target dimensions." << std::endl;
        return;
    }

    // --- 2. Prepare Output and Scaling Ratios ---
    output_tensor.resize(3 * target_width * target_height);
    const uint8_t* src_data = static_cast<const uint8_t*>(input_frame.data);

    const float x_ratio = static_cast<float>(input_frame.width) / static_cast<float>(target_width);
    const float y_ratio = static_cast<float>(input_frame.height) / static_cast<float>(target_height);

    const int src_stride = input_frame.width * 3;

    // --- 3. Main Loop over Target Tensor Pixels ---
    for (int y_out = 0; y_out < target_height; ++y_out) {
        for (int x_out = 0; x_out < target_width; ++x_out) {

            // --- 4. Map Target Coordinate to Source Coordinate ---
            float src_x_float = (x_out + 0.5f) * x_ratio - 0.5f;
            float src_y_float = (y_out + 0.5f) * y_ratio - 0.5f;

            // --- 5. Bilinear Interpolation ---
            int x_in = static_cast<int>(std::floor(src_x_float));
            int y_in = static_cast<int>(std::floor(src_y_float));

            float x_diff = src_x_float - x_in;
            float y_diff = src_y_float - y_in;

            // Clamp coordinates to be within image boundaries
            x_in = std::max(0, x_in);
            y_in = std::max(0, y_in);
            int x2_in = std::min(static_cast<int>(input_frame.width) - 1, x_in + 1);
            int y2_in = std::min(static_cast<int>(input_frame.height) - 1, y_in + 1);

            // Get pointers to the 4 neighboring pixels in the source image
            const uint8_t* p1 = src_data + (y_in * src_stride) + (x_in * 3);   // Top-left
            const uint8_t* p2 = src_data + (y_in * src_stride) + (x2_in * 3);  // Top-right
            const uint8_t* p3 = src_data + (y2_in * src_stride) + (x_in * 3);  // Bottom-left
            const uint8_t* p4 = src_data + (y2_in * src_stride) + (x2_in * 3); // Bottom-right

            // Interpolate for each channel (R, G, B)
            for (int c = 0; c < 3; ++c) {
                // Interpolate horizontally
                float top_interp = p1[c] * (1.0f - x_diff) + p2[c] * x_diff;
                float bottom_interp = p3[c] * (1.0f - x_diff) + p4[c] * x_diff;

                // Interpolate vertically
                float final_value = top_interp * (1.0f - y_diff) + bottom_interp * y_diff;

                // --- 6. Normalize and Store in CHW Format ---
                int dst_idx = c * (target_height * target_width) + y_out * target_width + x_out;
                output_tensor[dst_idx] = final_value / 255.0f;
            }
        }
    }
}

} // namespace via::perception::utils
