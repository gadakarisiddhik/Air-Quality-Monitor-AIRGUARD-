// ============================================================
// Air Quality Monitor — FINAL FIXED VERSION
// ESP32 + MQ2 + DHT22 + OLED + Buzzer + Firebase
// Fix: API_KEY added — was missing before
// ============================================================

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ══════════════════════════════════════════
// CHANGE ONLY THESE 2 LINES
// ══════════════════════════════════════════
#define WIFI_SSID     "SiddikGadkari"
#define WIFI_PASSWORD "12345678"

// ══════════════════════════════════════════
// YOUR FIREBASE — Both keys needed
// ══════════════════════════════════════════
#define API_KEY      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define DATABASE_URL "https://air-quality-monitor-28291-default-rtdb.asia-southeast1.firebasedatabase.app"

// ══════════════════════════════════════════
// PINS
// ══════════════════════════════════════════
#define DHTPIN    4
#define DHTTYPE   DHT22
#define MQ2PIN    34
#define BUZZPIN   15

// ══════════════════════════════════════════
// OLED
// ══════════════════════════════════════════
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ══════════════════════════════════════════
// OBJECTS
// ══════════════════════════════════════════
DHT          dht(DHTPIN, DHTTYPE);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ══════════════════════════════════════════
// STATE
// ══════════════════════════════════════════
bool wifiOK     = false;
bool firebaseOK = false;

unsigned long lastSend    = 0;
unsigned long lastRead    = 0;
unsigned long lastWifiChk = 0;
unsigned long animTimer   = 0;
int           animFrame   = 0;

int   gasVal  = 0;
float tempVal = 0.0;
float humVal  = 0.0;
bool  buzzerOn = false;

#define GAS_WARN    700
#define GAS_DANGER 1000
#define SEND_EVERY  3000
#define READ_EVERY  2000

// ============================================================
// WiFi CONNECT
// ============================================================
void connectWiFi() {
  Serial.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiOK = true;
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
  } else {
    wifiOK = false;
    Serial.println("\nWiFi FAILED — Offline mode");
  }
}

// ============================================================
// FIREBASE INIT — WITH API KEY (correct version)
// ============================================================
void initFirebase() {
  if (!wifiOK) return;

  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  // Anonymous sign-in
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign-in OK");
  } else {
    Serial.print("signUp msg: ");
    Serial.println(config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.print("Firebase connecting");
  unsigned long t = millis();
  while (!Firebase.ready() && millis() - t < 6000) {
    delay(300); Serial.print(".");
  }

  if (Firebase.ready()) {
    firebaseOK = true;
    Serial.println("\nFirebase READY!");
  } else {
    firebaseOK = false;
    Serial.println("\nFirebase NOT ready — check Anonymous Auth");
  }
}

// ============================================================
// SEND TO FIREBASE
// ============================================================
void sendToFirebase() {
  if (!wifiOK) return;
  if (millis() - lastSend < SEND_EVERY) return;
  lastSend = millis();

  if (!Firebase.ready()) {
    firebaseOK = false;
    initFirebase();
    return;
  }

  String b = "/sensor/latest/";
  bool ok = true;
  ok &= Firebase.RTDB.setInt(   &fbdo, (b+"gas_ppm").c_str(),     gasVal);
  ok &= Firebase.RTDB.setFloat( &fbdo, (b+"temperature").c_str(), tempVal);
  ok &= Firebase.RTDB.setFloat( &fbdo, (b+"humidity").c_str(),    humVal);
  ok &= Firebase.RTDB.setString(&fbdo, (b+"status").c_str(),
        gasVal >= GAS_DANGER ? "DANGER" :
        gasVal >= GAS_WARN   ? "WARNING" : "SAFE");
  ok &= Firebase.RTDB.setString(&fbdo, (b+"wifi_ip").c_str(),
        WiFi.localIP().toString());
  ok &= Firebase.RTDB.setInt(   &fbdo, (b+"uptime_sec").c_str(),
        (int)(millis()/1000));

  if (ok) {
    firebaseOK = true;
    Serial.printf("SENT OK > Gas:%d Temp:%.1f Hum:%.1f\n", gasVal, tempVal, humVal);
  } else {
    firebaseOK = false;
    Serial.println("Send FAILED: " + fbdo.errorReason());
  }
}

// ============================================================
// READ SENSORS
// ============================================================
void readSensors() {
  if (millis() - lastRead < READ_EVERY) return;
  lastRead = millis();

  gasVal = analogRead(MQ2PIN);
  float t = dht.readTemperature();
  float h = dht.readHu  midity();
  if (!isnan(t)) tempVal = t;
  if (!isnan(h)) humVal  = h;

  if (gasVal >= GAS_DANGER && !buzzerOn) {
    digitalWrite(BUZZPIN, HIGH); buzzerOn = true;
  } else if (gasVal < GAS_DANGER && buzzerOn) {
    digitalWrite(BUZZPIN, LOW);  buzzerOn = false;
  }

  Serial.printf("Gas:%4d | Temp:%5.1fC | Hum:%5.1f%% | WiFi:%s | FB:%s\n",
    gasVal, tempVal, humVal,
    wifiOK ? "YES":"NO", firebaseOK ? "YES":"NO");
}

// ============================================================
// OLED STARTUP
// ============================================================
void showStartup() {
  display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  display.display(); delay(120);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(14, 6);  display.print("AIR QUALITY MON");
  display.setCursor(22, 18); display.print("ESP32 + Firebase");
  display.setCursor(30, 30); display.print("Initializing");
  display.drawRect(4, 44, 120, 10, SSD1306_WHITE);
  display.display();
  for (int x = 6; x <= 122; x += 8) {
    display.fillRect(6, 46, x-6, 6, SSD1306_WHITE);
    display.display(); delay(45);
  }
  delay(300);
}

// ============================================================
// OLED SAFE SCREEN — Big CO2 + small temp/hum
// ============================================================
void drawSafeScreen() {
  display.clearDisplay();

  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);   display.print("AIR QUALITY");
  display.setCursor(84, 2);
  display.print(wifiOK ? "W":"-");
  display.print(firebaseOK ? "F":"-");

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 16);  display.print("CO2/GAS");

  // Big centred PPM
  display.setTextSize(3);
  int digits = (gasVal>=1000)?4:(gasVal>=100)?3:(gasVal>=10)?2:1;
  int gx = max(2, (int)(64 - digits*9));
  display.setCursor(gx, 14);
  display.print(gasVal);

  display.setTextSize(1);
  display.setCursor(98, 30); display.print("ppm");

  // Breathing dots
  if (millis() - animTimer > 480) { animFrame=(animFrame+1)%5; animTimer=millis(); }
  for (int i=0; i<animFrame; i++) display.fillCircle(106+i*5, 18, 2, SSD1306_WHITE);

  display.drawLine(0, 46, 128, 46, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 50);  display.print("T:"); display.print(tempVal,1); display.print("C");
  display.drawLine(56, 46, 56, 64, SSD1306_WHITE);
  display.setCursor(60, 50); display.print("H:"); display.print(humVal,1); display.print("%");
  display.setCursor(0, 57);  display.print("SAFE");
  display.setCursor(60, 57); display.print(firebaseOK ? "FB:LIVE":"OFFLINE");

  display.display();
}

// ============================================================
// OLED WARNING SCREEN
// ============================================================
void drawWarningScreen() {
  static bool bl=false; static unsigned long bt=0;
  if (millis()-bt>600){bl=!bl;bt=millis();}
  display.clearDisplay();
  if(bl){display.drawRect(0,0,128,64,SSD1306_WHITE);display.drawRect(2,2,124,60,SSD1306_WHITE);}
  display.fillRect(10,4,108,14,SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);display.setTextSize(1);display.setCursor(20,8);display.print("!! WARNING !!");
  display.setTextColor(SSD1306_WHITE);display.setTextSize(3);display.setCursor(8,22);display.print(gasVal);
  display.setTextSize(1);display.setCursor(84,32);display.print("ppm");
  display.drawLine(0,46,128,46,SSD1306_WHITE);
  display.setCursor(0,50);display.print("T:");display.print(tempVal,1);display.print("C");
  display.drawLine(52,46,52,64,SSD1306_WHITE);
  display.setCursor(56,50);display.print("H:");display.print(humVal,0);display.print("%");
  display.setCursor(8,57);display.print("Open windows now!");
  display.display();
}

// ============================================================
// OLED DANGER SCREEN
// ============================================================
void drawDangerScreen() {
  static bool inv=false; static unsigned long it=0;
  if(millis()-it>380){inv=!inv;it=millis();}
  display.clearDisplay();
  if(inv){display.fillRect(0,0,128,64,SSD1306_WHITE);display.setTextColor(SSD1306_BLACK);}
  else display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);display.setCursor(4,2);display.print("! DANGER !");
  display.setTextSize(2);display.setCursor(4,22);display.print(gasVal);
  display.setTextSize(1);display.setCursor(72,28);display.print("ppm");
  display.setTextSize(1);display.setCursor(4,44);display.print("EVACUATE AREA!");
  display.setCursor(0,56);
  display.print("T:");display.print(tempVal,0);
  display.print("C H:");display.print(humVal,0);display.print("%");
  display.display();
}

// ============================================================
// WIFI RECONNECT
// ============================================================
void checkWifi() {
  if (millis()-lastWifiChk<30000) return;
  lastWifiChk=millis();
  if (WiFi.status()!=WL_CONNECTED) {
    wifiOK=false; firebaseOK=false;
    Serial.println("WiFi lost — reconnecting...");
    WiFi.reconnect(); delay(4000);
    if(WiFi.status()==WL_CONNECTED){wifiOK=true;Serial.println("WiFi restored!");initFirebase();}
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(BUZZPIN, OUTPUT);
  digitalWrite(BUZZPIN, LOW);
  dht.begin();
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED ERROR!");
    while(true) delay(1000);
  }
  showStartup();
  connectWiFi();
  initFirebase();

  display.clearDisplay();
  display.fillRect(0,0,128,13,SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);display.setTextSize(1);display.setCursor(4,3);
  display.print(wifiOK ? "WiFi + Firebase OK!" : "Offline Mode Active");
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10,20);display.print("System Ready!");
  if(wifiOK){display.setCursor(4,34);display.print(WiFi.localIP().toString());}
  display.display();
  delay(2000);
  Serial.println("\n========== SYSTEM READY ==========");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  readSensors();
  sendToFirebase();
  checkWifi();
  if      (gasVal >= GAS_DANGER) drawDangerScreen();
  else if (gasVal >= GAS_WARN)   drawWarningScreen();
  else                           drawSafeScreen();
  delay(100);
}
