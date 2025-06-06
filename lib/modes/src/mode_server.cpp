#include <map>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncUDP.h>

#include "config.h"
#include "byte_stream.h"

uint64_t last_change_timestamp = 0;

AsyncUDP udp;

IPAddress clients[MAX_CLIENTS];
uint8_t n_clients = 0;

// std::map<IPAddress, uint32_t> client_heartbeat_requests;
// std::map<IPAddress, uint32_t> client_heartbeat_responses;

void add_client(IPAddress ip) {
  if (n_clients >= MAX_CLIENTS) return;
  for (int i = 0; i < n_clients; i++) { // prevent duplicates
    if (clients[i] == ip) return;
  }
  clients[n_clients++] = ip;
};

void remove_client(IPAddress ip) {
  for (int i = 0; i < n_clients; i++) {
    if (clients[i] == ip) {
      for (int j = i; j < n_clients - 1; j++) {
        clients[j] = clients[j+1];
      }
      clients[n_clients] = IPAddress();
      n_clients--;
      return;
    }
  }
};

void onUDPPacket(AsyncUDPPacket packet) {
  int64_t receive_time = esp_timer_get_time();
  IPAddress ip = packet.remoteIP();
  uint16_t port = packet.remotePort();
  size_t len = packet.length();
  
  uint8_t* data = packet.data();
  ByteStream stream(data, len);

  Serial.println(stream.can_read_bytes(1));

  if (!stream.can_read_bytes(1)) return;

  uint8_t command = stream.read_uint8();
  Serial.print("COMMAND ");
  Serial.print(command);
  Serial.print(" FROM ");
  Serial.println(ip);

  switch (command) {
    case COMMAND_OK: {
      // uint32_t now = millis() / 1000;
      // client_heartbeat_responses[ip] = now;
      // client_heartbeat_requests[ip] = now;
      break;
    }
    case COMMAND_LOGIN: {
      add_client(packet.remoteIP());
      break;
    }
    case COMMAND_LOGOUT: {
      remove_client(packet.remoteIP());
      break;
    }
    case COMMAND_SYNC: {
      uint8_t response[17];
      ByteStream response_stream(response, sizeof(response));
      response_stream.write_uint8(3);
      response_stream.write_uint64(receive_time);
      response_stream.write_uint64(esp_timer_get_time());
      packet.write(response, sizeof(response));
      Serial.println("Sent sync response");
      break;
    }
    default: {
      Serial.print("Invalid command ");
      Serial.println(command);
    }
  }
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
  bool on = ((esp_timer_get_time() / 500000) % 2) == 0;
  digitalWrite(LED_PIN, on);
  delay(1);
}