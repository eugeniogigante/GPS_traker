#include <TFT_eSPI.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "html.h"
// Configurazione hardware
#define RXD2 15
#define TXD2 17
HardwareSerial SerialA9G(2);
TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);

// Configurazioni WiFi
const char* AP_SSID = "GPSTrackerAP";
const char* AP_PASS = "tracker123";
const char* CLIENT_SSID = "XXXXXX";
const char* CLIENT_PASS = "XXXXXX";

bool isAPMode = true;
unsigned long apStartTime = 0;
const unsigned long AP_TIMEOUT = 60000; // 1 minuto

// Login web
const char* WEB_USER = "admin";
const char* WEB_PASS = "tracker123";
bool isAuthenticated = false;

String latString = "";
String lonString = "";


float lat = 0;
float lng = 0;
float acc = 0;
float sat = 0;
double currentLat = 0;
double currentLng = 0;
unsigned long lastGpsUpdate = 0;
int satelliteCount = 0;
float speedKmph = 0;
float accuracy = 0;

String token = "123456789";  // Token Bearer
String SERVER_API = "http://192.168.1.100:5004/api/location";// IP del server Flask
String TOKEN = "123456789";
String APN = "apn.vodafone.com";

// Cloud services
String FIREBASE_HOST = "your-project.firebaseio.com";
String FIREBASE_KEY = "your-secret-key";
String THINGSPEAK_KEY = "your-key";

// GPS
TinyGPSPlus gps;

void startAPMode() {
  WiFi.softAP(AP_SSID, AP_PASS);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("AP Mode");
  tft.println("SSID: " + String(AP_SSID));
  tft.println("IP: " + WiFi.softAPIP().toString());
  isAPMode = true;
  apStartTime = millis();
}

void connectToWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.print("Connecting to WiFi...");
  
  WiFi.begin(CLIENT_SSID, CLIENT_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    tft.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(10, 10);
    tft.println("WiFi Connected");
    tft.println("IP: " + WiFi.localIP().toString());
    isAPMode = false;
  } else {
    startAPMode();
  }
}
//--------------------
String convertGPRMCtoGPGLL(String gprmc) {
  // Verifica che la stringa sia una sentenza GPRMC valida
  if (!gprmc.startsWith("$GPRMC")) return "";
  // Dividi la stringa nei suoi componenti
  int idx = 0;
  String fields[13];
  while (gprmc.length() > 0 && idx < 13) {
    int commaIndex = gprmc.indexOf(',');
    if (commaIndex == -1) {
      fields[idx++] = gprmc;
      break;
    }
    fields[idx++] = gprmc.substring(0, commaIndex);
    gprmc = gprmc.substring(commaIndex + 1);
  }
  // Campi GPRMC: [0]$GPRMC, [1]Time, [2]Status, [3]Lat, [4]NS, [5]Lon, [6]EW
  if (fields[2] != "A") return ""; // A = attivo/valido, V = invalido
  latString = fields[3];
  String ns = fields[4];
  lonString = fields[5];
  String ew = fields[6];
  String time = fields[1];
  // Costruzione sentenza GPGLL
  String gpgll = "$GPGLL,";
  gpgll += latString + "," + ns + ",";
  gpgll += lonString  + "," + ew + ",";
  gpgll += time + ",A";  // "A" per status = attivo
  Serial.print("-----latString:");
  Serial.println(latString);
  Serial.print("-----lonString :");
  Serial.println(lonString);

  // Calcolo checksum
  byte checksum = 0;
  for (int i = 1; i < gpgll.length(); i++) {
    checksum ^= gpgll[i];
  }
  gpgll += "*";
  if (checksum < 16) gpgll += "0";
  String checksumStr = String(checksum, HEX);
  checksumStr.toUpperCase();
  gpgll += checksumStr;
  return gpgll;
}

void processGpsData(String line) {
  line.trim();
  //Serial.print("Sting captured...");
  //Serial.println(line);
  String GPGLL = convertGPRMCtoGPGLL(line);
  GPGLL.trim();
  //Serial.print("Sting GPRMC converted in GPGLL...");
  //Serial.println(GPGLL);
  Serial.print("-----------------");
  for (int i = 1; i < line.length(); i++) {
    gps.encode(line.charAt(i));
    Serial.print(line.charAt(i));
  }
  Serial.println("-----------------");
  lat = gps.location.lat();
  lng = gps.location.lng();
  acc = gps.hdop.value() / 100.0;
  sat = gps.satellites.value();
  double speed = gps.speed.kmph();
  //lat = latString.toFloat()/100;
  //lng = lonString.toFloat()/100;

  Serial.print("lat....");
  Serial.println(lat);
  Serial.print("lng....");
  Serial.println(lng);
  Serial.print("satellite captured....");
  Serial.println(gps.satellites.value()); 
  // Aggiorna display ogni 60s
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 6000) {
    lastDisplayUpdate = millis();
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 10);
    tft.printf("Lat: %.6f\nLng: %.6f\nAcc: %.2fm\nSat: %.0f\nSpeed: %.0f", lat, lng, acc, sat, speed);
    tft.setCursor(0, 90);//tft.setCursor(x, k);
    tft.println(line);
  }
}

bool sendAT(String cmd, String expected, unsigned long timeout = 2000) {
  SerialA9G.println(cmd);
  unsigned long t = millis();
  String response = "";
  while (millis() - t < timeout) {
    while (SerialA9G.available()) {
      char c = SerialA9G.read();
      response += c;
      if (response.indexOf(expected) != -1) {
        Serial.println("✓ " + cmd);
        return true;
      }
    }
  }
  Serial.println("✗ " + cmd + " -> " + response);
  return false;
}

// Invia posizione al server Flask via HTTP POST
void sendLocationToServer(String lat, String lng) {
  // Avvia GPRS
  sendAT("AT", "OK");
  sendAT("AT+CPIN?", "READY");
  sendAT("AT+CREG?", "0,1");  // registrato sulla rete
  sendAT("AT+CGATT=1", "OK");
  sendAT("AT+CGDCONT=1,\"IP\",\"your.apn\"", "OK");  // Sostituisci your.apn
  sendAT("AT+CGACT=1,1", "OK");
  sendAT("AT+CGPADDR=1", ".");

  // Inizializza HTTP
  sendAT("AT+HTTPTERM", "OK");  // pulizia
  sendAT("AT+HTTPINIT", "OK");
  sendAT("AT+HTTPPARA=\"CID\",1", "OK");
  sendAT("AT+HTTPPARA=\"URL\",\"" + SERVER_API + "\"", "OK");
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK");
  sendAT("AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer " + token + "\"", "OK");

  // Prepara payload JSON
  String payload = "{\"lat\": " + lat + ", \"lng\": " + lng + "}";
  int len = payload.length();

  sendAT("AT+HTTPDATA=" + String(len) + ",10000", "DOWNLOAD");
  SerialA9G.print(payload);
  delay(2000);  // tempo per invio dati

  sendAT("AT+HTTPACTION=1", "+HTTPACTION: 1,", 10000);  // POST
  delay(3000);  // attesa risposta

  SerialA9G.println("AT+HTTPREAD");
  delay(2000);
  while (SerialA9G.available()) Serial.write(SerialA9G.read());  // stampa risposta

  sendAT("AT+HTTPTERM", "OK");
}


// Web handler
void handleLoginGET(AsyncWebServerRequest *request) {
  request->send(200, "text/html", 
    "<form method='POST' action='/login'>"
    "<input type='text' name='user' placeholder='Username'>"
    "<input type='password' name='pass' placeholder='Password'>"
    "<button type='submit'>Login</button></form>");
}

void handleLoginPOST(AsyncWebServerRequest *request) {
  String user = request->arg("user");
  String pass = request->arg("pass");
  if (user == WEB_USER && pass == WEB_PASS) {
    isAuthenticated = true;
    request->send(200, "text/html",
      "<script>localStorage.setItem('authenticated','true');window.location='/';</script>");
  } else {
    request->send(401, "text/html", "Login Fallito");
  }
}

void handleMap(AsyncWebServerRequest *request) {
  if (!isAuthenticated) {
    request->send(401, "text/plain", "Non autorizzato");
    return;
  }

  // Crea stringa HTML unica sostituendo i placeholder
  String html = MAP_HTML_TEMPLATE;
  
  // Formatta l'orario dell'ultimo aggiornamento
  char timeStr[20];
  unsigned long seconds = millis() / 1000;
  snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", 
           (seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60);

  // Sostituisci tutti i placeholder
  html.replace("%LAT%", String(lat, 8));
  html.replace("%LNG%", String(lng, 9));
  html.replace("%SAT%", String(satelliteCount));
  html.replace("%SPEED%", String(speedKmph, 1));
  html.replace("%ACC%", String(accuracy, 1));
  html.replace("%LAST_UPDATE%", timeStr);

  request->send(200, "text/html", html);
}

void handleCurrentPosition(AsyncWebServerRequest *request) {
  StaticJsonDocument<200> doc;
  doc["lat"] = lat;
  doc["lng"] = lng;
  doc["timestamp"] = lastGpsUpdate;
  doc["valid"] = (currentLat != 0 && currentLng != 0);
  
  String response;
  serializeJson(doc, response);
  
  request->send(200, "application/json", response);
}

void handleConfigGET(AsyncWebServerRequest *request) {
  if (!isAuthenticated) {
    request->send(401, "text/plain", "Non autorizzato");
    return;
  }

  String html = CONFIG_HTML;
  html.replace("%FIREBASE_HOST%", FIREBASE_HOST);
  html.replace("%FIREBASE_KEY%", FIREBASE_KEY);
  html.replace("%SERVER_API%", SERVER_API);
  html.replace("%THINGSPEAK_KEY%", THINGSPEAK_KEY);
  html.replace("%TOKEN%", TOKEN);
  html.replace("%APN%", APN);
  
  request->send(200, "text/html", html);
}

void handleConfigPOST(AsyncWebServerRequest *request) {
  if (!isAuthenticated) {
    request->send(401, "text/plain", "Non autorizzato");
    return;
  }

  if (request->hasParam("firebaseHost", true)) {
    FIREBASE_HOST = request->getParam("firebaseHost", true)->value();
  }
  if (request->hasParam("firebaseKey", true)) {
    FIREBASE_KEY = request->getParam("firebaseKey", true)->value();
  }
  if (request->hasParam("serverApi", true)) {
    SERVER_API = request->getParam("serverApi", true)->value();
  }
  if (request->hasParam("thingspeakKey", true)) {
    THINGSPEAK_KEY = request->getParam("thingspeakKey", true)->value();
  }
  if (request->hasParam("telegramToken", true)) {
    TOKEN = request->getParam("Token", true)->value();
  }
  if (request->hasParam("chatId", true)) {
    APN = request->getParam("apn", true)->value();
  }

  request->send(200, "text/plain", "OK");
}

void sendLocationSMS(String number) {

  String msg = "Posizione:\nLat: " + String(lat, 6) +
               "\nLng: " + String(lng, 6) +
               "\nhttps://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
  // Invia comandi AT per SMS
  SerialA9G.println("AT+CMGF=1"); // Modalità testo
  delay(1000);
  SerialA9G.print("AT+CMGS=\"");
  SerialA9G.print(number);
  SerialA9G.println("\"");
  delay(1000);
  SerialA9G.print(msg);
  SerialA9G.write(26);  // CTRL+Z per invio SMS
  delay(3000);

  Serial.println("SMS inviato a " + number);
}

// Modifica la funzione checkForIncomingSMS per accettare la riga completa
void checkForIncomingSMS(String smsData) {
  Serial.print("IncomingsmsData string..");
  Serial.println(smsData);
  static String smsBuffer = "";
  static String senderNumber = "";
  smsData.trim();
  if (smsData.startsWith("+CMT:")) {
    // Estrai numero mittente
    int start = smsData.indexOf('"');
    int end = smsData.indexOf('"', start + 1);
    
    if (start != -1 && end != -1) {
      senderNumber = smsData.substring(start + 1, end);
    }
    
    Serial.print("--SMS incoming string start=");
    Serial.print(start);
    Serial.print("--end=");
    Serial.println(end);
    Serial.print("--substring=");
    Serial.println(senderNumber);

    sendLocationSMS(senderNumber);
  } 
  else {
    smsBuffer = smsData;
    smsBuffer.toUpperCase();
    
    
    // Reset per il prossimo SMS
   
    senderNumber = "";
  }
}

// Modifica la funzione checkForIncomingCall per accettare la riga completa
void checkForIncomingCall(String clipData) {
  Serial.print("IncomingCall string..");
  Serial.println(clipData);
  // Estrai numero di telefono dal tipo: +CLIP: "391234567890",145
  int firstQuote = clipData.indexOf('"');
  int secondQuote = clipData.indexOf('"', firstQuote + 1);
  if (firstQuote != -1 && secondQuote != -1) {
    String callerNumber = clipData.substring(firstQuote + 1, secondQuote);
    Serial.println("Chiamata da: " + callerNumber);
    sendLocationSMS(callerNumber);
  }
}

// Modifica il loop principale
void processSerialData() {
  static String serialBuffer = ""; 
  while (SerialA9G.available()) {
    char c = SerialA9G.read();
    serialBuffer += c;
    // Controlla fine riga
    if (c == '\n') {
      serialBuffer.trim();
      // Sostituzione prefisso GN con GP per compatibilità NMEA
      if (serialBuffer.startsWith("$")) {
        // 👇 Rimpiazza il prefisso $GN con $GP
        if (serialBuffer.startsWith("$GN")) {
          serialBuffer.replace("$GN", "$GP");
          Serial.println("Convertito prefisso GN->GP: " + serialBuffer);
        }
      }
      // Gestione dati GPS (NMEA)
      if (serialBuffer.startsWith("$GPRMC") || serialBuffer.startsWith("$GNRMC")) {
        // Modifica la validità se necessario (V -> A)
        int firstComma = serialBuffer.indexOf(',');
        int secondComma = serialBuffer.indexOf(',', firstComma + 1);
        int thirdComma = serialBuffer.indexOf(',', secondComma + 1);        
        if (thirdComma != -1) {
          String validity = serialBuffer.substring(secondComma + 1, thirdComma);
          if (validity == "V") {
            serialBuffer = serialBuffer.substring(0, secondComma + 1) + "A" + serialBuffer.substring(thirdComma);
            Serial.println("Forzata validità V->A: " + serialBuffer);
          }
        }       
        //processGpsData(serialBuffer);
      }
      // Gestione SMS in arrivo
      else if (serialBuffer.startsWith("+CMT:")) {
        checkForIncomingSMS(serialBuffer);
        Serial.print("--SMS incoming--");
        Serial.println(serialBuffer);
      }
      // Gestione chiamate in arrivo
      else if (serialBuffer.startsWith("RING")) {
      //else if (serialBuffer.startsWith("+CLIP:")) {  
        checkForIncomingCall(serialBuffer);
        Serial.print("--Call incoming--");
        Serial.println(serialBuffer);
        delay(2000);
      }
      // Log per altre sentenze NMEA utili (opzionale)
      else if (serialBuffer.startsWith("$GPGGA") || serialBuffer.startsWith("$GNGGA")) {
        Serial.println("Dati GGA ricevuti: " + serialBuffer);
      }

      processGpsData(serialBuffer);

      serialBuffer = ""; // Pulisci il buffer dopo l'elaborazione
    }
  }
}

// Setup
void setup() {
  Serial.begin(115200);
  SerialA9G.begin(115200, SERIAL_8N1, RXD2, TXD2);
  delay(3000);
  SerialA9G.println("AT+CMGF=1\r"); // Modalità testo per SMS
  Serial.println(SerialA9G.read());
  delay(2000);
  SerialA9G.println("AT+CNMI=1,2,0,0,0\r"); // Mostra SMS ricevuti direttamente via seriale
  Serial.println(SerialA9G.read());
  delay(2000);
  SerialA9G.println("AT+CLIP=1\r");  // Abilita identificazione chiamante
  Serial.println(SerialA9G.read());
  delay(2000);
  SerialA9G.println("AT+GPS=1");   // Abilita GPS
  Serial.println(SerialA9G.read());
  delay(2000);
  SerialA9G.println("AT+GPSRD=10"); // Abilita output continuo del GPS
  Serial.println(SerialA9G.read());
  delay(2000);
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("GPS Tracker", 10, 10);
  delay(2000);  
  // Prova prima a connettersi come client
  connectToWiFi();
  // Se non riesce, avvia AP mode
  if (isAPMode) {
    startAPMode();
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isAuthenticated) request->redirect("/login");
    else request->send(200, "text/html", "<h1>GPS Tracker</h1><a href='/map'>Map</a> <br> <h1>Config</h1><a href='/config'>Config</a>");
  });
  server.on("/login", HTTP_GET, handleLoginGET);
  server.on("/login", HTTP_POST, handleLoginPOST);
  server.on("/map", HTTP_GET, handleMap);
  server.on("/config", HTTP_GET, handleConfigGET);
  server.on("/config", HTTP_POST, handleConfigPOST);
  server.on("/currentPosition", HTTP_GET, handleCurrentPosition);
  server.begin();
}
// Loop
void loop() {
  processSerialData(); // Sostituisce tutte le letture seriali precedenti 
  static unsigned long lastCloudUpdate = 0;
  if (millis() - lastCloudUpdate > 300000) { // 5 minuti
    lastCloudUpdate = millis();
  }
}
