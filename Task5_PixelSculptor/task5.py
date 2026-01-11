import base64
import json
import time
from io import BytesIO

import numpy as np
from PIL import Image
import cv2
from skimage.metrics import structural_similarity as ssim
import paho.mqtt.client as mqtt

# ---------------- CONFIG ----------------
BROKER = "broker.mqttdashboard.com"
SOURCE_TOPIC = "coralcrib/img"

# Publishing later (Phase 6)
PUBLISH_TOPIC = "shouryadippizzachor_shouryadipchakrabortypizzachor"

TARGET_IMAGE_PATH = "CrabAndLobster.jpeg"
MQTT_TIMEOUT_SEC = 30
BLOCK_SIZE = 8   # OT block size (critical for SSIM)

# --------------------------------------
source_image = None

# ---------------- MQTT CALLBACKS ----------------
def on_connect(client, userdata, flags, rc):
    print("[MQTT] Connected:", rc)
    client.subscribe(SOURCE_TOPIC)
    print("[MQTT] Subscribed to", SOURCE_TOPIC)

def on_message(client, userdata, msg):
    global source_image
    print("[MQTT] RX on", msg.topic)

    payload = msg.payload

    # Raw image bytes (PNG/JPEG)
    try:
        img = Image.open(BytesIO(payload)).convert("RGB")
        source_image = img
        print("[MQTT] Source image loaded (raw bytes):", img.size)
        return
    except Exception:
        pass

    # JSON-wrapped base64
    try:
        payload_str = payload.decode("utf-8", errors="ignore")
        data = json.loads(payload_str)
        b64 = data.get("data") or data.get("image") or data.get("img")
        if b64:
            img_bytes = base64.b64decode(b64)
            img = Image.open(BytesIO(img_bytes)).convert("RGB")
            source_image = img
            print("[MQTT] Source image loaded (JSON base64):", img.size)
            return
    except Exception:
        pass

    # Plain base64
    try:
        img_bytes = base64.b64decode(payload, validate=False)
        img = Image.open(BytesIO(img_bytes)).convert("RGB")
        source_image = img
        print("[MQTT] Source image loaded (plain base64):", img.size)
        return
    except Exception as e:
        print("[ERROR] Failed to decode source image:", e)

# ---------------- BLOCK-WISE OT ----------------
def compute_transport(source_img, target_img, block=8):
    src = np.array(source_img)
    tgt = np.array(target_img)

    H, W, C = src.shape
    out = np.zeros_like(src)

    for y in range(0, H, block):
        for x in range(0, W, block):
            src_blk = src[y:y+block, x:x+block]
            tgt_blk = tgt[y:y+block, x:x+block]

            if src_blk.size == 0 or tgt_blk.size == 0:
                continue

            h, w, _ = src_blk.shape

            src_flat = src_blk.reshape(-1, 3)
            tgt_flat = tgt_blk.reshape(-1, 3)

            # Perceptual luminance
            src_lum = (
                0.2126 * src_flat[:,0] +
                0.7152 * src_flat[:,1] +
                0.0722 * src_flat[:,2]
            )
            tgt_lum = (
                0.2126 * tgt_flat[:,0] +
                0.7152 * tgt_flat[:,1] +
                0.0722 * tgt_flat[:,2]
            )

            src_idx = np.argsort(src_lum)
            tgt_idx = np.argsort(tgt_lum)

            mapped = np.zeros_like(src_flat)
            for s, t in zip(src_idx, tgt_idx):
                mapped[t] = src_flat[s]

            out[y:y+block, x:x+block] = mapped.reshape(h, w, 3)

    return out

# ---------------- SSIM ----------------
def compute_ssim(img1, img2):
    g1 = cv2.cvtColor(np.array(img1), cv2.COLOR_RGB2GRAY)
    g2 = cv2.cvtColor(np.array(img2), cv2.COLOR_RGB2GRAY)
    return ssim(g1, g2)

# ---------------- MAIN ----------------
def main():
    global source_image

    # Load target image
    target_img = Image.open(TARGET_IMAGE_PATH).convert("RGB")
    print("[+] Target image loaded:", target_img.size)

    # MQTT setup
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, 1883, 60)
    client.loop_start()

    # Wait for source image
    print("[*] Waiting for source image...")
    start = time.time()
    while source_image is None:
        if time.time() - start > MQTT_TIMEOUT_SEC:
            raise TimeoutError("No source image received via MQTT")
        time.sleep(0.2)

    # Resize source to match target
    if source_image.size != target_img.size:
        print("[!] Resizing source image to match target")
        source_image = source_image.resize(target_img.size, Image.BILINEAR)

    # OT transform
    transformed_arr = compute_transport(
        source_image,
        target_img,
        block=BLOCK_SIZE
    )
    transformed_img = Image.fromarray(transformed_arr)

    # SSIM validation
    score = compute_ssim(transformed_img, target_img)
    print(f"[SSIM] Score = {score:.4f}")

    if score < 0.70:
        print("[WARN] SSIM below minimum threshold")
    else:
        print("[OK] SSIM acceptable")

    # Optional visualization
    transformed_img.show(title="Transformed Image")

    # (Publishing happens in Phase 6)
    print("[*] Phase 2-5 complete")

    client.loop_stop()
    client.disconnect()

# ---------------- RUN ----------------
if __name__ == "__main__":
    main()
