#ifndef CONFIG
#define CONFIG

#define IS_SERVER 0

//

#define WIFI_SSID "airfx"
#define WIFI_PASS "20263768"

#define SERVER_MDNS_HOSTNAME "main"
#define UDP_PORT 100

#define MAX_CLIENTS 32
#define HEARTBEAT_TIMEOUT_SECONDS 20

#define NUM_SYNC_SAMPLES 5

#define LED_PIN 2

// command definitions

#define COMMAND_OK 0
#define COMMAND_LOGIN 1
#define COMMAND_LOGOUT 2
#define COMMAND_SYNC 3

#endif