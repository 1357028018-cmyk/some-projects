const net = require('net');
const EventEmitter = require('events');

/**
 * TCP Server - 接收板子WiFi发来的数据
 * 
 * 数据格式：与UART2输出完全一致
 *   "[AI_DBG] ret=0 out=[...] => posture=supine conf=85% cnt=123\r\n"
 *   "[STAT] F=123 isr=456 B=32856 raw_rem=0\r\n"
 * 
 * 直接复用 SerialReader 的解析逻辑
 */
class TcpServer extends EventEmitter {
  constructor(options = {}) {
    super();
    this.port = options.port || 8888;
    this.host = options.host || '0.0.0.0';
    this.server = null;
    this.clients = new Set();
    this.isRunning = false;
    this.parserLine = '';  // 处理TCP分包
    
    // 复用与 SerialReader 一致的解析状态
    this.pendingOutputs = null;
  }

  start() {
    return new Promise((resolve, reject) => {
      this.server = net.createServer((socket) => this.handleClient(socket));

      this.server.on('error', (err) => {
        console.error(`[TCP-Server] error: ${err.message}`);
        this.emit('error', err);
        reject(err);
      });

      this.server.listen(this.port, this.host, () => {
        this.isRunning = true;
        console.log(`[TCP-Server] listening on ${this.host}:${this.port}`);
        this.emit('started', { host: this.host, port: this.port });
        resolve();
      });
    });
  }

  handleClient(socket) {
    const clientInfo = `${socket.remoteAddress}:${socket.remotePort}`;
    console.log(`[TCP-Server] client connected: ${clientInfo}`);
    this.clients.add(socket);
    this.emit('client_connected', clientInfo);

    socket.on('data', (data) => {
      this.parseData(data);
    });

    socket.on('close', () => {
      console.log(`[TCP-Server] client disconnected: ${clientInfo}`);
      this.clients.delete(socket);
      this.emit('client_disconnected', clientInfo);
    });

    socket.on('error', (err) => {
      console.error(`[TCP-Server] client error ${clientInfo}: ${err.message}`);
    });
  }

  /**
   * 解析 TCP 数据（处理分包、半包）
   * 逐行解析，与 SerialReader.parseLine 行为完全一致
   */
  parseData(data) {
    this.parserLine += data.toString('utf8');

    let idx;
    while ((idx = this.parserLine.indexOf('\r\n')) >= 0) {
      const line = this.parserLine.substring(0, idx);
      this.parserLine = this.parserLine.substring(idx + 2);
      if (line.length > 0) {
        this.parseLine(line);
      }
    }
  }

  parseLine(line) {
    console.log('[TCP-RX]', line.substring(0, 300));
    if (line.includes('[AI_DBG]')) {
      const outMatch = line.match(/out=\[([^\]]+)\]/);
      if (outMatch) {
        const rawValues = outMatch[1].split(',').map(s => parseFloat(s.trim()));
        if (rawValues.length >= 5 && !rawValues.some(isNaN)) {
          this.pendingOutputs = rawValues;
          console.log('[TCP-RX] saved outputs:', rawValues.length, 'values');
        }
      }
    }

    if (line.includes('[PRESS')) {
      const match = line.match(/seq=(\d+)\]\s+(.+)/);
      if (match) {
        const values = match[2].split(',').map(v => parseInt(v.trim(), 10));
        if (values.length === 256 && !values.some(isNaN)) {
          this.emit('pressure', {
            seq: parseInt(match[1], 10),
            timestamp: new Date(),
            data: values
          });
        } else {
          console.warn('[TCP-RX] PRESS validation failed: len=' + values.length +
                       ' NaN=' + values.some(isNaN));
        }
      }
    }

    if (line.includes('=> posture=')) {
      console.log('[TCP-RX] line contains => posture=, trying match...');
      // Match posture (may contain spaces, e.g. "left lateral"), conf, cnt
      const match = line.match(/posture=([\w\s]+?)\s+conf=(\d+)%\s+cnt=(\d+)/);
      if (match) {
        console.log('[TCP-RX] POSTURE MATCHED:', match[1], match[2] + '%', 'cnt=' + match[3]);
        const data = {
          timestamp: new Date(),
          posture: match[1],
          confidence: parseInt(match[2]),
          inferenceCount: parseInt(match[3]),
          outputs: this.pendingOutputs || [],
          source: 'tcp'
        };
        this.pendingOutputs = null;
        this.emit('posture', data);
      }
    }

    if (line.includes('[STAT]')) {
      const match = line.match(/F=(\d+)\s+isr=(\d+)\s+B=(\d+)\s+raw_rem=(\d+)/);
      if (match) {
        this.emit('stats', {
          frames: parseInt(match[1]),
          interrupts: parseInt(match[2]),
          bytes: parseInt(match[3]),
          rawRemaining: parseInt(match[4])
        });
      }
    }

    if (line.includes('[RADAR]')) {
      const match = line.match(/HR=(-?\d+)\s+BR=(-?\d+)\s+motion=(-?\d+)\s+dist=(-?\d+)\s+exist=(0x[0-9A-Fa-f]+)/);
      if (match) {
        this.emit('radar', {
          timestamp: new Date(),
          hr: parseInt(match[1]),
          br: parseInt(match[2]),
          motion: parseInt(match[3]),
          dist: parseInt(match[4]),
          exist: parseInt(match[5])
        });
      }
    }

    if (line.includes('[SNORE]')) {
      const match = line.match(/snore=(\d+)\s+conf=(\d+)%/);
      if (match) {
        this.emit('snore', {
          timestamp: new Date(),
          snore: parseInt(match[1]),
          confidence: parseInt(match[2])
        });
      }
    }
  }

  /**
   * 向下行发送命令（手动控制气囊）
   */
  sendCmd(cmd) {
    for (const c of this.clients) {
      try { c.write(cmd + '\r\n'); } catch (e) { this.clients.delete(c); }
    }
  }

  getClientCount() {
    return this.clients.size;
  }

  stop() {
    return new Promise((resolve) => {
      for (const c of this.clients) {
        c.destroy();
      }
      this.clients.clear();
      if (this.server) {
        this.server.close(() => {
          this.isRunning = false;
          console.log('[TCP-Server] stopped');
          resolve();
        });
      } else {
        resolve();
      }
    });
  }
}

module.exports = TcpServer;
