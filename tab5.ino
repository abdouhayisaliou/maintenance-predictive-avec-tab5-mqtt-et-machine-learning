#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>

// =======================
// CONFIGURATION WIFI
// =======================
const char* WIFI_SSID = "iPhone";
const char* WIFI_PASSWORD = "Garbahayi";

// =======================
// CONFIGURATION MQTT
// =======================
const char* MQTT_SERVER = "test.mosquitto.org";
const int MQTT_PORT = 1883;

const char* TOPIC_DATA = "factory/cnc/data";
const char* TOPIC_PREDICTION = "factory/cnc/prediction";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// =======================
// VARIABLES PREDICTION IA
// =======================
String lastPrediction = "En attente";
String lastHealth = "--";
String lastReason = "--";

// =======================
// STRUCTURE CAPTEUR
// =======================
struct Sensor {
  const char* id;
  const char* name;
  float value;
  float minVal;
  float maxVal;
  float step;
};

Sensor sensors[10] = {
  {"I1", "Temp. broche", 65.0, 40, 120, 1},
  {"I2", "Temp. moteur", 58.0, 40, 120, 1},
  {"I3", "Pression hydro.", 110.0, 60, 200, 2},
  {"I4", "Vibration", 0.40, 0.0, 2.0, 0.05},
  {"I5", "Courant moteur", 5.2, 0, 15, 0.2},
  {"I6", "Vitesse broche", 1600, 500, 3000, 50},
  {"I7", "Charge outil", 45, 0, 100, 2},
  {"I8", "Pression air", 6.5, 0, 10, 0.2},
  {"I9", "Debit refroid.", 8.5, 0, 15, 0.3},
  {"I10", "Temp. refroid.", 26.0, 10, 60, 1}
};

int selectedSensor = 0;
bool autoMode = false;
String currentPreset = "NORMAL";
unsigned long lastAutoSend = 0;

// =======================
// OUTILS AFFICHAGE
// =======================
void drawButton(int x, int y, int w, int h, String label, uint16_t color) {
  M5.Display.fillRoundRect(x, y, w, h, 10, color);
  M5.Display.drawRoundRect(x, y, w, h, 10, TFT_WHITE);
  M5.Display.setTextColor(TFT_WHITE, color);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(x + 10, y + 12);
  M5.Display.print(label);
}

// =======================
// PANNEAU PREDICTION IA
// =======================
void drawPredictionPanel() {
  M5.Display.fillRoundRect(810, 415, 390, 165, 10, TFT_DARKGREY);

  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.setCursor(830, 435);
  M5.Display.print("Prediction IA :");

  uint16_t color = TFT_GREEN;

  if (lastPrediction == "FATIGUE") color = TFT_ORANGE;
  else if (lastPrediction == "DEGRADE") color = TFT_YELLOW;
  else if (lastPrediction == "CRITIQUE") color = TFT_RED;

  M5.Display.setTextSize(3);
  M5.Display.setTextColor(color, TFT_DARKGREY);
  M5.Display.setCursor(830, 470);
  M5.Display.print(lastPrediction);

  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.setCursor(830, 525);
  M5.Display.print("Sante : ");
  M5.Display.print(lastHealth);
  M5.Display.print("%");
}

// =======================
// EXTRACTION JSON SIMPLE
// =======================
String extractStringValue(String msg, String key) {
  String pattern = "\"" + key + "\":\"";
  int start = msg.indexOf(pattern);

  if (start < 0) return "";

  start += pattern.length();
  int end = msg.indexOf("\"", start);

  if (end < 0) return "";

  return msg.substring(start, end);
}

String extractNumberValue(String msg, String key) {
  String pattern = "\"" + key + "\":";
  int start = msg.indexOf(pattern);

  if (start < 0) return "";

  start += pattern.length();

  int endComma = msg.indexOf(",", start);
  int endBrace = msg.indexOf("}", start);

  int end = endComma;

  if (end < 0 || (endBrace > 0 && endBrace < endComma)) {
    end = endBrace;
  }

  if (end < 0) return "";

  return msg.substring(start, end);
}

// =======================
// CALLBACK MQTT PREDICTION
// =======================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println("Prediction recue MQTT :");
  Serial.println(message);

  message.replace(" ", "");

  String pred = extractStringValue(message, "prediction");
  String health = extractNumberValue(message, "health_score");
  String reason = extractStringValue(message, "reason");

  if (pred != "") lastPrediction = pred;
  if (health != "") lastHealth = health;
  if (reason != "") lastReason = reason;

  Serial.print("Prediction extraite : ");
  Serial.println(lastPrediction);

  Serial.print("Sante extraite : ");
  Serial.println(lastHealth);

  drawPredictionPanel();
}

// =======================
// CONNEXION WIFI
// =======================
void connectWiFi() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(40, 120);
  M5.Display.println("Connexion Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");

    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(40, 180);
    M5.Display.print("Tentative : ");
    M5.Display.print(attempts + 1);
    M5.Display.print("/40   ");

    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connecte");
    Serial.println(WiFi.localIP());

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(40, 120);
    M5.Display.println("Wi-Fi connecte");

    M5.Display.setTextSize(2);
    M5.Display.setCursor(40, 180);
    M5.Display.print("IP : ");
    M5.Display.println(WiFi.localIP());

    delay(1200);
  } else {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(40, 120);
    M5.Display.println("Echec Wi-Fi");

    Serial.println("\nEchec Wi-Fi");
    delay(3000);
  }
}

// =======================
// CONNEXION MQTT
// =======================
void connectMQTT() {
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(1024);

  while (!mqtt.connected()) {
    Serial.println("Connexion MQTT...");

    String clientId = "TAB5_CNC_" + String(random(0xffff), HEX);

    if (mqtt.connect(clientId.c_str())) {
      Serial.println("MQTT connecte");

      mqtt.subscribe(TOPIC_PREDICTION);
      Serial.println("Abonne au topic prediction");
    } else {
      Serial.print("Erreur MQTT : ");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// =======================
// PAYLOAD JSON CNC
// =======================
String buildPayload() {
  String p = "{";
  p += "\"machine\":\"CNC_MAZAK_TAB5\",";
  p += "\"timestamp\":\"" + String(millis()) + "\",";
  p += "\"machine_state\":\"" + currentPreset + "\",";
  p += "\"spindle_temp\":" + String(sensors[0].value, 2) + ",";
  p += "\"motor_temp\":" + String(sensors[1].value, 2) + ",";
  p += "\"hydraulic_pressure\":" + String(sensors[2].value, 2) + ",";
  p += "\"vibration\":" + String(sensors[3].value, 2) + ",";
  p += "\"motor_current\":" + String(sensors[4].value, 2) + ",";
  p += "\"spindle_speed\":" + String(sensors[5].value, 2) + ",";
  p += "\"tool_load\":" + String(sensors[6].value, 2) + ",";
  p += "\"air_pressure\":" + String(sensors[7].value, 2) + ",";
  p += "\"coolant_flow\":" + String(sensors[8].value, 2) + ",";
  p += "\"coolant_temp\":" + String(sensors[9].value, 2);
  p += "}";

  return p;
}

// =======================
// ENVOI MQTT
// =======================
void sendMQTT() {
  if (!mqtt.connected()) {
    connectMQTT();
  }

  String payload = buildPayload();

  Serial.print("Taille payload : ");
  Serial.println(payload.length());

  bool ok = mqtt.publish(TOPIC_DATA, payload.c_str());

  if (ok) {
    Serial.println("Publication MQTT OK");
    Serial.println(payload);

    M5.Display.fillRoundRect(820, 585, 350, 45, 8, TFT_BLUE);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLUE);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(840, 600);
    M5.Display.print("Donnees envoyees");
  } else {
    Serial.println("Echec publication MQTT");

    M5.Display.fillRoundRect(820, 585, 350, 45, 8, TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(840, 600);
    M5.Display.print("Erreur MQTT");
  }
}

// =======================
// INTERFACE PRINCIPALE
// =======================
void drawInterface() {
  M5.Display.fillScreen(TFT_BLACK);

  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(25, 20);
  M5.Display.print("Supervision CNC - TAB5");

  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(25, 60);
  M5.Display.print("Mode : ");
  M5.Display.print(autoMode ? "AUTO" : "MANUEL");

  M5.Display.setCursor(300, 60);
  M5.Display.print("Etat choisi : ");
  M5.Display.print(currentPreset);

  drawButton(900, 25, 130, 50, autoMode ? "AUTO ON" : "AUTO", TFT_DARKGREEN);
  drawButton(1050, 25, 140, 50, "ENVOYER", TFT_BLUE);

  int startY = 110;

  for (int i = 0; i < 10; i++) {
    int y = startY + i * 52;
    uint16_t bg = (i == selectedSensor) ? TFT_DARKGREY : TFT_BLACK;

    M5.Display.fillRoundRect(20, y - 5, 760, 45, 6, bg);

    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_CYAN, bg);
    M5.Display.setCursor(35, y + 5);
    M5.Display.print(sensors[i].id);

    M5.Display.setTextColor(TFT_WHITE, bg);
    M5.Display.setCursor(90, y + 5);
    M5.Display.print(sensors[i].name);

    M5.Display.setTextColor(TFT_YELLOW, bg);
    M5.Display.setCursor(390, y + 5);
    M5.Display.print(sensors[i].value, 2);

    int barX = 530;
    int barY = y + 12;
    int barW = 220;

    M5.Display.drawRoundRect(barX, barY, barW, 12, 5, TFT_WHITE);

    float ratio = (sensors[i].value - sensors[i].minVal) / (sensors[i].maxVal - sensors[i].minVal);
    ratio = constrain(ratio, 0, 1);

    M5.Display.fillRoundRect(barX, barY, ratio * barW, 12, 5, TFT_GREEN);
  }

  drawButton(820, 140, 120, 55, "-", TFT_RED);
  drawButton(960, 140, 120, 55, "+", TFT_GREEN);

  drawButton(820, 240, 170, 50, "NORMAL", TFT_DARKGREEN);
  drawButton(1010, 240, 170, 50, "FATIGUE", TFT_ORANGE);

  drawButton(820, 310, 170, 50, "DEGRADE", TFT_YELLOW);
  drawButton(1010, 310, 170, 50, "CRITIQUE", TFT_RED);

  drawPredictionPanel();
}

// =======================
// PRESETS MACHINE
// =======================
void applyPreset(String preset) {
  currentPreset = preset;

  if (preset == "NORMAL") {
    float vals[10] = {65, 58, 110, 0.35, 5.2, 1600, 45, 6.5, 8.5, 26};
    for (int i = 0; i < 10; i++) sensors[i].value = vals[i];
  }

  else if (preset == "FATIGUE") {
    float vals[10] = {76, 70, 128, 0.75, 6.8, 1650, 65, 6.0, 6.8, 31};
    for (int i = 0; i < 10; i++) sensors[i].value = vals[i];
  }

  else if (preset == "DEGRADE") {
    float vals[10] = {88, 82, 150, 1.10, 8.5, 1700, 78, 5.4, 5.0, 38};
    for (int i = 0; i < 10; i++) sensors[i].value = vals[i];
  }

  else if (preset == "CRITIQUE") {
    float vals[10] = {102, 95, 175, 1.65, 10.5, 1800, 92, 4.8, 3.0, 48};
    for (int i = 0; i < 10; i++) sensors[i].value = vals[i];
  }

  drawInterface();
}

// =======================
// MODE AUTO
// =======================
void randomizeValues() {
  for (int i = 0; i < 10; i++) {
    sensors[i].value += random(-100, 100) * sensors[i].step / 100.0;
    sensors[i].value = constrain(sensors[i].value, sensors[i].minVal, sensors[i].maxVal);
  }
}

// =======================
// GESTION TACTILE
// =======================
void handleTouch() {
  auto t = M5.Touch.getDetail();

  if (!t.wasPressed()) return;

  int x = t.x;
  int y = t.y;

  // Selection capteur
  if (x >= 20 && x <= 780 && y >= 105 && y <= 640) {
    int index = (y - 105) / 52;

    if (index >= 0 && index < 10) {
      selectedSensor = index;
      drawInterface();
      return;
    }
  }

  // Bouton moins
  if (x >= 820 && x <= 940 && y >= 140 && y <= 195) {
    sensors[selectedSensor].value -= sensors[selectedSensor].step;
    sensors[selectedSensor].value = max(sensors[selectedSensor].minVal, sensors[selectedSensor].value);
    drawInterface();
    return;
  }

  // Bouton plus
  if (x >= 960 && x <= 1080 && y >= 140 && y <= 195) {
    sensors[selectedSensor].value += sensors[selectedSensor].step;
    sensors[selectedSensor].value = min(sensors[selectedSensor].maxVal, sensors[selectedSensor].value);
    drawInterface();
    return;
  }

  // AUTO
  if (x >= 900 && x <= 1030 && y >= 25 && y <= 75) {
    autoMode = !autoMode;
    drawInterface();
    return;
  }

  // ENVOYER
  if (x >= 1050 && x <= 1190 && y >= 25 && y <= 75) {
    sendMQTT();
    return;
  }

  // Presets
  if (x >= 820 && x <= 990 && y >= 240 && y <= 290) {
    applyPreset("NORMAL");
    return;
  }

  if (x >= 1010 && x <= 1180 && y >= 240 && y <= 290) {
    applyPreset("FATIGUE");
    return;
  }

  if (x >= 820 && x <= 990 && y >= 310 && y <= 360) {
    applyPreset("DEGRADE");
    return;
  }

  if (x >= 1010 && x <= 1180 && y >= 310 && y <= 360) {
    applyPreset("CRITIQUE");
    return;
  }
}

// =======================
// SETUP
// =======================
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  randomSeed(millis());

  M5.Display.setRotation(1);

  mqtt.setBufferSize(1024);
  mqtt.setCallback(mqttCallback);

  connectWiFi();
  connectMQTT();

  drawInterface();

  Serial.println("TAB5 CNC pret");
}

// =======================
// LOOP
// =======================
void loop() {
  M5.update();

  if (!mqtt.connected()) {
    connectMQTT();
  }

  mqtt.loop();
  handleTouch();

  if (autoMode && millis() - lastAutoSend > 2000) {
    lastAutoSend = millis();

    randomizeValues();
    sendMQTT();
    drawInterface();
  }
}