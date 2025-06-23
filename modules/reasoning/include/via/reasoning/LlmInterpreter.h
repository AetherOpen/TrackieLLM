/**
 * @file LlmInterpreter.h
 * @author TrackieLLM Reasoning Team
 * @brief Defines the main reasoning module that interacts with the LLM.
 *
 * @copyright Copyright (c) 2024
 *
 * The LlmInterpreter class is the central reasoning module of TrackieLLM.
 * It inherits from IModule to integrate with the main application lifecycle.
 *
 * Its primary responsibilities are:
 *  - Loading a GGUF-formatted language model using the llama.cpp library.
 *  - Managing the conversational context (history) of the interaction.
 *  - Providing a thread-safe interface for other modules to submit reasoning tasks (prompts).
 *  - Processing these tasks in a background thread to avoid blocking the system.
 *  - Generating a textual response from the LLM.
 *  - Publishing the response back to the orchestrator.
 */

#pragma once

#include "via/core/IModule.h"
#include "via/shared/SafeQueue.h" // For thread-safe task queuing

#include <string>
#include <future>
#include <thread>
#include <atomic>

// Forward-declare llama.cpp structures to avoid including its headers here.
// This reduces coupling and improves compile times.
struct llama_model;
struct llama_context;

namespace via::reasoning {

/**
 * @struct LlmTask
 * @brief Represents a single reasoning task to be processed by the LLM.
 *
 * This struct bundles a prompt with a promise, allowing the caller to
 * asynchronously wait for the result of the LLM inference.
 */
struct LlmTask {
    std::string prompt;
    std::promise<std::string> promise;
};

/**
 * @class LlmInterpreter
 * @brief The main reasoning module, implementing IModule and managing llama.cpp.
 */
class LlmInterpreter final : public core::IModule {
public:
    /**
     * @brief Default constructor.
     */
    LlmInterpreter();

    /**
     * @brief Destructor. Ensures the worker thread is stopped and resources are freed.
     */
    ~LlmInterpreter() override;

    // --- Rule of Five: Non-copyable and non-movable ---
    LlmInterpreter(const LlmInterpreter&) = delete;
    LlmInterpreter& operator=(const LlmInterpreter&) = delete;
    LlmInterpreter(LlmInterpreter&&) = delete;
    LlmInterpreter& operator=(LlmInterpreter&&) = delete;

    // --- IModule Interface Implementation ---

    /**
     * @brief Initializes the LLM interpreter.
     *
     * Reads configuration, loads the GGUF model via llama.cpp, and prepares
     * the inference context.
     * @param config The global configuration object.
     * @return true on success, false otherwise.
     */
    bool initialize(const ViaConfig& config) override;

    /**
     * @brief Starts the reasoning worker thread.
     * @return true if the thread was started successfully.
     */
    bool start() override;

    /**
     * @brief Stops the reasoning worker thread gracefully.
     */
    void stop() override;

    /**
     * @brief Gets the name of this module.
     * @return The string "LlmInterpreter".
     */
    const char* getName() const override;

    // --- Public API for submitting tasks ---

    /**
     * @brief Submits a prompt to the LLM for asynchronous processing.
     *
     * This method is thread-safe. It places the prompt in a queue and
     * immediately returns a `std::future`, which the caller can use to
     * retrieve the result later without blocking.
     *
     * @param prompt The text prompt to be sent to the language model.
     * @return A `std::future<std::string>` that will eventually contain the
     *         LLM's response.
     */
    std::future<std::string> submitPrompt(const std::string& prompt);

private:
    /**
     * @brief The main function for the worker thread.
     *
     * This loop waits for tasks to appear in the queue, processes them one by
     * one using llama.cpp, and fulfills the associated promise with the result.
     */
    void reasoningLoop();

    /**
     * @brief Executes the core llama.cpp inference logic for a given prompt.
     * @param prompt The input text.
     * @return The generated text response from the model.
     */
    std::string generateResponse(const std::string& prompt);

    // --- Member Variables ---

    // Opaque pointers to the core llama.cpp data structures.
    llama_model* m_model;
    llama_context* m_context;

    // Configuration parameters for inference.
    int m_context_size;
    int m.n_threads;

    // --- Threading Members ---

    // The dedicated worker thread for LLM inference.
    std::thread m_thread;

    // An atomic flag to signal the worker thread to terminate.
    std::atomic<bool> m_isRunning;

    // A thread-safe queue to hold incoming reasoning tasks.
    shared::SafeQueue<LlmTask> m_task_queue;
};

} // namespace via::reasoning
