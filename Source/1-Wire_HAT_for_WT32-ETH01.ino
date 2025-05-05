#include <ElegantOTA.h> // OTA (OverTheAir) updates 
#include <ETH.h>  // Die ETH.h-Bibliothek für ESP32 Ethernet
#include <OneWire.h>  // Für die 1-Wire Kommunikation
#include <DallasTemperature.h>  // Für den DS18B20
#include <WebServer.h>  // HTTP-Server für Ethernet
#include <WiFi.h>    // WiFi for wireless connection

// Define static IP address and network information (if needed)
IPAddress staticIP(192, 168, 1, 100); // Example static IP
IPAddress gateway(192, 168, 1, 1);    // Gateway
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress dns(8, 8, 8, 8);            // DNS server (Google DNS)

// Flags to choose between DHCP or static IP
bool useDHCP = true;  // true = DHCP, false = static IP

// WiFi network information (if needed)
const char* ssid = "";
const char* password = "";

// Variable to store the current connection type
String connectionType = "None";  // Will hold "LAN DHCP", "LAN Static", "WiFi DHCP", "WiFi Static", etc.

#define ONE_WIRE_BUS 4  // Definiere den Pin für den 1-Wire-Sensor (z.B. GPIO 4)

OneWire oneWire(ONE_WIRE_BUS);  // Erstelle ein OneWire-Objekt
DallasTemperature sensors(&oneWire);  // Erstelle ein DallasTemperature-Objekt

String hostname = "Eth1Wire";  // Definiere deinen gewünschten Hostnamen

WebServer server(80);  // HTTP-Server auf Port 80

float temperatureC1 = 0.0;  // Variable für die Temperatur Sensor 1
float temperatureC2 = 0.0;  // Variable für die Temperatur Sensor 2
float temperatureC3 = 0.0;  // Variable für die Temperatur Sensor 3

IPAddress ipAddress;
String ipAddressString; // String für die IP-Adresse

String macAddressString;  // String für die MAC-Adresse

void setup() {
  // Serielle Kommunikation starten
  Serial.begin(115200);
  
  // Warten auf die serielle Kommunikation
  while (!Serial) {
    delay(100);
  }

  // Hostnamen setzen
  ETH.setHostname(hostname.c_str());  // Setze den Hostnamen, der an den DHCP-Server gesendet wird

  Serial.println("Attempting LAN connection...");

  bool ethernetConnected = false;  // Flag to track Ethernet connection status

  // Try Ethernet connection
  if (ETH.begin()) {
      Serial.println("Ethernet adapter started.");

      // If DHCP is enabled, try connecting via DHCP
      if (useDHCP) {
          Serial.print("Waiting for DHCP...");
          unsigned long startTime = millis();
          unsigned long timeout = 10000;  // Timeout for DHCP (10 seconds)

          while (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
              delay(1000);  // Wait for 1 second
              Serial.print(".");

              // Check if timeout is reached
              if (millis() - startTime > timeout) {
                  Serial.println("\nDHCP error: No IP received.");
                  break;  // If DHCP fails, stop waiting
              }
          }

          // If DHCP succeeded, print IP and update connection type
          if (ETH.localIP() != IPAddress(0, 0, 0, 0)) {
              Serial.println("\nDHCP successful!");
              connectionType = "LAN/DHCP";  // Store the connection type
              ethernetConnected = true;
          }
      }

      // If DHCP failed or was not enabled, try static IP
      if (!ethernetConnected && ETH.localIP() == IPAddress(0, 0, 0, 0)) {
          if (!useDHCP) {
              Serial.println("\nUsing static IP...");
              ETH.config(staticIP, gateway, subnet, dns);
              if (ETH.localIP() == staticIP) {
                  Serial.println("Static IP successfully assigned:");
                  connectionType = "LAN/Static";  // Store the connection type
                  ethernetConnected = true;
              } else {
                  Serial.println("Failed to assign static IP.");
              }
          }
      }
  } else {
      Serial.println("Ethernet connection failed.");
  }

  // If Ethernet connection failed or not available, try WiFi connection
  if (!ethernetConnected) {
      Serial.println("Attempting WiFi connection...");
      WiFi.begin(ssid, password);
      Serial.print("Waiting for WiFi connection...");
      unsigned long startTime = millis();
      unsigned long timeout = 10000;  // Timeout for WiFi (10 seconds)

      while (WiFi.status() != WL_CONNECTED) {
          delay(1000);  // Wait for 1 second
          Serial.print(".");

          // Check if timeout is reached
          if (millis() - startTime > timeout) {
              Serial.println("\nWiFi connection failed!");
              return;  // Exit if no connection is possible
          }
      }

      Serial.println("\nWiFi connected successfully!");
      Serial.print("MAC Address: ");
      Serial.println(WiFi.macAddress());  // Get the MAC address
      Serial.print("IP-Address: " + WiFi.localIP().toString());
      connectionType = (useDHCP ? "WiFi/DHCP" : "WiFi/Static");  // Store the connection type
      Serial.print(" (" + connectionType + ")");     
  }
  
  // MAC-Adresse abrufen
  uint8_t mac[6];  // Array zur Speicherung der MAC-Adresse
  ETH.macAddress(mac);  // MAC-Adresse abrufen

    // MAC-Adresse als String zusammenstellen
  macAddressString = "";
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16) {
      macAddressString += "0";  // Voranstellen einer Null, falls der Wert kleiner als 16 ist
    }
    macAddressString += String(mac[i], HEX);  // Byte in Hex umwandeln und zum String hinzufügen
    if (i < 5) {
      macAddressString += ":";  // Doppelpunkte zwischen den Bytes hinzufügen
    }
  }

  // MAC-Adresse als String ausgeben
  Serial.println();
  Serial.print("MAC-Adresse: ");
  Serial.println(macAddressString);  // Die MAC-Adresse als String ausgeben

  // Wenn eine IP-Adresse zugewiesen wurde, diese ausgeben

  ipAddress = ETH.localIP(); // Speichern der zugewiesenen IP-Adresse
  ipAddressString = ipAddress.toString();  // Umwandeln der IP-Adresse in einen String
  Serial.print("IP-Adresse: ");
  Serial.print(ipAddressString);
  Serial.println(" (" + connectionType + ")");

  // OTA-Erweiterung
  ElegantOTA.begin(&server);

  // HTTP-Server starten
  server.begin();
  
  // 1-Wire-Sensor starten
  sensors.begin();
  
  // Definiere die HTTP-Request-Handler
  server.on("/", HTTP_GET, handleRoot);  // Anfragen an "/" mit der Funktion handleRoot beantworten
}

void loop() {

  // HTTP-Server läuft und wartet auf Anfragen
  server.handleClient();

  // Optional: Temperatur alle 60 Sekunden im Seriellen Monitor ausgeben
  static unsigned long lastReadTime = 0;
  if (millis() - lastReadTime >= 60000) {  // Alle 60 Sekunden
    lastReadTime = millis();
    sensors.requestTemperatures();
    temperatureC1 = sensors.getTempCByIndex(0);
    Serial.print("Temperatur Sensor 1: ");
    Serial.println(temperatureC1);
    temperatureC1 = sensors.getTempCByIndex(1);
    Serial.print("Temperatur Sensor 2: ");
    Serial.println(temperatureC2);
    temperatureC1 = sensors.getTempCByIndex(2);
    Serial.print("Temperatur Sensor 3: ");
    Serial.println(temperatureC3);
  }
  ElegantOTA.loop(); 
}

// Funktion zum Beantworten der HTTP-Anfragen
void handleRoot() {
  sensors.requestTemperatures();  // Temperatur aus dem Sensor auslesen
  temperatureC1 = sensors.getTempCByIndex(0);  // Temperatur vom ersten Sensor holen
  temperatureC2 = sensors.getTempCByIndex(1);  // Temperatur vom zweiten Sensor holen
  temperatureC3 = sensors.getTempCByIndex(2);  // Temperatur vom dritten Sensor holen

  // HTTP-Antwort als HTML
  String html = "<html><head><title></title><meta http-equiv='refresh' content='60'></head><body>";
  html += "<h1>1-Wire Temperatur Sensor</h1>";
  html += "<p>MAC: " + macAddressString + "</p>";
  html += "<p>IP: " + ipAddressString + " (" + connectionType + ")";

  html += "<H2>Temperatur Sensor 1: <span id='temperature1'>" + String(temperatureC1) + "</span> &deg;C</H2>";
  html += "<H2>Temperatur Sensor 2: <span id='temperature2'>" + String(temperatureC2) + "</span> &deg;C</H2>";
  html += "<H2>Temperatur Sensor 3: <span id='temperature3'>" + String(temperatureC3) + "</span> &deg;C</H2>";

  html += "</body></html>";

  server.send(200, "text/html", html);  // HTTP-Antwort senden
}
