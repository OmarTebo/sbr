import tkinter as tk
from tkinter import ttk, messagebox
import asyncio
import threading
from .ble_service import BLEService
from .constants import CONTROL_MODE_NAMES

class SBRControllerApp:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("SBR Controller")
        self.root.geometry("500x700")
        
        self.ble = BLEService()
        self.loop = None
        self.loop_thread = None
        
        self.control_mode = 0
        self.emergency_stop = False
        self.left_tank = 0.0
        self.right_tank = 0.0
        self.kp = 1.0
        self.ki = 0.0
        self.kd = 1.0
        
        self.telemetry_data = {'pitch': 0, 'roll': 0, 'yaw': 0}
        self.telemetry_history = []
        self.max_history = 100
        
        self._setup_ui()
        self._start_loop()

    def _setup_ui(self):
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        self.root.columnconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        
        row = 0
        
        self.status_label = ttk.Label(main_frame, text="Disconnected", font=('Arial', 12, 'bold'))
        self.status_label.grid(row=row, column=0, pady=5)
        row += 1
        
        connect_frame = ttk.Frame(main_frame)
        connect_frame.grid(row=row, column=0, pady=5)
        ttk.Button(connect_frame, text="Scan & Connect", command=self._connect).pack(side=tk.LEFT, padx=5)
        ttk.Button(connect_frame, text="Disconnect", command=self._disconnect).pack(side=tk.LEFT, padx=5)
        row += 1
        
        sep = ttk.Separator(main_frame, orient='horizontal')
        sep.grid(row=row, column=0, sticky=(tk.W, tk.E), pady=10)
        row += 1
        
        telemetry_frame = ttk.LabelFrame(main_frame, text="Telemetry", padding="10")
        telemetry_frame.grid(row=row, column=0, sticky=(tk.W, tk.E), pady=5)
        telemetry_frame.columnconfigure(1, weight=1)
        
        ttk.Label(telemetry_frame, text="Pitch:").grid(row=0, column=0, sticky=tk.W)
        self.pitch_label = ttk.Label(telemetry_frame, text="0.0°", font=('Arial', 14, 'bold'))
        self.pitch_label.grid(row=0, column=1, sticky=tk.W)
        
        ttk.Label(telemetry_frame, text="Roll:").grid(row=1, column=0, sticky=tk.W)
        self.roll_label = ttk.Label(telemetry_frame, text="0.0°", font=('Arial', 14, 'bold'))
        self.roll_label.grid(row=1, column=1, sticky=tk.W)
        
        ttk.Label(telemetry_frame, text="Yaw:").grid(row=2, column=0, sticky=tk.W)
        self.yaw_label = ttk.Label(telemetry_frame, text="0.0°", font=('Arial', 14, 'bold'))
        self.yaw_label.grid(row=2, column=1, sticky=tk.W)
        
        self.mode_label = ttk.Label(telemetry_frame, text="Mode: AUTO", font=('Arial', 10))
        self.mode_label.grid(row=3, column=0, columnspan=2, pady=(10, 0))
        row += 1
        
        mode_frame = ttk.LabelFrame(main_frame, text="Control Mode", padding="10")
        mode_frame.grid(row=row, column=0, sticky=(tk.W, tk.E), pady=5)
        
        self.mode_var = tk.IntVar(value=0)
        for i, name in enumerate(CONTROL_MODE_NAMES):
            ttk.Radiobutton(
                mode_frame, text=name, value=i,
                variable=self.mode_var, command=self._on_mode_change
            ).pack(side=tk.LEFT, padx=10)
        row += 1
        
        tank_frame = ttk.LabelFrame(main_frame, text="Tank Control", padding="10")
        tank_frame.grid(row=row, column=0, sticky=(tk.W, tk.E), pady=5)
        tank_frame.columnconfigure(0, weight=1)
        tank_frame.columnconfigure(2, weight=1)
        
        ttk.Label(tank_frame, text="Left").grid(row=0, column=0)
        ttk.Label(tank_frame, text="Right").grid(row=0, column=2)
        
        self.left_var = tk.DoubleVar(value=0)
        self.right_var = tk.DoubleVar(value=0)
        
        self.left_slider = ttk.Scale(tank_frame, from_=-100, to=100,
                                       variable=self.left_var, command=self._on_tank_change)
        self.left_slider.grid(row=1, column=0, padx=5)
        
        self.right_slider = ttk.Scale(tank_frame, from_=-100, to=100,
                                        variable=self.right_var, command=self._on_tank_change)
        self.right_slider.grid(row=1, column=2, padx=5)
        
        self.left_value_label = ttk.Label(tank_frame, text="0%")
        self.left_value_label.grid(row=2, column=0)
        self.right_value_label = ttk.Label(tank_frame, text="0%")
        self.right_value_label.grid(row=2, column=2)
        
        quick_frame = ttk.Frame(tank_frame)
        quick_frame.grid(row=3, column=0, columnspan=3, pady=10)
        ttk.Button(quick_frame, text="◄◄", command=lambda: self._quick_tank(-50)).pack(side=tk.LEFT, padx=5)
        ttk.Button(quick_frame, text="STOP", command=self._quick_stop).pack(side=tk.LEFT, padx=5)
        ttk.Button(quick_frame, text="►►", command=lambda: self._quick_tank(50)).pack(side=tk.LEFT, padx=5)
        row += 1
        
        pid_frame = ttk.LabelFrame(main_frame, text="PID Tuning", padding="10")
        pid_frame.grid(row=row, column=0, sticky=(tk.W, tk.E), pady=5)
        pid_frame.columnconfigure(1, weight=1)
        pid_frame.columnconfigure(3, weight=1)
        
        ttk.Label(pid_frame, text="Kp:").grid(row=0, column=0, sticky=tk.W)
        self.kp_var = tk.DoubleVar(value=1.0)
        ttk.Scale(pid_frame, from_=0, to=10, variable=self.kp_var, command=self._on_pid_change).grid(row=0, column=1, sticky=(tk.W, tk.E))
        self.kp_label = ttk.Label(pid_frame, text="1.0")
        self.kp_label.grid(row=0, column=2, padx=5)
        
        ttk.Label(pid_frame, text="Ki:").grid(row=1, column=0, sticky=tk.W)
        self.ki_var = tk.DoubleVar(value=0.0)
        ttk.Scale(pid_frame, from_=0, to=1, variable=self.ki_var, command=self._on_pid_change).grid(row=1, column=1, sticky=(tk.W, tk.E))
        self.ki_label = ttk.Label(pid_frame, text="0.0")
        self.ki_label.grid(row=1, column=2, padx=5)
        
        ttk.Label(pid_frame, text="Kd:").grid(row=2, column=0, sticky=tk.W)
        self.kd_var = tk.DoubleVar(value=1.0)
        ttk.Scale(pid_frame, from_=0, to=10, variable=self.kd_var, command=self._on_pid_change).grid(row=2, column=1, sticky=(tk.W, tk.E))
        self.kd_label = ttk.Label(pid_frame, text="1.0")
        self.kd_label.grid(row=2, column=2, padx=5)
        
        row += 1
        
        action_frame = ttk.LabelFrame(main_frame, text="Actions", padding="10")
        action_frame.grid(row=row, column=0, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Button(action_frame, text="Calibrate", command=self._calibrate).pack(side=tk.LEFT, padx=5)
        self.estop_button = tk.Button(action_frame, text="E-STOP", bg='red', fg='white',
                                       font=('Arial', 12, 'bold'), command=self._emergency_stop)
        self.estop_button.pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)

    def _start_loop(self):
        def run_loop():
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)
            self.loop.run_forever()
        
        self.loop_thread = threading.Thread(target=run_loop, daemon=True)
        self.loop_thread.start()

    def _run_async(self, coro):
        if self.loop:
            asyncio.run_coroutine_threadsafe(coro, self.loop)

    def _connect(self):
        self._run_async(self._do_connect())

    async def _do_connect(self):
        if await self.ble.scan_and_connect():
            self.ble.telemetry_callback = self._on_telemetry
            self.status_label.config(text="Connected", foreground='green')
        else:
            self.status_label.config(text="Not Found", foreground='red')
            messagebox.showerror("Connection", "SBR-Bot not found!")

    def _disconnect(self):
        self._run_async(self.ble.disconnect())
        self.status_label.config(text="Disconnected", foreground='red')

    def _on_telemetry(self, data):
        def update():
            self.telemetry_data = data
            self.pitch_label.config(text=f"{data['pitch']:.1f}°")
            self.roll_label.config(text=f"{data['roll']:.1f}°")
            self.yaw_label.config(text=f"{data['yaw']:.1f}°")
            self.mode_label.config(text=f"Mode: {CONTROL_MODE_NAMES[data['mode']]}")
            
            if data['estop']:
                self.emergency_stop = True
                self.estop_button.config(bg='darkred', text="E-STOP ACTIVE")
        
        self.root.after(0, update)

    def _on_mode_change(self):
        mode = self.mode_var.get()
        self.control_mode = mode
        self._run_async(self.ble.set_control_mode(mode))

    def _on_tank_change(self, value=None):
        self.left_tank = self.left_var.get()
        self.right_tank = self.right_var.get()
        self.left_value_label.config(text=f"{self.left_tank:.0f}%")
        self.right_value_label.config(text=f"{self.right_tank:.0f}%")
        self._run_async(self.ble.set_tank_control(self.left_tank, self.right_tank))

    def _quick_tank(self, value):
        self.left_var.set(value)
        self.right_var.set(value)
        self._on_tank_change()

    def _quick_stop(self):
        self.left_var.set(0)
        self.right_var.set(0)
        self._on_tank_change()

    def _on_pid_change(self, value=None):
        self.kp = self.kp_var.get()
        self.ki = self.ki_var.get()
        self.kd = self.kd_var.get()
        self.kp_label.config(text=f"{self.kp:.3f}")
        self.ki_label.config(text=f"{self.ki:.3f}")
        self.kd_label.config(text=f"{self.kd:.3f}")
        self._run_async(self.ble.set_pid(self.kp, self.ki, self.kd))

    def _calibrate(self):
        self._run_async(self.ble.calibrate())
        messagebox.showinfo("Calibrate", "Calibration triggered!")

    def _emergency_stop(self):
        if self.emergency_stop:
            self.emergency_stop = False
            self.estop_button.config(bg='red', text="E-STOP")
            self.left_var.set(0)
            self.right_var.set(0)
            self._on_tank_change()
        else:
            self._run_async(self.ble.emergency_stop())
            self.emergency_stop = True
            self.estop_button.config(bg='darkred', text="E-STOP ACTIVE")
            messagebox.showwarning("Emergency Stop", "Motors disabled!")

    def run(self):
        self.root.mainloop()
