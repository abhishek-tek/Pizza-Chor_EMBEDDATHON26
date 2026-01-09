#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define LED_PIN 2   // change to 13 / 15 / whatever your LED is wired to
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RTOS Handles
QueueHandle_t commandQueue;
TaskHandle_t t_Input;
TaskHandle_t t_Display;
TaskHandle_t t_Blink;

// Shared Global Variable
volatile int currentDelay = 1000; // Default 1 second blink

// Data Structure for the Queue
struct Command {
  char text[20];
  int blinkRate;
};

// ==========================================
// TASK 1: THE HEART (Blink LED)
// ==========================================
void HeartTask(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);

  for (;;) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(100));   // ON for 100ms

    digitalWrite(LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(currentDelay));
  }
}



// ==========================================
// TASK 2: THE EAR (Serial Input)
// ==========================================
void InputTask(void *pvParameters) {
  Serial.begin(115200);

  for (;;) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, input);

      if (!error) {
        Command cmd;

        // Extract JSON fields
        strlcpy(cmd.text, doc["msg"] | "", sizeof(cmd.text));
        cmd.blinkRate = doc["delay"] | currentDelay;

        // Send command to queue
        xQueueSend(commandQueue, &cmd, portMAX_DELAY);

        Serial.println("Command sent to queue!");
      } else {
        Serial.println("JSON Error");
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// TASK 3: THE FACE (OLED Display)
// ==========================================
void DisplayTask(void *pvParameters) {
  Command receivedCmd;

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Waiting...");
  display.display();

  for (;;) {
    if (xQueueReceive(commandQueue, &receivedCmd, portMAX_DELAY)) {

      // Update heartbeat delay
      currentDelay = receivedCmd.blinkRate;

      // Update OLED
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println(receivedCmd.text);
      display.display();

      Serial.println("Screen Updated.");
    }
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  // Create Queue
  commandQueue = xQueueCreate(5, sizeof(Command));

  // Create Tasks
  xTaskCreate(HeartTask, "Heart", 2048, NULL, 1, &t_Blink);
  xTaskCreate(InputTask, "Input", 4096, NULL, 2, &t_Input);
  xTaskCreate(DisplayTask, "Display", 4096, NULL, 2, &t_Display);
}

void loop() {}

