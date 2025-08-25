#!/bin/bash

# Script to download Whisper models

set -e

echo "Whisper Model Downloader"
echo "========================"

# Change to project root
cd "$(dirname "$0")/.."

# Create models directory
mkdir -p models

# Model URLs
BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"

# Available models
echo ""
echo "Available models:"
echo "1) tiny.en    (39 MB)  - English only, fastest"
echo "2) base.en    (142 MB) - English only, recommended for real-time"
echo "3) small.en   (466 MB) - English only, better accuracy"
echo "4) tiny       (39 MB)  - Multilingual"
echo "5) base       (142 MB) - Multilingual"
echo "6) small      (466 MB) - Multilingual, good balance"
echo "7) medium     (1.5 GB) - Multilingual, high accuracy"
echo "8) large-v3   (3.1 GB) - Multilingual, best accuracy"
echo ""
echo "For lowest latency STT, we recommend 'base.en' (option 2)"
echo ""

read -p "Enter model number(s) to download (e.g., '2' or '2 6' for multiple): " choices

for choice in $choices; do
    case $choice in
        1)
            model="tiny.en"
            file="ggml-tiny.en.bin"
            ;;
        2)
            model="base.en"
            file="ggml-base.en.bin"
            ;;
        3)
            model="small.en"
            file="ggml-small.en.bin"
            ;;
        4)
            model="tiny"
            file="ggml-tiny.bin"
            ;;
        5)
            model="base"
            file="ggml-base.bin"
            ;;
        6)
            model="small"
            file="ggml-small.bin"
            ;;
        7)
            model="medium"
            file="ggml-medium.bin"
            ;;
        8)
            model="large-v3"
            file="ggml-large-v3.bin"
            ;;
        *)
            echo "Invalid choice: $choice"
            continue
            ;;
    esac
    
    if [ -f "models/$file" ]; then
        echo "Model $model already exists, skipping..."
    else
        echo "Downloading $model model..."
        curl -L "${BASE_URL}/${file}" -o "models/${file}" --progress-bar
        echo "Downloaded $model successfully!"
    fi
done

# Convert models to Core ML format for better performance
echo ""
echo "Would you like to convert models to Core ML format for better performance? (y/n)"
read -r response
if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
    cd third_party/whisper.cpp
    
    for model_file in ../../models/ggml-*.bin; do
        if [ -f "$model_file" ]; then
            model_name=$(basename "$model_file" .bin)
            echo "Converting $model_name to Core ML..."
            ./models/generate-coreml-model.sh "$model_file"
        fi
    done
    
    echo "Core ML conversion complete!"
fi

echo ""
echo "Model download complete!"
echo "Models are stored in: $(pwd)/models/"
echo ""
echo "To use a model, specify its path when running rt-stt:"
echo "  ./rt-stt-test --model models/ggml-base.en.bin"
