# Progress Log

## Session: 2026-06-18

### Phase 1: Requirements & Discovery
- **Status:** complete
- **Started:** 2026-06-18
- Actions taken:
  - 探索项目结构，确认现有 GPIO、PWM、AI 睡姿检测能力
  - 发现 P11.3 SMART_IO 冲突，与用户确认移除
  - 确认用户需求：3 充气泵 + 1 吸气泵 + 电磁阀
  - 确认物理原理：充左→头向右，充右→头向左
  - 确认状态机 6 状态设计
  - 确认 2 泵并发限制、放气自适应公式
- Files created/modified:
  - (探索阶段，无文件修改)

### Phase 2: Planning & Structure
- **Status:** complete
- **Started:** 2026-06-18
- Actions taken:
  - 设计 GPIO 分配表
  - 设计各姿势充气配置表（舒适模式 + 止鼾干预 + 侧卧+打鼾）
  - 设计状态机流程图（含 2 泵限制下 3 袋并行充气时序）
  - 设计软 PWM 实现方案（100Hz，全速时等同 GPIO 开关）
  - 用户审批通过完整设计方案
  - 创建 task_plan.md / findings.md / progress.md
- Files created/modified:
  - task_plan.md (created)
  - findings.md (created)
  - progress.md (created)

### Phase 3: Implementation
- **Status:** complete
- **Started:** 2026-06-18
- Actions taken:
  - [x] Task 0: 重写 posture_vote_update() 为批处理窗口 (10帧一批, 投票后清空)
  - [x] Task 0: 添加 posture_get_majority_index() 公共接口供气囊模块读取
  - [x] Task 1: 创建 airbag_control.h (GPIO宏/时间宏/枚举/API)
  - [x] Task 2: 创建 airbag_control.c (软件PWM/状态机/2泵限制/上电放气)
  - [x] Task 2: 重构 inflate_phase_step() 使用 g_pump_elapsed 跟踪各泵已充气时间
  - [x] Task 3: 移除 main.c 中 SMART_IO 相关全部代码
  - [x] Task 3: 在 main() 中添加 airbag_system_init() 调用
  - [x] Task 3: 主循环改为 100ms 周期, 调用 airbag_process()
  - [x] Task 4: 确认 SConscript 使用 Glob('*.c') 自动包含新文件
  - [x] Task 4: 确认所有 SMART_IO 引用已清除
  - [x] Spec Self-Review: 17项设计需求全部覆盖
- Files created/modified:
  - applications/main.c (modified: 投票机制+SMART_IO移除+气囊集成)
  - applications/airbag_control.h (created)
  - applications/airbag_control.c (created)
  - task_plan.md (updated)
  - findings.md (updated)
  - progress.md (updated)

### Phase 5: UI Enhancement - 气囊状态图标
- **Status:** complete
- **Started:** 2026-06-18
- Actions taken:
  - 确认屏幕分辨率 800x480 (横屏)
  - 发现原热力图 350x542 超出屏幕高度
  - 缩小热力图 CELL_W=15 CELL_H=24 (保持5:8比例), 新尺寸 255x399
  - 改为横向布局: 左侧热力图 + 右侧信息区
  - 添加枕头外框 (200x150, 圆角, 浅灰背景)
  - 添加3个气囊小长方形 (50x120, 水平排列)
  - 实现颜色映射: 灰(0s)/黄(<3s)/橙(<5s)/绿(5s)
  - 添加气囊标签 L/M/R + 充气秒数
  - 添加状态机状态文字显示
  - 添加颜色图例说明
  - 每100ms定时器更新气囊颜色和状态
- Files created/modified:
  - applications/ui_pressure.c (modified: 气囊图标+布局调整)

## 2026-06-24: STM32 雷达数据聚合改为 0xBB TLV 合并帧
- **r60abd1.h**: 新增 `radar_agg_t` 结构体、`R60_WIN_SIZE`/`R60_COMBINED_MAX`/`RADAR_TX_INTERVAL` 宏
- **r60abd1.c**: 重写 `R60_ParseByte()` 帧接收分支 → 按 ctrl/cmd 提取字段到 `g_radar`
  - 0x85 0x02 → hr; 0x81 0x02 → br; 0x80 0x03→motion; 0x80 0x04→dist; 0x80 0x05→exist+param1+param2
  - 删除透传帧缓冲(g_pending_frame/R60_BuildFrame/R60ABD1_SendPendingFrame/R60ABD1_GetFrame)
  - 新增 `tlv_put()`、窗口统计函数(calc_win_max/avg/min)、`R60ABD1_BuildCombinedFrame()`、`R60ABD1_ResetWindow()`
- **pressure_sensor.c**: `R60ABD1_SendPendingFrame()` → 每 15 周期 (3s) 构建并发 0xBB TLV 合并帧

## 2026-06-25: GPIO 控制下放到 STM32（架构变更）

### 背景
之前所有泵/阀控制由 PSoC M55 直接 GPIO 完成，但 PSoC GPIO 驱动能力不足。
STM32（PressureSensor 项目）USART1 空闲 RX 通道，且 GPIO 驱动能力强。

### 变更内容
- **STM32 端新增**：`airbag_driver.c/h` 命令帧状态机 + GPIO 控制（PB12-15/PD8）
- **STM32 端修改**：USART1 启用 RX 中断（`HAL_UART_Receive_IT`）
- **PSoC 端改造**：`pump_on/off()` 由 GPIO 直写改为 UART5 发送 4 字节命令帧
- **test 保留**：`test_airbag.c` 改为发命令帧；吸气泵测试时切电磁阀
- **airbag_control.h**：移除 GPIO 宏和 `<board.h>`，添加协议说明注释
- **airbag_control.c**：
  - 移除 `g_pump_pins[]`、`rt_pin_mode/write`、`Cy_GPIO_SetDriveSel`
  - 新增 `uart5_send_cmd()` — 通过 `rt_device_find("uart5")` + `rt_device_write()` 发送
  - `pump_on/off()` → 映射到命令码 `0x01-0x04/0x08-0x0B`
  - `valve_to_suction/inflate()` → 发送 `0x05/0x06`
  - `emergency_stop()` → 发送 `0x07` + 清状态
  - `airbag_system_init()` → 先发 `0x07` 确保关断；移除 GPIO 初始化代码
  - NVM 和状态机逻辑完全保留
- **test_airbag.c**：
  - `test_step()`: 吸气泵测试前调用 `valve_to_suction()`，测试后调用 `valve_to_inflate()`
  - GPIO 信息更新为 STM32 PB12-15/PD8

### 通信协议
```
PSoC → STM32: 0xA5 + CMD + PARAM + 0x5A (4B)
Command: 0x01-03 充气泵ON, 0x04 吸气泵ON, 0x05-06 电磁阀, 0x07 ALL_OFF, 0x08-0B 各泵OFF
```

### Files created/modified:
- PressureSensor/Core/Inc/airbag_driver.h (created)
- PressureSensor/Core/Src/airbag_driver.c (created)
- PressureSensor/Core/Inc/gpio.h (modified)
- PressureSensor/Core/Src/gpio.c (modified)
- PressureSensor/Core/Src/usart.c (modified)
- PressureSensor/Core/Src/stm32f1xx_it.c (modified)
- PressureSensor/Core/Src/r60abd1.c (modified)
- PressureSensor/Core/Src/main.c (modified)
- PressureSensor/Core/Src/pressure_sensor.c (modified)
- Edgi_Talk_M55_LVGL/applications/airbag_control.c (modified)
- Edgi_Talk_M55_LVGL/applications/airbag_control.h (modified)
- Edgi_Talk_M55_LVGL/applications/test_airbag.c (modified)

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| (尚未测试) | | | | |

## Error Log
| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
| (无错误) | | 1 | |

## 5-Question Reboot Check
| Question | Answer |
|----------|--------|
| Where am I? | Stabilization & Review - 最终调整 |
| Where am I going? | 用户确认行为后收尾 |
| What's the goal? | GPIO 气泵控制 + 6状态止鼾状态机实现 |
| What have I learned? | See findings.md |
| What have I done? | See above |

## 2026-06-18 第三轮: Stabilization & Testbench
- **PUMP_MAX_RUN_MS**: 15s→10s (仅监测充气泵)
- **ST_INFLATING**: 移除姿势中断, 干预过程完整执行
- **稳态入口**: 返回 IDLE/COMFORT 时重置姿势去抖
- **test_airbag.c**: 10场景自动化测试
  - 1.上电安全 2.仰卧打鼾 3.左侧头打鼾 4.侧卧舒适 5.侧卧+鼾声恢复
  - 6.侧卧→离开 7.干预不中断 8.noise不干预 9.放气自适应 10.超时安全
- **test mode API**: main()测试模式跳过, 由testbench接管

Files modified:
- airbag_control.h: PUMP_MAX_RUN_MS 10s + test mode API
- airbag_control.c: test mode impl + pump safety scoped to inflation pumps
- main.c: test mode check in main loop
- test_airbag.c: (created) full testbench
- test_airbag.h: (created) airbag_test_run() 公共API
- ui_pressure.c: Test按钮(气囊示意图右侧, 蓝色70x40)
- task_plan.md: phase 5 updated
- progress.md: this entry

## 2026-06-18 第五轮: LCD按钮触发测试
- test_airbag.c: 添加 airbag_test_run() 独立线程启动
- test_airbag.h: (created) 公共API声明
- ui_pressure.c: 气囊示意图右侧加蓝色"Test"按钮

## 2026-06-18 第六轮: Testbench修复+扩展
- 修复3个FAIL: 去抖off-by-one(5→6), GPIO检查时机, 安全超时重置
- 新增7个场景(10→17):
   11.右转头对称 12.右侧卧对称 13.HOLDING提前退出
   14.背靠背鼾声 15.unlabeled不干预 16.2泵限制验证 17.翻身去抖压力测试
   - 按钮回调调用 airbag_test_run() 创建线程
   - 不阻塞LVGL UI线程

## 2026-06-26: Phase 9 规划 — 状态机全部迁移到 STM32

### 决策
把 Phase 8 未完成的"完整迁移"补完：状态机、充气阶段管理、NVM、超时保护、紧急停止、测试序列全部从 M55 移入 STM32。

### 最终确认的设计原则
1. **STM32 纯执行 + 反馈**——收到 SET_STATE 做动作，做完报 `_DONE`，**没有任何自动跳转**
2. **M55 纯决策**——姿势/鼾声去抖 + 等 `_DONE` + 决定下一帧发什么
3. **M55 必须等到 `_DONE` 才发下一帧**——HOLDING 也不打断，跑满 5s
4. **放气时长 STM32 内部毫秒级计算**——`= inflate_elapsed[0]+inflate_elapsed[1]+inflate_elapsed[2]` 保底 3s
5. **帧里不传放气参数**——STM32 自己算

### 协议
- **下行**: `0xA5 + STATE_ID(1B) + 0x00 + 0x5A`
- **上行**: 0xAA 压力帧 byte[262]=current_state(含_DONE), byte[263-265]=各气囊秒数(200ms实时更新)

### 20 种 STATE_ID (含 _DONE)
```
IDLE / COMFORT_INFLATE(+DONE) / COMFORT /
INFLATING_SUPINE(+DONE) / INFLATING_LEFT_HEAD(+DONE) / INFLATING_RIGHT_HEAD(+DONE) /
INFLATING_LEFT_LATERAL(+DONE) / INFLATING_RIGHT_LATERAL(+DONE) /
HOLDING(+DONE) / DEFLATING(+DONE) / TEST_MODE(+DONE)
```

### 拆分边界
| 留在 M55 | 下放 STM32 |
|----------|-----------|
| 姿势检测(Imagimob AI) | 泵/阀 GPIO 控制 |
| 鼾声检测(M33 IPC) | 2泵并发充气阶段管理 |
| 姿势去抖(5帧) | 泵超时保护(10s) |
| 鼾声去抖(5帧) | NVM 持久化(内部Flash末页) |
| **状态跳转决策**（等_DONE再发帧） | 充气时间毫秒级跟踪(inflate_elapsed[3]) |
| 气囊 UI 图标（读 byte[263-265]） | 放气时间计算 |
| 发 SET_STATE(X) | 紧急停止 |
| 读 byte[262] 反馈 | 测试序列执行 |
| 看门狗复位恢复（读帧恢复） | 压力帧 byte[262-267] 回写 |

### 文件改动清单
**PressureSensor:**
- `airbag_driver.h` — 加20种STATE_ID枚举 + current/target_state + NVM API
- `airbag_driver.c` — 移植执行逻辑(充气/阶段/HOLDING计时/DEFLATING)+NVM
- `pressure_sensor.c` — 字节262-267回写
- `PressureSensor.sct` — 末页2KB预留NVM

**Edgi_Talk_M55_LVGL:**
- `airbag_control.h` — 清理无用API
- `airbag_control.c` — 删~500行 + 加 state_machine_decision() 决策循环
- `main.c` — airbag_process()改为决策→发SET_STATE→等_DONE
- `test_airbag.c` — 改为发SET_STATE(TEST_MODE)+等TEST_MODE_DONE
- `ui_pressure.c` — 用 byte[262-265] 替代 g_airbag_state_ms[]

### 2026-06-26 修正: _DONE 等待规则
- 修正前原则: "M55 必须等到 `_DONE` 才发下一个命令"（太宽泛，未区分有无 `_DONE` 的状态）
- 修正后精确规则:
  - 有 `_DONE` 的状态（COMFORT_INFLATE/INFLATING_*/HOLDING/DEFLATING/TEST_MODE）→ **必须等 DONE**
  - 无 `_DONE` 的状态（IDLE/COMFORT）→ **可直接发**
- 受影响的场景: COMFORT_INFLATE 中途姿势变化 → 必须等到 COMFORT_INFLATE_DONE 才发 DEFLATING，不能中途打断
- task_plan.md 场景 C 已修正, findings.md 已补充

## 2026-07-05: 上位机功能增强 + 云端部署

### 上位机新增功能
- 压力热力图改为 4:3（400×300）
- 热力图下方新增实时体征面板：心率、呼吸率、体动、打鼾
- 体征面板下方新增 6 个手动控制按钮（L+/M+/R+/L-/M-/R-），通过 TCP 命令控制气囊
- 周/月统计增加「平均心率」「平均呼吸」
- 数据库新增 `radar_records` 表和 `snore_records` 表

### M55 端新增
- 0xBB 雷达帧解析后发送 `[RADAR]` TCP 数据
- IPC 鼾声检测后发送 `[SNORE]` TCP 数据

### 云端部署流程
```
1. 上传修改后的文件到阿里云服务器：
   scp tcp-server.js root@8.136.125.241:/opt/shangweiji/
   scp server.js root@8.136.125.241:/opt/shangweiji/
   scp index.html root@8.136.125.241:/opt/shangweiji/public/

2. SSH 重启服务：
   ssh root@8.136.125.241
   pm2 restart sleep-monitor
   pm2 log sleep-monitor

3. M55 端：
   - wifi_tcp_sender.h 默认 IP 已是 8.136.125.241
   - 重新编译烧录 M55 固件即可
```

### 需要 SCP 上传的文件
上位机能跑在云端，只需上传 3 个 PC 端文件：

| 文件 | 说明 |
|------|------|
| `tcp-server.js` | 新增 `[RADAR]`/`[SNORE]` 解析 + `sendCmd` |
| `server.js` | 新增雷达/鼾声事件 + `airbag_cmd` socket 转发 |
| `public/index.html` | 4:3 热力图 + 体征面板 + 手动按钮 + 周月 HR/BR |

### 不需要上传的文件（只需本地重新编译）
- 所有 STM32 端文件（`PressureSensor/` 项目）
- 所有 M55 端文件（`Edgi_Talk_M55_LVGL/applications/` 目录下文件）
