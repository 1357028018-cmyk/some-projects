#ifndef WIFI_TCP_SENDER_H
#define WIFI_TCP_SENDER_H

#include <rtthread.h>

#define WIFI_TCP_DEFAULT_SERVER_IP    "8.136.125.241"
#define WIFI_TCP_DEFAULT_SERVER_PORT  8888

typedef enum {
    WIFI_TCP_STATE_DISCONNECTED = 0,
    WIFI_TCP_STATE_CONNECTING,
    WIFI_TCP_STATE_CONNECTED,
    WIFI_TCP_STATE_ERROR
} wifi_tcp_state_t;

typedef struct {
    char        server_ip[32];
    uint16_t    server_port;
    int         sock_fd;
    wifi_tcp_state_t state;
    rt_bool_t   running;
} wifi_tcp_t;

extern wifi_tcp_t g_wifi_tcp;

int  wifi_tcp_init(const char *server_ip, uint16_t server_port);
int  wifi_tcp_send(const char *data, int len);
int  wifi_tcp_send_str(const char *str);
int  wifi_tcp_is_connected(void);
void wifi_tcp_reconnect(void);
void wifi_tcp_close(void);
void wifi_tcp_cmd_start(void);

#endif
