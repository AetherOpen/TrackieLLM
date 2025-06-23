/**
 * @file via_config.h
 * @author TrackieLLM Rust Team (via cbindgen)
 * @brief C-ABI for the TrackieLLM Configuration Loader Module.
 *
 * @copyright Copyright (c) 2024
 *
 * This header defines the Foreign Function Interface (FFI) for the Rust-based
 * configuration parser. It provides a safe, C-compatible API for the main C++
 * application to load, access, and release configuration data.
 *
 * The core principle is an "opaque pointer" pattern. The C++ application
 * receives a handle (ViaConfig*) to the configuration data but has no knowledge
 * of its internal layout. All operations, including memory deallocation, are
 * managed by the Rust library through the functions defined here.
 *
 * USAGE LIFECYCLE:
 * 1. Call `via_config_load()` with paths to YAML files to get a `ViaConfig*` handle.
 * 2. Check if the returned handle is not NULL.
 * 3. Use the various `via_config_get_*()` functions to retrieve values.
 * 4. When done, call `via_config_free()` with the handle to prevent memory leaks.
 */

#ifndef VIA_CONFIG_H
#define VIA_CONFIG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief An opaque handle to the internal Rust configuration structure.
 *
 * The caller should treat this as a "black box". Its size and layout are
 * managed exclusively by the Rust library. Do not attempt to dereference or
*  free it directly.
 */
typedef struct ViaConfig ViaConfig;

/**
 * @brief Represents the status of a configuration operation.
 *
 * This enum is used as a return type for functions that can fail, providing
 * more detailed error information than a simple boolean or NULL pointer.
 */
typedef enum ViaConfigStatus {
  /**
   * The operation completed successfully.
   */
  ViaConfigStatus_Ok = 0,
  /**
   * A provided file path was not found on the filesystem.
   */
  ViaConfigStatus_FileNotFound = 1,
  /**
   * The YAML file is malformed and could not be parsed.
   */
  ViaConfigStatus_ParseError = 2,
  /**
   * The requested key does not exist in the configuration.
   */
  ViaConfigStatus_KeyNotFound = 3,
  /**
   * The value for the requested key has a different type than expected
   * (e.g., asking for an integer but the value is a string).
   */
  ViaConfigStatus_TypeError = 4,
  /**
   * A null pointer was passed as an argument where a valid pointer was expected.
   */
  ViaConfigStatus_NullArgument = 5,
  /**
   * An unknown or internal error occurred in the Rust library.
   */
  ViaConfigStatus_InternalError = 6,
} ViaConfigStatus;

/**
 * @brief Loads and parses configuration from specified YAML files.
 *
 * This is the main entry point to the library. It reads the system, hardware,
 * and user profile configuration files, merges them into a single data
 * structure, and returns an opaque handle to it.
 *
 * @param system_path   A UTF-8 encoded, null-terminated string for the system config path.
 * @param hardware_path A UTF-8 encoded, null-terminated string for the hardware config path.
 * @param profile_path  A UTF-8 encoded, null-terminated string for the user profile path.
 *
 * @return A pointer to a `ViaConfig` handle on success.
 * @return `NULL` on failure (e.g., file not found, parse error). Check logs for details.
 *
 * @note The returned pointer MUST be freed using `via_config_free()` to avoid memory leaks.
 */
ViaConfig *via_config_load(const char *system_path,
                           const char *hardware_path,
                           const char *profile_path);

/**
 * @brief Frees all memory associated with a `ViaConfig` handle.
 *
 * This function must be called for every handle successfully obtained from
 * `via_config_load()` to ensure proper cleanup of the Rust-managed memory.
 *
 * @param config A pointer to the `ViaConfig` handle to be freed. If `NULL` is passed,
 *               the function does nothing.
 */
void via_config_free(ViaConfig *config);

/**
 * @brief Retrieves a string value from the configuration.
 *
 * @param config A valid `ViaConfig` handle.
 * @param key A null-terminated string representing the key (e.g., "log.level").
 * @param out_value A pointer to a `const char*` where the result will be stored.
 *
 * @return `ViaConfigStatus_Ok` on success. The pointer at `out_value` will point
 *         to the string value.
 * @return An error status code on failure. `out_value` will be untouched.
 *
 * @warning LIFETIME: The returned string pointer is owned by the `ViaConfig`
 *          handle and is valid only until `via_config_free()` is called.
 *          DO NOT free the returned pointer. If you need to store the value,
 *          make a copy of the string immediately.
 */
ViaConfigStatus via_config_get_string(const ViaConfig *config, const char *key, const char **out_value);

/**
 * @brief Retrieves an integer value from the configuration.
 *
 * @param config A valid `ViaConfig` handle.
 * @param key A null-terminated string representing the key (e.g., "camera.resolution.width").
 * @param out_value A pointer to an `int64_t` where the result will be stored.
 *
 * @return `ViaConfigStatus_Ok` on success.
 * @return An error status code on failure. `out_value` will be untouched.
 */
ViaConfigStatus via_config_get_integer(const ViaConfig *config, const char *key, int64_t *out_value);

/**
 * @brief Retrieves a floating-point value from the configuration.
 *
 * @param config A valid `ViaConfig` handle.
 * @param key A null-terminated string representing the key (e.g., "perception.threshold.detection").
 * @param out_value A pointer to a `double` where the result will be stored.
 *
 * @return `ViaConfigStatus_Ok` on success.
 * @return An error status code on failure. `out_value` will be untouched.
 */
ViaConfigStatus via_config_get_float(const ViaConfig *config, const char *key, double *out_value);

/**
 * @brief Retrieves a boolean value from the configuration.
 *
 * @param config A valid `ViaConfig` handle.
 * @param key A null-terminated string representing the key (e.g., "audio.noise_filter.enabled").
 * @param out_value A pointer to a `bool` where the result will be stored.
 *
 * @return `ViaConfigStatus_Ok` on success.
 * @return An error status code on failure. `out_value` will be untouched.
 */
ViaConfigStatus via_config_get_boolean(const ViaConfig *config, const char *key, bool *out_value);

/**
 * @brief Converts a `ViaConfigStatus` enum to a human-readable string.
 *
 * This is useful for logging errors returned by the API.
 *
 * @param status The status enum to convert.
 * @return A static, null-terminated string describing the status.
 *
 * @note The returned pointer is to a static string literal and must not be freed.
 */
const char *via_config_status_to_string(ViaConfigStatus status);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // VIA_CONFIG_H
