#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncUDP.h>

#include "config.h"
#include "byte_stream.h"

uint64_t last_change_timestamp = 0;

AsyncUDP udp;

int64_t last_sync_request;

int64_t base_offset;
float drift_rate;
int64_t last_sync_time;

int64_t clock_offset;

uint8_t sync_sample_n = 0;
int64_t rtt_samples[NUM_SYNC_SAMPLES];
int64_t offset_samples[NUM_SYNC_SAMPLES];
int64_t time_samples[NUM_SYNC_SAMPLES];

void onUDPPacket(AsyncUDPPacket packet) {
  int64_t receive_time = esp_timer_get_time();
  size_t len = packet.length();
  
  uint8_t* data = packet.data();
  ByteStream stream(data, len);

  if (!stream.can_read_bytes(1)) return;

  uint8_t command = stream.read_uint8();

  switch (command) {
    case COMMAND_SYNC: {
      // Sync procedure:
      // - Calculates t1,t2,t3,t4 (like ntp sync) n times (to produce multiple samples)
      // - Sample with the lowest rtt is chosen
      // - Time offset between server and client is calculated
      // - Then, drift rate is calculated as d_offset/dt to (try) to counteract clock drift

      int64_t t1 = last_sync_request;
      int64_t t2 = stream.read_uint64();
      int64_t t3 = stream.read_uint64();
      int64_t t4 = receive_time;

      int64_t rtt = (t4 - t1) - (t3 - t2);
      int64_t this_clock_offset = ((t2 - t1) + (t3 - t4)) / 2;

      rtt_samples[sync_sample_n] = rtt;
      offset_samples[sync_sample_n] = this_clock_offset;
      time_samples[sync_sample_n] = t4;
      sync_sample_n++;

      if (sync_sample_n < NUM_SYNC_SAMPLES) {
        last_sync_request = esp_timer_get_time();
        udp.write(COMMAND_SYNC);
        break;
      }

      int64_t lowest_rtt = INT64_MAX;
      uint8_t lowest_i = 0;
      for (int i = 0; i <= NUM_SYNC_SAMPLES-1; i++) {
        if (rtt_samples[i] < lowest_rtt) {
          lowest_rtt = rtt_samples[i];
          lowest_i = i;
        }
      };

      if (last_sync_time != 0) {
        int64_t dt = time_samples[lowest_i] - last_sync_time;
        int64_t doffset = offset_samples[lowest_i] - base_offset;
        drift_rate = (float)doffset / (float)dt;
      }

      base_offset = offset_samples[lowest_i];
      last_sync_time = time_samples[lowest_i];

      break;
    }
    default: {
      Serial.print("Invalid command ");
      Serial.println(command);
    }
  }
}

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

  udp.onPacket(onUDPPacket);

  udp.write(COMMAND_LOGIN);
  delay(200);
  last_sync_request = esp_timer_get_time();
  udp.write(COMMAND_SYNC);
}

void loop_mode() {
  int64_t current_time = esp_timer_get_time();

  // client clock + clock offset + drift compensation
  int64_t dt = current_time - last_sync_time;
  int64_t current_time_adjusted = current_time + base_offset + (int64_t)(drift_rate * dt);

  if (current_time - last_sync_request > 5000000) {
    sync_sample_n = 0;
    last_sync_request = esp_timer_get_time();
    udp.write(COMMAND_SYNC);
  }
  bool on = ((current_time_adjusted / 500000) % 2) == 0;
  digitalWrite(LED_PIN, on);
  delay(1);
}