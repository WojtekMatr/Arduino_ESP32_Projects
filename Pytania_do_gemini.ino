#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


// ===== KONFIGURACJA WIFI I API =====
const char* ssid = "########";
const char* password = "######";
const char* apiKey = "############################";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BTN_LEFT   33
#define BTN_RIGHT  25
#define BTN_ENTER  27

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Preferences prefs;

enum Mode { KEYBOARD, SHOW_TEXT, LOADING };
Mode mode = KEYBOARD;
// ===== AUTO-REPEAT =====
unsigned long holdL = 0, holdR = 0;
const unsigned long firstDelay = 300;  
const unsigned long repeatDelay = 80; 

int scrollY = 0;
int scrollX = 0;

const char* keyboard[] = {
  "QWERTYUIOP",
  "ASDFGHJKL",
  "ZXCVBNMb",
  " ok"
};

const int rows = 4;
const int cols[rows] = {10, 9, 8, 2};
int row = 0, col = 0;
String text = "";
String aiResponse = "";

bool lastL = HIGH, lastR = HIGH, lastE = HIGH;
unsigned long lastTime = 0;

bool holdMove(int pin, unsigned long &t) {
  if (digitalRead(pin) == LOW) {
    if (millis() - t > repeatDelay) {
      t = millis();
      return true;
    }
  }
  return false;
}

bool click(int pin, bool &last) {
  bool s = digitalRead(pin);
  bool c = (last == HIGH && s == LOW);
  last = s;
  return c;
}

// ===== FUNKCJA KOMUNIKACJI Z GEMINI 1.5 FLASH =====
String askGemini(String userPrompt) {
  if (WiFi.status() != WL_CONNECTED) return "Brak WiFi";

  HTTPClient http;

// Zmień tę linię w funkcji askGemini:
String url = "https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash:generateContent?key=" + String(apiKey);
  http.setTimeout(10000); 
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  
  StaticJsonDocument<1024> doc;
  JsonArray contents = doc.createNestedArray("contents");
  JsonObject content = contents.createNestedObject();
  JsonArray parts = content.createNestedArray("parts");
  parts.createNestedObject()["text"] = userPrompt;

  String requestBody;
  serializeJson(doc, requestBody);

  int code = http.POST(requestBody);
  
  if (code <= 0) {
    String errorMsg = "HTTP error: " + http.errorToString(code);
    http.end();
    return errorMsg;
  }

  String payload = http.getString();
  http.end();


  DynamicJsonDocument resDoc(8192);
  
  DeserializationError err = deserializeJson(resDoc, payload);
  if (err) {
    Serial.print("JSON Error: "); 
    Serial.println(err.c_str());
    return "Błąd parsowania JSON";
  }


  if (resDoc.containsKey("error")) {
    const char* errMsg = resDoc["error"]["message"];
    return "API Error: " + String(errMsg);
  }


  const char* responseText = resDoc["candidates"][0]["content"]["parts"][0]["text"];

  if (responseText == NULL) {
    return "Brak odpowiedzi (NULL)";
  }

  return String(responseText);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(14, 13);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);

  // Połączenie z WiFi
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Laczenie z WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  prefs.begin("kbd", false);
  text = prefs.getString("text", "");
}
void loop() {
  if (millis() - lastTime < 30) return;
  lastTime = millis();

  bool left  = click(BTN_LEFT, lastL);
  bool right = click(BTN_RIGHT, lastR);
  bool enter = click(BTN_ENTER, lastE);

  if (mode == LOADING) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("Gemini mysli...");
    display.display();
    
    // --- DEBUGOWANIE W TERMINALU ---
    Serial.println("\n--- Wysylanie zapytania ---");
    Serial.println("Prompt: " + text);
    text = "( W maksymalnie 150 slowach, bez polskich znakow ) Odpowiedz na: "+ text;
    aiResponse = askGemini(text);

    Serial.println("--- Odpowiedz Gemini ---");
    Serial.println(aiResponse);
    Serial.println("------------------------\n");
    // --------------------------------

    mode = SHOW_TEXT;
    return;
  }

if (mode == SHOW_TEXT) {
  if (enter) {
    mode = KEYBOARD;
    text = "";
    scrollY = 0;
    scrollX = 0;
  }

  if (left)  scrollX -= 6;
  if (right) scrollX += 6;
  if (digitalRead(BTN_LEFT) == LOW && holdMove(BTN_LEFT, holdL))  scrollX -= 6;
  if (digitalRead(BTN_RIGHT) == LOW && holdMove(BTN_RIGHT, holdR)) scrollX += 6;
  if (digitalRead(BTN_ENTER) == LOW && left)  scrollY -= 8;
  if (digitalRead(BTN_ENTER) == LOW && right) scrollY += 8;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(-scrollX, -scrollY);
  display.println("AI:");
  display.println(aiResponse);
  display.display();
  return;
}

if (right) { col++; if (col >= cols[row]) { col = 0; row = (row + 1) % rows; } }
if (left) { col--; if (col < 0) { row--; if (row < 0) row = rows - 1; col = cols[row] - 1; } }


  if (enter) {
    char k = keyboard[row][col];
    if (k == 'b') {
      if (text.length() > 0) text.remove(text.length() - 1);
    }
    else if (k == 'o') {
      mode = LOADING; 
    }
    else {
      text += k;
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(text);
  int y = 16;
  for (int r = 0; r < rows; r++) {
    int x = 0;
    for (int c = 0; c < cols[r]; c++) {
      if (r == row && c == col) {
        display.fillRect(x, y, 12, 12, WHITE);
        display.setTextColor(BLACK);
      } else {
        display.setTextColor(WHITE);
      }
      display.setCursor(x + 3, y + 2);
      display.print(keyboard[r][c]);
      x += 12;
    }
    y += 12;
  }
  display.setTextColor(WHITE);
  display.display();
}
