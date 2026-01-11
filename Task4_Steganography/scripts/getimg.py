import json
import base64
import paho.mqtt.client as mqtt
import time

BROKER = "broker.mqttdashboard.com"
PORT = 1883

REQUEST_TOPIC = "kelpsaute/steganography"
RESPONSE_TOPIC = "mnjki_window"

REQUEST_PAYLOAD = {
    "request": "REEFING KRILLS :( CORALS BLOOM <3",
    "agent_id": "shouryadippizzachor"
}

buffer = ""
collecting = False

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code != 0:
        print("[!] Connection failed:", reason_code)
        return

    print("[+] Connected")

    client.subscribe(RESPONSE_TOPIC)

    client.publish(
        REQUEST_TOPIC,
        json.dumps(REQUEST_PAYLOAD),
        qos=1
    )
    print("[>] Request sent")


json_buffer = ""
collecting = False

def on_message(client, userdata, msg):
    global json_buffer, collecting

    payload = msg.payload.decode(errors="ignore")

    # Ignore known noise
    if payload.startswith("REEFING KRILLS"):
        return

    # Detect JSON start
    if "{" in payload and not collecting:
        collecting = True
        json_buffer = payload[payload.find("{"):]
    elif collecting:
        json_buffer += payload

    # Detect JSON end
    if collecting and "}" in json_buffer:
        try:
            obj = json.loads(json_buffer)
            print("[+] JSON fully received")

            b64 = obj["data"]
            img = base64.b64decode(b64)

            with open("artifact.png", "wb") as f:
                f.write(img)

            print("[+] Image written: artifact.png")
            print("[+] Size:", len(img), "bytes")

            # Reset state
            collecting = False
            json_buffer = ""

        except Exception as e:
            print("[!] JSON parse failed:", e)
            collecting = False
            json_buffer = ""


def process_json(raw):
    print("\n[+] JSON fully received")

    data = json.loads(raw)

    img_b64 = data["data"]
    png = base64.b64decode(img_b64)

    with open("artifact.png", "wb") as f:
        f.write(png)

    print("[+] Image written: artifact.png")
    print("[+] Size:", len(png), "bytes")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)
client.loop_forever()
