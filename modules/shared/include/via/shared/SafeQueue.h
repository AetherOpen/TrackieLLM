/**
 * @file SafeQueue.h
 * @author TrackieLLM Core Team
 * @brief A thread-safe, blocking queue for producer-consumer patterns.
 *
 * @copyright Copyright (c) 2024
 *
 * This header provides a generic, thread-safe queue implementation. It is
 * designed to be used in scenarios where one or more threads (producers) are
 * adding items to the queue, and another thread (the consumer) is removing
 * them.
 *
 * The queue is "blocking":
 * - If a consumer tries to pop an item from an empty queue, it will block
 *   (sleep) efficiently until an item is pushed by a producer.
 * - It is implemented using a std::mutex for mutual exclusion and a
 *   std::condition_variable for efficient waiting, avoiding CPU-intensive
 *   busy-loops.
 */

#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace via::shared {

template <typename T>
class SafeQueue {
public:
    /**
     * @brief Default constructor.
     */
    SafeQueue() = default;

    // --- Rule of Five: Make the queue non-copyable ---
    // Copying a queue with its mutex and condition variable is problematic
    // and often not the desired behavior.
    SafeQueue(const SafeQueue&) = delete;
    SafeQueue& operator=(const SafeQueue&) = delete;

    // --- Rule of Five: Allow moving ---
    // Moving a queue can be useful in some scenarios. We need to ensure
    // the mutex is locked during the move to prevent data races.
    SafeQueue(SafeQueue&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_queue = std::move(other.m_queue);
    }

    SafeQueue& operator=(SafeQueue&& other) noexcept {
        if (this != &other) {
            std::scoped_lock lock(m_mutex, other.m_mutex);
            m_queue = std::move(other.m_queue);
        }
        return *this;
    }

    /**
     * @brief Pushes an item onto the queue.
     *
     * This operation is thread-safe. It locks the mutex, adds the item,
     * and then notifies one waiting thread (if any) that an item is available.
     *
     * @param item The item to be added to the queue. Uses move semantics for efficiency.
     */
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(item));
        } // Mutex is unlocked here
        m_cond_var.notify_one();
    }

    /**
     * @brief Waits for an item and pops it from the queue.
     *
     * This operation is thread-safe and blocking. If the queue is empty,
     * the calling thread will sleep until an item is pushed or `notify_all` is called.
     *
     * @param[out] item A reference to a variable where the popped item will be stored.
     * @return `true` if an item was successfully popped.
     * @return `false` if the wait was interrupted by `notify_all` on an empty queue,
     *         typically used during shutdown.
     */
    bool wait_and_pop(T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond_var.wait(lock, [this] { return !m_queue.empty() || !m_is_valid; });

        // If the queue is still empty after waking up, it means we were notified
        // to shut down.
        if (m_queue.empty()) {
            return false;
        }

        item = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    /**
     * @brief Tries to pop an item from the queue without blocking.
     *
     * This operation is thread-safe. It will immediately return whether an
     * item was popped or not.
     *
     * @param[out] item A reference to a variable where the popped item will be stored.
     * @return `true` if an item was popped, `false` if the queue was empty.
     */
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        item = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    /**
     * @brief Checks if the queue is empty.
     * @return `true` if the queue is empty, `false` otherwise.
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    /**
     * @brief Gets the current size of the queue.
     * @return The number of items in the queue.
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    /**
     * @brief Notifies all waiting threads to wake up.
     *
     * This is primarily used during shutdown to unblock any consumer thread
     * that might be waiting on an empty queue.
     */
    void notify_all() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_is_valid = false; // Invalidate the queue to signal shutdown
        }
        m_cond_var.notify_all();
    }

private:
    // The underlying, non-thread-safe queue.
    std::queue<T> m_queue;

    // Mutex to protect access to the queue.
    // `mutable` allows it to be locked in const methods like `empty()` and `size()`.
    mutable std::mutex m_mutex;

    // Condition variable to manage blocking and waking of consumer threads.
    std::condition_variable m_cond_var;

    // A flag to handle shutdown notifications correctly.
    bool m_is_valid = true;
};

} // namespace via::shared
