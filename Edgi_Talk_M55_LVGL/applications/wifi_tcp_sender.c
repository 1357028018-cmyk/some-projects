#include "wifi_tcp_sender.h"
#include "airbag_control.h"
#include <rtthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>

#define WIFI_TCP_LOG(fmt, ...)  rt_kprintf("[WIFI-TCP] " fmt "\r\n", ##__VA_ARGS__)

wifi_tcp_t g_wifi_tcp = {
    .server_ip = {0},
    .server_port = WIFI_TCP_DEFAULT_SERVER_PORT,
    .sock_fd = -1,
    .state = WIFI_TCP_STATE_DISCONNECTED,
    .running = RT_FALSE
};

static rt_thread_t g_wifi_tcp_thread = RT_NULL;
static rt_mutex_t  g_wifi_tcp_mutex = RT_NULL;

extern int rt_wlan_is_connected(void);

static int wifi_tcp_do_connect(void)
{
    struct sockaddr_in server_addr;
    int ret;

    if (g_wifi_tcp.sock_fd >= 0)
    {
        closesocket(g_wifi_tcp.sock_fd);
        g_wifi_tcp.sock_fd = -1;
    }

    g_wifi_tcp.sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_wifi_tcp.sock_fd < 0)
    {
        WIFI_TCP_LOG("socket() failed");
        return -RT_ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_wifi_tcp.server_port);
    server_addr.sin_addr.s_addr = inet_addr(g_wifi_tcp.server_ip);

    ret = connect(g_wifi_tcp.sock_fd,
                  (struct sockaddr *)&server_addr,
                  sizeof(server_addr));
    if (ret < 0)
    {
        WIFI_TCP_LOG("connect %s:%d failed",
                     g_wifi_tcp.server_ip, g_wifi_tcp.server_port);
        closesocket(g_wifi_tcp.sock_fd);
        g_wifi_tcp.sock_fd = -1;
        return -RT_ERROR;
    }

    WIFI_TCP_LOG("connected to %s:%d",
                 g_wifi_tcp.server_ip, g_wifi_tcp.server_port);
    return RT_EOK;
}

static void wifi_tcp_monitor_thread_entry(void *parameter)
{
    while (g_wifi_tcp.running)
    {
        rt_bool_t wifi_up = rt_wlan_is_connected();

        if (wifi_up)
        {
            if (g_wifi_tcp.state == WIFI_TCP_STATE_DISCONNECTED ||
                g_wifi_tcp.state == WIFI_TCP_STATE_ERROR)
            {
                WIFI_TCP_LOG("WiFi up, attempting TCP connect...");
                g_wifi_tcp.state = WIFI_TCP_STATE_CONNECTING;
                if (wifi_tcp_do_connect() == RT_EOK)
                {
                    g_wifi_tcp.state = WIFI_TCP_STATE_CONNECTED;
                }
                else
                {
                    g_wifi_tcp.state = WIFI_TCP_STATE_ERROR;
                }
            }
        }
        else
        {
            if (g_wifi_tcp.state != WIFI_TCP_STATE_DISCONNECTED)
            {
                WIFI_TCP_LOG("WiFi down, closing TCP");
                wifi_tcp_close();
                g_wifi_tcp.state = WIFI_TCP_STATE_DISCONNECTED;
            }
        }

        rt_thread_mdelay(3000);
    }
}

int wifi_tcp_init(const char *server_ip, uint16_t server_port)
{
    if (server_ip != RT_NULL && server_ip[0] != '\0')
    {
        rt_strncpy(g_wifi_tcp.server_ip, server_ip, sizeof(g_wifi_tcp.server_ip) - 1);
    }
    else
    {
        rt_strncpy(g_wifi_tcp.server_ip, WIFI_TCP_DEFAULT_SERVER_IP,
                   sizeof(g_wifi_tcp.server_ip) - 1);
    }

    if (server_port > 0)
    {
        g_wifi_tcp.server_port = server_port;
    }
    else
    {
        g_wifi_tcp.server_port = WIFI_TCP_DEFAULT_SERVER_PORT;
    }

    g_wifi_tcp.sock_fd = -1;
    g_wifi_tcp.state = WIFI_TCP_STATE_DISCONNECTED;
    g_wifi_tcp.running = RT_TRUE;

    g_wifi_tcp_mutex = rt_mutex_create("wifi_tcp", RT_IPC_FLAG_PRIO);
    if (g_wifi_tcp_mutex == RT_NULL)
    {
        WIFI_TCP_LOG("mutex create failed");
        return -RT_ENOMEM;
    }

    g_wifi_tcp_thread = rt_thread_create("wifi_tcp",
                                         wifi_tcp_monitor_thread_entry,
                                         RT_NULL,
                                         2048, 15, 10);
    if (g_wifi_tcp_thread == RT_NULL)
    {
        WIFI_TCP_LOG("thread create failed");
        return -RT_ENOMEM;
    }

    rt_thread_startup(g_wifi_tcp_thread);
    WIFI_TCP_LOG("init: target=%s:%d", g_wifi_tcp.server_ip, g_wifi_tcp.server_port);
    return RT_EOK;
}

int wifi_tcp_send(const char *data, int len)
{
    int ret;

    if (g_wifi_tcp.state != WIFI_TCP_STATE_CONNECTED)
    {
        return -RT_ERROR;
    }

    if (g_wifi_tcp.sock_fd < 0)
    {
        return -RT_ERROR;
    }

    if (g_wifi_tcp_mutex)
        rt_mutex_take(g_wifi_tcp_mutex, RT_WAITING_FOREVER);

    ret = send(g_wifi_tcp.sock_fd, data, len, 0);
    if (ret < 0)
    {
        WIFI_TCP_LOG("send() failed, will reconnect");
        closesocket(g_wifi_tcp.sock_fd);
        g_wifi_tcp.sock_fd = -1;
        g_wifi_tcp.state = WIFI_TCP_STATE_ERROR;
    }

    if (g_wifi_tcp_mutex)
        rt_mutex_release(g_wifi_tcp_mutex);

    return ret;
}

int wifi_tcp_send_str(const char *str)
{
    if (str == RT_NULL)
        return -RT_ERROR;
    return wifi_tcp_send(str, (int)rt_strlen(str));
}

int wifi_tcp_is_connected(void)
{
    return (g_wifi_tcp.state == WIFI_TCP_STATE_CONNECTED) ? 1 : 0;
}

void wifi_tcp_reconnect(void)
{
    WIFI_TCP_LOG("reconnect requested to %s:%d",
                 g_wifi_tcp.server_ip, g_wifi_tcp.server_port);
    /* 关闭现有连接, 触发 monitor 线程重连 */
    if (g_wifi_tcp.sock_fd >= 0) {
        closesocket(g_wifi_tcp.sock_fd);
        g_wifi_tcp.sock_fd = -1;
    }
    g_wifi_tcp.state = WIFI_TCP_STATE_DISCONNECTED;
}

void wifi_tcp_close(void)
{
    if (g_wifi_tcp.sock_fd >= 0)
    {
        closesocket(g_wifi_tcp.sock_fd);
        g_wifi_tcp.sock_fd = -1;
    }
    g_wifi_tcp.state = WIFI_TCP_STATE_DISCONNECTED;
}

/* ========== TCP 命令接收线程 ========== */

static void tcp_cmd_entry(void *param)
{
    (void)param;
    char buf[64];

    while (1) {
        if (g_wifi_tcp.state == WIFI_TCP_STATE_CONNECTED && g_wifi_tcp.sock_fd >= 0) {
            int n = (int)recv(g_wifi_tcp.sock_fd, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                buf[n] = '\0';
                int bag;
                if (sscanf(buf, "MAN_INFLATE %d", &bag) == 1 && bag >= 0 && bag <= 2)
                    airbag_manual_inflate((uint8_t)bag);
                else if (sscanf(buf, "MAN_DEFLATE %d", &bag) == 1 && bag >= 0 && bag <= 2)
                    airbag_manual_deflate((uint8_t)bag);
            } else if (n == 0) {
                wifi_tcp_close();
                rt_thread_mdelay(1000);
            } else {
                rt_thread_mdelay(100);
            }
        } else {
            rt_thread_mdelay(500);
        }
    }
}

void wifi_tcp_cmd_start(void)
{
    rt_thread_t tid = rt_thread_create("tcp_cmd", tcp_cmd_entry, RT_NULL, 1536, 12, 10);
    if (tid != RT_NULL)
        rt_thread_startup(tid);
}
