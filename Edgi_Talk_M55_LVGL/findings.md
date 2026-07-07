# Findings & Decisions

## Requirements
- GPIO 控制 3 个充气泵(P11.3/5/7) + 1 个吸气泵(P14.7) + 电磁阀(P13.3)
- 电磁阀 HIGH=吸气通路(放气)，LOW=充气通路(充气)
- 三种充气级别：多充 5s，少充 3s，不充 0s（宏定义，可后期修改）
- 左右侧卧舒适模式：3 气囊全部充气并保持
- 仰卧/左右侧头/noise 稳态：3 气囊全部不充气
- 鼾声触发干预（框架预留，后续接入真实鼾声检测）
- 同一时间最多 2 个泵工作
- 放气只能一起放气，时间 = 各气囊当前充气时间之和
- 上电强制放气保证安全

## Research Findings

### 项目现有状态
- Cortex-M55 + RT-Thread + FreeRTOS on Infineon PSE84
- AI 睡姿检测 (Imagimob)：7 分类 (unlabeled/supine/right_lateral/right_head/left_lateral/left_head/noise)
- 10 帧多数投票平均 (`posture_vote_update`)
- 压力数据通过 UART5 来自 STM32 (16x16 矩阵)
- P11.3 原用于 SMART_IO 软件 PWM（需移除）

### GPIO 引脚状态
| 引脚 | 当前用途 | 新用途 | 冲突？
|------|---------|--------|-------|
| P11.3 | SMART_IO 软 PWM (100Hz) | 左充气泵 | ✅ 用户确认移除 SMART_IO |
| P11.5 | 未使用 | 中充气泵 | ✅ |
| P11.7 | 未使用 | 右充气泵 | ✅ |
| P14.7 | 未在 cycfg 中定义 | 吸气泵 | ✅ run `rt_pin_mode()` 动态配置 |
| P13.3 | 未在 cycfg 中定义 | 电磁阀 | ✅ run `rt_pin_mode()` 动态配置 |

### 屏幕信息 (2026-06-18 确认)
- 型号: tl043wvv02 (4.3寸)
- 分辨率: **512×800 竖屏** (MY_DISP_HOR_RES=512, MY_DISP_VER_RES=800)
- 配置文件: `libraries/Common/board/ports/lvgl/lv_port_disp.h:33-34`
- 原热力图 350×542 在竖屏中上部合理 (350<512, 542<800)
- 新布局: 缩小热力图为 303×463, 腾出空间给气囊图标

### TCPWM/PWM 资源
- PSE84 有 1024 个硬件 TCPWM 计数器（4 组 x 256）
- RT-Thread PWM 框架已启用 (`RT_USING_PWM` + `BSP_USING_PWM`)
- 当前仅启用 PWM18（用于 LCD 背光，但实际未使用 PWM 驱动）
- drv_pwm.c 已完整实现（4 个预配置通道：PWM5/6/18/13）
- 用户选择 **软件 PWM** 方案，不涉及 TCPWM 硬件改动

### 物理原理
- 左气囊充气 → 左侧升高 → 身体/头向右倾斜
- 右气囊充气 → 右侧升高 → 身体/头向左倾斜
- 中气囊充气 → 中部升高 → 上半身略微抬升

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| 软件 PWM via GPIO on/off | 全速充气时等同 GPIO 高低电平，无需复杂 PWM 逻辑 |
| `airbag_process()` 每 200ms 调用 | 匹配姿势检测帧率（STM32 每 200ms 发一帧） |
| 姿势去抖 5 帧 | 防止姿势抖动导致频繁机械动作 |
| 状态机拆分 6 个状态 | 清晰表达：稳态(IDLE/COMFORT) + 瞬态(COMFORT_INFLATE/INFLATING/HOLDING/DEFLATING) |
| 上电 `airbag_system_init()` 强制放气 5s | 无论断电前状态如何，确保安全启动 |
| `g_airbag_state_ms[3]` RAM 数组 | 运行时追踪当前充气状态，用于计算放气时间 |
| POSTURE_CONFIRM_COUNT=5 | 鲁棒性优先，做足连续确认再触发 |

### 充气配置表
| 场景 | 姿势 | 左 | 中 | 右 |
|------|------|----|----|----|
| 舒适模式稳态 | left_lateral / right_lateral | 5s | 3s | 5s |
| 止鼾干预 | supine | 0 | 0 | 5s (推向左转头) |
| 止鼾干预 | left_head | 5s | 0 | 0 (推向右转头) |
| 止鼾干预 | right_head | 0 | 0 | 5s (推向左转头) |
| 侧卧+打鼾干预 | left_lateral | 0 | 0 | 5s |
| 侧卧+打鼾干预 | right_lateral | 5s | 0 | 0 |
| 默认稳态 | supine/left_head/right_head/noise | 0 | 0 | 0 |

### 2 泵限制下 3 袋充气时序 (例: 5/3/5)
| 时间段 | 泵状态 | 说明 |
|--------|--------|------|
| t=0 ~ t=3s | 左泵 + 中泵 | 2 泵满额 |
| t=3s ~ t=5s | 左泵 + 右泵 | 中泵完成，右泵启动 |
| t=5s ~ t=8s | 右泵 | 左泵完成，右泵继续 3s |
| 总耗时 8s | | 比串行(13s)快 |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| P11.3 已被 SMART_IO 占用 | 用户确认移除 SMART_IO，改做左泵控制 |
| P14.7/P13.3 未在 cycfg 定义 | `rt_pin_mode()` 可动态配置，无需改 cycfg |

## 投票机制变更分析 (2026-06-18)

### 变更内容
滑动窗口 (1-10→2-11→3-12) → **批处理窗口** (1-10→11-20→21-30)

### 受影响文件
| 文件 | 内容 | 影响 |
|------|------|------|
| `main.c:131-178` | `posture_vote_update()` | 重写：累积满10帧才投票一次，然后重置 |
| `main.c:137` | `g_posture_win_pos` | 改为 `g_posture_batch_pos` |
| `main.c:138` | `g_posture_win_cnt` | **删除**（无需累计计数） |
| `main.c:132` | `POSTURE_WINDOW_SIZE` | 改名 `POSTURE_BATCH_SIZE` |
| `main.c:136` | `g_posture_win[]` | 改名 `g_posture_batch[]` |
| `main.c:181-189` | `posture_get_majority_label()` | 调整检查逻辑：是否已有有效批结果 |
| `main.c:615` | 调用点 | ✅ 签名不变，无需改动 |
| `ui_pressure.c` | 调用 `posture_get_majority_label()` | ✅ 接口不变 |
| 气囊控制(待写) | 读取 `g_majority_posture` | ✅ 接口不变，频率降低对机械系统反而有利 |

### 对气囊控制的影响
- **有利**：`g_majority_posture` 每~2s才跳变一次，减少机械系统抖动
- **无影响**：气囊状态机去抖（POSTURE_CONFIRM_COUNT=5次）依然正常工作
- **无需调整**：airbag_process() 的接口和逻辑完全兼容批处理输出

## 2026-06-25: GPIO 控制迁移至 STM32 架构变更

### 变更动机
PSoC（Cortex-M55）GPIO 驱动能力不足（最大 8mA），无法可靠驱动气泵继电器/电磁阀。STM32F103VE GPIO 驱动能力更强（最大 25mA）。

### STM32 端 GPIO 分配
| 功能 | STM32 Pin | 端口/引脚 | 说明 |
|------|-----------|-----------|------|
| 左充气泵 | PB12 | GPIOB.12 | 原已初始化(SET) → 改 RESET |
| 中充气泵 | PB13 | GPIOB.13 | 原已初始化(SET) → 改 RESET |
| 右充气泵 | PB14 | GPIOB.14 | 新增手动初始化 |
| 吸气泵 | PB15 | GPIOB.15 | 新增手动初始化 |
| 电磁阀 | PD8 | GPIOD.8 | 新增手动初始化, HIGH=吸气/LOW=充气 |

### USART1 双向通信
```
STM32 → PSoC: 0xAA 压力帧(268B) + 0xBB 雷达合并帧(每3s)
PSoC → STM32: 0xA5 命令帧(4B)  [新增]
```
USART1 配置：115200 8N1，STM32 PA9(TX) ↔ PSoC P17.1(UART5_TX)

### 命令帧协议
```
Byte[0] = 0xA5  帧头
Byte[1] = CMD   命令码
Byte[2] = PARAM  参数(×100ms, 0=无超时)
Byte[3] = 0x5A  帧尾
```
命令码见 `airbag_driver.h` 中 `CMD_LEFT_PUMP_ON` 等 define。

### 关键决策
| 决策 | 理由 |
|------|------|
| STM32 端采用 `HAL_UART_Receive_IT`（与 USART3 一致）而非裸 RXNE | 错误处理自动、代码风格一致 |
| PB12/13 手动改初始电平为 RESET | CubeMX 再生后会恢复为 SET，需记住 |
| PB14/15/PD8 在 gpio.c USER CODE 手动初始化 | 与现有 PB12/13 模式一致 |
| PSoC 端 test_airbag.c 保留并改造 | 验证 GPIO 下放后的整条链路完整性 |
| 吸气泵测试时先切电磁阀到吸气通路 | 硬性要求：不放气时开吸气泵无意义 |
| NVM 持久化保留 | PSoC 端上电恢复状态机，决定是否需要触发强制放气 |

## 2026-06-26: Phase 9 最终架构决策 — 状态机全部迁移至 STM32

### 决策背景
Phase 8 仅迁移了 GPIO 执行层到 STM32，M55 仍运行完整 6 状态止鼾状态机、泵时间跟踪、NVM 持久化。M55 需同时跑 LVGL/AI/IPC，增加不可预测的调度延迟，且泵时间跟踪/超时保护在 RTOS 环境中不够硬实时。

### 决策结论
STM32 **纯执行**——收到 SET_STATE 做动作，做完报 `_DONE`，不做任何自动跳转。
M55 **纯决策**——姿势/鼾声去抖 + 等 `_DONE` + 决定下一帧发什么。

### 协议设计

| 方向 | 格式 | 说明 |
|------|------|------|
| M55 → STM32 | `0xA5 + STATE_ID + 0x00 + 0x5A` | 4字节固定帧 |
| STM32 → M55 | 0xAA 压力帧 byte[262-267] | 每200ms实时捎带 |

### 上行反馈格式 (0xAA 压力帧字节 262-267, 每 200ms 更新)
```
byte[262] = current_state (0x00-0x13, 含 _DONE)
byte[263] = left_bag_sec  (0-255, 实时更新)
byte[264] = mid_bag_sec   (0-255, 实时更新)
byte[265] = right_bag_sec (0-255, 实时更新)
byte[266] = flags         (bit0=test, bit1=error)
byte[267] = reserved
```

### 20 种 STATE_ID（含 _DONE）
| ID | 名称 | 说明 |
|----|------|------|
| 0x00 | IDLE | 稳态, 全关 |
| 0x01 | COMFORT_INFLATE | 动作中: 充5/3/5 |
| 0x02 | COMFORT_INFLATE_DONE | 充完泵关 |
| 0x03 | COMFORT | 稳态, 气囊有气 |
| 0x04 | INFLATING_SUPINE | 动作中: 0/0/5 |
| 0x05 | INFLATING_SUPINE_DONE | |
| 0x06 | INFLATING_LEFT_HEAD | 动作中: 5/0/0 |
| 0x07 | INFLATING_LEFT_HEAD_DONE | |
| 0x08 | INFLATING_RIGHT_HEAD | 动作中: 0/0/5 |
| 0x09 | INFLATING_RIGHT_HEAD_DONE | |
| 0x0A | INFLATING_LEFT_LATERAL | 动作中: 0/0/5 |
| 0x0B | INFLATING_LEFT_LATERAL_DONE | |
| 0x0C | INFLATING_RIGHT_LATERAL | 动作中: 5/0/0 |
| 0x0D | INFLATING_RIGHT_LATERAL_DONE | |
| 0x0E | HOLDING | 动作中: 计时5s |
| 0x0F | HOLDING_DONE | 5s到 |
| 0x10 | DEFLATING | 动作中: 放气 |
| 0x11 | DEFLATING_DONE | 放完泵阀关 |
| 0x12 | TEST_MODE | |
| 0x13 | TEST_MODE_DONE | |

### _DONE 等待规则
- 有 `_DONE` 的状态（**必须等 DONE**）：COMFORT_INFLATE / INFLATING_* / HOLDING / DEFLATING / TEST_MODE
- 无 `_DONE` 的状态（**可直接发**）：IDLE / COMFORT
- 例：姿势在 COMFORT_INFLATE 中途变了 → 必须等到 COMFORT_INFLATE_DONE → 再发 DEFLATING

### 关键决策表
| 决策 | 理由 |
|------|------|
| STM32 没有任何自动跳转 | M55 有全局姿势/鼾声信息，全权决策 |
| 有 `_DONE` 的状态必须等到 DONE 才发下一帧 | HOLDING 也不打断，跑满5s |
| 无 `_DONE` 的状态（IDLE/COMFORT）可直接发 | 稳态无需等待 |
| 放气时长由 STM32 内部毫秒级 inflate_elapsed[3] 计算 | 精准 = 实际充气时间和，保底3s |
| 帧里不需要传放气时长参数 | STM32 自己算 |
| COMFORT_INFLATE 充完关泵 | 气体不漏气，保持靠物理 |
| INFLATING 按姿势拆成 5 个独立 STATE_ID | 避免额外传模式参数，一帧搞定 |
| byte[263-265] 每200ms实时更新 | 复位后 M55 立即知道充气状态 |
| NVM 用内部 Flash 末页 (2KB, 2-slot 磨损均衡) | 无需外挂芯片，0成本 |

### M55 看门狗复位恢复
```
复位后发 SET_STATE(IDLE) → 等 0xAA 帧 → 读 byte[262]:

byte[262]= IDLE/任何_DONE        → 直接决策下一步
byte[262]= COMFORT_INFLATE/       → 发 DEFLATING
          INFLATING_*/HOLDING/     等 DEFLATING_DONE
          DEFLATING   
byte[262]= COMFORT                → 侧卧就留着, 非侧卧就 DEFLATING
```

## 2026-07-05: 上位机云端部署方案

### 部署架构
```
M55 (WiFi) ──TCP:8888──▶ 阿里云服务器 (8.136.125.241)
                          ├── Node.js (shangweiji)
                          ├── Web:3000
                          ├── TCP:8888
                          └── SQLite: sleep_data.db

用户手机 ──HTTP:3000──▶ 云服务器 Web 页面
```

### 通信协议新增（M55→云端 TCP）
| 行前缀 | 格式 | 触发事件 |
|--------|------|---------|
| `[RADAR]` | `HR=72 BR=16 motion=15 dist=85 exist=0x80\r\n` | `'radar'` |
| `[SNORE]` | `snore=1 conf=95%\r\n` | `'snore'` |

### 下行命令（手机→M55）
| 命令 | 说明 |
|------|------|
| `MAN_INFLATE 0\|1\|2\r\n` | 充气左/中/右 |
| `MAN_DEFLATE 0\|1\|2\r\n` | 放气左/中/右 |

### 数据库新增表
| 表名 | 字段 |
|------|------|
| `radar_records` | id, session_id, timestamp, heart_rate, breath_rate, motion, distance, exist |
| `snore_records` | id, session_id, timestamp, snore, confidence |

### 云端部署步骤
```bash
# 1. 上传修改文件
scp tcp-server.js root@8.136.125.241:/opt/shangweiji/
scp server.js root@8.136.125.241:/opt/shangweiji/
scp public/index.html root@8.136.125.241:/opt/shangweiji/public/

# 2. 重启服务
ssh root@8.136.125.241
pm2 restart sleep-monitor
pm2 log sleep-monitor

# 3. M55 端重新编译烧录（wifi_tcp_sender.h 已配 8.136.125.241）
```

## Resources
- 设计文档规划路径: `docs/superpowers/specs/2026-06-18-airbag-posture-control-design.md`
- 实施计划: `docs/superpowers/plans/2026-06-18-airbag-posture-control-plan.md`
- GPIO 驱动: `libraries/HAL_Drivers/drv_gpio.h`
- 现有姿势检测: `applications/main.c` (posture_vote_update, g_majority_posture)
- AI 模型输出: `model/model.h` (IMPRESSURE_DATA_OUT_COUNT=7)
- 引脚配置: `libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/cycfg_pins.h`
- 测试框架: `applications/test_airbag.c` (MSH命令 `test_airbag_all`)
- 测试模式API: `applications/airbag_control.h` (airbag_test_mode_set/is_active)
