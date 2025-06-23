/**
 * @file OnnxRuntimeProcessor.cpp
 * @author TrackieLLM Perception Team
 * @brief Concrete implementation of an IProcessor using the ONNX Runtime.
 *
 * @copyright Copyright (c) 2024
 *
 * This class wraps the ONNX Runtime C++ API to execute a single .onnx model.
 * It handles session creation, tensor pre-processing, running the model,
 * and post-processing the output tensors into a structured format.
 */

// Corresponding header
#include "via/perception/OnnxRuntimeProcessor.h"

// C++ Standard Library
#include <vector>
#include <stdexcept>
#include <iostream> // Replace with a proper logger in production

// Project-specific headers
#include "via/shared/DataStructures.h" // For SceneData and BoundingBox

// ONNX Runtime C++ API
#include <onnxruntime_cxx_api.h>

namespace via::perception {

// ============================================================================
// Private Helper Class for PIMPL Idiom
// ============================================================================

/**
 * @struct OnnxRuntimeProcessor::Impl
 * @brief Private implementation details for the processor (PIMPL idiom).
 *
 * This hides the ONNX Runtime headers and implementation details from the
 * OnnxRuntimeProcessor.h file, reducing compile times and header coupling.
 */
struct OnnxRuntimeProcessor::Impl {
    Ort::Env env;
    Ort::Session session;
    Ort::AllocatorWithDefaultOptions allocator;

    std::vector<const char*> input_node_names;
    std::vector<int64_t> input_node_dims; // Assuming single input for simplicity

    std::vector<const char*> output_node_names;

    std::string name;

    // Constructor for the Impl struct
    Impl(const std::string& model_path, std::string processor_name)
        : env(ORT_LOGGING_LEVEL_WARNING, "TrackieLLM"),
          session(nullptr),
          name(std::move(processor_name))
    {
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1); // Optimize for single-threaded inference on embedded
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Use wide characters for model path on Windows
        #ifdef _WIN32
            const std::wstring w_model_path(model_path.begin(), model_path.end());
            session = Ort::Session(env, w_model_path.c_str(), session_options);
        #else
            session = Ort::Session(env, model_path.c_str(), session_options);
        #endif

        // --- Introspect model inputs ---
        size_t num_input_nodes = session.GetInputCount();
        if (num_input_nodes != 1) {
            throw std::runtime_error("This processor only supports models with a single input.");
        }
        // Memory for names is owned by the session, but we need to manage our pointers
        char* input_name = session.GetInputName(0, allocator);
        input_node_names.push_back(input_name);

        Ort::TypeInfo input_type_info = session.GetInputTypeInfo(0);
        auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        input_node_dims = input_tensor_info.GetShape();

        // --- Introspect model outputs ---
        size_t num_output_nodes = session.GetOutputCount();
        output_node_names.resize(num_output_nodes);
        for (size_t i = 0; i < num_output_nodes; i++) {
            char* output_name = session.GetOutputName(i, allocator);
            output_node_names[i] = output_name;
        }
    }

    // Destructor for the Impl struct
    ~Impl() {
        // Free the memory allocated by ONNX Runtime for node names
        for (auto& name : input_node_names) {
            allocator.Free((void*)name);
        }
        for (auto& name : output_node_names) {
            allocator.Free((void*)name);
        }
    }

    // Placeholder for pre-processing logic
    void preprocess(const hal_frame_t& frame, std::vector<float>& out_tensor_data) {
        // This is a highly model-specific function.
        // Example for a YOLO-like model expecting a [1, 3, H, W] float tensor.
        // Assumes input frame is already resized to model's expected dimensions.
        // A real implementation would need a robust resizing step here.
        const int64_t model_h = input_node_dims[2];
        const int64_t model_w = input_node_dims[3];

        if (frame.width != model_w || frame.height != model_h) {
            // In a real scenario, log this error and potentially resize.
            // For now, we'll just proceed, which may lead to incorrect results.
            std::cerr << "Warning: Frame size does not match model input size!" << std::endl;
        }

        out_tensor_data.resize(1 * 3 * model_h * model_w);
        const uint8_t* pixel_data = static_cast<const uint8_t*>(frame.data);

        // HWC to CHW conversion and normalization
        for (int c = 0; c < 3; ++c) {
            for (int h = 0; h < model_h; ++h) {
                for (int w = 0; w < model_w; ++w) {
                    // Assuming RGB24 format
                    int src_idx = (h * frame.width + w) * 3 + c;
                    int dst_idx = c * (model_h * model_w) + h * model_w + w;
                    out_tensor_data[dst_idx] = static_cast<float>(pixel_data[src_idx]) / 255.0f;
                }
            }
        }
    }

    // Placeholder for post-processing logic
    void postprocess(const std::vector<Ort::Value>& output_tensors, shared::SceneData& scene_data) {
        // This is also highly model-specific.
        // Example for a YOLOv8 detection model.
        // Output tensor shape: [1, 84, 8400] -> [batch, 4_bbox+80_classes, num_proposals]
        const float* output_data = output_tensors[0].GetTensorData<float>();
        auto output_dims = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        const int num_proposals = output_dims[2]; // 8400
        const int num_classes_plus_box = output_dims[1]; // 84

        float confidence_threshold = 0.5f; // Should be configurable

        for (int i = 0; i < num_proposals; ++i) {
            // Transpose the output: from [84, 8400] to [8400, 84] for easier access
            const float* proposal = output_data + i;
            float box_cx = proposal[0 * num_proposals];
            float box_cy = proposal[1 * num_proposals];
            float box_w  = proposal[2 * num_proposals];
            float box_h  = proposal[3 * num_proposals];

            // Find the class with the highest score
            float max_class_score = 0.0f;
            int class_id = -1;
            for (int j = 4; j < num_classes_plus_box; ++j) {
                float score = proposal[j * num_proposals];
                if (score > max_class_score) {
                    max_class_score = score;
                    class_id = j - 4;
                }
            }

            if (max_class_score > confidence_threshold) {
                shared::BoundingBox box;
                box.x1 = box_cx - box_w / 2.0f;
                box.y1 = box_cy - box_h / 2.0f;
                box.x2 = box_cx + box_w / 2.0f;
                box.y2 = box_cy + box_h / 2.0f;
                box.score = max_class_score;
                box.class_id = class_id;
                // box.class_name would be looked up from a class map
                scene_data.detections.push_back(box);
            }
        }

        // TODO: A critical step is to run Non-Maximum Suppression (NMS) here
        // on `scene_data.detections` to remove overlapping boxes.
        // This could call the optimized Assembly function.
    }
};

// ============================================================================
// Public Class Methods (forwarding to PIMPL)
// ============================================================================

OnnxRuntimeProcessor::OnnxRuntimeProcessor(const std::string& model_path, std::string processor_name) {
    try {
        p_impl = std::make_unique<Impl>(model_path, std::move(processor_name));
    } catch (const Ort::Exception& e) {
        // Convert Ort::Exception to a standard exception for the caller
        throw std::runtime_error(std::string("ONNX Runtime Error: ") + e.what());
    }
}

OnnxRuntimeProcessor::~OnnxRuntimeProcessor() = default; // std::unique_ptr handles cleanup

const char* OnnxRuntimeProcessor::getName() const {
    return p_impl->name.c_str();
}

bool OnnxRuntimeProcessor::process(shared::SceneData& scene_data) {
    if (!scene_data.frame.data) {
        std::cerr << "Error: " << getName() << " received invalid frame data." << std::endl;
        return false;
    }

    try {
        // 1. Pre-process the input frame into a float tensor
        std::vector<float> input_tensor_values;
        p_impl->preprocess(scene_data.frame, input_tensor_values);

        // 2. Create the input tensor object
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor_values.data(),
            input_tensor_values.size(),
            p_impl->input_node_dims.data(),
            p_impl->input_node_dims.size()
        );

        // 3. Run inference
        auto output_tensors = p_impl->session.Run(
            Ort::RunOptions{nullptr},
            p_impl->input_node_names.data(),
            &input_tensor,
            1, // Number of input tensors
            p_impl->output_node_names.data(),
            p_impl->output_node_names.size()
        );

        // 4. Post-process the output tensors to generate results
        p_impl->postprocess(output_tensors, scene_data);

    } catch (const Ort::Exception& e) {
        std::cerr << "Error during ONNX processing in " << getName() << ": " << e.what() << std::endl;
        return false;
    }

    return true;
}

} // namespace via::perception
