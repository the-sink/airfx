#ifndef CONFIG
#define CONFIG

#define IS_SERVER 1

//

#define WIFI_SSID "airfx"
#define WIFI_PASS "20263768"

#define SERVER_MDNS_HOSTNAME "main"
#define UDP_PORT 100

#define MAX_CLIENTS 32
#define HEARTBEAT_TIMEOUT_SECONDS 20

#define NUM_SYNC_SAMPLES 4

#define LED_PIN 2

#define DATA_TIMESTEP_MS 20

// commands between server & client MCUs
#define COMMAND_OK 0
#define COMMAND_LOGIN 1
#define COMMAND_LOGOUT 2
#define COMMAND_SYNC 3
#define COMMAND_START 4
#define COMMAND_STOP 5
#define COMMAND_DATA 6

// commands between computer/interaction device and server MCU
#define COMMAND_DISCOVER 130
#define COMMAND_DOWNLOAD 131
#define COMMAND_SYSTEM_START_NOW 132
#define COMMAND_SYSTEM_STOP_NOW 133

// client states
#define STATE_IDLE 0
#define STATE_PLAYING 1

#endif