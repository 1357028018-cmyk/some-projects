# Edgi-Talk 睡姿监测系统 — 上位机文档

> 云服务器: 8.136.125.241 (阿里云 ECS, 2核2G, Ubuntu 22.04)
> 项目路径: `/opt/shangweiji/`
> 技术栈: Node.js 18 + Express + Socket.IO + SQL.js (WASM SQLite) + Chart.js

---

## 1. 系统架构

```
┌──────────────────────────────────────────────────────────────┐
│  STM32 (压力传感器 16×16)                                    │
│     │ UART5 115200bps, 268字节/帧                            │
│     ▼                                                        │
│  M55 应用核 (板子端)                                         │
│     │                                                        │
│     ├── 压力数据 → 闭运算滤波 → AI推理 (7类睡姿)              │
│     ├── LVGL 显示: 温湿度 / 压力热力图 / WiFi管理             │
│     └── WiFi TCP → 发送数据到云服务器 :8888                  │
│                                                              │
│     ▼ WiFi                                                    │
│  ☁️ 云服务器 8.136.125.241                                    │
│     │                                                        │
│     ├── tcp-server.js :8888 ← 接收板子数据                   │
│     ├── database.js (SQLite) → sleep_data.db                 │
│     ├── server.js :3000  ← Web 服务 + REST API              │
│     └── public/index.html → 浏览器/手机访问                  │
│                                                              │
│     ▼                                                        │
│  📱 手机浏览器 → http://8.136.125.241:3000                  │
│     ├── Today 标签: 实时睡姿 / 今日统计 / 图表               │
│     ├── Week 标签:  7天趋势图 / 每日摘要                      │
│     ├── Month 标签: 月份日历 (可翻页) / 月度统计              │
│     └── Report 标签: AI睡眠分析报告                           │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. 板子端功能清单 (M55)

| 功能 | 文件 | 状态 |
|------|------|------|
| 接收 STM32 压力数据 (UART5) | `main.c` | ✅ |
| 3×3 闭运算滤波 | `main.c:close_filter_3x3()` | ✅ |
| AI 推理 7 类睡姿 | `model.h:IMPressure_compute()` | ✅ |
| 10帧多数投票去抖动 | `main.c:posture_vote_update()` | ✅ |
| LVGL 主界面 (顶栏温湿度 + 三按钮) | `ui_main.c` | ✅ |
| Pressure 热力图 | `ui_pressure.c` | ✅ |
| WiFi 连接 (GUI 输入 SSID/密码) | `wifi_gui.c` | ✅ |
| WiFi 记忆 (保存到 Flash, 断电不丢) | `wifi_saved.c` | ✅ |
| WiFi 一键连接 / Save / Delete | `wifi_gui.c` | ✅ |
| TCP 发送数据到云服务器 | `wifi_tcp_sender.c` | ✅ |
| IPC 从 M33 核接收温湿度 | `main.c:ipc_sensor_entry()` | ✅ |
| IP Config 按钮改服务器地址 | `ip_gui.c` | ✅ |

### AI 模型 7 类输出

| 索引 | 标签 | 含义 |
|------|------|------|
| 0 | unlabeled | 未标记 |
| 1 | supine | 仰卧 |
| 2 | right_lateral | 右侧卧 |
| 3 | right_head | 右侧头 |
| 4 | left_lateral | 左侧卧 |
| 5 | left_head | 左侧头 |
| 6 | noise | 噪声 |

---

## 3. 云服务器部署

### 硬件信息

| 项目 | 值 |
|------|-----|
| 云服务器 IP | `8.136.125.241` |
| 厂商 | 阿里云 ECS |
| 配置 | 2 vCPU, 2GiB RAM, 40GB ESSD |
| 操作系统 | Ubuntu 22.04 LTS |
| Web 端口 | `3000` (公网可访问) |
| TCP 端口 | `8888` (板子数据接收) |

### 部署位置

```
/opt/shangweiji/
├── server.js           ← Express + Socket.IO Web 服务主文件
├── tcp-server.js       ← TCP 服务器 (接收板子数据)
├── database.js         ← SQLite 数据库操作
├── package.json
├── node_modules/
├── public/
│   ├── index.html      ← Web 前端 (Nocturne & Dawn 主题)
│   └── chart.umd.min.js
├── sleep_data.db       ← 数据库文件 (自动创建)
├── SETUP.md            ← 本文档
└── hlppdat_head_deepcraft/  ← AI 训练数据 (不参与运行)
```

### 首次部署步骤

```bash
# 1. SSH 登录
ssh root@8.136.125.241

# 2. 安装 Node.js
apt update
apt install -y nodejs npm
node -v   # 应 v18+

# 3. 上传代码 (在本地 PowerShell)
scp -r C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\shangweiji root@8.136.125.241:/opt/

# 4. 安装依赖
cd /opt/shangweiji
npm install

# 5. 配置 pm2 守护进程
npm install -g pm2
pm2 start server.js --name "sleep-monitor"
pm2 save
pm2 startup           # 执行后按提示运行那行命令，使开机自启

# 6. 配置阿里云安全组 (入方向)
#    TCP 8888 → 0.0.0.0/0  (板子连接)
#    TCP 3000 → 0.0.0.0/0  (Web 页面)
```

### 日常管理

```bash
pm2 status                 # 查看运行状态
pm2 log sleep-monitor      # 查看实时日志
pm2 restart sleep-monitor  # 重启
pm2 stop sleep-monitor     # 停止
```

---

## 4. 代码更新方法

### 修改本地的三个文件后，更新到云服务器

```powershell
# 在本地 PowerShell 中执行 (替换路径为你实际的路径)
scp "C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\shangweiji\database.js" root@8.136.125.241:/opt/shangweiji/
scp "C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\shangweiji\server.js" root@8.136.125.241:/opt/shangweiji/
scp "C:\RT-ThreadStudio\workspace\Edgi_Talk_M55_LVGL\tools\shangweiji\public\index.html" root@8.136.125.241:/opt/shangweiji/public/
```

然后 SSH 登录重启：

```powershell
ssh root@8.136.125.241

# 在服务器中
pm2 restart sleep-monitor
pm2 log sleep-monitor
```

看到 `Web service started` 即成功。手机刷新 `http://8.136.125.241:3000` 查看。

---

## 5. REST API 清单

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/stats/today` | 今日睡姿统计 |
| GET | `/api/stats/weekly` | 最近 7 天每日统计 + 趋势 |
| GET | `/api/stats/monthly?year=2026&month=5` | 指定月份日历数据 (month 0-indexed) |
| GET | `/api/records/recent?limit=50` | 最近 N 条记录 |
| GET | `/api/status` | 连接状态 |
| GET | `/api/network` | 服务器网络地址 |
| GET | `/api/sessions` | 睡眠会话列表 |
| GET | `/api/sessions/:id/stats` | 指定会话统计 |
| POST | `/api/sessions/new` | 新建睡眠会话 |
| GET | `/api/report/latest?type=weekly&label=...` | 获取缓存的 AI 报告 |
| POST | `/api/report/generate` | 生成 AI 报告 `{ periodType: 'weekly'|'monthly' }` |

### 常用测试命令 (在服务器上)

```bash
# 测试每周统计
curl http://localhost:3000/api/stats/weekly | python3 -m json.tool

# 测试月历数据 (6月 = month=5)
curl "http://localhost:3000/api/stats/monthly?year=2026&month=5" | python3 -m json.tool
```

---

## 6. 数据库 Schema

文件: `/opt/shangweiji/sleep_data.db` (sql.js, WASM 版 SQLite)

### sleep_records 表 (原始睡姿记录)

| 列 | 类型 | 说明 |
|----|------|------|
| id | INTEGER | 主键自增 |
| session_id | INTEGER | 所属会话 |
| timestamp | TEXT | ISO 时间戳 |
| posture | TEXT | 睡姿标签 (supine/left_lateral/...) |
| confidence | REAL | 置信度 (0-1) |
| prob_unlabeled | REAL | 7 个概率值 |
| prob_supine | REAL | ... |
| prob_left_lateral | REAL | ... |
| prob_right_lateral | REAL | ... |
| prob_prone | REAL | ⚠️ 旧字段, 模型实际无 prone |
| prob_noise | REAL | ... |
| inference_count | INTEGER | AI 推理计数 |

> ⚠️ 已知问题: 模型实际输出 7 类 (含 right_head/left_head), 但表中有 prob_prone 列, 缺少 prob_right_head/prob_left_head。`insertRecord` 中 outputs[4] 写入 prob_prone 为错误映射, 但不影响睡姿统计 (posture 字段正确)。

### sleep_sessions 表 (睡眠会话)

| 列 | 类型 | 说明 |
|----|------|------|
| id | INTEGER | 主键自增 |
| start_time | TEXT | 会话开始时间 |
| end_time | TEXT | 会话结束时间 |
| total_records | INTEGER | 记录总数 |
| notes | TEXT | 备注 |

### ai_reports 表 (AI 分析报告缓存)

| 列 | 类型 | 说明 |
|----|------|------|
| id | INTEGER | 主键自增 |
| period_type | TEXT | 'weekly' / 'monthly' |
| period_label | TEXT | 'week_{start}_{end}' / 'month_{start}_{end}' |
| report_content | TEXT | AI 返回的报告正文 |
| score | INTEGER | 综合评分 0-100 |
| created_at | TEXT | 生成时间 |

---

## 7. AI 分析功能

### 原理

```
用户点击 [生成报告] → 服务器聚合睡眠数据
    → 构造 Prompt → 调用 DeepSeek API
    → 解析返回 → 缓存到 ai_reports 表 → 返回给前端
```

### 配置 DeepSeek API Key

在云服务器上：

```bash
export AI_API_KEY="sk-你的key"
pm2 restart sleep-monitor --update-env
pm2 save
```

验证是否配置成功：

```bash
curl -X POST http://localhost:3000/api/report/generate \
  -H "Content-Type: application/json" \
  -d '{"periodType":"weekly"}'
```

返回 `AI_API_KEY not configured` → 未配置成功  
返回 `{ "content": "...", "score": 85 }` → 配置成功

---

## 8. 前端页面 (Nocturne & Dawn)

| 特性 | 说明 |
|------|------|
| 暗色主题 | 深午夜蓝, 靛蓝卡片, 冷蓝强调 |
| 亮色主题 | 暖奶油, 白卡片, 鼠尾草绿强调 |
| 切换方式 | 右上角 🌙/☀️ 按钮 |
| 持久化 | 偏好存 `localStorage` |
| 响应式 | 手机/平板/PC 自适应 |

---

## 9. 已知问题与技术债

| 类别 | 问题 | 影响 |
|------|------|------|
| **数据库** | `sleep_records` 表有 `prob_prone` 列但模型无此输出 | 不影响主要功能, posture 字段正确 |
| **数据库** | `_save()` 每次插入全量写盘, 大数据量时性能差 | 建议迁移到 better-sqlite3 或定期归档 |
| **数据库** | `confidence/100` 无范围校验 | 如果板子传来的已经是小数会导致统计错误 |
| **连接** | TCP 半开连接导致断开后状态不及时更新 | 可加心跳超时检测 |
| **信号** | 未处理 SIGTERM | Docker 或 systemctl restart 时数据库不优雅关闭 |
| **安全** | API 无鉴权, 数据对外公开 | 内网使用可忽略 |
| **API** | `/api/records/recent` 无最大 limit 限制 | 可加 Math.min(limit, 200) |
| **前端** | `setInterval(loadTodayStats, 2000)` 始终轮询 | 切换标签页后浪费请求 |
| **前端** | 月历周日开始, 中文习惯周一 | 可配置 |
