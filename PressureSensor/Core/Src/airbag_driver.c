#include "airbag_driver.h"
#include "usart.h"
#include "gpio.h"
#include "stm32f1xx_hal_flash.h"

uint8_t g_usart1_rx_byte = 0;
static uint8_t g_cmd_state = 0;
#define STATE_WAIT_HEADER  0
#define STATE_WAIT_CMD     1
#define STATE_WAIT_PARAM   2
#define STATE_WAIT_FOOTER  3
static uint8_t g_cmd_buf[2];
static uint32_t g_last_rx_tick = 0;

volatile uint8_t g_airbag_current_state = ST_IDLE;
volatile uint8_t g_airbag_target_state  = ST_IDLE;
volatile uint8_t g_airbag_fb_state      = ST_IDLE;
volatile uint8_t g_airbag_fb_left_sec   = 0;
volatile uint8_t g_airbag_fb_mid_sec    = 0;
volatile uint8_t g_airbag_fb_right_sec  = 0;
volatile uint8_t g_airbag_fb_flags      = 0;

static uint8_t  g_pump_timer_active[4] = {0};
static uint32_t g_pump_timer_expire[4] = {0};
static uint8_t  g_pwm_duty[4] = {0};
static uint8_t  g_pwm_cnt = 0;
TIM_HandleTypeDef htim4_pwm;

int           g_inflate_elapsed[3]       = {0};
static int    g_inflate_phase_elapsed[3] = {0};
static inflate_pattern_t g_target = {{0, 0, 0}};
static uint32_t g_deflate_end_tick = 0;
static uint32_t g_hold_end_tick = 0;
static int g_test_step = 0;
static uint32_t g_test_tick = 0;

static int g_deflate_phase = 0;
static int g_deflate_remaining[3] = {0};
static int g_deflate_switching = 0;

static const inflate_pattern_t PATTERN_COMFORT = {
    { INFLATE_TIME_HIGH_MS, INFLATE_TIME_LOW_MS, INFLATE_TIME_HIGH_MS }
};
static const inflate_pattern_t PATTERN_SNORE_SUPINE = {
    { INFLATE_TIME_HIGH_MS, INFLATE_TIME_NONE_MS, INFLATE_TIME_NONE_MS }
};
static const inflate_pattern_t PATTERN_SNORE_LEFT_LATERAL = {
    { INFLATE_TIME_NONE_MS, INFLATE_TIME_NONE_MS, INFLATE_TIME_HIGH_MS }
};
static const inflate_pattern_t PATTERN_SNORE_RIGHT_LATERAL = {
    { INFLATE_TIME_HIGH_MS, INFLATE_TIME_NONE_MS, INFLATE_TIME_NONE_MS }
};

static const inflate_pattern_t *get_pattern(uint8_t state)
{
    switch (state) {
    case ST_COMFORT_INFLATE: return &PATTERN_COMFORT;
    case ST_INFLATING_SUPINE: return &PATTERN_SNORE_SUPINE;
    case ST_INFLATING_LEFT_LAT: return &PATTERN_SNORE_LEFT_LATERAL;
    case ST_INFLATING_RIGHT_LAT: return &PATTERN_SNORE_RIGHT_LATERAL;
    default: return 0;
    }
}

static uint8_t state_to_done(uint8_t state)
{
    switch (state) {
    case ST_COMFORT_INFLATE: return ST_COMFORT_INFLATE_DONE;
    case ST_INFLATING_SUPINE: return ST_INFLATING_SUPINE_D;
    case ST_INFLATING_LEFT_LAT: return ST_INFLATING_LEFT_LD;
    case ST_INFLATING_RIGHT_LAT: return ST_INFLATING_RIGHT_LD;
    case ST_HOLDING: return ST_HOLDING_DONE;
    case ST_DEFLATING: return ST_DEFLATING_DONE;
    case ST_TEST_MODE: return ST_TEST_MODE_DONE;
    case ST_MANUAL_INFLATE_L: return ST_MANUAL_INFLATE_LD;
    case ST_MANUAL_INFLATE_M: return ST_MANUAL_INFLATE_MD;
    case ST_MANUAL_INFLATE_R: return ST_MANUAL_INFLATE_RD;
    case ST_MANUAL_DEFLATE_L: return ST_MANUAL_DEFLATE_LD;
    case ST_MANUAL_DEFLATE_M: return ST_MANUAL_DEFLATE_MD;
    case ST_MANUAL_DEFLATE_R: return ST_MANUAL_DEFLATE_RD;
    default: return state;
    }
}

static const struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} g_pump_map[4] = {
    {GPIOB, GPIO_PIN_12},
    {GPIOB, GPIO_PIN_13},
    {GPIOB, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_15},
};

static const struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} g_valve_map[3] = {
    {GPIOD, GPIO_PIN_8},
    {GPIOD, GPIO_PIN_9},
    {GPIOD, GPIO_PIN_10},
};

void Airbag_PumpOn(uint8_t id)
{
    if (id >= 4) return;
    HAL_GPIO_WritePin(g_pump_map[id].port, g_pump_map[id].pin, GPIO_PIN_SET);
    g_pwm_duty[id] = 1;
}

void Airbag_PumpOff(uint8_t id)
{
    if (id >= 4) return;
    HAL_GPIO_WritePin(g_pump_map[id].port, g_pump_map[id].pin, GPIO_PIN_RESET);
    g_pwm_duty[id] = 0;
}

void Airbag_ValveSet(uint8_t bag, uint8_t mode)
{
    if (bag >= 3) return;
    if (mode)
        HAL_GPIO_WritePin(g_valve_map[bag].port, g_valve_map[bag].pin, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(g_valve_map[bag].port, g_valve_map[bag].pin, GPIO_PIN_RESET);
}

void Airbag_AllValvesInflate(void)
{
    for (int i = 0; i < 3; i++)
        Airbag_ValveSet(i, 0);
}

void Airbag_AllOff(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);
    Airbag_AllValvesInflate();
    for (int i = 0; i < 4; i++) g_pwm_duty[i] = 0;
}

void Airbag_PWM_ISR(void)
{
    __HAL_TIM_CLEAR_IT(&htim4_pwm, TIM_IT_UPDATE);
}

void Airbag_NVM_Save(void)
{
    airbag_nvm_t nvm;
    nvm.magic = AIRBAG_NVM_MAGIC;
    nvm.state_id = g_airbag_current_state;
    nvm.inflate_ms[0] = g_inflate_elapsed[0];
    nvm.inflate_ms[1] = g_inflate_elapsed[1];
    nvm.inflate_ms[2] = g_inflate_elapsed[2];
    nvm.checksum = nvm.magic ^ nvm.state_id ^ nvm.inflate_ms[0] ^ nvm.inflate_ms[1] ^ nvm.inflate_ms[2];

    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t err = 0;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = AIRBAG_NVM_ADDR;
    erase.NbPages = 1;
    HAL_FLASHEx_Erase(&erase, &err);

    uint32_t *p = (uint32_t *)&nvm;
    for (int i = 0; i < sizeof(airbag_nvm_t) / 4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, AIRBAG_NVM_ADDR + i * 4, p[i]);
    }
    HAL_FLASH_Lock();
}

uint8_t Airbag_NVM_Load(airbag_nvm_t *nvm)
{
    if (!nvm) return 0;
    memcpy(nvm, (void *)AIRBAG_NVM_ADDR, sizeof(airbag_nvm_t));
    if (nvm->magic != AIRBAG_NVM_MAGIC) return 0;
    uint32_t chk = nvm->magic ^ nvm->state_id ^ nvm->inflate_ms[0] ^ nvm->inflate_ms[1] ^ nvm->inflate_ms[2];
    if (chk != nvm->checksum) return 0;
    return 1;
}

void Airbag_NVM_Clear(void)
{
    airbag_nvm_t z;
    memset(&z, 0, sizeof(z));
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t err = 0;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = AIRBAG_NVM_ADDR;
    erase.NbPages = 1;
    HAL_FLASHEx_Erase(&erase, &err);
    uint32_t *p = (uint32_t *)&z;
    for (int i = 0; i < sizeof(airbag_nvm_t) / 4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, AIRBAG_NVM_ADDR + i * 4, 0);
    }
    HAL_FLASH_Lock();
}

static void inflate_slot_process(void)
{
    int targets[3] = {g_target.time_ms[0], g_target.time_ms[1], g_target.time_ms[2]};
    int order[3] = {AIRBAG_LEFT, AIRBAG_RIGHT, AIRBAG_MID};

    for (int i = 0; i < 3; i++) {
        if (g_pwm_duty[i] > 0) {
            g_inflate_elapsed[i]       += 200;
            g_inflate_phase_elapsed[i] += 200;
            if (g_inflate_phase_elapsed[i] >= targets[i])
                Airbag_PumpOff(i);
        }
    }

    int slots = 0;
    for (int i = 0; i < 3; i++) slots += (g_pwm_duty[i] > 0) ? 1 : 0;

    for (int p = 0; p < 3 && slots < 2; p++) {
        int i = order[p];
        if (targets[i] > 0 && g_pwm_duty[i] == 0 && g_inflate_phase_elapsed[i] < targets[i]) {
            Airbag_PumpOn(i);
            slots++;
        }
    }

    int done = 1;
    for (int i = 0; i < 3; i++)
        if (targets[i] > 0 && g_inflate_phase_elapsed[i] < targets[i]) { done = 0; break; }
    if (done) {
        Airbag_AllOff();
        g_airbag_current_state = state_to_done(g_airbag_target_state);
    }
}

static void execute_set_state(uint8_t state)
{
    g_airbag_target_state = state;

    switch (state) {
    case ST_IDLE:
        Airbag_AllOff();
        g_airbag_current_state = ST_IDLE;
        Airbag_NVM_Save();
        break;

    case ST_COMFORT:
        g_airbag_current_state = ST_COMFORT;
        Airbag_NVM_Save();
        break;

    case ST_COMFORT_INFLATE:
    case ST_INFLATING_SUPINE:
    case ST_INFLATING_LEFT_LAT:
    case ST_INFLATING_RIGHT_LAT: {
        const inflate_pattern_t *p = get_pattern(state);
        if (!p) break;
        g_target = *p;
        g_inflate_phase_elapsed[0] = 0;
        g_inflate_phase_elapsed[1] = 0;
        g_inflate_phase_elapsed[2] = 0;
        Airbag_AllValvesInflate();
        g_airbag_current_state = state;
        Airbag_NVM_Save();
        break;
    }

    case ST_HOLDING:
        g_hold_end_tick = HAL_GetTick() + HOLD_TIME_MS;
        g_airbag_current_state = ST_HOLDING;
        Airbag_NVM_Save();
        break;

    case ST_DEFLATING: {
        Airbag_AllOff();
        g_deflate_remaining[0] = g_inflate_elapsed[0] >= 2000 ? g_inflate_elapsed[0] - 2000 : 0;
        g_deflate_remaining[1] = g_inflate_elapsed[1] >= 2000 ? g_inflate_elapsed[1] - 2000 : 0;
        g_deflate_remaining[2] = g_inflate_elapsed[2] >= 2000 ? g_inflate_elapsed[2] - 2000 : 0;
        for (int i = 0; i < 3; i++)
            if (g_deflate_remaining[i] < 1000) g_deflate_remaining[i] = 1000;
        Airbag_ValveSet(0, 1);
        Airbag_PumpOn(PUMP_SUCTION);
        g_deflate_phase = 0;
        g_deflate_switching = 0;
        g_deflate_end_tick = HAL_GetTick() + g_deflate_remaining[0];
        g_airbag_current_state = ST_DEFLATING;
        Airbag_NVM_Save();
        break;
    }

    case ST_TEST_MODE:
        Airbag_AllOff();
        g_test_step = 0;
        g_test_tick = HAL_GetTick();
        g_airbag_current_state = ST_TEST_MODE;
        break;

    case ST_MANUAL_INFLATE_L:
        Airbag_AllOff();
        Airbag_AllValvesInflate();
        g_inflate_elapsed[AIRBAG_LEFT] += 1000;
        Airbag_PumpOn(PUMP_LEFT);
        g_hold_end_tick = HAL_GetTick() + 1000;
        g_airbag_current_state = ST_MANUAL_INFLATE_L;
        break;
    case ST_MANUAL_INFLATE_M:
        Airbag_AllOff();
        Airbag_AllValvesInflate();
        g_inflate_elapsed[AIRBAG_MID] += 1000;
        Airbag_PumpOn(PUMP_MID);
        g_hold_end_tick = HAL_GetTick() + 1000;
        g_airbag_current_state = ST_MANUAL_INFLATE_M;
        break;
    case ST_MANUAL_INFLATE_R:
        Airbag_AllOff();
        Airbag_AllValvesInflate();
        g_inflate_elapsed[AIRBAG_RIGHT] += 1000;
        Airbag_PumpOn(PUMP_RIGHT);
        g_hold_end_tick = HAL_GetTick() + 1000;
        g_airbag_current_state = ST_MANUAL_INFLATE_R;
        break;
    case ST_MANUAL_DEFLATE_L:
        Airbag_AllOff();
        Airbag_ValveSet(0, 1);
        {   int cur = g_inflate_elapsed[AIRBAG_LEFT];
            g_inflate_elapsed[AIRBAG_LEFT] = (cur >= 1000) ? (cur - 1000) : 0;
        }
        Airbag_PumpOn(PUMP_SUCTION);
        g_hold_end_tick = HAL_GetTick() + 1000;
        g_airbag_current_state = ST_MANUAL_DEFLATE_L;
        break;
    case ST_MANUAL_DEFLATE_M:
        Airbag_AllOff();
        Airbag_ValveSet(1, 1);
        {   int cur = g_inflate_elapsed[AIRBAG_MID];
            g_inflate_elapsed[AIRBAG_MID] = (cur >= 1000) ? (cur - 1000) : 0;
        }
        Airbag_PumpOn(PUMP_SUCTION);
        g_hold_end_tick = HAL_GetTick() + 1000;
        g_airbag_current_state = ST_MANUAL_DEFLATE_M;
        break;
    case ST_MANUAL_DEFLATE_R:
        Airbag_AllOff();
        Airbag_ValveSet(2, 1);
        {   int cur = g_inflate_elapsed[AIRBAG_RIGHT];
            g_inflate_elapsed[AIRBAG_RIGHT] = (cur >= 1000) ? (cur - 1000) : 0;
        }
        Airbag_PumpOn(PUMP_SUCTION);
        g_hold_end_tick = HAL_GetTick() + 1000;
        g_airbag_current_state = ST_MANUAL_DEFLATE_R;
        break;

    default:
        break;
    }
}

void Airbag_ParseCmdByte(uint8_t ch)
{
    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - g_last_rx_tick) > 100)
        g_cmd_state = STATE_WAIT_HEADER;
    g_last_rx_tick = now;

    switch (g_cmd_state) {
    case STATE_WAIT_HEADER:
        if (ch == CMD_FRAME_HEADER) g_cmd_state = STATE_WAIT_CMD;
        break;
    case STATE_WAIT_CMD:
        g_cmd_buf[0] = ch;
        g_cmd_state = STATE_WAIT_PARAM;
        break;
    case STATE_WAIT_PARAM:
        g_cmd_buf[1] = ch;
        g_cmd_state = STATE_WAIT_FOOTER;
        break;
    case STATE_WAIT_FOOTER:
        if (ch == CMD_FRAME_FOOTER) {
            execute_set_state(g_cmd_buf[0]);
        }
        g_cmd_state = STATE_WAIT_HEADER;
        break;
    default:
        g_cmd_state = STATE_WAIT_HEADER;
        break;
    }
}

void Airbag_CheckTimers(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < 4; i++) {
        if (g_pump_timer_active[i]) {
            if ((int32_t)(now - g_pump_timer_expire[i]) >= 0) {
                Airbag_PumpOff(i);
                g_pump_timer_active[i] = 0;
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        if (g_pwm_duty[i] > 0) {
            int limit = g_target.time_ms[i] + 2000;
            if (limit > PUMP_MAX_RUN_MS) limit = PUMP_MAX_RUN_MS;
            if (g_inflate_phase_elapsed[i] >= limit) {
                Airbag_PumpOff(i);
                g_airbag_current_state = ST_IDLE;
            }
        }
    }

    switch (g_airbag_current_state) {
    case ST_COMFORT_INFLATE:
    case ST_INFLATING_SUPINE:
    case ST_INFLATING_LEFT_LAT:
    case ST_INFLATING_RIGHT_LAT:
        inflate_slot_process();
        break;

    case ST_HOLDING:
        if ((int32_t)(now - g_hold_end_tick) >= 0) {
            g_airbag_current_state = ST_HOLDING_DONE;
        }
        break;

    case ST_MANUAL_INFLATE_L:
    case ST_MANUAL_INFLATE_M:
    case ST_MANUAL_INFLATE_R:
    case ST_MANUAL_DEFLATE_L:
    case ST_MANUAL_DEFLATE_M:
    case ST_MANUAL_DEFLATE_R:
        if ((int32_t)(now - g_hold_end_tick) >= 0) {
            Airbag_AllOff();
            g_airbag_current_state = state_to_done(g_airbag_current_state);
        }
        break;

    case ST_DEFLATING: {
        int d = g_deflate_phase;
        if (d >= 3) { break; }

        if (g_deflate_switching) {
            if ((int32_t)(now - g_deflate_end_tick) >= 0) {
                g_deflate_switching = 0;
                Airbag_PumpOn(PUMP_SUCTION);
                g_deflate_end_tick = now + g_deflate_remaining[d];
            }
            break;
        }

        if (g_inflate_elapsed[d] > 0) {
            int dec = 200;
            if (g_inflate_elapsed[d] < dec) dec = g_inflate_elapsed[d];
            g_inflate_elapsed[d] -= dec;
        }

        if ((int32_t)(now - g_deflate_end_tick) >= 0) {
            Airbag_ValveSet(d, 0);
            Airbag_PumpOff(PUMP_SUCTION);
            g_inflate_elapsed[d] = 0;
            d++;
            g_deflate_phase = d;
            if (d >= 3) {
                g_airbag_current_state = ST_DEFLATING_DONE;
                Airbag_NVM_Save();
            } else {
                Airbag_ValveSet(d, 1);
                g_deflate_switching = 1;
                g_deflate_end_tick = now + 1000;
            }
        }
        break;
    }

    case ST_TEST_MODE: {
        switch (g_test_step) {
        case 0:
            Airbag_AllValvesInflate(); Airbag_PumpOn(PUMP_LEFT);
            g_test_tick = HAL_GetTick(); g_test_step = 1; break;
        case 1:
            if (now - g_test_tick >= 3000) { Airbag_PumpOff(PUMP_LEFT); g_test_tick = HAL_GetTick(); g_test_step = 2; }
            break;
        case 2:
            if (now - g_test_tick >= 1000) { Airbag_PumpOn(PUMP_MID); g_test_tick = HAL_GetTick(); g_test_step = 3; }
            break;
        case 3:
            if (now - g_test_tick >= 3000) { Airbag_PumpOff(PUMP_MID); g_test_tick = HAL_GetTick(); g_test_step = 4; }
            break;
        case 4:
            if (now - g_test_tick >= 1000) { Airbag_PumpOn(PUMP_RIGHT); g_test_tick = HAL_GetTick(); g_test_step = 5; }
            break;
        case 5:
            if (now - g_test_tick >= 3000) { Airbag_PumpOff(PUMP_RIGHT); g_test_tick = HAL_GetTick(); g_test_step = 6; }
            break;
        case 6:
            if (now - g_test_tick >= 1000) { Airbag_ValveSet(0, 1); Airbag_PumpOn(PUMP_SUCTION); g_test_tick = HAL_GetTick(); g_test_step = 7; }
            break;
        case 7:
            if (now - g_test_tick >= 3000) { Airbag_PumpOff(PUMP_SUCTION); Airbag_ValveSet(0, 0); g_test_tick = HAL_GetTick(); g_test_step = 8; }
            break;
        case 8:
            if (now - g_test_tick >= 1000) { Airbag_ValveSet(1, 1); Airbag_PumpOn(PUMP_SUCTION); g_test_tick = HAL_GetTick(); g_test_step = 9; }
            break;
        case 9:
            if (now - g_test_tick >= 3000) { Airbag_PumpOff(PUMP_SUCTION); Airbag_ValveSet(1, 0); g_test_tick = HAL_GetTick(); g_test_step = 10; }
            break;
        case 10:
            if (now - g_test_tick >= 1000) { Airbag_ValveSet(2, 1); Airbag_PumpOn(PUMP_SUCTION); g_test_tick = HAL_GetTick(); g_test_step = 11; }
            break;
        case 11:
            if (now - g_test_tick >= 3000) {
                Airbag_PumpOff(PUMP_SUCTION); Airbag_AllValvesInflate();
                g_airbag_current_state = ST_TEST_MODE_DONE;
            }
            break;
        }
        break;
    }

    default:
        break;
    }

    Airbag_UpdateFeedback();
}

void Airbag_UpdateFeedback(void)
{
    g_airbag_fb_state     = g_airbag_current_state;
    g_airbag_fb_left_sec  = (uint8_t)(g_inflate_elapsed[0] / 1000);
    g_airbag_fb_mid_sec   = (uint8_t)(g_inflate_elapsed[1] / 1000);
    g_airbag_fb_right_sec = (uint8_t)(g_inflate_elapsed[2] / 1000);
    g_airbag_fb_flags     = 0;
    if (g_airbag_current_state == ST_TEST_MODE || g_airbag_current_state == ST_TEST_MODE_DONE)
        g_airbag_fb_flags |= FB_FLAG_TEST;
}

void AirbagDriver_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    gpio_init.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOD, &gpio_init);

    Airbag_AllOff();

    airbag_nvm_t nvm;
    uint8_t has_pending = 0;
    if (Airbag_NVM_Load(&nvm)) {
        if (nvm.state_id != ST_IDLE && nvm.state_id != ST_COMFORT &&
            !ST_IS_DONE(nvm.state_id)) {
            has_pending = 1;
        }
    }
    if (has_pending) {
        int total = nvm.inflate_ms[0] + nvm.inflate_ms[1] + nvm.inflate_ms[2];
        if (total < BOOT_DEFLATE_MS) total = BOOT_DEFLATE_MS;
        Airbag_AllValvesInflate();
        Airbag_ValveSet(0, 1);
        Airbag_PumpOn(PUMP_SUCTION);
        HAL_Delay(nvm.inflate_ms[0] < 1000 ? 1000 : nvm.inflate_ms[0]);
        Airbag_ValveSet(0, 0);
        Airbag_ValveSet(1, 1);
        HAL_Delay(nvm.inflate_ms[1] < 1000 ? 1000 : nvm.inflate_ms[1]);
        Airbag_ValveSet(1, 0);
        Airbag_ValveSet(2, 1);
        HAL_Delay(nvm.inflate_ms[2] < 1000 ? 1000 : nvm.inflate_ms[2]);
        Airbag_ValveSet(2, 0);
        Airbag_PumpOff(PUMP_SUCTION);
    }
    Airbag_NVM_Clear();

    g_airbag_current_state = ST_IDLE;
    g_airbag_target_state  = ST_IDLE;

    __HAL_RCC_TIM4_CLK_ENABLE();
    htim4_pwm.Instance = TIM4;
    htim4_pwm.Init.Prescaler = 72 - 1;
    htim4_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4_pwm.Init.Period = 5000 - 1;
    htim4_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim4_pwm);
    HAL_NVIC_SetPriority(TIM4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    HAL_TIM_Base_Start_IT(&htim4_pwm);

    HAL_UART_Receive_IT(&huart1, &g_usart1_rx_byte, 1);
}