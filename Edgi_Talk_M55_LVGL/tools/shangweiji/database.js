const initSqlJs = require('sql.js');
const fs = require('fs');
const path = require('path');

const DB_PATH = path.join(__dirname, 'sleep_data.db');

class SleepDatabase {
  constructor() {
    this.db = null;
    this.currentSessionId = null;
  }

  async init() {
    const SQL = await initSqlJs();

    let fileBuffer = null;
    if (fs.existsSync(DB_PATH)) {
      fileBuffer = fs.readFileSync(DB_PATH);
    }

    if (fileBuffer) {
      this.db = new SQL.Database(fileBuffer);
    } else {
      this.db = new SQL.Database();
    }

    this.db.run(`
      CREATE TABLE IF NOT EXISTS sleep_sessions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        start_time TEXT NOT NULL,
        end_time TEXT,
        total_records INTEGER DEFAULT 0,
        notes TEXT
      )
    `);

    this.db.run(`
      CREATE TABLE IF NOT EXISTS sleep_records (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id INTEGER NOT NULL,
        timestamp TEXT NOT NULL,
        posture TEXT NOT NULL,
        confidence REAL NOT NULL,
        prob_unlabeled REAL DEFAULT 0,
        prob_supine REAL DEFAULT 0,
        prob_left_lateral REAL DEFAULT 0,
        prob_right_lateral REAL DEFAULT 0,
        prob_prone REAL DEFAULT 0,
        prob_noise REAL DEFAULT 0,
        inference_count INTEGER DEFAULT 0
      )
    `);

    this.db.run(`CREATE INDEX IF NOT EXISTS idx_records_session ON sleep_records(session_id)`);
    this.db.run(`CREATE INDEX IF NOT EXISTS idx_records_timestamp ON sleep_records(timestamp)`);
    this.db.run(`CREATE INDEX IF NOT EXISTS idx_records_posture ON sleep_records(posture)`);

    this.db.run(`
      CREATE TABLE IF NOT EXISTS ai_reports (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        period_type TEXT NOT NULL,
        period_label TEXT NOT NULL,
        report_content TEXT NOT NULL,
        score INTEGER DEFAULT 0,
        created_at TEXT NOT NULL
      )
    `);

    this.db.run(`CREATE INDEX IF NOT EXISTS idx_ai_reports_label ON ai_reports(period_type, period_label)`);

    this.db.run(`
      CREATE TABLE IF NOT EXISTS radar_records (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id INTEGER NOT NULL,
        timestamp TEXT NOT NULL,
        heart_rate INTEGER DEFAULT -1,
        breath_rate INTEGER DEFAULT -1,
        motion INTEGER DEFAULT -1,
        distance INTEGER DEFAULT -1,
        exist INTEGER DEFAULT 0
      )
    `);

    this.db.run(`
      CREATE TABLE IF NOT EXISTS snore_records (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id INTEGER NOT NULL,
        timestamp TEXT NOT NULL,
        snore INTEGER DEFAULT 0,
        confidence REAL DEFAULT 0
      )
    `);

    this.db.run(`CREATE INDEX IF NOT EXISTS idx_radar_ts ON radar_records(timestamp)`);
    this.db.run(`CREATE INDEX IF NOT EXISTS idx_snore_ts ON snore_records(timestamp)`);

    this.db.run(`
      CREATE TABLE IF NOT EXISTS radar_daily (
        date TEXT PRIMARY KEY,
        avg_hr INTEGER DEFAULT 0,
        avg_br INTEGER DEFAULT 0,
        max_motion INTEGER DEFAULT 0,
        avg_motion REAL DEFAULT 0,
        record_count INTEGER DEFAULT 0
      )
    `);

    this.db.run(`
      CREATE TABLE IF NOT EXISTS snore_daily (
        date TEXT PRIMARY KEY,
        total_snore INTEGER DEFAULT 0,
        avg_confidence REAL DEFAULT 0,
        record_count INTEGER DEFAULT 0
      )
    `);

    this._save();
    console.log(`Database initialized: ${DB_PATH}`);
  }

  _save() {
    const data = this.db.export();
    const buffer = Buffer.from(data);
    fs.writeFileSync(DB_PATH, buffer);
  }

  startSession() {
    const now = new Date().toISOString();
    this.db.run('INSERT INTO sleep_sessions (start_time) VALUES (?)', [now]);
    const res = this.db.exec('SELECT last_insert_rowid()');
    this.currentSessionId = res[0].values[0][0];
    this._save();
    console.log(`Session started: #${this.currentSessionId}`);
    return this.currentSessionId;
  }

  endSession() {
    if (!this.currentSessionId) return;
    const now = new Date().toISOString();
    this.db.run(
      `UPDATE sleep_sessions SET end_time = ?, total_records = (SELECT COUNT(*) FROM sleep_records WHERE session_id = ?) WHERE id = ?`,
      [now, this.currentSessionId, this.currentSessionId]
    );
    this._save();
    console.log(`Session ended: #${this.currentSessionId}`);
    this.currentSessionId = null;
  }

  insertRecord(data) {
    if (!this.currentSessionId) {
      this.startSession();
    }

    this.db.run(
      `INSERT INTO sleep_records (
        session_id, timestamp, posture, confidence,
        prob_unlabeled, prob_supine, prob_left_lateral,
        prob_right_lateral, prob_prone, prob_noise,
        inference_count
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
      [
        this.currentSessionId,
        data.timestamp.toISOString(),
        data.posture,
        data.confidence / 100,
        data.outputs[0] || 0,
        data.outputs[1] || 0,
        data.outputs[2] || 0,
        data.outputs[3] || 0,
        0,
        data.outputs[4] || 0,
        data.inferenceCount || 0
      ]
    );
    this._save();
  }

  insertRadarRecord(data) {
    if (!this.currentSessionId) this.startSession();
    this.db.run(
      `INSERT INTO radar_records (session_id, timestamp, heart_rate, breath_rate, motion, distance, exist)
       VALUES (?, ?, ?, ?, ?, ?, ?)`,
      [
        this.currentSessionId,
        data.timestamp.toISOString(),
        data.hr, data.br, data.motion, data.dist, data.exist
      ]
    );
    this._save();
  }

  insertSnoreRecord(data) {
    if (!this.currentSessionId) this.startSession();
    this.db.run(
      `INSERT INTO snore_records (session_id, timestamp, snore, confidence)
       VALUES (?, ?, ?, ?)`,
      [this.currentSessionId, data.timestamp.toISOString(), data.snore, data.confidence]
    );
    this._save();
  }

  aggregateOldData() {
    const cutoff = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString();

    this.db.run(`
      INSERT OR IGNORE INTO radar_daily (date, avg_hr, avg_br, max_motion, avg_motion, record_count)
      SELECT substr(timestamp,1,10), ROUND(AVG(heart_rate)), ROUND(AVG(breath_rate)),
             MAX(motion), ROUND(AVG(motion),1), COUNT(*)
      FROM radar_records
      WHERE timestamp < '${cutoff}' AND heart_rate > 0
      GROUP BY substr(timestamp,1,10)
    `);
    this.db.run(`DELETE FROM radar_records WHERE timestamp < '${cutoff}'`);

    this.db.run(`
      INSERT OR IGNORE INTO snore_daily (date, total_snore, avg_confidence, record_count)
      SELECT substr(timestamp,1,10), SUM(snore), ROUND(AVG(confidence),1), COUNT(*)
      FROM snore_records
      WHERE timestamp < '${cutoff}'
      GROUP BY substr(timestamp,1,10)
    `);
    this.db.run(`DELETE FROM snore_records WHERE timestamp < '${cutoff}'`);

    this._save();
    console.log(`[DB] Aggregated data older than ${cutoff}`);
  }

  getTodayStats() {
    const today = new Date();
    today.setHours(0, 0, 0, 0);
    const todayStr = today.toISOString();

    const res = this.db.exec(`
      SELECT posture, COUNT(*) as count, AVG(confidence) as avg_confidence
      FROM sleep_records
      WHERE timestamp >= '${todayStr}' AND posture != 'noise'
      GROUP BY posture
      ORDER BY count DESC
    `);

    let rows = [];
    if (res.length > 0 && res[0].values.length > 0) {
      rows = res[0].values.map(v => ({
        posture: v[0],
        count: v[1],
        avg_confidence: v[2]
      }));
    }

    const total = rows.reduce((sum, r) => sum + r.count, 0);
    return {
      total,
      date: today.toISOString().split('T')[0],
      postures: rows.map(r => ({
        posture: r.posture,
        count: r.count,
        percentage: total > 0 ? ((r.count / total) * 100).toFixed(1) : 0,
        avgConfidence: (r.avg_confidence * 100).toFixed(1)
      }))
    };
  }

  getRecentRecords(limit = 50) {
    const res = this.db.exec(`
      SELECT timestamp, posture, confidence, inference_count
      FROM sleep_records
      ORDER BY id DESC
      LIMIT ${limit}
    `);

    if (res.length === 0) return [];
    const cols = res[0].columns;
    return res[0].values.map(v => {
      const obj = {};
      cols.forEach((c, i) => obj[c] = v[i]);
      return obj;
    });
  }

  getSessions() {
    const res = this.db.exec(`
      SELECT id, start_time, end_time, total_records
      FROM sleep_sessions
      ORDER BY id DESC
      LIMIT 30
    `);

    if (res.length === 0) return [];
    const cols = res[0].columns;
    return res[0].values.map(v => {
      const obj = {};
      cols.forEach((c, i) => obj[c] = v[i]);
      return obj;
    });
  }

  getSessionStats(sessionId) {
    const res = this.db.exec(`
      SELECT posture, COUNT(*) as count, AVG(confidence) as avg_confidence
      FROM sleep_records
      WHERE session_id = ${sessionId} AND posture != 'noise'
      GROUP BY posture
      ORDER BY count DESC
    `);

    let rows = [];
    if (res.length > 0 && res[0].values.length > 0) {
      rows = res[0].values.map(v => ({
        posture: v[0],
        count: v[1],
        avg_confidence: v[2]
      }));
    }

    const total = rows.reduce((sum, r) => sum + r.count, 0);
    return {
      total,
      postures: rows.map(r => ({
        posture: r.posture,
        count: r.count,
        percentage: total > 0 ? ((r.count / total) * 100).toFixed(1) : 0,
        avgConfidence: (r.avg_confidence * 100).toFixed(1)
      }))
    };
  }

  getCurrentSessionId() {
    return this.currentSessionId;
  }

  getWeeklyStats() {
    const days = [];
    for (let i = 6; i >= 0; i--) {
      const d = new Date();
      d.setDate(d.getDate() - i);
      days.push(d.toISOString().split('T')[0]);
    }

    const res = this.db.exec(`
      SELECT substr(timestamp, 1, 10) as day, posture, COUNT(*) as count
      FROM sleep_records
      WHERE substr(timestamp, 1, 10) >= '${days[0]}' AND posture != 'noise'
      GROUP BY day, posture
      ORDER BY day, count DESC
    `);

    const stats = days.map(day => {
      const dayRows = [];
      if (res.length > 0) {
        res[0].values.forEach(v => {
          if (v[0] === day) dayRows.push({ posture: v[1], count: v[2] });
        });
      }

      const total = dayRows.reduce((s, r) => s + r.count, 0);
      const postures = dayRows.sort((a, b) => b.count - a.count).map(r => ({
        posture: r.posture,
        count: r.count,
        percentage: total > 0 ? ((r.count / total) * 100).toFixed(1) : 0
      }));

      return {
        day,
        total,
        dominant: postures.length > 0 ? postures[0].posture : 'N/A',
        dominantPct: postures.length > 0 ? postures[0].percentage : 0,
        active: total > 0,
        postures
      };
    });

    const totalRecords = stats.reduce((s, d) => s + d.total, 0);
    const activeDays = stats.filter(d => d.active).length;
    const allPostures = {};
    stats.forEach(d => {
      d.postures.forEach(p => {
        allPostures[p.posture] = (allPostures[p.posture] || 0) + p.count;
      });
    });
    const overallTotal = Object.values(allPostures).reduce((s, c) => s + c, 0);
    const postureSummary = Object.entries(allPostures)
      .sort((a, b) => b[1] - a[1])
      .map(([k, v]) => ({ posture: k, count: v, percentage: overallTotal > 0 ? ((v / overallTotal) * 100).toFixed(1) : 0 }));

    const hrRes = this.db.exec(`SELECT AVG(heart_rate), AVG(breath_rate), SUM(snore) FROM radar_records r LEFT JOIN snore_records s ON r.session_id=s.session_id AND substr(r.timestamp,1,10)=substr(s.timestamp,1,10) WHERE substr(r.timestamp,1,10)>='${days[0]}'`);
    let avgHR = 0, avgBR = 0;
    if (hrRes.length > 0 && hrRes[0].values.length > 0) {
      const v = hrRes[0].values[0];
      if (v[0]) avgHR = Math.round(v[0]); if (v[1]) avgBR = Math.round(v[1]);
    }

    return {
      days: stats,
      summary: {
        totalRecords,
        activeDays,
        dominant: postureSummary.length > 0 ? postureSummary[0].posture : 'N/A',
        dominantPct: postureSummary.length > 0 ? postureSummary[0].percentage : 0,
        postures: postureSummary,
        avgHR, avgBR
      }
    };
  }

  getMonthlyStats(year, month) {
    if (year === undefined) { const n = new Date(); year = n.getFullYear(); month = n.getMonth(); }
    const days = [];
    const totalDays = new Date(year, month + 1, 0).getDate();
    for (let d = 1; d <= totalDays; d++) {
      days.push(`${year}-${String(month + 1).padStart(2, '0')}-${String(d).padStart(2, '0')}`);
    }

    const res = this.db.exec(`
      SELECT substr(timestamp, 1, 10) as day, posture, COUNT(*) as count
      FROM sleep_records
      WHERE substr(timestamp, 1, 10) >= '${days[0]}' AND posture != 'noise'
      GROUP BY day, posture
      ORDER BY day, count DESC
    `);

    const stats = days.map(day => {
      const dayRows = [];
      if (res.length > 0) {
        res[0].values.forEach(v => {
          if (v[0] === day) dayRows.push({ posture: v[1], count: v[2] });
        });
      }

      const total = dayRows.reduce((s, r) => s + r.count, 0);
      const sorted = dayRows.sort((a, b) => b.count - a.count);

      return {
        day,
        total,
        dominant: sorted.length > 0 ? sorted[0].posture : null,
        active: total > 0
      };
    });

    const totalRecords = stats.reduce((s, d) => s + d.total, 0);
    const activeDays = stats.filter(d => d.active).length;
    const allPostures = {};
    if (res.length > 0) {
      res[0].values.forEach(v => {
        if (days.includes(v[0])) {
          allPostures[v[1]] = (allPostures[v[1]] || 0) + v[2];
        }
      });
    }
    const overallTotal = Object.values(allPostures).reduce((s, c) => s + c, 0);
    const postureSummary = Object.entries(allPostures)
      .sort((a, b) => b[1] - a[1])
      .map(([k, v]) => ({ posture: k, count: v, percentage: overallTotal > 0 ? ((v / overallTotal) * 100).toFixed(1) : 0 }));

    const hrRes = this.db.exec(`SELECT AVG(heart_rate), AVG(breath_rate) FROM radar_records WHERE substr(timestamp,1,7)='${String(year)}-${String(month+1).padStart(2,'0')}'`);
    let avgHR = 0, avgBR = 0;
    if (hrRes.length > 0 && hrRes[0].values.length > 0) {
      const v = hrRes[0].values[0];
      if (v[0]) avgHR = Math.round(v[0]); if (v[1]) avgBR = Math.round(v[1]);
    }

    return {
      days: stats,
      summary: {
        totalRecords,
        activeDays,
        postures: postureSummary,
        avgHR, avgBR
      }
    };
  }

  saveReport(periodType, periodLabel, content, score) {
    this.db.run(
      `INSERT INTO ai_reports (period_type, period_label, report_content, score, created_at) VALUES (?, ?, ?, ?, ?)`,
      [periodType, periodLabel, content, score, new Date().toISOString()]
    );
    this._save();
    console.log(`Report saved: ${periodType}/${periodLabel} score=${score}`);
  }

  getReport(periodType, periodLabel) {
    const stmt = this.db.prepare(
      `SELECT report_content, score, created_at FROM ai_reports WHERE period_type = ? AND period_label = ? ORDER BY id DESC LIMIT 1`
    );
    stmt.bind([periodType, periodLabel]);
    if (stmt.step()) {
      const row = stmt.getAsObject();
      stmt.free();
      return { content: row.report_content, score: row.score, createdAt: row.created_at };
    }
    stmt.free();
    return null;
  }

  getPeriodSnoreStats(startDate, endDate) {
    const res = this.db.exec(`
      SELECT COUNT(*) as total_snore, AVG(snore) as avg_snore, AVG(confidence) as avg_conf
      FROM snore_records
      WHERE timestamp >= '${startDate}' AND timestamp < '${endDate}'
    `);
    if (res.length > 0 && res[0].values.length > 0 && res[0].values[0][0] > 0) {
      const v = res[0].values[0];
      return { totalRecords: v[0], avgSnore: v[1], avgConfidence: v[2] };
    }
    return null;
  }

  close() {
    this.endSession();
    console.log('Database closed');
  }
}

module.exports = SleepDatabase;
