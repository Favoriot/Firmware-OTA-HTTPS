#include <WiFi.h>
#include <NetworkClientSecure.h>  // Updated to NetworkClientSecure
#include <Update.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "FavoriotCA.h"            // Include the Favoriot CA certificate

const char ssid[] = "Your WIFI SSID";
const char pass[] = "Your WIFI password";

const char apikey[] = "replace with your access token"; // Use Device access token
const char device_developer_id [] = "replace with your device developer id";
// Updated URLs with HTTPS
#define downloadUrl "https://apiv2.favoriot.com/v2/firmware/update?device_developer_id="+String(device_developer_id)
#define installationUrl "https://apiv2.favoriot.com/v2/firmware/status?device_developer_id="+String(device_developer_id)

NetworkClientSecure secureClient;  // Updated to use NetworkClientSecure
HTTPClient client;

// Global variables
int totalLength;       // Total size of firmware
int currentLength = 0; // Current size of written firmware

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  Serial.println("WIFI Connected");
  Serial.print("Max free block size: ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

  // Set the root certificate from FavoriotCA.h for secure HTTPS communication
  secureClient.setCACert(FavoriotCA);  
}

void updateFirmware() {
  client.begin(secureClient, downloadUrl);  // Use secure client with HTTPS URL
  client.addHeader("Content-Type", "application/json");
  client.addHeader("apikey", apikey);

  int httpCode = client.GET();

  if (httpCode == HTTP_CODE_OK) {
    totalLength = client.getSize();
    int len = totalLength;
    Update.begin(UPDATE_SIZE_UNKNOWN);
    Serial.printf("FW Size: %u\n", totalLength);

    uint8_t buff[128] = {0};
    WiFiClient *stream = client.getStreamPtr();
    Serial.println("Updating firmware...");

    while (client.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();
      if (size) {
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        firmwareWrite(buff, c);
        if (len > 0) {
          len -= c;
        }
      }
      delay(1);
    }
    delay(1000);

  } else {
    Serial.println("Error getting update file");
    String payload = client.getString();
    
    // Parse JSON to get the "message" field
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      const char* message = doc["message"];
      Serial.println(message);
    } else {
      updateInstallationStatus("failed");
    }
  }
}

void firmwareWrite(uint8_t *data, size_t len) {
  Update.write(data, len);
  currentLength += len;
  Serial.print('.');
  
  if (currentLength != totalLength) return;
  Update.end(true);
  delay(1000);
  updateInstallationStatus("inprogress");
  Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", currentLength); 
  delay(1000);
  updateInstallationStatus("success");
  ESP.restart();
}

void updateInstallationStatus(const char *status) {
  client.begin(secureClient, installationUrl);  // Use secure client for HTTPS URL
  client.addHeader("Content-Type", "application/json");
  client.addHeader("apikey", apikey);

  String body = "{\"status\":\"";
  body += status;
  body += "\"}";

  int rescode = client.POST(body);
  rescode == 200 ? Serial.print("Status updated: ") : Serial.print("Error updating status: ");
  Serial.println(client.getString());
  client.end();
}

void loop() {
  Serial.print("Free heap memory: ");
  Serial.println(ESP.getFreeHeap());
  delay(10000);
  updateFirmware();
}
