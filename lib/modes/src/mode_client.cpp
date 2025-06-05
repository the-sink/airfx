#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncUDP.h>

#include "config.h"
#include "byte_stream.h"

uint64_t last_change_timestamp = 0;

AsyncUDP udp;
   
void setup_mode() {
    Serial.println("CLIENT");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
     
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  
  while(mdns_init()!= ESP_OK){
    delay(1000);
    Serial.println("Starting mDNS...");
  }
 
  Serial.println("mDNS started");
 
  IPAddress serverIp;
 
  while (serverIp.toString() == "0.0.0.0") {
    Serial.println("Resolving host...");
    delay(250);
    serverIp = MDNS.queryHost(SERVER_MDNS_HOSTNAME);
  }
 
  Serial.println("Host address resolved:");
  Serial.println(serverIp.toString());

  if (!udp.connect(serverIp, UDP_PORT)) {
    Serial.println("Failed to connect to UDP");
    return;
  }

  // test
  uint8_t buffer[8];
  ByteStream stream(buffer, sizeof(buffer));
  stream.write_uint8(0);
  stream.write_uint8(120);
  udp.write(buffer, sizeof(buffer));
 
}
   
void loop_mode() {
  if (micros() - last_change_timestamp > 1000000) {
    digitalWrite(LED_PIN, !digitalRead(2));
    last_change_timestamp = micros();
  }
  delay(1);
}