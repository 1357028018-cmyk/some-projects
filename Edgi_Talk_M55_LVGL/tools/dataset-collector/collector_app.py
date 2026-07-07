import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, filedialog
import serial
import serial.tools.list_ports
import threading
import queue
import time
import os
import struct

FRAME_TOTAL = 268
FRAME_HEADER = 0xAA
FRAME_TAIL = 0x55
FRAME_ADC_START = 4
FRAME_ADC_DATA_LEN = 256
FRAME_CRC_POS = 260
FRAME_TAIL_POS = 261

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(BASE_DIR, "output_dataset")

POSTURES = [
    ("仰卧", "supine"),
    ("右侧头", "right_head"),
    ("右侧卧", "right_lateral"),
    ("左侧头", "left_head"),
    ("左侧卧", "left_lateral"),
]

def calc_crc8(data):
    crc = 0
    for b in data:
        crc ^= b
    return crc


class SerialReader:
    def __init__(self, port, baud=115200, diag_callback=None):
        self.port = port
        self.baud = baud
        self.ser = None
        self.running = False
        self.frame_queue = queue.Queue()
        self._buffer = bytearray()
        self._thread = None
        self.bytes_read = 0
        self.frames_ok = 0
        self.frames_bad_tail = 0
        self.frames_bad_crc = 0
        self._diag = diag_callback

    def open(self):
        self.ser = serial.Serial(self.port, self.baud, timeout=0.05)
        self._buffer.clear()
        self.bytes_read = 0
        self.frames_ok = 0
        self.frames_bad_tail = 0
        self.frames_bad_crc = 0
        while not self.frame_queue.empty():
            self.frame_queue.get_nowait()
        self.running = True
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()
        return True

    def close(self):
        self.running = False
        if self._thread:
            self._thread.join(timeout=1)
        if self.ser and self.ser.is_open:
            self.ser.close()

    def _read_loop(self):
        while self.running:
            try:
                if self.ser.in_waiting:
                    data = self.ser.read(self.ser.in_waiting)
                    self.bytes_read += len(data)
                    self._buffer.extend(data)
                    n = len(self._buffer)
                    self._extract_frames()
                    if len(self._buffer) == n and n >= FRAME_TOTAL and self.frames_ok < 3:
                        self.extract_stats_queue.put((n, bytes(self._buffer[:8])))
                else:
                    time.sleep(0.001)
            except Exception:
                break

    def _extract_frames(self):
        while len(self._buffer) >= FRAME_TOTAL:
            found = -1
            for i in range(len(self._buffer) - FRAME_TOTAL + 1):
                if (self._buffer[i] != 0xAA or
                    self._buffer[i+2] != 0x01 or self._buffer[i+3] != 0x00 or
                    self._buffer[i+FRAME_TAIL_POS] != FRAME_TAIL):
                    continue
                found = i
                break
            if found < 0:
                self._buffer.clear()
                break
            if found > 0:
                self._buffer = self._buffer[found:]
                continue
            frame = bytes(self._buffer[:FRAME_TOTAL])
            self._buffer = self._buffer[FRAME_TOTAL:]
            self.frames_ok += 1
            self.frame_queue.put(frame)

    def get_frame(self, timeout=0):
        try:
            return self.frame_queue.get(timeout=timeout)
        except queue.Empty:
            return None


class Application(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Pressure Sensor Dataset Collector v1.0")
        self.geometry("920x680")

        self.reader = None
        self.collecting = False
        self.current_label = "unlabeled"
        self.frame_count = 0
        self.frame_limit = 500
        self.start_time = 0
        self.session_frames = []
        self.sample_seq = 0
        self._last_frame = None
        self._label_log = ""
        self.output_dir = OUTPUT_DIR
        self.hex_view_enabled = tk.BooleanVar(value=True)
        self._hex_skip_counter = 0

        os.makedirs(self.output_dir, exist_ok=True)
        existing = [d for d in os.listdir(self.output_dir) if d.startswith("HLPPDat_")]
        if existing:
            nums = [int(d.split("_")[1]) for d in existing if d.split("_")[1].isdigit()]
            self.sample_seq = max(nums) if nums else 0

        self._build_ui()
        self._poll_serial()

    def _build_ui(self):
        conn_frame = ttk.LabelFrame(self, text="Serial Connection", padding=5)
        conn_frame.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(conn_frame, text="COM:").pack(side=tk.LEFT, padx=5)
        self.combo_com = ttk.Combobox(conn_frame, width=12, state="readonly")
        self.combo_com.pack(side=tk.LEFT, padx=5)
        self._refresh_ports()
        ttk.Button(conn_frame, text="Refresh", command=self._refresh_ports).pack(side=tk.LEFT, padx=2)
        self.btn_connect = ttk.Button(conn_frame, text="Connect", command=self._toggle_connect)
        self.btn_connect.pack(side=tk.LEFT, padx=10)
        self.lbl_status = ttk.Label(conn_frame, text="\u25cf Disconnected", foreground="red")
        self.lbl_status.pack(side=tk.LEFT, padx=10)

        dir_frame = ttk.LabelFrame(self, text="Output Directory", padding=5)
        dir_frame.pack(fill=tk.X, padx=10, pady=2)
        self.entry_dir = ttk.Entry(dir_frame)
        self.entry_dir.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.entry_dir.insert(0, self.output_dir)
        self.entry_dir.config(state="readonly")
        ttk.Button(dir_frame, text="Browse...", command=self._browse_output_dir).pack(side=tk.LEFT, padx=5)

        ctrl_frame = ttk.LabelFrame(self, text="Collection Control", padding=5)
        ctrl_frame.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(ctrl_frame, text="Frame Limit:").pack(side=tk.LEFT, padx=5)
        self.entry_limit = ttk.Entry(ctrl_frame, width=10)
        self.entry_limit.insert(0, "500")
        self.entry_limit.pack(side=tk.LEFT, padx=5)
        ttk.Label(ctrl_frame, text="(0=unlimited)").pack(side=tk.LEFT, padx=2)

        self.btn_start = ttk.Button(ctrl_frame, text="\u25b6 Start", command=self._toggle_collect, state=tk.DISABLED)
        self.btn_start.pack(side=tk.LEFT, padx=20)
        self.btn_stop = ttk.Button(ctrl_frame, text="\u25a0 Stop & Save", command=self._stop_and_save, state=tk.DISABLED)
        self.btn_stop.pack(side=tk.LEFT, padx=5)

        self.lbl_frame_count = ttk.Label(ctrl_frame, text="Collected: 0 frames")
        self.lbl_frame_count.pack(side=tk.LEFT, padx=15)

        btn_frame = ttk.LabelFrame(self, text="Sleep Posture Labels (click to switch)", padding=5)
        btn_frame.pack(fill=tk.X, padx=10, pady=5)

        self.lbl_current_label = ttk.Label(btn_frame, text="Current label: unlabeled", foreground="gray", font=("", 10, "bold"))
        self.lbl_current_label.pack(side=tk.LEFT, padx=10)

        colors = ["#4CAF50", "#FF9800", "#9C27B0", "#2196F3", "#F44336"]
        for i, (name, val) in enumerate(POSTURES):
            btn = tk.Button(btn_frame, text=name, bg=colors[i], fg="white",
                            font=("", 10, "bold"), padx=15, pady=4,
                            command=lambda v=val, n=name: self._set_label(v, n))
            btn.pack(side=tk.LEFT, padx=5)

        monitor_frame = ttk.LabelFrame(self, text="Monitor", padding=2)
        monitor_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.monitor_notebook = ttk.Notebook(monitor_frame)
        self.monitor_notebook.pack(fill=tk.BOTH, expand=True)

        hex_tab = ttk.Frame(self.monitor_notebook)
        self.monitor_notebook.add(hex_tab, text="Serial Monitor (hex)")
        self.txt_hex = scrolledtext.ScrolledText(hex_tab, height=10, font=("Consolas", 9))
        self.txt_hex.pack(fill=tk.BOTH, expand=True)
        self.txt_hex.insert(tk.END, "[Serial Monitor] Waiting for data...\n")
        self.txt_hex.config(state=tk.DISABLED)
        ctrl_row = ttk.Frame(hex_tab)
        ctrl_row.pack(fill=tk.X)
        ttk.Button(ctrl_row, text="Clear", command=self._clear_hex).pack(side=tk.LEFT, padx=5, pady=2)
        ttk.Checkbutton(ctrl_row, text="Auto-scroll", variable=self.hex_view_enabled).pack(side=tk.LEFT, padx=5)

        log_tab = ttk.Frame(self.monitor_notebook)
        self.monitor_notebook.add(log_tab, text="Event Log")
        self.txt_info = scrolledtext.ScrolledText(log_tab, height=10, font=("Consolas", 9))
        self.txt_info.pack(fill=tk.BOTH, expand=True)
        self.txt_info.insert(tk.END, "Ready - connect serial port and start collection.\n")
        self.txt_info.config(state=tk.DISABLED)

        status_frame = ttk.Frame(self)
        status_frame.pack(fill=tk.X, padx=10, pady=2)
        self.lbl_seq = ttk.Label(status_frame,
                                 text=f"Next output: HLPPDat_{self.sample_seq + 1:04d}")
        self.lbl_seq.pack(side=tk.LEFT)
        self.lbl_rate = ttk.Label(status_frame, text="0 fps")
        self.lbl_rate.pack(side=tk.RIGHT, padx=10)
        self.lbl_bytes = ttk.Label(status_frame, text="0 B")
        self.lbl_bytes.pack(side=tk.RIGHT, padx=5)
        self.lbl_diag = ttk.Label(status_frame, text="ok=0 bad=0")
        self.lbl_diag.pack(side=tk.RIGHT, padx=5)

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.combo_com["values"] = ports
        if ports and not self.combo_com.get():
            self.combo_com.current(0)

    def _log(self, msg):
        self.txt_info.config(state=tk.NORMAL)
        self.txt_info.insert(tk.END, time.strftime("%H:%M:%S ") + msg + "\n")
        self.txt_info.see(tk.END)
        self.txt_info.config(state=tk.DISABLED)

    def _browse_output_dir(self):
        d = filedialog.askdirectory(initialdir=self.output_dir, title="Select output directory")
        if d:
            self.output_dir = d
            self.entry_dir.config(state=tk.NORMAL)
            self.entry_dir.delete(0, tk.END)
            self.entry_dir.insert(0, d)
            self.entry_dir.config(state="readonly")
            os.makedirs(d, exist_ok=True)
            existing = [x for x in os.listdir(d) if x.startswith("HLPPDat_")]
            if existing:
                nums = [int(x.split("_")[1]) for x in existing if x.split("_")[1].isdigit()]
                self.sample_seq = max(nums) if nums else 0
            self.lbl_seq.config(text=f"Next output: HLPPDat_{self.sample_seq + 1:04d}")
            self._log(f"Output directory changed to: {d}")

    def _clear_hex(self):
        self.txt_hex.config(state=tk.NORMAL)
        self.txt_hex.delete(1.0, tk.END)
        self.txt_hex.config(state=tk.DISABLED)

    def _format_hex_line(self, frame):
        hdr = " ".join(f"{frame[i]:02X}" for i in range(4))
        adc_front = " ".join(f"{frame[FRAME_ADC_START + i]:02X}" for i in range(8))
        adc_back = " ".join(f"{frame[FRAME_ADC_START + FRAME_ADC_DATA_LEN - 8 + i]:02X}" for i in range(8))
        tail = " ".join(f"{frame[FRAME_CRC_POS + i]:02X}" for i in range(8))
        return f"[{self.frame_count:05d}] {hdr} | {adc_front} ... {adc_back} | {tail}"

    def _append_hex_batch(self, lines):
        if not lines:
            return
        self.txt_hex.config(state=tk.NORMAL)
        for line in lines:
            self.txt_hex.insert(tk.END, line + "\n")
        if self.hex_view_enabled.get():
            self.txt_hex.see(tk.END)
        if int(self.txt_hex.index('end-1c').split('.')[0]) > 2000:
            self.txt_hex.delete(1.0, "1000 lines")
        self.txt_hex.config(state=tk.DISABLED)

    def _toggle_connect(self):
        if self.reader and self.reader.running:
            if self.collecting:
                self._stop_and_save()
            self.reader.close()
            self.reader = None
            self.btn_connect.config(text="Connect")
            self.lbl_status.config(text="\u25cf Disconnected", foreground="red")
            self.btn_start.config(state=tk.DISABLED)
            self._log("Serial disconnected")
        else:
            port = self.combo_com.get()
            if not port:
                messagebox.showwarning("Warning", "Please select a COM port")
                return
            try:
                self.reader = SerialReader(port, diag_callback=self._log)
                self.reader.open()
                self.btn_connect.config(text="Disconnect")
                self.lbl_status.config(text="\u25cf Connected", foreground="green")
                self.btn_start.config(state=tk.NORMAL)
                self._log(f"Serial {port} connected @115200")
            except Exception as e:
                messagebox.showerror("Error", f"Cannot open serial port:\n{str(e)}")

    def _toggle_collect(self):
        if not self.collecting:
            self._start_collect()
        else:
            self._stop_and_save()

    def _start_collect(self):
        try:
            limit = int(self.entry_limit.get().strip())
            self.frame_limit = limit if limit > 0 else float('inf')
        except ValueError:
            self.frame_limit = float('inf')

        self.collecting = True
        self.session_frames = []
        self.frame_count = 0
        self.start_time = time.time()
        self._label_log = self.current_label

        self.btn_start.config(text="\u23f8 Pause", state=tk.NORMAL)
        self.btn_stop.config(state=tk.NORMAL)
        limit_str = str(int(self.frame_limit)) if self.frame_limit != float('inf') else "unlimited"
        self.lbl_current_label.config(text="Current label: unlabeled (waiting)", foreground="gray")
        self._log(f"Collection started - target: {limit_str} frames")

    def _stop_and_save(self):
        if not self.collecting:
            return
        self.collecting = False
        self.btn_start.config(text="\u25b6 Start", state=tk.NORMAL if self.reader else tk.DISABLED)
        self.btn_stop.config(state=tk.DISABLED)

        if self.session_frames:
            self._save_batch(self.session_frames, self.current_label)
            self.session_frames = []

        self.current_label = "unlabeled"
        self.lbl_current_label.config(text="Current label: unlabeled", foreground="gray")

        elapsed = time.time() - self.start_time if self.start_time else 0
        rate = self.frame_count / elapsed if elapsed > 0 else 0
        self._log(f"Collection stopped - total {self.frame_count} frames, {elapsed:.1f}s ({rate:.1f} fps)")
        self.lbl_frame_count.config(text=f"Collected: {self.frame_count} frames")

    def _set_label(self, label_val, label_name):
        if not self.collecting:
            self.current_label = label_val
            self.lbl_current_label.config(text=f"Current label: {label_name}", foreground="blue")
            self._log(f"Label set to: {label_name}")
            return

        if self.session_frames and self._label_log != label_val:
            self._save_batch(self.session_frames, self.current_label)
            self.session_frames = []

        self.current_label = label_val
        self._label_log = label_val
        self.lbl_current_label.config(text=f"Current label: {label_name}", foreground="blue")
        self._log(f"Label switched to: {label_name} - new batch started")

    def _save_one_sample(self, ts, frame, label):
        self.sample_seq += 1
        sample_dir = os.path.join(self.output_dir, f"HLPPDat_{self.sample_seq:04d}")
        os.makedirs(sample_dir, exist_ok=True)

        adc_values = list(frame[FRAME_ADC_START:FRAME_ADC_START + FRAME_ADC_DATA_LEN])

        data_path = os.path.join(sample_dir, "sensor_data.data")
        with open(data_path, "w") as f:
            headers = ["Time"] + [f"Pixel_{r}_{c}" for r in range(16) for c in range(16)]
            f.write(",".join(headers) + "\n")
            row = ",".join([str(v) for v in adc_values])
            f.write(f"0.0,{row}\n")
            f.write(f"1.0,{row}\n")

        label_path = os.path.join(sample_dir, "sensor_data.label")
        with open(label_path, "w") as f:
            f.write("Time,Length,Label,Confidence,Comment\n")
            f.write(f"0.0,1.0,{label},1.0,\n")

        self.lbl_seq.config(text=f"Next output: HLPPDat_{self.sample_seq + 1:04d}")
        self._log(f"Saved: HLPPDat_{self.sample_seq:04d} - {label}")

    def _save_batch(self, frames, label):
        if not frames:
            return
        for ts, frame, _ in frames:
            self._save_one_sample(ts, frame, label)

    def _poll_serial(self):
        now = time.time()
        if not hasattr(self, '_fps_last'):
            self._fps_last = now
            self._fps_frames = 0

        if self.reader and self.reader.running:
            hex_lines = []
            new_collected = 0
            while True:
                frame = self.reader.get_frame(timeout=0)
                if not frame:
                    break
                self._last_frame = frame
                self.frame_count += 1
                self._fps_frames += 1
                hex_lines.append(self._format_hex_line(frame))

                if self.collecting and self.current_label != "unlabeled":
                    self.session_frames.append((now, frame, self.current_label))
                    new_collected += 1

            if hex_lines:
                self._append_hex_batch(hex_lines)
            if new_collected > 0:
                self.lbl_frame_count.config(text=f"Collected: {len(self.session_frames)} frames")
                if len(self.session_frames) >= self.frame_limit:
                    self._log(f"Frame limit ({int(self.frame_limit)}) reached, auto-stopping")
                    self._stop_and_save()

        if now - self._fps_last >= 1.0:
            fps = self._fps_frames / (now - self._fps_last)
            self.lbl_rate.config(text=f"{fps:.1f} fps")
            if self.reader:
                self.lbl_bytes.config(text=f"{self.reader.bytes_read} B")
                self.lbl_diag.config(
                    text=f"ok={self.reader.frames_ok} tail={self.reader.frames_bad_tail} crc={self.reader.frames_bad_crc}")
            self._fps_frames = 0
            self._fps_last = now

        self.after(20, self._poll_serial)


if __name__ == "__main__":
    app = Application()
    app.mainloop()
