/**
 * @file audio.c
 * @author TrackieLLM HAL Team
 * @brief Platform-specific implementation of the HAL audio interface.
 *
 * @copyright Copyright (c) 2024
 *
 * This file provides the concrete implementations for audio capture (microphone)
 * and playback (speaker) functions declared in hal.h. It uses conditional
 * compilation to select the appropriate backend API at compile time:
 *
 * - On Linux systems (including Raspberry Pi), it uses the ALSA library.
 * - On Windows systems, it uses the classic Waveform Audio API (waveIn/waveOut).
 * - On other systems, it provides stub implementations that return NOT_SUPPORTED.
 *
 * The internal `hal_audio_device_t` struct is defined here, hiding the
 * platform-specific handles (e.g., `snd_pcm_t*` or `HWAVEIN`) from the caller.
 */

#include "via/hal/hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Platform-Specific Includes and Definitions
// ============================================================================

#if defined(__linux__)
    #include <alsa/asoundlib.h>
    #define PLATFORM_SUPPORTED 1
#elif defined(_WIN32)
    #include <windows.h>
    #include <mmsystem.h>
    #pragma comment(lib, "winmm.lib") // Link against the winmm library
    #define PLATFORM_SUPPORTED 1
#else
    #define PLATFORM_SUPPORTED 0
#endif

// ============================================================================
// Internal State and Structures
// ============================================================================

/**
 * @brief The concrete, platform-specific definition of an audio device handle.
 * This structure is hidden from the user of the HAL.
 */
struct hal_audio_device_t {
    hal_audio_direction_t direction;
    uint32_t sample_rate;
    uint16_t num_channels;
    size_t bytes_per_sample;

#if defined(__linux__)
    snd_pcm_t* pcm_handle;
    snd_pcm_uframes_t period_size;
#elif defined(_WIN32)
    union {
        HWAVEIN  h_wave_in;
        HWAVEOUT h_wave_out;
    } handle;
    // For Windows, we need to manage buffers via WAVEHDR
    WAVEHDR wave_header;
    char* buffer;
#endif
};

/**
 * @brief Global function pointer for the optimized audio filter.
 * This can be set by the application layer to inject an optimized routine
 * (like the one in Assembly) into the HAL's processing pipeline.
 */
static hal_audio_filter_func_t g_audio_filter_func = NULL;


// ============================================================================
// Public API Implementation
// ============================================================================

hal_status_t hal_audio_open(int device_id, hal_audio_direction_t direction, uint32_t sample_rate, uint16_t num_channels, hal_audio_device_t** out_device) {
#if !PLATFORM_SUPPORTED
    (void)device_id; (void)direction; (void)sample_rate; (void)num_channels; (void)out_device;
    fprintf(stderr, "HAL_AUDIO: Not supported on this platform.\n");
    return HAL_STATUS_NOT_SUPPORTED;
#else
    if (!out_device) {
        return HAL_STATUS_INVALID_ARG;
    }

    hal_audio_device_t* dev = (hal_audio_device_t*)calloc(1, sizeof(hal_audio_device_t));
    if (!dev) {
        return HAL_STATUS_ERROR; // Memory allocation failed
    }

    dev->direction = direction;
    dev->sample_rate = sample_rate;
    dev->num_channels = num_channels;
    dev->bytes_per_sample = sizeof(int16_t); // Assuming 16-bit audio

    // --- Platform-Specific Opening Logic ---
#if defined(__linux__)
    const char* device_name = (device_id == -1) ? "default" : "plughw:0,0"; // Simplified
    snd_pcm_stream_t stream = (direction == HAL_AUDIO_CAPTURE) ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK;
    int err;

    if ((err = snd_pcm_open(&dev->pcm_handle, device_name, stream, 0)) < 0) {
        fprintf(stderr, "HAL_AUDIO: Cannot open ALSA device %s: %s\n", device_name, snd_strerror(err));
        free(dev);
        return HAL_STATUS_DEVICE_NOT_FOUND;
    }

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(dev->pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(dev->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(dev->pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(dev->pcm_handle, hw_params, num_channels);
    snd_pcm_hw_params_set_rate_near(dev->pcm_handle, hw_params, &sample_rate, 0);

    if ((err = snd_pcm_hw_params(dev->pcm_handle, hw_params)) < 0) {
        fprintf(stderr, "HAL_AUDIO: Cannot set ALSA hw params: %s\n", snd_strerror(err));
        snd_pcm_close(dev->pcm_handle);
        free(dev);
        return HAL_STATUS_ERROR;
    }
    dev->sample_rate = sample_rate; // Update with actual rate set

#elif defined(_WIN32)
    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = num_channels;
    wfx.nSamplesPerSec = sample_rate;
    wfx.wBitsPerSample = dev->bytes_per_sample * 8;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    MMRESULT result;
    UINT win_device_id = (device_id == -1) ? WAVE_MAPPER : (UINT)device_id;

    if (direction == HAL_AUDIO_CAPTURE) {
        result = waveInOpen(&dev->handle.h_wave_in, win_device_id, &wfx, 0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            fprintf(stderr, "HAL_AUDIO: Failed to open waveIn device, error %d\n", result);
            free(dev);
            return HAL_STATUS_DEVICE_NOT_FOUND;
        }
    } else { // PLAYBACK
        result = waveOutOpen(&dev->handle.h_wave_out, win_device_id, &wfx, 0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            fprintf(stderr, "HAL_AUDIO: Failed to open waveOut device, error %d\n", result);
            free(dev);
            return HAL_STATUS_DEVICE_NOT_FOUND;
        }
    }
#endif
    *out_device = dev;
    return HAL_STATUS_OK;
#endif
}

void hal_audio_close(hal_audio_device_t* device) {
    if (!device) return;

#if PLATFORM_SUPPORTED
#if defined(__linux__)
    if (device->pcm_handle) {
        snd_pcm_close(device->pcm_handle);
    }
#elif defined(_WIN32)
    if (device->direction == HAL_AUDIO_CAPTURE) {
        waveInClose(device->handle.h_wave_in);
    } else {
        waveOutClose(device->handle.h_wave_out);
    }
    // Free any lingering buffer resources
    if (device->buffer) {
        if (device->wave_header.dwFlags & WHDR_PREPARED) {
            if (device->direction == HAL_AUDIO_CAPTURE) {
                waveInUnprepareHeader(device->handle.h_wave_in, &device->wave_header, sizeof(WAVEHDR));
            } else {
                waveOutUnprepareHeader(device->handle.h_wave_out, &device->wave_header, sizeof(WAVEHDR));
            }
        }
        free(device->buffer);
    }
#endif
#endif
    free(device);
}

hal_status_t hal_audio_capture_chunk(hal_audio_device_t* mic_device, hal_audio_chunk_t* chunk, size_t num_samples_to_read) {
#if !PLATFORM_SUPPORTED
    (void)mic_device; (void)chunk; (void)num_samples_to_read;
    return HAL_STATUS_NOT_SUPPORTED;
#else
    if (!mic_device || !chunk || mic_device->direction != HAL_AUDIO_CAPTURE) {
        return HAL_STATUS_INVALID_ARG;
    }

    chunk->size_bytes = num_samples_to_read * mic_device->num_channels * mic_device->bytes_per_sample;
    chunk->data = malloc(chunk->size_bytes);
    if (!chunk->data) {
        return HAL_STATUS_ERROR; // Memory allocation failed
    }

    chunk->num_samples = 0;
    chunk->sample_rate = mic_device->sample_rate;
    chunk->num_channels = mic_device->num_channels;

#if defined(__linux__)
    int frames_read = snd_pcm_readi(mic_device->pcm_handle, chunk->data, num_samples_to_read);
    if (frames_read < 0) {
        fprintf(stderr, "HAL_AUDIO: ALSA read error: %s\n", snd_strerror(frames_read));
        // Attempt to recover from error (e.g., buffer underrun)
        snd_pcm_recover(mic_device->pcm_handle, frames_read, 0);
        free(chunk->data);
        chunk->data = NULL;
        return HAL_STATUS_IO_ERROR;
    }
    chunk->num_samples = frames_read;
#elif defined(_WIN32)
    mic_device->buffer = chunk->data;
    mic_device->wave_header.lpData = mic_device->buffer;
    mic_device->wave_header.dwBufferLength = chunk->size_bytes;
    mic_device->wave_header.dwFlags = 0;

    waveInPrepareHeader(mic_device->handle.h_wave_in, &mic_device->wave_header, sizeof(WAVEHDR));
    waveInAddBuffer(mic_device->handle.h_wave_in, &mic_device->wave_header, sizeof(WAVEHDR));
    waveInStart(mic_device->handle.h_wave_in);

    // Block until the buffer is filled
    while (!(mic_device->wave_header.dwFlags & WHDR_DONE)) {
        Sleep(10); // Polling is simple, but event-driven would be better for production
    }

    waveInUnprepareHeader(mic_device->handle.h_wave_in, &mic_device->wave_header, sizeof(WAVEHDR));
    chunk->num_samples = num_samples_to_read;
    mic_device->buffer = NULL; // Ownership of the buffer is transferred to the caller
#endif

    // --- Apply Injected Filter if Available ---
    if (g_audio_filter_func && chunk->data) {
        // Assuming 16-bit signed audio for the filter
        g_audio_filter_func((int16_t*)chunk->data, (const int16_t*)chunk->data, chunk->num_samples, 3); // Example window size
    }

    return HAL_STATUS_OK;
#endif
}

hal_status_t hal_audio_playback_chunk(hal_audio_device_t* speaker_device, const hal_audio_chunk_t* chunk) {
#if !PLATFORM_SUPPORTED
    (void)speaker_device; (void)chunk;
    return HAL_STATUS_NOT_SUPPORTED;
#else
    if (!speaker_device || !chunk || speaker_device->direction != HAL_AUDIO_PLAYBACK) {
        return HAL_STATUS_INVALID_ARG;
    }

#if defined(__linux__)
    int frames_written = snd_pcm_writei(speaker_device->pcm_handle, chunk->data, chunk->num_samples);
    if (frames_written < 0) {
        fprintf(stderr, "HAL_AUDIO: ALSA write error: %s\n", snd_strerror(frames_written));
        snd_pcm_recover(speaker_device->pcm_handle, frames_written, 0);
        return HAL_STATUS_IO_ERROR;
    }
    if ((size_t)frames_written != chunk->num_samples) {
        fprintf(stderr, "HAL_AUDIO: ALSA short write, wrote %d of %zu frames\n", frames_written, chunk->num_samples);
    }
#elif defined(_WIN32)
    // For Windows, we need a mutable copy of the header
    WAVEHDR header = {0};
    header.lpData = (LPSTR)chunk->data;
    header.dwBufferLength = chunk->size_bytes;

    waveOutPrepareHeader(speaker_device->handle.h_wave_out, &header, sizeof(WAVEHDR));
    waveOutWrite(speaker_device->handle.h_wave_out, &header, sizeof(WAVEHDR));

    // Block until playback is finished
    while (!(header.dwFlags & WHDR_DONE)) {
        Sleep(10);
    }
    waveOutUnprepareHeader(speaker_device->handle.h_wave_out, &header, sizeof(WAVEHDR));
#endif
    return HAL_STATUS_OK;
#endif
}

void hal_set_audio_filter(hal_audio_filter_func_t filter_func) {
    g_audio_filter_func = filter_func;
}
