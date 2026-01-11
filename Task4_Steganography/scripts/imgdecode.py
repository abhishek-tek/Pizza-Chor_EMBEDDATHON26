import paho.mqtt.client as mqtt
from PIL import Image
import base64
import json
import os

# --- PATH CONFIGURATION ---
# Fixed the syntax error here:
IMAGE_PATH = "artifact.png"

def extract_hidden_language(path):
    try:
        # 1. Import the image
        img = Image.open(path).convert('RGB')
        pixels = img.load()
        width, height = img.size
        
        binary_data = ""
        print(f"Examining {width}x{height} pixels...")

        # 2. Methodical examination of components
        for y in range(height):
            for x in range(width):
                r, g, b = pixels[x, y]
                
                # RELATIONSHIP 1: Comparison (Red > Green)
                # If this gives gibberish, change to: if (r + g + b) % 2 == 0:
                if r > g:
                    binary_data += "1"
                else:
                    binary_data += "0"

        # 3. Assemble observations into language (ASCII)
        message = ""
        for i in range(0, len(binary_data), 8):
            byte = binary_data[i:i+8]
            if len(byte) < 8: break
            
            char_code = int(byte, 2)
            
            # Filter for printable ASCII characters
            if 32 <= char_code <= 126:
                message += chr(char_code)
            
            # Stop if we hit a common flag/message closer
            if "}" in message:
                break
        
        return message

    except Exception as e:
        return f"Error processing image: {e}"

# Execute Extraction
if __name__ == "__main__":
    if os.path.exists(IMAGE_PATH):
        result = extract_hidden_language(IMAGE_PATH)
        print("\n--- EXTRACTED MESSAGE ---")
        print(result if result else "No readable text found with this relationship.")
        print("-------------------------\n")
    else:
        print(f"Error: File not found at {IMAGE_PATH}")
        print("Ensure you have run your ESP32/MQTT script first to save the image.")