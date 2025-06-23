/**
 * @file build.rs
 * @author TrackieLLM Build System
 * @brief Build script for the config_loader-rs crate.
 *
 * @copyright Copyright (c) 2024
 *
 * This script is executed by Cargo before compiling the crate. Its primary
 * purpose is to automatically generate the C header file (`via_config.h`)
 * from the Rust source code using the `cbindgen` tool.
 *
 * This ensures that the C-ABI exposed by this Rust library is always
 * in sync with its implementation, providing a single source of truth
 * (the Rust code) for the interface definition.
 */

use std::env;
use std::path::PathBuf;

fn main() {
    // 1. Inform Cargo to re-run this script only if `build.rs` or `src/lib.rs` changes.
    // This prevents unnecessary re-execution on every build.
    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=build.rs");

    // 2. Get the path to the output directory for this crate.
    // Cargo sets the `OUT_DIR` environment variable for build scripts.
    let crate_dir = env::var("CARGO_MANIFEST_DIR")
        .expect("CARGO_MANIFEST_DIR env var is not set, please use Cargo to build");

    // 3. Configure cbindgen.
    let config = cbindgen::Config::from_file("cbindgen.toml")
        .expect("Failed to load cbindgen.toml");

    // 4. Run cbindgen to generate the C header.
    // The output path is configured in `cbindgen.toml` to be `include/via_config.h`.
    match cbindgen::generate_with_config(&crate_dir, config) {
        Ok(bindings) => {
            // The `generate_with_config` function already writes the file if a path
            // is specified in the config. We can optionally write it manually if needed.
            // For example, to place it in a different directory:
            // bindings.write_to_file("path/to/output/header.h");
            println!("cargo:warning=Successfully generated C header file via_config.h");
        }
        Err(err) => {
            // If generation fails, panic to stop the build process with a clear error.
            panic!("Failed to generate C bindings: {:?}", err);
        }
    }
}
