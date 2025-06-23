/**
 * @file hal.h
 * @author TrackieLLM HAL Team
 * @brief C-ABI for the TrackieLLM Hardware Abstraction Layer.
 *
 * @copyright Copyright (c) 2024
 *
 * This header defines the pure C interface for all hardware interactions in
 * the TrackieLLM system. It provides a platform-agnostic API for accessing
 * the camera, microphone, and speaker.
 *
 * The design uses opaque pointers (e.g., `hal_camera_t`) as handles to hardware
 * resources. The internal structure of these handles is defined in the platform-
 * specific implementation files (e.g., `camera_linux.c`, `camera_windows.c`)
 * and is hidden from the application layer.
 *
 * RESOURCE MANAGEMENT:
 * - Each device type has an `_open()` function that returns a handle.
 * - This handle must be passed to all subsequent functions for that device.
 * - Each device type has a `_close()` function that must be called to release
 *   the handle and its associated system resources.
 */

#ifndef VIA_HAL_H
#define VIA_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Global HAL State & Status Codes
// ============================================================================

/**
 * @brief Represents the status of a HAL operation.
 */
typedef enum {
    HAL_STATUS_OK = 0,              /**< Operation was successful. */
    HAL_STATUS_ERROR,               /**< A generic, unspecified error occurred. */
    HAL_STATUS_INVALID_ARG,         /**< An invalid argument was provided to a function. */
    HAL_STATUS_DEVICE_NOT_FOUND,    /**< The requested hardware device does not exist. */
    HAL_STATUS_DEVICE_BUSY,         /**< The device is already in use or could not be acquired. */
    HAL_STATUS_IO_ERROR,            /**< An error occurred during a read/write operation. */
    HAL_STATUS_TIMEOUT,             /**< The operation did not complete within the specified time. */
    HAL_STATUS_NOT_SUPPORTED,       /**< The requested operation or configuration is not supported. */
} hal_status_t;

/**
 * @brief Initializes the entire Hardware Abstraction Layer.
 * Must be called once before any other HAL function.
 * @return HAL_STATUS_OK on success.
 */
hal_status_t hal_initialize(void);

/**
 * @brief Shuts down the HAL and releases any global resources.
 * Must be called once at application termination.
 */
void hal_shutdown(void);


// ============================================================================
// Camera Abstraction
// ============================================================================

/** @brief An opaque handle to a camera device. */
typedef struct hal_camera_t hal_camera_t;

/** @brief Defines common pixel formats for camera frames. */
typedef enum {
    HAL_PIXEL_FORMAT_UNKNOWN,
    HAL_PIXEL_FORMAT_RGB24,  /**< 24 bits per pixel, 8-bit R, G, B. */
    HAL_PIXEL_FORMAT_BGR24,  /**< 24 bits per pixel, 8-bit B, G, R. */
    HAL_PIXEL_FORMAT_YUYV,   /**< Packed YUV 4:2:2 format. */
} hal_pixel_format_t;

/** @brief Represents a single frame captured from the camera. */
typedef struct {
    void* data;                     /**< Pointer to the raw pixel data buffer. */
    uint32_t width;                 /**< Width of the frame in pixels. */
    uint32_t height;                /**< Height of the frame in pixels. */
    size_t size_bytes;              /**< Total size of the data buffer in bytes. */
    hal_pixel_format_t format;      /**< The pixel format of the data. */
    uint64_t timestamp_ns;          /**< Capture timestamp in nanoseconds (monotonic clock). */
} hal_frame_t;

/**
 * @brief Opens and configures a camera device.
 * @param device_id The system ID of the camera (e.g., 0 for /dev/video0).
 * @param width Desired frame width. The HAL may choose the closest supported resolution.
 * @param height Desired frame height.
 * @param[out] out_camera Pointer to a `hal_camera_t*` that will receive the handle.
 * @return HAL_STATUS_OK on success, `*out_camera` is valid.
 * @return An error code on failure, `*out_camera` is `NULL`.
 */
hal_status_t hal_camera_open(int device_id, uint32_t width, uint32_t height, hal_camera_t** out_camera);

/**
 * @brief Closes a camera device and releases all associated resources.
 * @param camera The handle to the camera to close. Becomes invalid after this call.
 */
void hal_camera_close(hal_camera_t* camera);

/**
 * @brief Starts the video capture stream.
 * @param camera A valid camera handle.
 * @return HAL_STATUS_OK on success.
 */
hal_status_t hal_camera_start_capture(hal_camera_t* camera);

/**
 * @brief Grabs the next available frame from the camera.
 *
 * This function blocks until a frame is available or the timeout expires.
 *
 * @param camera A valid camera handle.
 * @param[out] frame A pointer to a `hal_frame_t` struct to be filled with frame info.
 *                   The `data` pointer within the frame is owned by the HAL.
 * @param timeout_ms The maximum time to wait for a frame in milliseconds.
 * @return HAL_STATUS_OK on success.
 * @return HAL_STATUS_TIMEOUT if no frame was available in time.
 * @return HAL_STATUS_IO_ERROR on a device error.
 *
 * @warning The `frame->data` buffer is managed by the HAL. Do not free it.
 *          Call `hal_camera_release_frame` when you are done processing it.
 */
hal_status_t hal_camera_grab_frame(hal_camera_t* camera, hal_frame_t* frame, uint32_t timeout_ms);

/**
 * @brief Releases a frame buffer back to the HAL's internal pool.
 * This MUST be called for every frame successfully grabbed via `hal_camera_grab_frame`.
 * @param camera A valid camera handle.
 * @param frame The frame to be released.
 */
void hal_camera_release_frame(hal_camera_t* camera, const hal_frame_t* frame);


// ============================================================================
// Audio Abstraction
// ============================================================================

/** @brief An opaque handle to an audio device (can be capture or playback). */
typedef struct hal_audio_device_t hal_audio_device_t;

/** @brief Defines the direction of an audio device. */
typedef enum {
    HAL_AUDIO_CAPTURE,  /**< For microphone devices. */
    HAL_AUDIO_PLAYBACK, /**< For speaker/output devices. */
} hal_audio_direction_t;

/** @brief Represents a chunk of audio data. */
typedef struct {
    void* data;             /**< Pointer to the raw PCM audio samples. */
    size_t num_samples;     /**< Number of samples in the buffer (not bytes). */
    size_t size_bytes;      /**< Total size of the data buffer in bytes. */
    uint32_t sample_rate;   /**< e.g., 16000, 44100 */
    uint16_t num_channels;  /**< 1 for mono, 2 for stereo */
} hal_audio_chunk_t;

/**
 * @brief Opens an audio device for capture or playback.
 * @param device_id System-specific device ID. Use -1 for the system default.
 * @param direction Whether to open for capture (mic) or playback (speaker).
 * @param sample_rate Desired sample rate (e.g., 16000).
 * @param num_channels Desired number of channels (e.g., 1 for mono).
 * @param[out] out_device Pointer to a `hal_audio_device_t*` that will receive the handle.
 * @return HAL_STATUS_OK on success.
 */
hal_status_t hal_audio_open(int device_id, hal_audio_direction_t direction, uint32_t sample_rate, uint16_t num_channels, hal_audio_device_t** out_device);

/**
 * @brief Closes an audio device.
 * @param device The handle to the audio device to close.
 */
void hal_audio_close(hal_audio_device_t* device);

/**
 * @brief Captures a chunk of audio from a microphone device.
 * This function blocks until the requested number of samples is read or an error occurs.
 * @param mic_device A valid handle to a capture device.
 * @param[out] chunk A pointer to a chunk struct to be filled. The `data` buffer
 *                   is allocated by this function and must be freed by the caller.
 * @param num_samples_to_read The number of samples to capture.
 * @return HAL_STATUS_OK on success.
 *
 * @note OWNERSHIP: The caller is responsible for freeing `chunk->data` via `free()`.
 */
hal_status_t hal_audio_capture_chunk(hal_audio_device_t* mic_device, hal_audio_chunk_t* chunk, size_t num_samples_to_read);

/**
 * @brief Plays a chunk of audio on a playback device.
 * This function blocks until the entire chunk has been written to the device buffer.
 * @param speaker_device A valid handle to a playback device.
 * @param chunk A pointer to a chunk struct containing the audio to play.
 * @return HAL_STATUS_OK on success.
 */
hal_status_t hal_audio_playback_chunk(hal_audio_device_t* speaker_device, const hal_audio_chunk_t* chunk);


// ============================================================================
// Extensibility for Optimized Functions
// ============================================================================

/**
 * @brief A function pointer type for an audio filter.
 * This allows the C++ application to inject the optimized Assembly function
 * at runtime to override a default C implementation.
 */
typedef void (*hal_audio_filter_func_t)(int16_t* output_buffer,
                                        const int16_t* input_buffer,
                                        uint32_t num_samples,
                                        uint32_t window_size);

/**
 * @brief Sets a custom function to be used for audio filtering within the HAL.
 * @param filter_func A pointer to a function matching `hal_audio_filter_func_t`.
 *                    Pass `NULL` to reset to the default (likely no-op) filter.
 */
void hal_set_audio_filter(hal_audio_filter_func_t filter_func);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // VIA_HAL_H
