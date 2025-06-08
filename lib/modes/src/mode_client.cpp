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

uint8_t state = STATE_IDLE;
uint8_t requested_state = STATE_IDLE;

int64_t start_ts;
int64_t stop_ts;

uint32_t buffer_a[256];
uint32_t buffer_b[256];

uint8_t buffer_a_index = 0;
uint8_t buffer_b_index = 0;

int64_t buffer_a_start_ts;
int64_t buffer_b_start_ts;

bool read_buffer_a = true;

int64_t next_entry_timestamp;

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
    case COMMAND_START: {
      start_ts = stream.read_uint64();
      requested_state = STATE_PLAYING;
      break;
    }
    case COMMAND_STOP: {
      stop_ts = stream.read_uint64();
      requested_state = STATE_IDLE;
      break;
    }
    case COMMAND_DATA: {
      bool write_buffer_a = !read_buffer_a;
      int64_t this_start_ts = stream.read_uint64();
      uint32_t this_buffer[256];
      uint8_t i = 0;
      while (stream.can_read_bytes(4)) {
        uint8_t r = stream.read_uint8();
        uint8_t g = stream.read_uint8();
        uint8_t b = stream.read_uint8();
        uint8_t d = stream.read_uint8();
        this_buffer[i++] = (r << 24) | (g << 16) | (b << 8) | d;
      }
      if (write_buffer_a) {
        buffer_a_start_ts = this_start_ts;
        memcpy(buffer_a, this_buffer, 256);
        buffer_a_index = 0;
      } else {
        buffer_b_start_ts = this_start_ts;
        memcpy(buffer_b, this_buffer, 256);
        buffer_b_index = 0;
      }
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
    Serial.println("Failed to connect over UDP");
    digitalWrite(LED_PIN, HIGH);
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

  // client clock + clock offset + drift compensation = adjusted timestamp
  int64_t dt = current_time - last_sync_time;
  int64_t current_time_adjusted = current_time + base_offset + (int64_t)(drift_rate * dt);

  if (current_time - last_sync_request > 5000000) {
    sync_sample_n = 0;
    last_sync_request = esp_timer_get_time();
    udp.write(COMMAND_SYNC);
  }

  //Serial.println("Ready");

  if (state != requested_state) {
    //Serial.println("State transition");
    switch (requested_state) {
      case STATE_PLAYING: {
        if (current_time_adjusted >= start_ts) state = STATE_PLAYING;
        break;
      } case STATE_IDLE: {
        if (current_time_adjusted >= stop_ts) state = STATE_IDLE;
      }
    }
  }

  switch (state) {
    case STATE_PLAYING: {
      //Serial.println("Playing");
      if (current_time_adjusted > next_entry_timestamp) {
        if ((read_buffer_a ? buffer_a_index : buffer_b_index) > 255) read_buffer_a = !read_buffer_a;
        uint32_t next_entry = read_buffer_a ? buffer_a[buffer_a_index++] : buffer_b[buffer_b_index++];
        uint8_t r = (next_entry >> 24) & 0xFF;
        uint8_t g = (next_entry >> 16) & 0xFF;
        uint8_t b = (next_entry >> 8) & 0xFF;
        uint8_t d = next_entry & 0xFF;

        Serial.print("R "); Serial.print(r);
        Serial.print(" G "); Serial.print(g);
        Serial.print(" B "); Serial.print(b);
        Serial.print(" D "); Serial.println(d);

        next_entry_timestamp = current_time_adjusted + ((d + 1) * DATA_TIMESTEP_MS * 1000);
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      }
      break;
    }
    case STATE_IDLE: {
      //Serial.println("Idle");
      bool on = ((current_time_adjusted / 200000) % 2) == 0;
      digitalWrite(LED_PIN, on);
      break;
    }
  }
  delay(1);
}