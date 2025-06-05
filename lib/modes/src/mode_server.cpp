#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncUDP.h>

#include "config.h"
#include "byte_stream.h"

uint64_t last_change_timestamp = 0;

AsyncUDP udp;

void onUDPPacket(AsyncUDPPacket packet) {
  Serial.print("UDP Packet");
  Serial.print("From: ");
  Serial.print(packet.remoteIP());
  Serial.print(":");
  Serial.print(packet.remotePort());
  Serial.print(", Length: ");
  Serial.print(packet.length());
  Serial.println(", Data: ");
  
  uint8_t* data = packet.data();
  size_t len = packet.length();
  ByteStream stream(data, len);

  for (size_t i = 0; i < len; ++i) {
    Serial.printf("%02X", stream.read_uint8());
    Serial.println();
  }
  Serial.println();

  packet.printf("Got %u bytes of data", packet.length());
}

void setup_mode()
{
    Serial.println("SERVER");

    delay(1000);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("Connecting...");

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("WiFi Failed");
      while (1) delay(1000);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    if (!MDNS.begin(SERVER_MDNS_HOSTNAME)) {
      Serial.println("Error starting mDNS");
      return;
    }

    Serial.println();
    Serial.println("mDNS started");

    if (!udp.listen(UDP_PORT)) {
      Serial.println("Error starting UDP");
      return;
    }

    Serial.println("hmm");
    udp.onPacket(onUDPPacket);
    Serial.println("Listening UDP");
}

void loop_mode() {
  if (micros() - last_change_timestamp > 1000000) {
    digitalWrite(LED_PIN, !digitalRead(2));
    last_change_timestamp = micros();
  }
  delay(1);
}