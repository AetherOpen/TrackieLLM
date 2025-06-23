/**
 * @file models.rs
 * @author TrackieLLM Rust Team
 * @brief Defines the Rust data structures that map to the YAML configuration files.
 *
 * @copyright Copyright (c) 2024
 *
 * This file serves as a schema and documentation for the expected structure of
 * the configuration files. These structs are used with `serde` to enable
 * strongly-typed deserialization, primarily for validation and testing purposes
 * within the Rust crate.
 *
 * While the main `lib.rs` uses a dynamic `serde_yaml::Value` for flexible key
 * lookups, these models ensure that the underlying configuration format is
 * well-defined and consistent.
 */

use serde::Deserialize;
use std::collections::HashMap;

// ============================================================================
// Top-Level Merged Configuration Structure
// ============================================================================

/// Represents the final, merged configuration from all source files.
#[derive(Debug, Deserialize)]
pub struct MergedConfig {
    pub system: SystemConfig,
    pub hardware: HardwareConfig,
    pub profile: ProfileConfig,
}

// ============================================================================
// Structure for `system.default.yml`
// ============================================================================

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct SystemConfig {
    pub log_level: String,
    pub threads: ThreadConfig,
}

#[derive(Debug, Deserialize)]
pub struct ThreadConfig {
    pub perception: u32,
    pub reasoning: u32,
    pub audio: u32,
}

// ============================================================================
// Structure for `hardware.default.yml`
// ============================================================================

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct HardwareConfig {
    pub camera: CameraConfig,
    pub microphone: MicrophoneConfig,
    pub perception: PerceptionHardwareConfig,
    pub reasoning: ReasoningHardwareConfig,
}

#[derive(Debug, Deserialize)]
pub struct CameraConfig {
    pub device_id: i32,
    pub resolution: Resolution,
}

#[derive(Debug, Deserialize)]
pub struct Resolution {
    pub width: u32,
    pub height: u32,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct MicrophoneConfig {
    pub device_id: i32,
    pub sample_rate: u32,
    pub noise_filter: NoiseFilterConfig,
}

#[derive(Debug, Deserialize)]
pub struct NoiseFilterConfig {
    pub enabled: bool,
    pub window_size: u32,
}

#[derive(Debug, Deserialize)]
pub struct PerceptionHardwareConfig {
    pub model_paths: HashMap<String, String>,
    pub thresholds: HashMap<String, f32>,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct ReasoningHardwareConfig {
    pub llm: LlmConfig,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct LlmConfig {
    pub model_path: String,
    pub context_size: u32,
}

// ============================================================================
// Structure for `profiles/joao.default.yml` (ATAD)
// ============================================================================

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct ProfileConfig {
    pub user_name: String,
    pub known_faces_db_path: String,
    pub alert_preferences: AlertPreferences,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct AlertPreferences {
    pub dangerous_objects: Vec<String>,
    pub play_sounds: bool,
}
