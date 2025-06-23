#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
TrackieLLM - ATAD (Assistive Technology Adaptation and Development) Script

This script provides tools for personalizing the TrackieLLM experience.
It is intended to be run on a development machine with more resources
(and potentially a GPU) than the target embedded device.

It supports two main commands:
1. `add-face`:   Registers a new person's face by capturing images from a
                 webcam, extracting facial embeddings, and saving them to a
                 database.
2. `finetune-llm`: Performs instruction-based fine-tuning on the base LLM
                   (e.g., Gemma) using a custom dataset. This adapts the
                   model's responses to a specific user's preferences.

Dependencies:
  - torch, transformers, peft, accelerate, bitsandbytes (for LLM fine-tuning)
  - onnxruntime, numpy, opencv-python (for face processing)
"""

import argparse
import json
import sys
import time
from pathlib import Path

import cv2
import numpy as np
import onnxruntime as ort
from tqdm import tqdm

# --- Configuration ---
# This section should mirror paths from the main project config for consistency.
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
ASSETS_DIR = PROJECT_ROOT / "assets"
MODELS_DIR = ASSETS_DIR / "models"
FACES_DB_PATH = ASSETS_DIR / "faces_db" / "known_faces.dat"
FINETUNE_DATASET_PATH = PROJECT_ROOT / "scripts" / "training" / "finetune_dataset.json"

# --- Face Processing Globals ---
FACE_DETECTOR_MODEL = MODELS_DIR / "yolo_v8n.onnx" # Using YOLO for face detection
FACE_EMBEDDER_MODEL = MODELS_DIR / "mobilefacenet.onnx"
EMBEDDING_SIZE = 128 # MobileFaceNet produces a 128-dimensional vector

# --- Helper Functions & Classes ---

def get_onnx_session(model_path: Path) -> ort.InferenceSession:
    """Initializes and returns an ONNX Runtime inference session."""
    if not model_path.exists():
        raise FileNotFoundError(f"Model not found at {model_path}. Please run download_models.py.")
    return ort.InferenceSession(str(model_path))

class FaceProcessor:
    """Encapsulates face detection and embedding extraction."""
    def __init__(self):
        print("Initializing Face Processor...")
        self.detector = get_onnx_session(FACE_DETECTOR_MODEL)
        self.embedder = get_onnx_session(FACE_EMBEDDER_MODEL)
        self.detector_input_name = self.detector.get_inputs()[0].name
        self.embedder_input_name = self.embedder.get_inputs()[0].name
        self.detector_input_shape = self.detector.get_inputs()[0].shape[2:] # H, W
        self.embedder_input_shape = self.embedder.get_inputs()[0].shape[2:] # H, W

    def detect_largest_face(self, frame: np.ndarray) -> tuple[int, int, int, int] | None:
        """Detects faces in a frame and returns the bounding box of the largest one."""
        # Preprocess for YOLO
        h, w, _ = frame.shape
        scale = min(self.detector_input_shape[0] / h, self.detector_input_shape[1] / w)
        resized_w, resized_h = int(w * scale), int(h * scale)
        resized_frame = cv2.resize(frame, (resized_w, resized_h))
        padded_frame = np.full((self.detector_input_shape[0], self.detector_input_shape[1], 3), 128, dtype=np.uint8)
        padded_frame[:resized_h, :resized_w] = resized_frame
        
        input_tensor = padded_frame.astype(np.float32) / 255.0
        input_tensor = np.transpose(input_tensor, (2, 0, 1))
        input_tensor = np.expand_dims(input_tensor, axis=0)

        # Run YOLO detector
        outputs = self.detector.run(None, {self.detector_input_name: input_tensor})
        
        # Post-process to find the face with the largest area (assuming person class is 0)
        # This is a simplified post-processing. A real one would be more complex.
        boxes = []
        for i in range(outputs[0].shape[2]):
            class_id = np.argmax(outputs[0][0, 4:, i])
            confidence = outputs[0][0, 4+class_id, i]
            if class_id == 0 and confidence > 0.6: # Class 0 is 'person' in COCO
                cx, cy, bw, bh = outputs[0][0, 0:4, i]
                x1 = int((cx - bw/2) / scale)
                y1 = int((cy - bh/2) / scale)
                x2 = int((cx + bw/2) / scale)
                y2 = int((cy + bh/2) / scale)
                boxes.append((x1, y1, x2, y2, (x2-x1)*(y2-y1)))
        
        if not boxes:
            return None
        
        # Return the box with the largest area
        largest_box = max(boxes, key=lambda item: item[4])
        return largest_box[:4]

    def get_embedding(self, frame: np.ndarray, box: tuple[int, int, int, int]) -> np.ndarray:
        """Extracts a facial embedding from a cropped and aligned face."""
        x1, y1, x2, y2 = box
        face_crop = frame[y1:y2, x1:x2]
        
        # Preprocess for MobileFaceNet
        resized_face = cv2.resize(face_crop, self.embedder_input_shape)
        input_tensor = resized_face.astype(np.float32)
        input_tensor = (input_tensor - 127.5) / 128.0 # Normalize
        input_tensor = np.transpose(input_tensor, (2, 0, 1))
        input_tensor = np.expand_dims(input_tensor, axis=0)

        # Run embedder
        embedding = self.embedder.run(None, {self.embedder_input_name: input_tensor})[0][0]
        
        # L2-normalize the embedding
        norm = np.linalg.norm(embedding)
        return embedding / norm

# --- Command Handlers ---

def handle_add_face(args):
    """Handler for the 'add-face' command."""
    print(f"--- Registering a new face for: {args.name} ---")
    FACES_DB_PATH.parent.mkdir(exist_ok=True)

    # Load existing database or create a new one
    if FACES_DB_PATH.exists():
        with open(FACES_DB_PATH, 'r') as f:
            face_db = json.load(f)
    else:
        face_db = {"users": []}

    if any(user['name'] == args.name for user in face_db['users']):
        print(f"ERROR: A user named '{args.name}' already exists in the database.")
        sys.exit(1)

    processor = FaceProcessor()
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("ERROR: Cannot open webcam.")
        sys.exit(1)

    print("\nWebcam opened. Please look at the camera.")
    print(f"Collecting {args.samples} face samples. Press 'q' to quit.")

    embeddings = []
    pbar = tqdm(total=args.samples, desc="Capturing samples")
    while len(embeddings) < args.samples:
        ret, frame = cap.read()
        if not ret:
            break

        display_frame = frame.copy()
        box = processor.detect_largest_face(frame)

        if box:
            x1, y1, x2, y2 = box
            cv2.rectangle(display_frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
            
            # Add a small delay and check for stability to avoid blurry images
            time.sleep(0.1)
            
            embedding = processor.get_embedding(frame, box)
            embeddings.append(embedding.tolist())
            pbar.update(1)

        cv2.imshow('Face Registration - Press "q" to quit', display_frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    pbar.close()
    cap.release()
    cv2.destroyAllWindows()

    if len(embeddings) < args.samples:
        print("\nRegistration cancelled or not enough samples collected.")
        return

    # Average the embeddings to get a single, robust representation
    avg_embedding = np.mean(np.array(embeddings), axis=0)
    
    # Add new user to the database
    new_user = {"name": args.name, "embedding": avg_embedding.tolist()}
    face_db["users"].append(new_user)

    # Save the updated database
    with open(FACES_DB_PATH, 'w') as f:
        json.dump(face_db, f, indent=2)

    print(f"\nSuccessfully registered '{args.name}' with {len(embeddings)} samples.")
    print(f"Database saved to {FACES_DB_PATH}")

def handle_finetune_llm(args):
    """Handler for the 'finetune-llm' command."""
    print("--- Fine-tuning the Language Model ---")
    try:
        import torch
        from transformers import AutoTokenizer, AutoModelForCausalLM, TrainingArguments, Trainer, DataCollatorForLanguageModeling
        from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training
        from datasets import Dataset
    except ImportError:
        print("ERROR: LLM fine-tuning requires PyTorch, transformers, peft, accelerate, bitsandbytes, and datasets.")
        print("Please install them: pip install torch transformers peft accelerate bitsandbytes datasets")
        sys.exit(1)

    # 1. Load Model and Tokenizer
    model_name = args.base_model
    print(f"Loading base model: {model_name}")
    
    # Use 4-bit quantization for memory efficiency
    model = AutoModelForCausalLM.from_pretrained(
        model_name,
        load_in_4bit=True,
        torch_dtype=torch.bfloat16,
        device_map="auto",
    )
    tokenizer = AutoTokenizer.from_pretrained(model_name)
    tokenizer.pad_token = tokenizer.eos_token

    # 2. Prepare model for LoRA training
    model = prepare_model_for_kbit_training(model)
    lora_config = LoraConfig(
        r=16,
        lora_alpha=32,
        target_modules=["q_proj", "v_proj"], # Target modules depend on the model architecture
        lora_dropout=0.05,
        bias="none",
        task_type="CAUSAL_LM",
    )
    model = get_peft_model(model, lora_config)
    model.print_trainable_parameters()

    # 3. Load and prepare dataset
    print(f"Loading dataset from {FINETUNE_DATASET_PATH}")
    if not FINETUNE_DATASET_PATH.exists():
        print(f"ERROR: Dataset file not found at {FINETUNE_DATASET_PATH}")
        sys.exit(1)
        
    with open(FINETUNE_DATASET_PATH, 'r') as f:
        data = json.load(f)

    # Format data into a prompt template
    def format_prompt(item):
        return f"### Instruction:\n{item['instruction']}\n\n### Response:\n{item['response']}"

    text_data = [format_prompt(item) for item in data]
    dataset = Dataset.from_dict({"text": text_data})

    # 4. Configure and run Trainer
    output_dir = PROJECT_ROOT / "finetuned_models" / f"{Path(model_name).name}-lora-adapter"
    print(f"Training artifacts will be saved to: {output_dir}")

    training_args = TrainingArguments(
        output_dir=str(output_dir),
        per_device_train_batch_size=args.batch_size,
        gradient_accumulation_steps=4,
        learning_rate=2e-4,
        num_train_epochs=args.epochs,
        logging_steps=10,
        save_strategy="epoch",
        fp16=True, # Use mixed precision
    )

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=dataset,
        data_collator=DataCollatorForLanguageModeling(tokenizer, mlm=False),
    )

    print("Starting fine-tuning process...")
    trainer.train()
    
    print("Fine-tuning complete.")
    print(f"LoRA adapter saved to {output_dir}")


# --- Main Argument Parser ---

def main():
    """Main function to parse arguments and dispatch to handlers."""
    parser = argparse.ArgumentParser(description="TrackieLLM Adaptation and Development (ATAD) Toolkit.")
    subparsers = parser.add_subparsers(dest="command", required=True, help="Available commands")

    # Sub-parser for 'add-face'
    parser_add_face = subparsers.add_parser("add-face", help="Register a new person's face in the database.")
    parser_add_face.add_argument("name", type=str, help="The name of the person to register.")
    parser_add_face.add_argument("-s", "--samples", type=int, default=20, help="Number of face samples to collect.")
    parser_add_face.set_defaults(func=handle_add_face)

    # Sub-parser for 'finetune-llm'
    parser_finetune = subparsers.add_parser("finetune-llm", help="Fine-tune the language model on a custom dataset.")
    parser_finetune.add_argument("--base-model", type=str, default="google/gemma-2b-it", help="The base Hugging Face model to fine-tune.")
    parser_finetune.add_argument("--epochs", type=int, default=3, help="Number of training epochs.")
    parser_finetune.add_argument("--batch-size", type=int, default=1, help="Training batch size per device.")
    parser_finetune.set_defaults(func=handle_finetune_llm)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
