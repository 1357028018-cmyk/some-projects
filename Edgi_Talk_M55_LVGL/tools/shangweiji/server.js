const express = require('express');
const http = require('http');
const path = require('path');
const { Server } = require('socket.io');
const os = require('os');
const TcpServer = require('./tcp-server');
const SleepDatabase = require('./database');

const CONFIG = {
  port: parseInt(process.env.PORT || '3000'),
  tcpPort: parseInt(process.env.TCP_PORT || '8888')
};

const MAX_PRESSURE_FRAMES = 10;

const app = express();
const server = http.createServer(app);
const io = new Server(server);
const db = new SleepDatabase();

const tcp = new TcpServer({ port: CONFIG.tcpPort, host: '0.0.0.0' });

let latestPosture = null;
let connectionStatus = 'disconnected';

const pressureFrames = [];
let pressureHead = 0;
let pressureCount = 0;

app.use(express.static('public'));
// Serve socket.io client library to the browser
app.get('/socket.io/socket.io.js', (req, res) => {
  const clientPath = path.join(__dirname, 'node_modules', 'socket.io-client', 'dist', 'socket.io.min.js');
  res.sendFile(clientPath, (err) => {
    if (err) {
      console.error('Failed to serve socket.io.js:', err.message);
      res.status(404).end();
    }
  });
});
app.use(express.json());

// =================== REST API ===================

app.get('/api/stats/today', (req, res) => res.json(db.getTodayStats()));

app.get('/api/records/recent', (req, res) => {
  const limit = parseInt(req.query.limit) || 50;
  res.json(db.getRecentRecords(limit));
});

app.get('/api/sessions', (req, res) => res.json(db.getSessions()));

app.get('/api/sessions/:id/stats', (req, res) => {
  res.json(db.getSessionStats(parseInt(req.params.id)));
});

app.post('/api/sessions/new', (req, res) => {
  db.endSession();
  const id = db.startSession();
  res.json({ sessionId: id });
});

app.get('/api/status', (req, res) => {
  res.json({
    connectionStatus: connectionStatus,
    tcpConnected: tcp.getClientCount() > 0,
    tcpClients: tcp.getClientCount()
  });
});

app.get('/api/pressure/frames', (req, res) => {
  const result = [];
  for (let i = 0; i < pressureCount; i++) {
    const idx = (pressureHead - pressureCount + i + MAX_PRESSURE_FRAMES) % MAX_PRESSURE_FRAMES;
    result.push(pressureFrames[idx]);
  }
  res.json(result);
});

app.get('/api/network', (req, res) => {
  const interfaces = os.networkInterfaces();
  const addresses = [];
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family === 'IPv4' && !iface.internal) {
        addresses.push({ interface: name, address: iface.address });
      }
    }
  }
  res.json({ addresses });
});

app.get('/api/stats/weekly', (req, res) => res.json(db.getWeeklyStats()));

app.get('/api/stats/monthly', (req, res) => {
  const y = parseInt(req.query.year);
  const m = parseInt(req.query.month);
  res.json(db.getMonthlyStats(y, m));
});

app.get('/api/report/latest', (req, res) => {
  const type = req.query.type || 'weekly';
  const label = req.query.label || new Date().toISOString().split('T')[0];
  const report = db.getReport(type, label);
  res.json(report || { content: null, score: 0, cached: false });
});

app.post('/api/report/generate', async (req, res) => {
  const { periodType } = req.body;  // 'weekly' or 'monthly'
  let stats, label;

  if (periodType === 'weekly') {
    stats = db.getWeeklyStats();
    label = `week_${stats.days[0].day}_${stats.days[6].day}`;
  } else {
    stats = db.getMonthlyStats();
    label = `month_${stats.days[0].day}_${stats.days[stats.days.length - 1].day}`;
  }

  const cached = db.getReport(periodType, label);
  if (cached) {
    /* 旧缓存（7分类时代）不含"多模态分析"章节，跳过重新生成 */
    if (!cached.content.includes('多模态分析')) {
      console.log(`[Report] Skip stale cache for ${periodType}/${label}, regenerating...`);
    } else {
      return res.json({ ...cached, cached: true });
    }
  }

  const apiKey = process.env.AI_API_KEY;
  if (!apiKey) {
    return res.json({
      content: 'AI_API_KEY not configured. Set environment variable AI_API_KEY with your DeepSeek API key.',
      score: 0, cached: false
    });
  }

  const postureText = stats.summary.postures
    .map(p => `${p.posture}: ${p.percentage}%`)
    .join(', ');

  const dayList = stats.days.slice(0, 7).map(d =>
    `${d.day}: ${d.active ? d.dominant + ' ' + d.dominantPct + '%' : '无数据'}`
  ).join('; ');

  /* 获取多模态数据 */
  let hrText = '无', brText = '无', snoreText = '无';
  if (stats.summary.avgHR > 0) hrText = stats.summary.avgHR + ' bpm';
  if (stats.summary.avgBR > 0) brText = stats.summary.avgBR + ' /min';

  const periodStart = stats.days[0].day + 'T00:00:00';
  const periodEnd = stats.days[stats.days.length - 1].day + 'T23:59:59';
  const snoreStats = db.getPeriodSnoreStats(periodStart, periodEnd);
  if (snoreStats) {
    snoreText = `总打鼾次数 ${snoreStats.totalRecords}次，平均置信度 ${(snoreStats.avgConfidence * 100).toFixed(0)}%`;
  }

  const prompt = `你是一位专业的睡眠健康分析师。请根据以下用户的睡眠监测数据，生成一份中文睡眠分析报告。

数据周期：${stats.days[0].day} 至 ${stats.days[stats.days.length - 1].day}（${stats.days.length}天）
睡姿分布：${postureText}
活跃天数：${stats.summary.activeDays}/${stats.days.length}
总记录数：${stats.summary.totalRecords}

每日详情：${dayList}

体征数据：
- 平均心率：${hrText}
- 平均呼吸率：${brText}
- 打鼾情况：${snoreText}

请按以下结构回复（控制在300字以内）：
1. **综合评分**（只输出一个1-100的整数）
2. **睡姿分析**（解释主要睡姿是否健康，注意：睡姿分析中请忽略打鼾因素，打鼾作为独立指标）
3. **多模态分析**（结合心率、呼吸率、打鼾情况综合分析）
4. **改善建议**（1-3条具体可行的建议）

评分请用格式 "评分: 85"，这样我可以自动解析。`;

  try {
    const aiRes = await fetch('https://api.deepseek.com/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${apiKey}`
      },
      body: JSON.stringify({
        model: 'deepseek-chat',
        messages: [
          { role: 'system', content: '你是一位专业的睡眠健康分析师，用中文回答，简洁专业。' },
          { role: 'user', content: prompt }
        ],
        max_tokens: 600,
        temperature: 0.7
      })
    });

    const aiData = await aiRes.json();
    const content = aiData.choices?.[0]?.message?.content || 'AI返回为空';

    const scoreMatch = content.match(/评分[:\s]*(\d+)/);
    const score = scoreMatch ? parseInt(scoreMatch[1]) : 0;

    db.saveReport(periodType, label, content, score);
    res.json({ content, score, cached: false });
  } catch (err) {
    console.error('AI API error:', err.message);
    res.json({ content: 'AI分析服务暂时不可用，请稍后重试。', score: 0, cached: false });
  }
});

// =================== WebSocket ===================

io.on('connection', (socket) => {
  console.log('Client connected:', socket.id);
  socket.emit('status', {
    connection: connectionStatus,
    latestPosture: latestPosture,
    sessionId: db.getCurrentSessionId()
  });

  socket.on('disconnect', () => {
    console.log('Client disconnected:', socket.id);
  });

  socket.on('airbag_cmd', (cmd) => {
    console.log('[Web] airbag_cmd:', cmd);
    tcp.sendCmd(cmd);
  });
});

// =================== TCP Server Events ===================

tcp.on('client_connected', (info) => {
  console.log(`[TCP] Board connected: ${info}`);
  connectionStatus = 'connected';
  io.emit('status', { connection: connectionStatus });
});

tcp.on('client_disconnected', (info) => {
  console.log(`[TCP] Board disconnected: ${info}`);
  if (tcp.getClientCount() === 0) {
    connectionStatus = 'disconnected';
    io.emit('status', { connection: connectionStatus });
  }
});

tcp.on('posture', (data) => {
  console.log(`[POSTURE] ${data.posture} ${data.confidence}% cnt=${data.inferenceCount}`);
  latestPosture = {
    timestamp: data.timestamp,
    posture: data.posture,
    confidence: data.confidence,
    outputs: data.outputs,
    inferenceCount: data.inferenceCount
  };

  try {
    db.insertRecord(data);
  } catch (err) {
    console.error('DB write failed:', err.message);
  }

  io.emit('posture', latestPosture);
});

tcp.on('radar', (data) => {
  io.emit('radar', data);
  try { db.insertRadarRecord(data); } catch (e) { console.error('radar db:', e.message); }
});

tcp.on('snore', (data) => {
  io.emit('snore', data);
  try { db.insertSnoreRecord(data); } catch (e) { console.error('snore db:', e.message); }
});

tcp.on('stats', (stats) => {
  io.emit('device_stats', stats);
});

tcp.on('pressure', (data) => {
  pressureFrames[pressureHead] = data;
  pressureHead = (pressureHead + 1) % MAX_PRESSURE_FRAMES;
  if (pressureCount < MAX_PRESSURE_FRAMES) pressureCount++;
  io.emit('pressure', data);
});

tcp.on('error', (err) => {
  console.error('[TCP] error:', err.message);
});

// =================== Start ===================

async function start() {
  console.log('\n========================================');
  console.log('  Sleep Monitor v1.0 (WiFi TCP)');
  console.log('========================================\n');

  await db.init();

  try {
    await tcp.start();
  } catch (err) {
    console.error('Failed to start TCP server:', err.message);
  }

  server.listen(CONFIG.port, '0.0.0.0', () => {
    console.log(`Web service started`);
    console.log(`   Local: http://localhost:${CONFIG.port}`);
    const interfaces = os.networkInterfaces();
    for (const name of Object.keys(interfaces)) {
      for (const iface of interfaces[name]) {
        if (iface.family === 'IPv4' && !iface.internal) {
          console.log(`   LAN:   http://${iface.address}:${CONFIG.port}`);
        }
      }
    }
    console.log(`\nTCP listening on: 0.0.0.0:${CONFIG.tcpPort}`);
    console.log(`Waiting for board to connect...\n`);

    /* 每日凌晨 3 点执行一次数据聚合 */
    const scheduleAgg = () => {
      const now = new Date();
      const next = new Date(now.getFullYear(), now.getMonth(), now.getDate() + 1, 3, 0, 0, 0);
      setTimeout(() => {
        db.aggregateOldData();
        scheduleAgg();
      }, next - now);
    };
    db.aggregateOldData();
    scheduleAgg();
  });
}

process.on('SIGINT', () => {
  console.log('\nShutting down...');
  tcp.stop();
  db.close();
  server.close();
  process.exit(0);
});

start();
