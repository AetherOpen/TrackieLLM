/**
 * @file lib.rs
 * @author TrackieLLM Rust Team
 * @brief Rust implementation of the safe configuration loader for TrackieLLM.
 *
 * @copyright Copyright (c) 2024
 *
 * This library provides a C-compatible ABI for loading, merging, and querying
 * YAML configuration files. It is designed to be memory-safe and robust,
 * handling file I/O and parsing within Rust to prevent common C/C++ vulnerabilities.
 */

use serde_yaml::Value;
use std::ffi::{c_char, CStr};
use std::fs;
use std::path::Path;

// --- Data Structures ---

/// The main struct that holds the merged configuration.
/// This is exposed to C as an opaque pointer `ViaConfig*`.
#[derive(Debug)]
pub struct ViaConfig {
    /// The merged configuration tree. We use the dynamic `Value` type
    /// to make querying by string key straightforward.
    merged_value: Value,
}

/// C-compatible enum representing the status of an operation.
/// Must match the definition in `via_config.h`.
#[repr(C)]
pub enum ViaConfigStatus {
    Ok = 0,
    FileNotFound = 1,
    ParseError = 2,
    KeyNotFound = 3,
    TypeError = 4,
    NullArgument = 5,
    InternalError = 6,
}

// --- Internal Helper Functions ---

/// Merges `source` Value into `dest` Value recursively.
/// `dest` is modified in place.
fn merge(dest: &mut Value, source: &Value) {
    if let Value::Mapping(dest_map) = dest {
        if let Value::Mapping(source_map) = source {
            for (key, source_val) in source_map {
                if let Some(dest_val) = dest_map.get_mut(key) {
                    merge(dest_val, source_val);
                } else {
                    dest_map.insert(key.clone(), source_val.clone());
                }
            }
        }
    }
}

/// Traverses the YAML `Value` using a dot-separated key string.
fn get_value_by_key<'a>(mut current_val: &'a Value, key: &str) -> Option<&'a Value> {
    for part in key.split('.') {
        if let Some(map) = current_val.as_mapping() {
            if let Some(next_val) = map.get(&Value::String(part.to_string())) {
                current_val = next_val;
            } else {
                return None; // Key part not found
            }
        } else {
            return None; // Tried to index into a non-map value
        }
    }
    Some(current_val)
}

// ============================================================================
// Public C-ABI Functions
// ============================================================================

/// Loads and parses configuration from specified YAML files.
///
/// # Safety
/// The caller must ensure that all `_path` arguments are valid, null-terminated
/// C strings. The returned pointer must be freed with `via_config_free`.
#[no_mangle]
pub unsafe extern "C" fn via_config_load(
    system_path_c: *const c_char,
    hardware_path_c: *const c_char,
    profile_path_c: *const c_char,
) -> *mut ViaConfig {
    // --- 1. Convert C strings to Rust strings safely ---
    let to_string = |s: *const c_char| {
        if s.is_null() {
            return None;
        }
        CStr::from_ptr(s).to_str().ok().map(String::from)
    };

    let Some(system_path) = to_string(system_path_c) else { return std::ptr::null_mut(); };
    let Some(hardware_path) = to_string(hardware_path_c) else { return std::ptr::null_mut(); };
    let Some(profile_path) = to_string(profile_path_c) else { return std::ptr::null_mut(); };

    // --- 2. Read and parse files ---
    let parse_file = |p: &Path| -> Result<Value, ()> {
        let content = fs::read_to_string(p).map_err(|_| eprintln!("Error: Failed to read file {:?}", p))?;
        serde_yaml::from_str(&content).map_err(|_| eprintln!("Error: Failed to parse YAML in file {:?}", p))
    };

    let Ok(mut system_config) = parse_file(Path::new(&system_path)) else { return std::ptr::null_mut(); };
    let Ok(hardware_config) = parse_file(Path::new(&hardware_path)) else { return std::ptr::null_mut(); };
    let Ok(profile_config) = parse_file(Path::new(&profile_path)) else { return std::ptr::null_mut(); };

    // --- 3. Merge configurations (profile > hardware > system) ---
    merge(&mut system_config, &hardware_config);
    merge(&mut system_config, &profile_config);

    // --- 4. Create heap-allocated object and return raw pointer ---
    let config = ViaConfig { merged_value: system_config };
    Box::into_raw(Box::new(config))
}

/// Frees all memory associated with a `ViaConfig` handle.
///
/// # Safety
/// The `config` pointer must be one that was returned from `via_config_load`
/// and has not been freed yet. Passing a null pointer is safe.
#[no_mangle]
pub unsafe extern "C" fn via_config_free(config: *mut ViaConfig) {
    if !config.is_null() {
        // Re-constitute the Box and let Rust's RAII drop it, freeing the memory.
        let _ = Box::from_raw(config);
    }
}

/// Retrieves a string value from the configuration.
///
/// # Safety
/// All pointers must be valid. The returned string pointer is owned by the
/// `ViaConfig` object and is only valid until `via_config_free` is called.
#[no_mangle]
pub unsafe extern "C" fn via_config_get_string(
    config: *const ViaConfig,
    key_c: *const c_char,
    out_value: *mut *const c_char,
) -> ViaConfigStatus {
    if config.is_null() || key_c.is_null() || out_value.is_null() {
        return ViaConfigStatus::NullArgument;
    }
    let config = &*config;
    let Ok(key) = CStr::from_ptr(key_c).to_str() else { return ViaConfigStatus::InternalError; };

    match get_value_by_key(&config.merged_value, key) {
        Some(val) => {
            if let Some(s) = val.as_str() {
                // WARNING: This relies on the C++ side to copy the string immediately.
                // The pointer becomes invalid after `via_config_free`.
                *out_value = s.as_ptr() as *const c_char;
                ViaConfigStatus::Ok
            } else {
                ViaConfigStatus::TypeError
            }
        }
        None => ViaConfigStatus::KeyNotFound,
    }
}

/// Retrieves an integer value from the configuration.
#[no_mangle]
pub unsafe extern "C" fn via_config_get_integer(
    config: *const ViaConfig,
    key_c: *const c_char,
    out_value: *mut i64,
) -> ViaConfigStatus {
    if config.is_null() || key_c.is_null() || out_value.is_null() {
        return ViaConfigStatus::NullArgument;
    }
    let config = &*config;
    let Ok(key) = CStr::from_ptr(key_c).to_str() else { return ViaConfigStatus::InternalError; };

    match get_value_by_key(&config.merged_value, key) {
        Some(val) => {
            if let Some(i) = val.as_i64() {
                *out_value = i;
                ViaConfigStatus::Ok
            } else {
                ViaConfigStatus::TypeError
            }
        }
        None => ViaConfigStatus::KeyNotFound,
    }
}

/// Retrieves a floating-point value from the configuration.
#[no_mangle]
pub unsafe extern "C" fn via_config_get_float(
    config: *const ViaConfig,
    key_c: *const c_char,
    out_value: *mut f64,
) -> ViaConfigStatus {
    if config.is_null() || key_c.is_null() || out_value.is_null() {
        return ViaConfigStatus::NullArgument;
    }
    let config = &*config;
    let Ok(key) = CStr::from_ptr(key_c).to_str() else { return ViaConfigStatus::InternalError; };

    match get_value_by_key(&config.merged_value, key) {
        Some(val) => {
            if let Some(f) = val.as_f64() {
                *out_value = f;
                ViaConfigStatus::Ok
            } else {
                ViaConfigStatus::TypeError
            }
        }
        None => ViaConfigStatus::KeyNotFound,
    }
}

/// Retrieves a boolean value from the configuration.
#[no_mangle]
pub unsafe extern "C" fn via_config_get_boolean(
    config: *const ViaConfig,
    key_c: *const c_char,
    out_value: *mut bool,
) -> ViaConfigStatus {
    if config.is_null() || key_c.is_null() || out_value.is_null() {
        return ViaConfigStatus::NullArgument;
    }
    let config = &*config;
    let Ok(key) = CStr::from_ptr(key_c).to_str() else { return ViaConfigStatus::InternalError; };

    match get_value_by_key(&config.merged_value, key) {
        Some(val) => {
            if let Some(b) = val.as_bool() {
                *out_value = b;
                ViaConfigStatus::Ok
            } else {
                ViaConfigStatus::TypeError
            }
        }
        None => ViaConfigStatus::KeyNotFound,
    }
}

/// Converts a `ViaConfigStatus` enum to a human-readable string.
#[no_mangle]
pub extern "C" fn via_config_status_to_string(status: ViaConfigStatus) -> *const c_char {
    match status {
        ViaConfigStatus::Ok => b"Ok\0".as_ptr() as *const c_char,
        ViaConfigStatus::FileNotFound => b"Error: File not found\0".as_ptr() as *const c_char,
        ViaConfigStatus::ParseError => b"Error: Could not parse YAML file\0".as_ptr() as *const c_char,
        ViaConfigStatus::KeyNotFound => b"Error: The requested key was not found\0".as_ptr() as *const c_char,
        ViaConfigStatus::TypeError => b"Error: Value has an unexpected type\0".as_ptr() as *const c_char,
        ViaConfigStatus::NullArgument => b"Error: A null argument was provided\0".as_ptr() as *const c_char,
        ViaConfigStatus::InternalError => b"Error: An internal error occurred in the Rust library\0".as_ptr() as *const c_char,
    }
}
