#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ===== WIFI & Backend =====
const char* ssid = "Nord";
const char* password = "12345678";
const char* backend_ip = "172.19.146.197"; 
const int backend_port = 8000;

WebSocketsClient webSocket;
unsigned long lastWsTime = 0;
unsigned long lastLidarTime = 0;
const unsigned long TIMEOUT_MS = 2000; // Stop if no data for 2 seconds

// ===== WSL LIDAR Server =====
WiFiServer lidarServer(5000);
WiFiClient lidarClient;
bool isAutoMode = false; // Tracks current mode

// ===== Motor Pins (NodeMCU-32S) =====
#define DIR1 21  // Left Motor Direction
#define PWM1 22  // Left Motor Speed
#define DIR2 25  // Right Motor Direction
#define PWM2 26  // Right Motor Speed

#define SPEED 200      // 0-255
#define PWM_FREQ 20000
#define PWM_RES 8

// ===== Movement Functions =====
void stopMotors() {
  ledcWrite(PWM1, 0);
  ledcWrite(PWM2, 0);
}

void moveForward() {
  digitalWrite(DIR1, HIGH);
  digitalWrite(DIR2, LOW);
  ledcWrite(PWM1, SPEED);
  ledcWrite(PWM2, SPEED);
}

void moveBackward() {
  digitalWrite(DIR1, LOW);
  digitalWrite(DIR2, HIGH);
  ledcWrite(PWM1, SPEED);
  ledcWrite(PWM2, SPEED);
}

void turnLeft() {
  digitalWrite(DIR1, LOW);
  digitalWrite(DIR2, LOW);
  ledcWrite(PWM1, SPEED);
  ledcWrite(PWM2, SPEED);
}

void turnRight() {
  digitalWrite(DIR1, HIGH);
  digitalWrite(DIR2, HIGH);
  ledcWrite(PWM1, SPEED);
  ledcWrite(PWM2, SPEED);
}

// ===== WebSocket Event Handler (UI Control) =====
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected!");
      stopMotors();
      break;
      
    case WStype_CONNECTED:
      Serial.println("[WS] Connected to AiravatX Backend!");
      break;
      
    case WStype_TEXT:
      lastWsTime = millis(); // Reset WS safety timer
      
      StaticJsonDocument<1024> doc; 
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        // 1. Check Emergency Stop First
        bool is_e_stop = doc["is_emergency_stopped"] | false;
        if (is_e_stop) {
            stopMotors();
            Serial.println("!!! EMERGENCY STOP !!!");
            return;
        }

        // 2. Extract Mode
        const char* mode = doc["mode"];
        if (mode) {
            bool newModeIsAuto = (strcmp(mode, "auto") == 0);
            
            // If the mode just changed, stop the motors for safety
            if (newModeIsAuto != isAutoMode) {
                isAutoMode = newModeIsAuto;
                stopMotors(); 
                Serial.print("\n>> SYSTEM SWITCHED TO: ");
                Serial.println(isAutoMode ? "AUTONOMOUS MODE" : "MANUAL MODE");
            }
        }

        // 3. Manual Control Logic
        if (!isAutoMode) {
            const char* current_cmd = doc["current_command"];
            if (current_cmd) {
                
                // --- SPAM PREVENTION PRINTING ---
                // 'static' means the ESP32 remembers this variable forever
                static String lastManualCmd = ""; 
                
                // Only print to Serial if you pressed a new button
                if (lastManualCmd != current_cmd) {
                    Serial.print("[MANUAL MODE] Executing: ");
                    Serial.println(current_cmd);
                    lastManualCmd = current_cmd; // Update the memory
                }
                // --------------------------------

                if (strcmp(current_cmd, "forward") == 0) moveForward();
                else if (strcmp(current_cmd, "backward") == 0) moveBackward();
                else if (strcmp(current_cmd, "left") == 0) turnLeft();
                else if (strcmp(current_cmd, "right") == 0) turnRight();
                else stopMotors();
            }
        }
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize Pins
  pinMode(DIR1, OUTPUT);
  pinMode(DIR2, OUTPUT);
  ledcAttach(PWM1, PWM_FREQ, PWM_RES);
  ledcAttach(PWM2, PWM_FREQ, PWM_RES);
  stopMotors();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Start LIDAR TCP Server on Port 5000
  lidarServer.begin();
  Serial.println("LIDAR Server started on Port 5000");

  // Start WebSocket Connection to Backend
  webSocket.begin(backend_ip, backend_port, "/ws/control");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000); 
}

void loop() {
  // 1. Maintain React UI Connection
  webSocket.loop();

  // 2. Handle incoming LIDAR Client connections (Non-blocking)
  if (lidarServer.hasClient()) {
    if (!lidarClient || !lidarClient.connected()) {
      lidarClient = lidarServer.available();
      Serial.println("[LIDAR] WSL Script Connected!");
    } else {
      // Reject any extra clients trying to connect
      WiFiClient rejected = lidarServer.available();
      rejected.stop(); 
    }
  }

  // 3. Read Data from LIDAR Script (Non-blocking)
  if (lidarClient && lidarClient.connected()) {
    while (lidarClient.available()) {
      String cmd = lidarClient.readStringUntil('\n');
      cmd.trim();
      
      // ONLY execute WSL commands if the UI put us in Autonomous Mode
      if (isAutoMode) {
        lastLidarTime = millis(); // Reset safety timer
        
        if (cmd == "F") moveForward();
        else if (cmd == "L") turnLeft();
        else if (cmd == "R") turnRight();
        else if (cmd == "S") stopMotors();
        
        // Optional: Print to verify
        // Serial.print("[AUTO] Command: ");
        // Serial.println(cmd);
      }
    }
  }

  // 4. SAFETY TIMEOUTS
  // If in manual mode, check WS timeout. If in auto mode, check LIDAR timeout.
  if (!isAutoMode && (millis() - lastWsTime > TIMEOUT_MS) && lastWsTime != 0) {
    stopMotors();
  }
  if (isAutoMode && (millis() - lastLidarTime > TIMEOUT_MS) && lastLidarTime != 0) {
    stopMotors();
  }
}