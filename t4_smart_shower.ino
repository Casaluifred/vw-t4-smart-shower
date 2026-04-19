/*
  VW T4 Smart Shower - ESP-01S Relais v4.0
  -------------------------------------------------------
  Steuert eine 12V Pumpe über Taster UND WLAN (SoftAP).
  VW T4 Smart Shower - ESP-01S Relais v4.0 + WebInterface + EEPROM
    
  Features:
  - Automatischer Timer (Auto-Off)
  - Dauer dauerhaft speicherbar (EEPROM) über Webseite
  - Bedienung per Taster (RX-Pin)
  - Bedienung per Handy (WLAN Access Point)
  - Live-Sync zwischen WebApp und Hardware-Taster
  - SoftAP: "T4 SmartShower" (IP: 192.168.4.5)
  
  Hardware:
  - ESP8266 ESP-01S
  - Relais Modul v4.0 (5V Version)
  - Edelstahl-Taster (mit LED-Ring)

  IDE-Board-Verwaltung: 
  - Board: Generic ESP8266 Module
  - Upload Speed: 115200
-------------------------------------------------------
V3.0 vom 11.01.2026
Fred Fiedler
Creative Commons (4.0 Internationale Lizenz)
-------------------------------------------------------
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// Pins und Konfiguration
const int RELAY_PIN = 0;    
const int BUTTON_PIN = 3;   // RX-Pin
const int RELAY_ON_STATE  = LOW; 
const int RELAY_OFF_STATE = HIGH;

// Variable für Laufzeit (wird im Setup aus EEPROM überschrieben)
unsigned long runTimeMs = 120000; // Default: 2 Minuten

// WLAN Zugangsdaten
const char* ssid = "T4 SmartShower";
const char* password = "multivan"; 

// Netzwerk-Konfiguration
IPAddress local_IP(192, 168, 4, 5);
IPAddress gateway(192, 168, 4, 5);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);
unsigned long pumpStartTime = 0;
bool isPumpRunning = false;

// Funktionen zur Pumpensteuerung
void pumpOn() {
  digitalWrite(RELAY_PIN, RELAY_ON_STATE);
  isPumpRunning = true;
  pumpStartTime = millis();
}

void pumpOff() {
  digitalWrite(RELAY_PIN, RELAY_OFF_STATE);
  isPumpRunning = false;
}

// JSON Endpunkt für den Live-Status
void handleStatus() {
  long remaining = 0;
  if (isPumpRunning) {
    remaining = (runTimeMs - (millis() - pumpStartTime)) / 1000;
    if (remaining < 0) remaining = 0;
  }
  String json = "{";
  json += "\"running\":" + String(isPumpRunning ? "true" : "false") + ",";
  json += "\"remaining\":" + String(remaining);
  json += "}";
  server.send(200, "application/json", json);
}

// Speichern der Einstellungen (EEPROM)
void handleSave() {
  if (server.hasArg("duration")) {
    int seconds = server.arg("duration").toInt();
    // Sicherheit: Nur Werte zwischen 5 Sek und 10 Min (600s) zulassen
    if (seconds >= 5 && seconds <= 600) {
      runTimeMs = seconds * 1000;
      EEPROM.put(0, seconds); // Schreibe Sekunden an Adresse 0
      EEPROM.commit();        // Wichtig: Dauerhaft speichern!
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Web-Interface HTML
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body { font-family: sans-serif; text-align: center; padding-top: 30px; background-color: #222; color: #fff; }";
  html += ".btn { display: inline-block; padding: 25px 50px; font-size: 28px; color: white; border: none; border-radius: 15px; cursor: pointer; text-decoration: none; transition: 0.3s; margin-bottom: 20px;}";
  html += ".on { background-color: #e74c3c; } .off { background-color: #2ecc71; }";
  html += ".save-btn { padding: 10px 20px; font-size: 16px; background-color: #555; color: white; border: none; border-radius: 5px; cursor: pointer; }";
  html += "input[type=number] { padding: 10px; font-size: 16px; width: 80px; text-align: center; border-radius: 5px; border: none; }";
  html += "#timer { font-size: 60px; font-weight: bold; color: #3498db; margin: 20px 0; min-height: 70px; }</style></head><body>";
  
  html += "<h1>T4 SmartShower</h1> &copy; 2026 by Fred";
  html += "<p>Status: <strong id='statusText'>...</strong></p>";
  html += "<div id='timer'>--</div>";
  html += "<a href='/toggle' id='mainBtn' class='btn'>LÄDT...</a>";
  
  // Einstellungs-Formular
  html += "<hr style='border-color:#444; margin: 30px 0;'>";
  html += "<form action='/save' method='POST'>";
  html += "<label>Laufzeit (Sekunden): </label><br><br>";
  html += "<input type='number' name='duration' value='" + String(runTimeMs / 1000) + "'> ";
  html += "<input type='submit' value='SPEICHERN' class='save-btn'>";
  html += "</form>";
  
  html += "<p><small>IP: 192.168.4.5</small></p>";

  // AJAX Script zur Live-Synchronisierung
  html += "<script>";
  html += "function updateUI() {";
  html += "  fetch('/status').then(response => response.json()).then(data => {";
  html += "    const btn = document.getElementById('mainBtn');";
  html += "    const stat = document.getElementById('statusText');";
  html += "    const timer = document.getElementById('timer');";
  html += "    if (data.running) {";
  html += "      stat.innerHTML = 'LÄUFT'; stat.style.color = '#e74c3c';";
  html += "      timer.innerHTML = data.remaining + 's';";
  html += "      btn.innerHTML = 'STOPPEN'; btn.className = 'btn on';";
  html += "    } else {";
  html += "      stat.innerHTML = 'BEREIT'; stat.style.color = '#2ecc71';";
  html += "      timer.innerHTML = '--';";
  html += "      btn.innerHTML = 'STARTEN'; btn.className = 'btn off';";
  html += "    }";
  html += "  });";
  html += "}";
  html += "setInterval(updateUI, 1000); updateUI();";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
}

void handleToggle() {
  if (isPumpRunning) pumpOff(); else pumpOn();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); 
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF_STATE);

  // EEPROM Initialisieren (512 Byte)
  EEPROM.begin(512);
  
  // Gespeicherte Zeit lesen (Adresse 0, Integer)
  int storedSeconds;
  EEPROM.get(0, storedSeconds);

  // Validierung: Wenn Speicher leer (0) oder Unsinn (>10 Min), Standard nutzen
  if (storedSeconds < 5 || storedSeconds > 600) {
    runTimeMs = 120000; // 2 Minuten Default
  } else {
    runTimeMs = storedSeconds * 1000;
  }

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/status", handleStatus);
  server.on("/save", handleSave); // Speichern-Handler
  server.begin();
}

void loop() {
  server.handleClient();

  // Taster-Logik
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(200); 
    if (isPumpRunning) pumpOff(); else pumpOn();
    while(digitalRead(BUTTON_PIN) == LOW) yield();
  }

  // Timer-Check
  if (isPumpRunning && (millis() - pumpStartTime >= runTimeMs)) {
    pumpOff();
  }
}
