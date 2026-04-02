#!/usr/bin/env python3
"""
SBR Serial Debug Tool
- Compact PID control with 2D horizon + strip graph
- Command buttons for common operations
- GET STATUS display (mode, calibration, test mode)
- Auto-scroll follows only when at bottom
Requires: pyserial
Install: pip install pyserial
Run: python pid_gui.py
"""
import threading, queue, time, re, math, collections
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports

READ_TIMEOUT = 0.1
TELEMETRY_THROTTLE = 0.05
GRAPH_HISTORY = 400

class SerialReader(threading.Thread):
    def __init__(self, ser, out_q, stop_event):
        super().__init__(daemon=True)
        self.ser = ser
        self.out_q = out_q
        self.stop_event = stop_event

    def run(self):
        buf = b""
        while not self.stop_event.is_set():
            try:
                data = self.ser.read(256)
                if data:
                    buf += data
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        try:
                            text = line.decode('utf-8', errors='ignore').strip()
                        except:
                            text = repr(line)
                        self.out_q.put(text)
                else:
                    time.sleep(0.01)
            except Exception as e:
                self.out_q.put(f"<SERIAL ERROR> {e}")
                break

class PIDGui:
    def __init__(self, root):
        self.root = root
        root.title("SBR Serial Debug")
        frm = ttk.Frame(root, padding=6)
        frm.grid(sticky="nsew")
        root.columnconfigure(0, weight=1)
        root.rowconfigure(4, weight=1)

        # Top: port and connect
        top = ttk.Frame(frm)
        top.grid(row=0, column=0, sticky="we")
        ttk.Label(top, text="Serial:").pack(side="left")
        self.port_cb = ttk.Combobox(top, width=20, values=self._list_ports())
        self.port_cb.pack(side="left", padx=(4,6))
        ttk.Button(top, text="Refresh", command=self._refresh_ports).pack(side="left")
        self.b_connect = ttk.Button(top, text="Connect", command=self.toggle_connect)
        self.b_connect.pack(side="right")
        self.status_label = ttk.Label(top, text="Disconnected", foreground="red")
        self.status_label.pack(side="right", padx=10)

        # PID row
        pidrow = ttk.Frame(frm)
        pidrow.grid(row=1, column=0, sticky="we", pady=(6,2))
        lbl_kp = ttk.Label(pidrow, text="KP"); lbl_kp.grid(row=0, column=0, padx=(0,2))
        self.e_kp = ttk.Entry(pidrow, width=9); self.e_kp.grid(row=0, column=1)
        lbl_ki = ttk.Label(pidrow, text="KI"); lbl_ki.grid(row=0, column=2, padx=(8,2))
        self.e_ki = ttk.Entry(pidrow, width=9); self.e_ki.grid(row=0, column=3)
        lbl_kd = ttk.Label(pidrow, text="KD"); lbl_kd.grid(row=0, column=4, padx=(8,2))
        self.e_kd = ttk.Entry(pidrow, width=9); self.e_kd.grid(row=0, column=5)
        ttk.Button(pidrow, text="GET PID", width=8, command=self.cmd_get_pid).grid(row=0, column=6, padx=(12,4))
        ttk.Button(pidrow, text="SET PID", width=8, command=self.cmd_set_pid).grid(row=0, column=7)
        self.raw_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(pidrow, text="Show raw", variable=self.raw_var).grid(row=0, column=8, padx=(12,0))

        # Command buttons row
        cmdrow = ttk.Frame(frm)
        cmdrow.grid(row=2, column=0, sticky="we", pady=(4,2))
        
        ttk.Button(cmdrow, text="GET STATUS", command=self.cmd_get_status).pack(side="left", padx=2)
        ttk.Button(cmdrow, text="CALIBRATE", command=self.cmd_calibrate).pack(side="left", padx=2)
        ttk.Button(cmdrow, text="SAVE_CAL", command=self.cmd_save_cal).pack(side="left", padx=2)
        ttk.Button(cmdrow, text="LOAD_CAL", command=self.cmd_load_cal).pack(side="left", padx=2)
        ttk.Button(cmdrow, text="GET_CAL_INFO", command=self.cmd_get_cal_info).pack(side="left", padx=2)
        
        ttk.Separator(cmdrow, orient='vertical').pack(side="left", fill='y', padx=8)
        
        ttk.Button(cmdrow, text="TEST ON", command=self.cmd_test_on).pack(side="left", padx=2)
        ttk.Button(cmdrow, text="TEST OFF", command=self.cmd_test_off).pack(side="left", padx=2)
        
        ttk.Separator(cmdrow, orient='vertical').pack(side="left", fill='y', padx=8)
        
        # Estop - red and prominent
        self.b_estop = tk.Button(cmdrow, text="ESTOP", bg="red", fg="white", font=("TkDefaultFont", 10, "bold"), command=self.cmd_estop)
        self.b_estop.pack(side="left", padx=2)
        self.estop_active = False

        # Middle: horizon canvas + strip graph
        canvas_frame = ttk.Frame(frm)
        canvas_frame.grid(row=3, column=0, sticky="we", pady=(6,4))
        canvas_frame.columnconfigure(0, weight=1)

        self.canvas = tk.Canvas(canvas_frame, width=560, height=150, bg="#f6f6f6", bd=1, relief="solid")
        self.canvas.grid(sticky="we")
        self.graph = tk.Canvas(canvas_frame, width=560, height=70, bg="#111", bd=1, relief="solid")
        self.graph.grid(sticky="we", pady=(6,0))

        self.pitch_hist = collections.deque([0.0]*GRAPH_HISTORY, maxlen=GRAPH_HISTORY)
        self.roll_hist  = collections.deque([0.0]*GRAPH_HISTORY, maxlen=GRAPH_HISTORY)
        self._last_telemetry_ts = 0.0
        self.pitch = 0.0; self.roll = 0.0; self.yaw = 0.0
        self.mode = "?"
        self.test_mode = "?"
        self.calibrated = False

        self._draw_horizon_static()
        self._draw_graph_static()

        # Bottom: tabbed outputs
        bottom_frame = ttk.Frame(frm)
        bottom_frame.grid(row=4, column=0, sticky="nsew")
        bottom_frame.columnconfigure(0, weight=1)
        bottom_frame.rowconfigure(0, weight=1)
        self.notebook = ttk.Notebook(bottom_frame)
        self.notebook.grid(sticky="nsew")
        self.filtered_tab = ttk.Frame(self.notebook)
        self.raw_tab = ttk.Frame(self.notebook)
        self.out_filtered = scrolledtext.ScrolledText(self.filtered_tab, width=120, height=15, state='disabled')
        self.out_filtered.pack(fill="both", expand=True)
        self.out_raw = scrolledtext.ScrolledText(self.raw_tab, width=120, height=15, state='disabled')
        self.out_raw.pack(fill="both", expand=True)
        self.notebook.add(self.filtered_tab, text="Filtered")
        self.notebook.add(self.raw_tab, text="Raw")
        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_change)

        # Manual send
        manual_row = ttk.Frame(frm)
        manual_row.grid(row=5, column=0, sticky="we", pady=(6,0))
        self.manual = ttk.Entry(manual_row, width=80)
        self.manual.pack(side="left", fill="x", expand=True)
        ttk.Button(manual_row, text="Send", command=self.manual_send).pack(side="left", padx=(6,0))

        # Serial state
        self.ser = None; self.reader = None; self.q = queue.Queue(); self.stop_event = threading.Event()
        self.root.after(40, self._poll_queue)

        # Regex parsers - FIXED for new format: PITCH:0.00 ROLL:0.00 YAW:0.00
        self.re_kv = re.compile(r'\b(KP|KI|KD)[:=]\s*([-\d.]+)', re.IGNORECASE)
        self.re_pid_line = re.compile(r'KP[:=]\s*([-\d.]+)\s+KI[:=]\s*([-\d.]+)\s+KD[:=]\s*([-\d.]+)', re.IGNORECASE)
        self.re_telemetry = re.compile(r'PITCH[:=]\s*([-\d.]+)\s+ROLL[:=]\s*([-\d.]+)\s+YAW[:=]\s*([-\d.]+)', re.IGNORECASE)
        self.re_status_mode = re.compile(r'control_mode:\s*(\w+)', re.IGNORECASE)
        self.re_status_test = re.compile(r'test_mode:\s*(\w+)', re.IGNORECASE)
        self.re_status_cal = re.compile(r'has_calibration:\s*(\w+)', re.IGNORECASE)

        self._follow_filtered = True
        self._follow_raw = True
        self._attach_scroll_handlers()

    def _attach_scroll_handlers(self):
        def bind_widget(widget, follow_attr):
            def on_scroll_user(event):
                try:
                    f, l = widget.yview()
                    setattr(self, follow_attr, l >= 0.999)
                except:
                    setattr(self, follow_attr, True)
            widget.bind("<Button-1>", on_scroll_user)
            widget.bind("<MouseWheel>", on_scroll_user)
            widget.bind("<Key>", on_scroll_user)
            widget.bind("<Button-4>", on_scroll_user)
            widget.bind("<Button-5>", on_scroll_user)
        bind_widget(self.out_filtered, "_follow_filtered")
        bind_widget(self.out_raw, "_follow_raw")

    def _on_tab_change(self, event):
        if self.notebook.index("current") == 1:
            self._follow_raw = True
        else:
            self._follow_filtered = True

    def _draw_horizon_static(self):
        c = self.canvas
        c.delete("all")
        w = int(c.winfo_reqwidth()); h = int(c.winfo_reqheight())
        c.create_rectangle(10, 10, w-10, h-10, outline="#aaa", width=2, tags="frame")
        c.create_line(0, h//2, w, h//2, fill="#fff", width=2, tags="horizon_line")
        c.create_text(12, h-24, anchor="w", text="Pitch: 0.00°  Roll: 0.00°", tags="txt_pr", fill="#111")
        c.create_text(w-12, 12, anchor="ne", text="Mode: ?  Test: ?  Cal: ?", tags="txt_status", fill="#333")

    def _draw_graph_static(self):
        g = self.graph
        g.delete("all")
        w = int(g.winfo_reqwidth()); h = int(g.winfo_reqheight())
        g.create_text(6, 6, anchor="nw", text="roll (red)   pitch (green)", fill="#ddd", tags="lbl", font=("TkDefaultFont", 8))

    def _update_horizon(self):
        try:
            pitch = float(self.pitch); roll = float(self.roll)
        except:
            return
        c = self.canvas
        w = int(c.winfo_reqwidth()); h = int(c.winfo_reqheight())
        cx, cy = w//2, h//2
        wbox, hbox = 200, 70
        angle = math.radians(-roll)
        corners = [(-wbox/2, -hbox/2), (wbox/2, -hbox/2), (wbox/2, hbox/2), (-wbox/2, hbox/2)]
        pts = []
        for x, y in corners:
            xr = x*math.cos(angle) - y*math.sin(angle)
            yr = x*math.sin(angle) + y*math.cos(angle)
            pts.extend([cx + xr, cy + yr + (pitch*0.6)])
        if c.find_withtag("body"):
            c.coords("body", *pts)
        else:
            c.create_polygon(*pts, fill="#61aaff", outline="#003", tags="body")
        cal_str = "Yes" if self.calibrated else "No"
        c.itemconfigure("txt_pr", text=f"Pitch: {pitch:.2f}°  Roll: {roll:.2f}°")
        c.itemconfigure("txt_status", text=f"Mode: {self.mode}  Test: {self.test_mode}  Cal: {cal_str}")

    def _update_graph(self):
        g = self.graph
        g.delete("line_roll"); g.delete("line_pitch")
        w = int(g.winfo_reqwidth()); h = int(g.winfo_reqheight())
        ph = list(self.pitch_hist); rh = list(self.roll_hist)
        def map_y(val):
            r = max(-60.0, min(60.0, val))
            return int((h-10)/2 - (r / 60.0) * ((h-10)/2)) + 8
        step = max(1, w // GRAPH_HISTORY)
        xs = list(range(2, 2 + step*len(ph), step))[:len(ph)]
        if len(xs) < 2: return
        coords_r = []
        coords_p = []
        for xi, rv, pv in zip(xs[-len(rh):], rh[-len(xs):], ph[-len(xs):]):
            coords_r.extend([xi, map_y(rv)])
            coords_p.extend([xi, map_y(pv)])
        if coords_r:
            g.create_line(*coords_r, fill="#ff4444", width=1, tags="line_roll", smooth=True)
        if coords_p:
            g.create_line(*coords_p, fill="#33ff77", width=1, tags="line_pitch", smooth=True)

    def _list_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def _refresh_ports(self):
        self.port_cb['values'] = self._list_ports()

    def toggle_connect(self):
        if self.ser:
            self._disconnect()
        else:
            port = self.port_cb.get().strip()
            if not port:
                messagebox.showwarning("Port", "Select a serial port first.")
                return
            try:
                self.ser = serial.Serial(port, 115200, timeout=READ_TIMEOUT)
            except Exception as e:
                messagebox.showerror("Open serial", str(e))
                self.ser = None
                return
            self.stop_event.clear()
            self.reader = SerialReader(self.ser, self.q, self.stop_event)
            self.reader.start()
            self.b_connect.config(text="Disconnect")
            self.status_label.config(text="Connected", foreground="green")
            self._clear_outputs()
            self._log_filtered(f"Connected to {port}")

    def _disconnect(self):
        self.stop_event.set()
        time.sleep(0.05)
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
        except:
            pass
        self.ser = None
        self.b_connect.config(text="Connect")
        self.status_label.config(text="Disconnected", foreground="red")
        self._log_filtered("Disconnected")

    def _poll_queue(self):
        flushed = False
        while True:
            try:
                line = self.q.get_nowait()
            except queue.Empty:
                break
            flushed = True

            # Parse GET STATUS responses
            m_mode = self.re_status_mode.search(line)
            if m_mode:
                self.mode = m_mode.group(1).upper()
                self._update_horizon()
            
            m_test = self.re_status_test.search(line)
            if m_test:
                self.test_mode = m_test.group(1)
                self._update_horizon()
            
            m_cal = self.re_status_cal.search(line)
            if m_cal:
                self.calibrated = m_cal.group(1).lower() == "true"
                self._update_horizon()

            # PID triple
            m_pid = self.re_pid_line.search(line)
            if m_pid:
                try:
                    kp = float(m_pid.group(1)); ki = float(m_pid.group(2)); kd = float(m_pid.group(3))
                    self._set_entry(self.e_kp, f"{kp:.6f}")
                    self._set_entry(self.e_ki, f"{ki:.6f}")
                    self._set_entry(self.e_kd, f"{kd:.6f}")
                except:
                    pass
                self._log_filtered("PID: " + line)
                if self.raw_var.get(): self._log_raw(line)
                continue

            # KP/KI/KD anywhere
            kvs = self.re_kv.findall(line)
            if kvs:
                for k, v in kvs:
                    try:
                        val = float(v)
                    except:
                        continue
                    kk = k.upper()
                    if kk == "KP": self._set_entry(self.e_kp, f"{val:.6f}")
                    elif kk == "KI": self._set_entry(self.e_ki, f"{val:.6f}")
                    elif kk == "KD": self._set_entry(self.e_kd, f"{val:.6f}")
                self._log_filtered(line)
                if self.raw_var.get(): self._log_raw(line)
                continue

            # Telemetry - FIXED format
            mt = self.re_telemetry.search(line)
            if mt:
                now = time.time()
                if (now - self._last_telemetry_ts) >= TELEMETRY_THROTTLE:
                    self._last_telemetry_ts = now
                    try:
                        self.pitch = float(mt.group(1)); self.roll = float(mt.group(2)); self.yaw = float(mt.group(3))
                    except:
                        pass
                    self.pitch_hist.append(self.pitch); self.roll_hist.append(self.roll)
                    self._update_horizon(); self._update_graph()
                    self._log_filtered(line)
                if self.raw_var.get(): self._log_raw(line)
                continue

            # Other: log to raw if enabled
            if self.raw_var.get():
                self._log_raw(line)

        if flushed:
            self._trim_all()
        self.root.after(40, self._poll_queue)

    def _append_text_widget(self, widget, text, follow_attr_name):
        widget.configure(state='normal')
        widget.insert('end', text + "\n")
        try:
            f, l = widget.yview()
        except:
            f, l = 0.0, 1.0
        if getattr(self, follow_attr_name):
            widget.see("end")
        widget.configure(state='disabled')

    def _log_filtered(self, text):
        self._append_text_widget(self.out_filtered, text, "_follow_filtered")

    def _log_raw(self, text):
        self._append_text_widget(self.out_raw, text, "_follow_raw")

    def _clear_outputs(self):
        for w in (self.out_filtered, self.out_raw):
            w.configure(state='normal')
            w.delete('1.0', 'end')
            w.configure(state='disabled')

    def _trim_all(self):
        for w in (self.out_filtered, self.out_raw):
            lines = int(w.index('end-1c').split('.')[0])
            max_lines = 3000
            if lines > max_lines:
                w.configure(state='normal')
                w.delete('1.0', f'{lines - max_lines}.0')
                w.configure(state='disabled')

    def _set_entry(self, entry, val):
        try:
            entry.delete(0, 'end'); entry.insert(0, str(val))
        except:
            pass

    def _send(self, cmd):
        if not self._ensure_ser(): return
        self.ser.write((cmd + "\n").encode('utf-8'))
        self._log_filtered(f"> {cmd}")

    def _ensure_ser(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showwarning("Serial", "Not connected.")
            return False
        return True

    # Commands
    def cmd_get_pid(self):
        self._send("GET PID")

    def cmd_set_pid(self):
        if not self._ensure_ser(): return
        try:
            kp = float(self.e_kp.get()); ki = float(self.e_ki.get()); kd = float(self.e_kd.get())
        except:
            messagebox.showerror("Parse", "KP/KI/KD must be numbers"); return
        self._send(f"SET PID {kp:.6f} {ki:.6f} {kd:.6f}")

    def cmd_get_status(self):
        self._send("GET STATUS")

    def cmd_calibrate(self):
        self._send("CALIBRATE")

    def cmd_save_cal(self):
        self._send("SAVE_CAL")

    def cmd_load_cal(self):
        self._send("LOAD_CAL")

    def cmd_get_cal_info(self):
        self._send("GET_CAL_INFO")

    def cmd_test_on(self):
        self._send("TEST_MODE_ON")

    def cmd_test_off(self):
        self._send("TEST_MODE_OFF")

    def cmd_estop(self):
        self._send("ESTOP")
        self.estop_active = True
        self.b_estop.config(relief="sunken", bg="darkred")
        self.root.after(2000, self._reset_estop)

    def _reset_estop(self):
        self.estop_active = False
        self.b_estop.config(relief="raised", bg="red")

    def manual_send(self):
        if not self._ensure_ser(): return
        txt = self.manual.get().strip()
        if not txt: return
        self._send(txt)
        self.manual.delete(0, 'end')

    def on_close(self):
        if self.ser:
            self._disconnect()
        self.root.quit()

if __name__ == "__main__":
    root = tk.Tk()
    gui = PIDGui(root)
    root.protocol("WM_DELETE_WINDOW", gui.on_close)
    root.mainloop()
