import asyncio
from bleak import BleakClient, BleakScanner
from .constants import *

class BLEService:
    def __init__(self):
        self.client = None
        self.connected = False
        self.telemetry_callback = None
        self._chars = {}

    async def scan_and_connect(self):
        print("Scanning for devices...")
        devices = await BleakScanner.discover(timeout=5.0)
        
        for d in devices:
            if d.name == DEVICE_NAME:
                print(f"Found {DEVICE_NAME}!")
                await self.connect(d)
                return True
        
        print(f"{DEVICE_NAME} not found")
        return False

    async def connect(self, device):
        try:
            self.client = BleakClient(device)
            await self.client.connect()
            self.connected = True
            print(f"Connected to {device.name}")
            
            await self._discover_characteristics()
            
            if CHAR_TELEMETRY_UUID in self._chars:
                await self._chars[CHAR_TELEMETRY_UUID].start_notify(
                    self._on_telemetry
                )
            
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            self.connected = False
            return False

    async def _discover_characteristics(self):
        for service in self.client.services:
            if service.uuid.lower() == SERVICE_UUID.lower():
                for char in service.characteristics:
                    self._chars[char.uuid.lower()] = char

    def _on_telemetry(self, sender, data):
        try:
            decoded = data.decode('utf-8')
            parts = decoded.split(',')
            if len(parts) >= 7 and self.telemetry_callback:
                self.telemetry_callback({
                    'pitch': float(parts[0]),
                    'roll': float(parts[1]),
                    'yaw': float(parts[2]),
                    'left_speed': float(parts[3]),
                    'right_speed': float(parts[4]),
                    'mode': int(parts[5]),
                    'estop': parts[6] == '1'
                })
        except Exception as e:
            print(f"Telemetry parse error: {e}")

    async def set_pid(self, kp, ki, kd):
        if not self.connected:
            return
        
        try:
            if CHAR_KP_UUID in self._chars:
                await self._chars[CHAR_KP_UUID].write(str(kp).encode())
            if CHAR_KI_UUID in self._chars:
                await self._chars[CHAR_KI_UUID].write(str(ki).encode())
            if CHAR_KD_UUID in self._chars:
                await self._chars[CHAR_KD_UUID].write(str(kd).encode())
        except Exception as e:
            print(f"PID write error: {e}")

    async def set_tank_control(self, left, right):
        if not self.connected:
            return
        
        try:
            if CHAR_TANK_LEFT_UUID in self._chars:
                await self._chars[CHAR_TANK_LEFT_UUID].write(str(left).encode())
            if CHAR_TANK_RIGHT_UUID in self._chars:
                await self._chars[CHAR_TANK_RIGHT_UUID].write(str(right).encode())
        except Exception as e:
            print(f"Tank write error: {e}")

    async def set_control_mode(self, mode):
        if not self.connected:
            return
        
        try:
            if CHAR_CONTROL_MODE_UUID in self._chars:
                await self._chars[CHAR_CONTROL_MODE_UUID].write(bytes([mode]))
        except Exception as e:
            print(f"Mode write error: {e}")

    async def emergency_stop(self):
        if not self.connected:
            return
        
        try:
            if CHAR_EMERGENCY_STOP_UUID in self._chars:
                await self._chars[CHAR_EMERGENCY_STOP_UUID].write(bytes([1]))
        except Exception as e:
            print(f"Estop write error: {e}")

    async def calibrate(self):
        if not self.connected:
            return
        
        try:
            if CHAR_CALIBRATE_UUID in self._chars:
                await self._chars[CHAR_CALIBRATE_UUID].write(bytes([1]))
        except Exception as e:
            print(f"Calibrate write error: {e}")

    async def disconnect(self):
        if self.client:
            await self.client.disconnect()
        self.connected = False
        self._chars = {}
