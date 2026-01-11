# 🪸 EMBEDDATHON 26 — The Shrimphub Reef

### 📋 Team Information

* **Team ID:** `shouryadippizzachor`
* **Reef ID:** `shouryadipchakrabortypizzachor`
* **Members:**
    - Member 1: **Shouryadip Chakraborty**
    - Member 2: **Abhishek Agrawal**
    - Member 3: **Joseph Varghese**


---

### 🚀 Project Overview

This project involves a series of technical challenges centered around embedded programming, FreeRTOS, MQTT, computer vision.


This repository documents our step-by-step interaction with the **Reef Communication System**, covering Tasks 1 through 5. Each task progressively combines embedded communication, signal reconstruction,perception, and optimization-based image transformation.

---

### 🎥 Video Links

| Task | Status | Description | Link |
| --- | --- | --- | --- |
| **Task 1** | Complete | TimingKeeper | [Watch Here](https://drive.google.com/drive/u/0/folders/16MjN7a2jGITHiJQ0Yph9IkpfgPqUEv4e) |
| **Task 2** | Complete | PriorityGuardian | [Watch Here](https://drive.google.com/drive/u/0/folders/1EaSCldSbkz9K8mZ-1lcq-wq4Ph8s5YH6) |
| **Task 3** | Complete | WindowSync | [Watch Here](https://drive.google.com/drive/u/0/folders/1rGp70zk-BbdO2d55ptRekyOxH7--pEMM) |
| **Task 4** | Complete | Steganography | NA |
| **Task 5** | In Progress | PixelSculptor | NA |
| **Task 6** |  Not Done | SequenceRenderer |  NA  |

---

### 🛠️ Build Instructions

#### **Prerequisites**

Ensure the following tools and dependencies are installed on the host system:

* **Operating System:** Linux / macOS / Windows
* **ESP-IDF Version:** v5.x (recommended)
* **Python:** ≥ 3.10
* **Git**
* **USB drivers** for ESP32 (CP210x / CH340 as applicable)


#### **1. Install ESP-IDF**

Clone and install ESP-IDF following the official Espressif instructions:

```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
```

Activate the ESP-IDF environment:

```bash
source export.sh
```

> On Windows (PowerShell):

```powershell
export.ps1
```

#### **2. Clone Project Repository**

```bash
git clone <YOUR_GITHUB_REPOSITORY_URL>
cd <PROJECT_DIRECTORY>
```


#### **3. Set Target Chip**

Configure the target ESP chip (example: ESP32):

```bash
idf.py set-target esp32
```

Supported targets may include:

* `esp32`
* `esp32s3`
* `esp32c3`


#### **4. Configure Project**

Open the ESP-IDF configuration menu:

```bash
idf.py menuconfig
```

Configure the following (if applicable):

* Wi-Fi credentials
* MQTT broker address and port
* GPIO pin mappings
* Logging level

Save and exit.


#### **5. Build the Project**

Compile the Task:

```bash
cd <TaskDirectory>
idf.py build
```

On success, the compiled binaries will be generated in the `build/` directory.


#### **6. Flash to ESP Device**

Connect the ESP board via USB and flash:

```bash
idf.py flash
```

If multiple serial ports exist, specify explicitly:

```bash
idf.py -p /dev/ttyUSB0 flash
```

#### **7. Monitor Serial Output**

To view runtime logs:

```bash
idf.py monitor
```

Combined flash + monitor:

```bash
idf.py flash monitor
```

Exit monitor using:

```
Ctrl + ]
```



#### **8. Clean Build (Optional)**

If build errors occur due to configuration changes:

```bash
idf.py fullclean
idf.py build
```

---

### 🤝 Collaborators Note

This project was developed for the Embeddathon 2026 Challenge built over 24 hours, through division of tasks within members and aiming for the optimal solution.
