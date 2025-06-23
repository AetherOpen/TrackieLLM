#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
TrackieLLM Model Downloader

This script downloads the required machine learning models for the TrackieLLM
project from their public hosting locations (like Hugging Face).

It checks for existing files and will not re-download them. A progress bar
is displayed for large files.

Dependencies:
  - requests
  - tqdm

Install them using:
  pip install requests tqdm
  or
  pip install -r requirements.txt (if a requirements file is provided)
"""

import requests
import sys
from pathlib import Path
from tqdm import tqdm

# --- Configuration ---

# Define the models to be downloaded.
# Each entry is a dictionary with the target filename and the download URL.
MODELS_TO_DOWNLOAD = [
    {
        "filename": "gemma-2b-it.gguf",
        "url": "https://huggingface.co/TheBloke/gemma-2b-it-GGUF/resolve/main/gemma-2b-it.Q4_K_M.gguf",
    },
    {
        "filename": "yolo_v8n.onnx",
        "url": "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8n.onnx",
    },
    {
        "filename": "midas_v2_small.onnx",
        "url": "https://github.com/intel-isl/MiDaS/releases/download/v2_1/midas_v21_small.onnx",
    },
    {
        "filename": "mobilefacenet.onnx",
        "url": "https://github.com/deepinsight/insightface/raw/master/model_zoo/arcface_onnx/mobilefacenet.onnx",
    },
]

# --- Helper Functions ---

def get_project_root() -> Path:
    """Finds the project root directory relative to this script's location."""
    return Path(__file__).resolve().parent.parent.parent

def download_file(url: str, dest_path: Path):
    """
    Downloads a file from a URL to a destination path with a progress bar.

    Args:
        url (str): The URL to download from.
        dest_path (Path): The local path to save the file to.
    """
    if dest_path.exists() and dest_path.stat().st_size > 0:
        print(f"  -> File already exists: {dest_path.name}. Skipping.")
        return

    print(f"  -> Downloading {dest_path.name} from {url}...")
    try:
        response = requests.get(url, stream=True, timeout=30)
        response.raise_for_status()  # Raise an exception for bad status codes (4xx or 5xx)

        total_size_in_bytes = int(response.headers.get('content-length', 0))
        block_size = 1024 * 8 # 8 KB

        progress_bar = tqdm(
            total=total_size_in_bytes,
            unit='iB',
            unit_scale=True,
            desc=f"     {dest_path.name}"
        )

        with open(dest_path, 'wb') as file:
            for data in response.iter_content(block_size):
                progress_bar.update(len(data))
                file.write(data)

        progress_bar.close()

        if total_size_in_bytes != 0 and progress_bar.n != total_size_in_bytes:
            print(f"  -> ERROR: Download failed for {dest_path.name}. Size mismatch.", file=sys.stderr)
            dest_path.unlink() # Clean up partial download
        else:
            print(f"  -> Successfully downloaded {dest_path.name}.")

    except requests.exceptions.RequestException as e:
        print(f"\n  -> ERROR: Could not download {dest_path.name}. Reason: {e}", file=sys.stderr)
        if dest_path.exists():
            dest_path.unlink() # Clean up partial download
    except Exception as e:
        print(f"\n  -> An unexpected error occurred: {e}", file=sys.stderr)
        if dest_path.exists():
            dest_path.unlink()

# --- Main Execution ---

def main():
    """
    Main function to orchestrate the model download process.
    """
    print("--- Starting TrackieLLM Model Download ---")

    project_root = get_project_root()
    models_dir = project_root / "assets" / "models"

    # 1. Ensure the target directory exists.
    print(f"Ensuring model directory exists: {models_dir}")
    models_dir.mkdir(parents=True, exist_ok=True)

    # 2. Iterate through the list of models and download each one.
    print("\nChecking and downloading models...")
    for model_info in MODELS_TO_DOWNLOAD:
        filename = model_info["filename"]
        url = model_info["url"]
        destination = models_dir / filename

        download_file(url, destination)

    print("\n--- Model download process complete. ---")
    print("Please verify that all models were downloaded successfully into the assets/models/ directory.")

if __name__ == "__main__":
    main()
