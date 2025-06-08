#include <map>
#include <vector>
#include <array>
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

bool running = false;
int current_i = 0;
int64_t next_ts = 0;

// data[device_id][block_index] = vector<uint32_t> of 256 packed entries of [r,g,b,d]
std::map<uint8_t, std::map<uint16_t, std::array<uint32_t, 256>>> device_data;

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
      ByteStream out(response, sizeof(response));
      out.write_uint8(3);
      out.write_uint64(receive_time);
      out.write_uint64(esp_timer_get_time());
      packet.write(response, sizeof(response));
      Serial.println("Sent sync response");
      break;
    }


    case COMMAND_DISCOVER: {
      uint8_t response[1 + n_clients * 4];
      ByteStream out(response, sizeof(response));
      out.write_uint8(COMMAND_DISCOVER);

      for (const IPAddress ip : clients) {
        uint32_t ip_data = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
        out.write_uint32(ip_data);
      }

      packet.write(response, sizeof(response));
      break;
    }
    case COMMAND_DOWNLOAD: {
      size_t expected_size = 1 + 1 + 2 + 256*4; // checksum + device ID + block index + 256 4-byte entries
      
      Serial.println(len);

      if (!stream.can_read_bytes(expected_size)) {
        Serial.println("Download packet did not contain enough data");
        break;
        // TODO: Reattempt
      }

      uint8_t received_checksum = stream.read_uint8();
      uint8_t device_id = stream.read_uint8();
      uint16_t block_index = stream.read_uint16();

      uint8_t computed_checksum = 0;

      for (size_t i = 2; i < len; i++) {
        computed_checksum ^= data[i];
        //Serial.print(data[i]); Serial.print(" ");
      }

      if (computed_checksum != received_checksum) {
        Serial.print("Checksum mismatch. Computed: "); Serial.print(computed_checksum);
        Serial.print(" ... received: "); Serial.println(received_checksum);
        break;
        // TODO: Reattempt
      }

      std::array<uint32_t, 256> entries;
      //uint32_t entries[256];
      for (int i = 0; i < 256; i++) {
        entries[i] = stream.read_uint32();
      };
      device_data[device_id][block_index] = entries;
      Serial.print("Wrote block_index "); Serial.print(block_index);
      Serial.print(" to device_id "); Serial.println(device_id);
      Serial.print(ESP.getFreeHeap()); Serial.println(" free heap");

      // OK response
      uint8_t response[5] = {COMMAND_DOWNLOAD, 0x00, device_id, 
        static_cast<uint8_t>(block_index & 0xFF),
        static_cast<uint8_t>(block_index >> 8)};
      packet.write(response, sizeof(response));
      break;
    }
    case COMMAND_SYSTEM_START_NOW: {
      running = true;
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
      Serial.println("Error starting UDP listen");
      return;
    }

    udp.onPacket(onUDPPacket);
}

void loop_mode() {
  if (running) { // TEST
    int64_t timestamp = esp_timer_get_time();
    if (timestamp > next_ts) {
      current_i++;
      current_i = current_i % 256;

      uint32_t this_frame = device_data[0][0][current_i];
      uint8_t r = this_frame & 0xFF;
      uint8_t g = (this_frame >> 8) & 0xFF;
      uint8_t b = (this_frame >> 16) & 0xFF;
      uint8_t d = (this_frame >> 24) & 0xFF;

      Serial.print("R "); Serial.print(r);
      Serial.print(" G "); Serial.print(g);
      Serial.print(" B "); Serial.print(b);
      Serial.print(" D "); Serial.println(d);

      digitalWrite(LED_PIN, r > 128);

      next_ts = timestamp + ((d + 1) * DATA_TIMESTEP_MS * 1000);
    }
  }
  // bool on = ((esp_timer_get_time() / 200000) % 2) == 0;
  // digitalWrite(LED_PIN, on);
  delay(1);
}