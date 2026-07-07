#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>

#include "drv_ipc.h"
#include "aht10.h"
#include "snore_detector.h"

#define LED_PIN_B                 GET_PIN(16, 5)
#define SENSOR_INTERVAL_MS        (2000U)
#define AHT10_I2C_BUS_NAME        "i2c1"

#define RECORD_CHUNK_BYTES    1024
#define MONO_SAMPLES_PER_READ 256

#define WDT_TIMEOUT_SEC          5

typedef struct
{
    float temperature;
    float humidity;
} sensor_data_t;

static rt_device_t g_ipc_dev = RT_NULL;
static rt_device_t g_wdt_dev = RT_NULL;

static void m33_watchdog_kick(void)
{
    if (g_wdt_dev) {
        rt_device_control(g_wdt_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, RT_NULL);
    }
}

static int m33_wdt_init(void)
{
    rt_uint32_t timeout = WDT_TIMEOUT_SEC;
    g_wdt_dev = rt_device_find("wdt");
    if (!g_wdt_dev) {
        rt_kprintf("[M33-WDT] wdt device not found!\n");
        return 0;
    }
    rt_device_control(g_wdt_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout);
    rt_device_control(g_wdt_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL);
    m33_watchdog_kick();
    rt_kprintf("[M33-WDT] started, timeout=%ds\n", WDT_TIMEOUT_SEC);
    return 0;
}
INIT_APP_EXPORT(m33_wdt_init);

static void ipc_sensor_entry(void *parameter)
{
    edge_rc_frame_t tx_frame;
    aht10_device_t aht_dev;
    sensor_data_t sensor;
    rt_uint32_t seq = 0;
    rt_tick_t last_send_tick = 0;
    rt_uint32_t tx_count = 0;

    rt_kprintf("[M33] Sensor thread started\r\n");
    m33_watchdog_kick();

    aht_dev = aht10_init(AHT10_I2C_BUS_NAME);
    if (aht_dev == RT_NULL) {
        rt_kprintf("[M33] AHT10 init failed on I2C bus '%s'\r\n", AHT10_I2C_BUS_NAME);
        return;
    }
    rt_kprintf("[M33] AHT10 sensor initialized\r\n");

    g_ipc_dev = edge_ipc_device_find();
    if (g_ipc_dev == RT_NULL) {
        if (edge_ipc_device_register() != RT_EOK) {
            rt_kprintf("[M33] IPC: Device register failed\r\n");
            return;
        }
        g_ipc_dev = edge_ipc_device_find();
        if (g_ipc_dev == RT_NULL) {
            rt_kprintf("[M33] IPC: Device not found\r\n");
            return;
        }
    }

    if (rt_device_open(g_ipc_dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK) {
        rt_kprintf("[M33] IPC: Open device failed\r\n");
        return;
    }
    rt_kprintf("[M33] IPC opened OK\r\n");

    rt_kprintf("\r\n");
    rt_kprintf("========================================\r\n");
    rt_kprintf("[M33] IPC Sensor Started\r\n");
    rt_kprintf("----------------------------------------\r\n");
    rt_kprintf("Sensor:   AHT10 @ %d ms\r\n", SENSOR_INTERVAL_MS);
    rt_kprintf("========================================\r\n");
    rt_kprintf("\r\n");

    last_send_tick = rt_tick_get();

    while (1) {
        m33_watchdog_kick();
        if ((rt_tick_get() - last_send_tick) >= rt_tick_from_millisecond(SENSOR_INTERVAL_MS)) {
            sensor.temperature = aht10_read_temperature(aht_dev);
            sensor.humidity = aht10_read_humidity(aht_dev);

            memset(&tx_frame, 0, sizeof(tx_frame));
            tx_frame.client_id = CM55_IPC_PIPE_CLIENT_ID;
            tx_frame.role = RC_ROLE_M33_SENSOR;
            tx_frame.magic = RC_MAGIC_WORD;
            tx_frame.seq = ++seq;

            uint32_t *data_ptr = (uint32_t *)tx_frame.channel;
            data_ptr[0] = *(uint32_t *)&sensor.temperature;
            data_ptr[1] = *(uint32_t *)&sensor.humidity;

            tx_frame.checksum = edge_rc_checksum(&tx_frame);

            if (rt_device_write(g_ipc_dev, 0, &tx_frame, 1) == 1) {
                m33_watchdog_kick();
                int temp_i = (int)sensor.temperature;
                int temp_f = (int)((sensor.temperature >= 0 ? sensor.temperature - (float)temp_i : (float)temp_i - sensor.temperature) * 100);
                int humi_i = (int)sensor.humidity;
                int humi_f = (int)((sensor.humidity - (float)humi_i) * 100);
                rt_uint32_t time_ms = (rt_uint32_t)((rt_uint64_t)rt_tick_get() * 1000U / RT_TICK_PER_SECOND);
                rt_kprintf("[M33] TX -> [M55]: T=%d.%02dC, H=%d.%02d%% | Seq: %5lu | Time: %8lu ms\r\n",
                           temp_i, temp_f, humi_i, humi_f,
                           seq, (unsigned long)time_ms);
                tx_count++;
            } else {
                rt_kprintf("[M33] TX Failed\r\n");
            }

            last_send_tick = rt_tick_get();
        }

        rt_thread_mdelay(10);
    }
}
MSH_CMD_EXPORT(ipc_sensor_entry, Start M33 IPC sensor data transmission);

static int ipc_sensor_auto_start(void)
{
    rt_kprintf("[M33] Creating sensor thread...\r\n");
    rt_thread_t tid = rt_thread_create("sensor", ipc_sensor_entry, RT_NULL, 2048, 15, 20);
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
        rt_kprintf("[M33] Sensor thread created OK\r\n");
    } else {
        rt_kprintf("[M33] Failed to create sensor thread\r\n");
    }
    return 0;
}
INIT_APP_EXPORT(ipc_sensor_auto_start);

static void audio_snore_entry(void *parameter)
{
    rt_device_t mic_dev;
    rt_uint8_t  buffer[RECORD_CHUNK_BYTES];
    int16_t     mono_buf[MONO_SAMPLES_PER_READ];
    rt_uint32_t length;

    rt_kprintf("[Audio] Starting audio+snore thread...\n");

    /* Give sensor thread time to init IPC first */
    rt_thread_mdelay(1000);

    mic_dev = rt_device_find("mic0");
    if (!mic_dev) {
        rt_kprintf("[Audio] mic0 not found!\n");
        return;
    }

    struct rt_audio_caps caps;
    caps.main_type = AUDIO_TYPE_MIXER;
    caps.sub_type  = AUDIO_MIXER_VOLUME;
    caps.udata.value = 25;
    rt_device_control(mic_dev, AUDIO_CTL_CONFIGURE, &caps);
    if (rt_device_open(mic_dev, RT_DEVICE_OFLAG_RDONLY) != RT_EOK) {
        rt_kprintf("[Audio] mic0 open failed\n");
        return;
    }
    rt_kprintf("[Audio] mic0 opened\n");

    if (snore_detector_init() != 0) {
        rt_kprintf("[Audio] Failed to init snore detector\n");
        rt_device_close(mic_dev);
        return;
    }

    rt_kprintf("[Audio] Entering audio loop...\n");

    while (1)
    {
        length = rt_device_read(mic_dev, 0, buffer, RECORD_CHUNK_BYTES);
        if (length == 0) {
            rt_thread_mdelay(10);
            continue;
        }

        int16_t *stereo = (int16_t *)buffer;
        int samples = (int)(length / 4);
        if (samples > MONO_SAMPLES_PER_READ) {
            samples = MONO_SAMPLES_PER_READ;
        }

        for (int i = 0; i < samples; i++) {
            mono_buf[i] = stereo[i * 2];
        }

        m33_watchdog_kick();

        snore_detector_feed(mono_buf, samples);

        snore_result_t result;
        if (snore_detector_get_result(&result)) {
            rt_kprintf(">> %s (%d.%02d)\n", result.label,
                       (int)result.value, (int)(result.value * 100) % 100);
        }
    }
}

static int audio_snore_auto_start(void)
{
    rt_kprintf("[M33] Creating audio thread...\r\n");
    rt_thread_t tid = rt_thread_create("asr", audio_snore_entry, RT_NULL, 8192, 12, 20);
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
        rt_kprintf("[M33] Audio thread created OK\r\n");
    } else {
        rt_kprintf("[M33] Failed to create audio thread\n");
    }
    return 0;
}
INIT_APP_EXPORT(audio_snore_auto_start);

int main(void)
{
    rt_kprintf("\r\n");
    rt_kprintf("========================================\r\n");
    rt_kprintf("   RT-Thread on Cortex-M33 Core         \r\n");
    rt_kprintf("   IPC Sensor + Snore Detection         \r\n");
    rt_kprintf("========================================\r\n");
    rt_kprintf("\r\n");

    rt_pin_mode(LED_PIN_B, PIN_MODE_OUTPUT);

    while (1)
    {
        m33_watchdog_kick();
        rt_pin_write(LED_PIN_B, PIN_HIGH);
        rt_thread_mdelay(500);
        m33_watchdog_kick();
        rt_pin_write(LED_PIN_B, PIN_LOW);
        rt_thread_mdelay(500);
    }

    return 0;
}
