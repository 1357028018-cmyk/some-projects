# Task Plan: GPIO 气泵/电磁阀控制 + 止鼾状态机

## Goal
在现有 AI 睡姿检测基础上，实现基于 PSE84 GPIO 的气囊控制系统：包含软件 PWM 泵控、6 状态止鼾状态机、姿势去抖/鼾声框架、上电安全放气。

## Current Phase
Phase 5 (Stabilization & Review)

## Phase 5: Stabilization & Testbench
- [x] PUMP_MAX_RUN_MS 改为 10s（仅充气泵, 不监测吸气泵）
- [x] ST_INFLATING 去掉姿势中止逻辑（干预过程不被打断）
- [x] 回到稳态(ST_IDLE/COMFORT)时重置姿势去抖计数
- [x] 创建 test_airbag.c — 10 场景自动化测试
- [x] 添加 airbag_test_mode_set/is_active API
- [x] main() 中测试模式下跳过 airbag_process()
- **Status:** complete

## Phases

### Phase 1: Requirements & Discovery
- [x] 梳理用户 GPIO 分配需求 (P11.3/5/7, P14.7, P13.3)
- [x] 发现 P11.3 与现有 SMART_IO PWM 冲突，用户确认移除 SMART_IO
- [x] 确认软件 PWM 方案（非 TCPWM 硬件 PWM）
- [x] 确认状态机设计：IDLE → INFLATING/COMFORT_INFLATE → HOLDING/COMFORT → DEFLATING → IDLE
- [x] 确认 2 泵并发限制
- [x] 确认放气自适应公式：deflation_ms = sum(g_airbag_state_ms)
- [x] 确认上电强制放气策略
- **Status:** complete

### Phase 2: Planning & Structure
- [x] 设计 GPIO 分配表
- [x] 设计各姿势充气配置表
- [x] 设计状态机流程图
- [x] 设计文件结构：airbag_control.h + airbag_control.c + main.c(修改)
- [x] 用户审批通过完整设计方案
- **Status:** complete

### Phase 3: Implementation
- [x] Task 0: 修改 `applications/main.c` 投票机制 — 滑动窗口→批处理窗口
- [x] Task 1: 创建 `applications/airbag_control.h` — 宏定义、枚举、API
- [x] Task 2: 创建 `applications/airbag_control.c` — 软件 PWM + 状态机 + 初始化
- [x] Task 3: 修改 `applications/main.c` — 移除 SMART_IO，添加气囊初始化 + 暴露 `g_majority_posture`
- **Status:** complete

### Phase 4: Spec Self-Review & Verification
- [ ] 检查宏定义一致性
- [ ] 检查状态机覆盖所有设计需求
- [ ] 确保无占位符/TODO 遗留
- [ ] 输出文件列表确认
- **Status:** pending

### Phase 5: Delivery
- [ ] 用户审阅代码
- [ ] 确认无误后交付
- **Status:** pending

## Key Questions
1. P11.3 SMART_IO 冲突 → 用户确认移除 SMART_IO，改用软件 PWM
2. 硬件 PWM vs 软件 PWM → 用户选择软件 PWM（定时器方式）
3. 侧卧+鼾声微调策略 → 用户确认：全部放气→侧卧止鼾模式→保持→放气→恢复舒适
4. 气囊状态存储方式 → RAM 数组 + 上电强制放气保障安全
5. 充气时间宏定义 → 5000/3000/0 ms，便于配置

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| 软件 PWM (RT-Thread Timer) | 不改底层配置，代码改动最小，满足 100Hz 需求 |
| 2 泵并发限制 | 用户明确硬件限制：最多 2 泵同时工作 |
| 放气时间 = sum(充气时间) | 用户确认：5+3+5=13s 放气 |
| RAM 数组 + 上电放气 | 不需要 NVM，上电强制放气保证安全 |
| 姿势去抖连续 5 次 | 鲁棒性优先，响应速度可让步 |
| PUMP_MAX_RUN_MS=10s(仅充气泵) | 用户要求; 吸气泵放气13s正常,不纳入超时 |
| 止鼾干预过程中不打断 | 干预过程压力变化不应误判为新姿势 |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| BUG#1 主循环200ms调用但timer减100ms | 1 | 改为100ms周期调用airbag_process |
| BUG#2 泵无超时保护,主循环卡住泵一直运行 | 1 | 添加pump_max_run_ms超时强制关泵 |
| BUG#3 default分支未关泵 | 1 | default中调用pump_off_all+valve_to_inflate |
| BUG#4 侧卧+鼾声放气后恢复舒适而非干预 | 1 | 增加标志位区分干预路径 |
| BUG#5 HOLDING提前结束太敏感 | 1 | 要求连续N帧无鼾声才结束 |
| BUG#6 充气中姿势变化未中止 | 1 | 充气态检测姿势变化→放气 |
| BUG#7 dt变量冗余 | 1 | 删除 |
| UI#1 热力图542px高超出480px屏幕 | 1 | 调查错误:实际是512x800竖屏,原布局合理 |
| UI#2 误改横屏布局 | 1 | 恢复竖屏纵向布局,缩小热力图腾空间 |
| TEST#1 testbench去抖off-by-one | 1 | 5次→6次(1帧发现变化+5帧确认) |
| TEST#2 场景2 GPIO检查太晚 | 1 | 进入INFLATING后立即检查,不等5s |
| TEST#3 安全超时后立即重新充气 | 1 | pump_safety_check重置posture_debounce |
| BUG#8 Phase2设phase=3但泵还在运行 | 1 | max_rem>0时留在phase2, 仅max_rem==0时关泵进phase3 |
| TEST#4 场景5等待循环太短(36s流程只等20s) | 1 | 200→450次循环 |
| TEST#5 场景13前场景残留COMFORT状态未清除 | 1 | 确保IDLE前先触发放气 |
| REFACTOR testbench重写为简单泵测试 | 1 | 638→90行, 顺序激活左中右吸各3s |

## Phase 6: Testbench Expansion (10→17 scenarios)
- [x] 11: 右转头打鼾(对称性)
- [x] 12: 右侧卧舒适模式(对称性)
- [x] 13: 鼾声HOLDING期间提前停止
- [x] 14: 连续鼾声背靠背循环
- [x] 15: unlabeled姿势不干预
- [x] 16: 2泵并发限制全程验证
- [x] 17: 快速姿势切换(翻身)去抖压力测试
- **Status:** complete

## Phase 7: Testbench Rewrite (simple pump test)
- [x] pump_on/pump_off 暴露为公共API
- [x] 重写 test_airbag.c (638→90行, 顺序激活4泵各3s)
- **Status:** complete

## Current Phase
Phase 9 (状态机/测试/决策迁移到 STM32)

## Phase 8: GPIO 控制迁移到 STM32（USART1 双向通信）

### 背景
PSoC（M55）GPIO 驱动能力不足，无法可靠驱动气泵继电器/电磁阀。STM32 端 USART1 仅有 TX、**RX 空闲**，且 GPIO 驱动能力强。

### 架构变更
```
改造前: PSoC pump_on(0) → rt_pin_write(P11.3, HIGH)
改造后: PSoC pump_on(0) → uart5_send_cmd(0xA5 01 param 0x5A)
                           → STM32 USART1 RX 中断 → Airbag_ParseCmdByte()
                           → HAL_GPIO_WritePin(PB12, SET)
```

### 通信协议（PSoC→STM32，复用 USART1 双向）
- 帧格式: `0xA5 + CMD + PARAM + 0x5A`（4 字节固定）
- PARAM = 充气时间 × 100ms（param=0 表示常开至显式关闭）

### STM32 端（PressureSensor 项目）
- [x] Task S1: 新建 `airbag_driver.h` + `airbag_driver.c` — 命令状态机 + GPIO 原语 + 定时关泵
- [x] Task S2: `gpio.h` 加宏定义 PB12-15/PD8 + `gpio.c` 初始化 + 改 PB12/13 初始 LOW
- [x] Task S3: `usart.c` USART1 RX 中断使能 + NVIC
- [x] Task S4: `stm32f1xx_it.c` 加 USART1_IRQHandler
- [x] Task S5: `r60abd1.c` HAL_UART_RxCpltCallback 追加 USART1 分支
- [x] Task S6: `main.c` 调用 AirbagDriver_Init()
- [x] Task S7: `pressure_sensor.c` 加 Airbag_CheckTimers() 轮询
- **Status:** complete

### PSoC 端（Edgi_Talk_M55_LVGL 项目）
- [x] Task P1: `airbag_control.c` 加 uart5_send_cmd() + 改造 pump_on/off、valve 控制
- [x] Task P2: `airbag_control.c` 移除 GPIO 初始化代码（rt_pin_mode/Cy_GPIO_SetDriveSel）
- [x] Task P3: `airbag_control.h` 移除 GPIO 引脚宏和 board.h
- [x] Task P4: `test_airbag.c` 改造（吸气泵测试时切电磁阀）
- **Status:** complete

### 验证
- [ ] V1: 串口助手发 A5 01 32 5A → 验证 PB12 5s 自动关
- [ ] V2: 发 A5 05 00 5A + A5 04 00 5A ... → 验证放气流程
- [ ] V3: PSoC LCD Test 按钮 → STM32 GPIO 依次动作
- [ ] V4: 完整联调
- **Status:** complete（已验证通过后完成）

---

## Phase 9: 状态机/测试/决策全部迁移到 STM32

### 动机
Phase 8 只迁移了 GPIO 执行层，状态机逻辑仍在 M55 端。M55 需要跟踪泵时间/NVM/超时保护。
本阶段将完整状态机、充气阶段管理、NVM 持久化、紧急停止、泵超时保护全部迁入 STM32。

### 核心设计原则
1. **STM32 没有任何自动跳转**——每个动作完成只报 `_DONE`，等 M55 发下一帧
2. **所有状态跳转都在 M55 决策**——M55 有全局姿势/鼾声信息
3. **如果当前状态有 `_DONE` 版本（即动作中状态），M55 必须等到 `_DONE** 才发下一帧。`IDLE` 和 `COMFORT` 是稳态无 `_DONE`，可直接发命令
4. **放气时长由 STM32 内部毫秒级变量计算**——`= inflate_elapsed[0]+inflate_elapsed[1]+inflate_elapsed[2]`，保底 3s

> 有 `_DONE` 的状态（必须等 DONE）：COMFORT_INFLATE / INFLATING_* / HOLDING / DEFLATING / TEST_MODE
> 无 `_DONE` 的状态（可直接发）：IDLE / COMFORT

### 架构变更
```
Phase 8 (当前):
  M55(状态机+决策+泵追踪+NVM+安全) ──单泵控制帧──▶ STM32(GPIO执行)

Phase 9 (目标):
  M55(姿势/鼾声去抖+决策+发SET_STATE) ──SET_STATE(1B)──▶ STM32(纯执行+反馈)
                                       ◀──byte[262-267]── (200ms实时反馈)
```

### 通信协议

#### 下行 (M55 → STM32, UART5)
```
帧格式: 0xA5 + STATE_ID + 0x00 + 0x5A  (4B)
```

#### 上行 (STM32 → M55, 捎带在 0xAA 压力帧字节 262-267, 每 200ms 更新)
```
byte[262] = current_state  (0x00-0x13, 含_DONE)
byte[263] = left_bag_sec   (0-255, 实时更新)
byte[264] = mid_bag_sec    (0-255, 实时更新)
byte[265] = right_bag_sec  (0-255, 实时更新)
byte[266] = flags          (bit0=test_active, bit1=error)
byte[267] = reserved       (0x00)
```

### STATE_ID 定义（20 种，含 _DONE）

```
0x00 = IDLE                        (稳态, 全关)
0x01 = COMFORT_INFLATE             (动作中: 充5/3/5)
0x02 = COMFORT_INFLATE_DONE        (充完, 泵已关)
0x03 = COMFORT                     (稳态, 气囊有气)
0x04 = INFLATING_SUPINE            (动作中: 0/0/5)
0x05 = INFLATING_SUPINE_DONE
0x06 = INFLATING_LEFT_HEAD         (动作中: 5/0/0)
0x07 = INFLATING_LEFT_HEAD_DONE
0x08 = INFLATING_RIGHT_HEAD        (动作中: 0/0/5)
0x09 = INFLATING_RIGHT_HEAD_DONE
0x0A = INFLATING_LEFT_LATERAL      (动作中: 0/0/5)
0x0B = INFLATING_LEFT_LATERAL_DONE
0x0C = INFLATING_RIGHT_LATERAL     (动作中: 5/0/0)
0x0D = INFLATING_RIGHT_LATERAL_DONE
0x0E = HOLDING                     (动作中: 计时5s)
0x0F = HOLDING_DONE                (5s到)
0x10 = DEFLATING                   (动作中: 放气)
0x11 = DEFLATING_DONE              (放完, 泵阀已关)
0x12 = TEST_MODE
0x13 = TEST_MODE_DONE
```

### 转换关系（所有跳转均由 M55 发 SET_STATE 触发）

```
场景 A: 空闲 → 侧卧舒适 → 保持 → 鼾声干预
─────────────────────────────────────────────────────────────────
M55 发什么              STM32 byte[262]      M55 决策条件
─────────────────────────────────────────────────────────────────
SET_STATE(0x01)         0x01(COMFORT_INFLATE)         姿势=侧卧
—                       0x02(DONE)                   充完
SET_STATE(0x03)         0x03(COMFORT)                 继续侧卧
—                       0x03                         鼾声确认
SET_STATE(0x10)         0x10(DEFLATING)               
—                       0x11(DONE)                   放完
SET_STATE(0x0A)         0x0A(INFLATING_LEFT_LATERAL)  侧卧+鼾声
—                       0x0B(DONE)                   充完
SET_STATE(0x0E)         0x0E(HOLDING)                 计时5s
—                       0x0F(DONE)                   5s到
SET_STATE(0x10)         0x10(DEFLATING)               
—                       0x11(DONE)                   放完
SET_STATE(0x01)         0x01(COMFORT_INFLATE)         恢复舒适

场景 B: 舒适中姿势离开侧卧（COMFORT 无 _DONE，可直接发）
SET_STATE(0x10)         0x10(DEFLATING)               姿势→非侧卧
—                       0x11(DONE)                   等放完
SET_STATE(0x00)         0x00(IDLE)

场景 C: 充气中姿势变化（COMFORT_INFLATE 有 _DONE，必须等充完）
SET_STATE(0x01)         0x01(COMFORT_INFLATE)         姿势→非侧卧
—                       0x01                         M55看到姿势变了
—                       0x02(DONE)                   但必须等到DONE
SET_STATE(0x10)         0x10(DEFLATING)               现在才放气
—                       0x11(DONE)
SET_STATE(0x00)         0x00(IDLE)

场景 D: 任何状态→急停/复位
SET_STATE(0x00)         0x00(IDLE)

场景 E: 测试模式
SET_STATE(0x12)         0x12(TEST_MODE)
—                       0x13(DONE)
SET_STATE(0x00)         0x00(IDLE)

场景 F: HOLDING中鼾声清除
—                       0x0E(HOLDING)                M55看到鼾声清
SET_STATE(0x0E)         0x0E(HOLDING)                不发DEFLATING!
                                                      等DONE再说
—                       0x0F(DONE)                   5s到
SET_STATE(0x10)         0x10(DEFLATING)               此时才放气
```

### 放气时长计算（STM32 内部）
```
收到 DEFLATING 命令:
  停所有泵, 阀切吸气
  读取内部 inflate_elapsed[3] (毫秒级, 每100ms累加)
  deflate_ms = inflate_elapsed[0] + inflate_elapsed[1] + inflate_elapsed[2]
  if (deflate_ms < 3000) deflate_ms = 3000
  开吸气泵, 计时 deflate_ms
  到点停泵, 阀回充气, current_state = DEFLATING_DONE
```

### M55 看门狗复位恢复流程
```
M55 复位后:
  1. 发 SET_STATE(0x00) → 确保STM32知道M55重启
  2. 等待首帧 0xAA, 读 byte[262]:

  byte[262]                     M55 动作
  ────────────────────────      ──────────────────────
  IDLE / 任何_DONE              无需放气, 直接决策下一步
  COMFORT_INFLATE /              发 SET_STATE(0x10)
  INFLATING_* / HOLDING /        等 DEFLATING_DONE
  DEFLATING                      等 DEFLATING_DONE (已经在放)
  COMFORT                        看姿势: 侧卧→留着, 非侧卧→DEFLATING
```

### STM32 端（PressureSensor 项目）

- [ ] **Task S9.1 — 协议层改造**
  - [ ] S9.1.1: `airbag_driver.h` 加 STATE_ID 枚举 + current/target_state 变量 + NVM API
  - [ ] S9.1.2: `airbag_driver.c` 加 SET_STATE 解析框架 + current/target 更新逻辑
  - [ ] S9.1.3: `pressure_sensor.c` 字节262-265回写气囊状态
  - [ ] S9.1.4: `PressureSensor.sct` ER_IROM1 0x00080000→0x0007F800
- **Status:** pending

- [ ] **Task S9.2 — 状态机执行体移植（STM32 纯执行，不做跳转决策）**
  - [ ] S9.2.1: 移植充气配置表(6种模式)到 airbag_driver.c
  - [ ] S9.2.2: 移植 2泵并发充气阶段管理 (inflate_phase_step)
  - [ ] S9.2.3: 各动作执行: COMFORT_INFLATE / INFLATING_* / HOLDING 计时 / DEFLATING
  - [ ] S9.2.4: 动作完成后设 current_state = _DONE（不做自动跳转）
  - [ ] S9.2.5: 移植 泵超时保护(10s) + 紧急停止
  - [ ] S9.2.6: 移植 放气时间计算(=内部inflate_elapsed和+3s保底)
  - [ ] S9.2.7: 实现 NVM (内部Flash, 2-slot磨损均衡)
- **Status:** pending

### PSoC 端（Edgi_Talk_M55_LVGL 项目）

- [ ] **Task P9.3 — M55 端简化（决策 + UI）**
  - [ ] P9.3.1: `airbag_control.h` 清理无用 API（泵宏/NVM宏/pump/valve声明删掉）
  - [ ] P9.3.2: `airbag_control.c` 删减 ~500行 + 加 state_machine_decision() 决策函数
  - [ ] P9.3.3: `main.c` `airbag_process()` 改为决策→发SET_STATE→等_DONE→发下一帧
  - [ ] P9.3.4: UART5 RX 解析 0xAA 帧字节262-265（可集成到 airbag_control.c）
  - [ ] P9.3.5: `test_airbag.c` 改为发 SET_STATE(TEST_MODE)，等TEST_MODE_DONE
  - [ ] P9.3.6: `ui_pressure.c` 改用帧反馈数据替代 g_airbag_state_ms[]
- **Status:** pending

### 验证
- [ ] **Task V1:** 串口助手发各 SET_STATE → STM32 状态跳转 + GPIO 验证
- [ ] **Task V2:** 完整联调（M55 决策 + STM32 纯执行 + 双向通信）
- [ ] **Task V3:** LCD UI 显示验证（状态名 + byte[263-265] 秒数）
- [ ] **Task V4:** 异常测试（M55 复位发 IDLE + 读帧恢复、STM32 断电 NVM 恢复）
- **Status:** pending

## Notes
- Update phase status as you progress: pending -> in_progress -> complete
- Log ALL errors
- Never repeat a failed action
