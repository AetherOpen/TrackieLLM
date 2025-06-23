/**
 * @file LlmInterpreter.cpp
 * @author TrackieLLM Reasoning Team
 * @brief Concrete implementation of the LlmInterpreter module using llama.cpp.
 *
 * @copyright Copyright (c) 2024
 *
 * This file provides the implementation for interacting with a GGUF language
 * model via the llama.cpp library. It handles the full lifecycle from model
 * loading to asynchronous prompt processing and resource cleanup.
 */

#include "via/reasoning/LlmInterpreter.h"

// C++ Standard Library
#include <stdexcept>
#include <iostream> // Replace with a proper logger
#include <vector>
#include <algorithm>

// Include the C API for llama.cpp
#include <llama.h>

// Include the C-ABI header for the config loader
#include "via_config.h"

namespace via::reasoning {

// ============================================================================
// Constructor / Destructor
// ============================================================================

LlmInterpreter::LlmInterpreter()
    : m_model(nullptr),
      m_context(nullptr),
      m_context_size(2048), // Default value, should be overridden by config
      m_n_threads(4),       // Default value
      m_isRunning(false)
{
    // Initialize llama.cpp backend
    // This should be called once at the beginning of the program.
    llama_backend_init(false); // num_a = false
}

LlmInterpreter::~LlmInterpreter() {
    // The stop() method should have already been called, but as a safeguard:
    if (m_isRunning.load()) {
        stop();
    }
    // Free llama.cpp resources
    if (m_context) {
        llama_free(m_context);
        m_context = nullptr;
    }
    if (m_model) {
        llama_free_model(m_model);
        m_model = nullptr;
    }
    // De-initialize the backend
    llama_backend_free();
}

// ============================================================================
// IModule Interface Implementation
// ============================================================================

bool LlmInterpreter::initialize(const ViaConfig& config) {
    std::cout << "Initializing LlmInterpreter..." << std::endl;

    // --- Load configuration ---
    const char* model_path_c_str = nullptr;
    if (via_config_get_string(&config, "reasoning.llm.model_path", &model_path_c_str) != ViaConfigStatus_Ok) {
        std::cerr << "Error: 'reasoning.llm.model_path' not found in configuration." << std::endl;
        return false;
    }
    std::string model_path(model_path_c_str);

    int64_t ctx_size = 0;
    if (via_config_get_integer(&config, "reasoning.llm.context_size", &ctx_size) == ViaConfigStatus_Ok) {
        m_context_size = static_cast<int>(ctx_size);
    }

    int64_t n_threads_val = 0;
    if (via_config_get_integer(&config, "system.threads.reasoning", &n_threads_val) == ViaConfigStatus_Ok) {
        m_n_threads = static_cast<int>(n_threads_val);
    }

    // --- Load Model ---
    llama_model_params model_params = llama_model_default_params();
    // model_params.n_gpu_layers = 0; // Set to > 0 for GPU offloading

    m_model = llama_load_model_from_file(model_path.c_str(), model_params);
    if (!m_model) {
        std::cerr << "Error: Failed to load LLM model from " << model_path << std::endl;
        return false;
    }

    // --- Create Context ---
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = m_context_size;
    ctx_params.n_threads = m_n_threads;
    ctx_params.n_threads_batch = m_n_threads;

    m_context = llama_new_context_with_model(m_model, ctx_params);
    if (!m_context) {
        std::cerr << "Error: Failed to create LLM context." << std::endl;
        llama_free_model(m_model);
        m_model = nullptr;
        return false;
    }

    std::cout << "LlmInterpreter initialized successfully." << std::endl;
    return true;
}

bool LlmInterpreter::start() {
    if (!m_context || !m_model) {
        std::cerr << "Error: Cannot start LlmInterpreter, not initialized." << std::endl;
        return false;
    }
    m_isRunning.store(true);
    m_thread = std::thread(&LlmInterpreter::reasoningLoop, this);
    std::cout << "LlmInterpreter worker thread started." << std::endl;
    return true;
}

void LlmInterpreter::stop() {
    if (m_isRunning.exchange(false)) {
        std::cout << "Stopping LlmInterpreter worker thread..." << std::endl;
        m_task_queue.notify_all(); // Wake up the thread if it's waiting
        if (m_thread.joinable()) {
            m_thread.join();
        }
        std::cout << "LlmInterpreter worker thread stopped." << std::endl;
    }
}

const char* LlmInterpreter::getName() const {
    return "LlmInterpreter";
}

// ============================================================================
// Public API for Submitting Tasks
// ============================================================================

std::future<std::string> LlmInterpreter::submitPrompt(const std::string& prompt) {
    LlmTask task;
    task.prompt = prompt;
    auto future = task.promise.get_future();
    m_task_queue.push(std::move(task));
    return future;
}

// ============================================================================
// Private Worker Thread and Inference Logic
// ============================================================================

void LlmInterpreter::reasoningLoop() {
    while (m_isRunning.load()) {
        LlmTask task;
        if (m_task_queue.wait_and_pop(task)) {
            if (!m_isRunning.load()) break; // Check again after waking up

            try {
                std::string result = generateResponse(task.prompt);
                task.promise.set_value(result);
            } catch (const std::exception& e) {
                task.promise.set_exception(std::current_exception());
            }
        }
    }
}

std::string LlmInterpreter::generateResponse(const std::string& prompt) {
    // --- Tokenize the prompt ---
    // Add a space in front of the prompt to ensure proper tokenization
    std::string processed_prompt = " " + prompt;
    std::vector<llama_token> tokens_list;
    tokens_list.resize(m_context_size);
    int n_tokens = llama_tokenize(m_model, processed_prompt.c_str(), processed_prompt.length(), tokens_list.data(), tokens_list.size(), true, false);
    if (n_tokens < 0) {
        throw std::runtime_error("Failed to tokenize prompt (buffer too small).");
    }
    tokens_list.resize(n_tokens);

    // TODO: A more robust implementation would manage the context window,
    // clearing old tokens if the total number of tokens exceeds n_ctx.
    // For now, we clear the context for each new prompt for simplicity.
    llama_kv_cache_clear(m_context);

    // --- Evaluate the prompt ---
    if (llama_eval(m_context, tokens_list.data(), tokens_list.size(), 0, m_n_threads)) {
        throw std::runtime_error("Failed to evaluate prompt.");
    }

    // --- Generate the response ---
    std::string response_text;
    const int max_new_tokens = 256; // Limit response length

    for (int i = 0; i < max_new_tokens; ++i) {
        // Sample the next token
        llama_token new_token_id = llama_sample_token_greedy(m_context, nullptr);

        // Check for End-Of-Sequence token
        if (new_token_id == llama_token_eos(m_model)) {
            break;
        }

        // Append the new token to the response text
        response_text += llama_token_to_piece(m_context, new_token_id);

        // Prepare for the next iteration
        llama_eval(m_context, &new_token_id, 1, llama_get_kv_cache_token_count(m_context), m_n_threads);
    }

    return response_text;
}

} // namespace via::reasoning
