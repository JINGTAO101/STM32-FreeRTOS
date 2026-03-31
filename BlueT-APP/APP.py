import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time

class BluetoothApp:
    def __init__(self, root):
        self.root = root
        self.root.title("JDY-31 蓝牙控制面板")
        self.root.geometry("550x550")
        
        self.serial_port = None
        self.is_connected = False
        self.read_thread = None
        
        self.setup_ui()
        self.refresh_ports()

    def setup_ui(self):
        # 1. 连接区
        conn_frame = ttk.LabelFrame(self.root, text="蓝牙连接 (串口)")
        conn_frame.pack(padx=10, pady=5, fill="x")
        
        ttk.Label(conn_frame, text="选择端口:").pack(side="left", padx=5, pady=5)
        self.port_combobox = ttk.Combobox(conn_frame, width=15)
        self.port_combobox.pack(side="left", padx=5)
        
        ttk.Button(conn_frame, text="刷新", command=self.refresh_ports).pack(side="left", padx=5)
        self.btn_connect = ttk.Button(conn_frame, text="连接", command=self.toggle_connection)
        self.btn_connect.pack(side="left", padx=5)

        # 2. 数据显示区
        data_frame = ttk.LabelFrame(self.root, text="传感器状态")
        data_frame.pack(padx=10, pady=5, fill="x")
        
        self.lbl_temp = ttk.Label(data_frame, text="温度: -- °C", font=("Arial", 12))
        self.lbl_temp.grid(row=0, column=0, padx=20, pady=10)
        
        self.lbl_hum = ttk.Label(data_frame, text="湿度: -- %", font=("Arial", 12))
        self.lbl_hum.grid(row=0, column=1, padx=20, pady=10)

        # 3. 控制区
        ctrl_frame = ttk.LabelFrame(self.root, text="设备控制")
        ctrl_frame.pack(padx=10, pady=5, fill="x")
        
        # LED 控制
        ttk.Label(ctrl_frame, text="LED 控制:").grid(row=0, column=0, padx=5, pady=5, sticky="e")
        ttk.Button(ctrl_frame, text="打开 (LED_ON)", command=lambda: self.send_command("LED_ON")).grid(row=0, column=1, padx=5, pady=5)
        ttk.Button(ctrl_frame, text="关闭 (LED_OFF)", command=lambda: self.send_command("LED_OFF")).grid(row=0, column=2, padx=5, pady=5)
        ttk.Button(ctrl_frame, text="闪烁 (LED_BLINK)", command=lambda: self.send_command("LED_BLINK")).grid(row=0, column=3, padx=5, pady=5)
        
        # 舵机控制
        ttk.Label(ctrl_frame, text="舵机角度 (0-180):").grid(row=1, column=0, padx=5, pady=5, sticky="e")
        self.angle_var = tk.IntVar(value=90)
        self.scale_angle = ttk.Scale(ctrl_frame, from_=0, to=180, variable=self.angle_var, orient="horizontal", command=self.update_angle_lbl)
        self.scale_angle.grid(row=1, column=1, columnspan=2, sticky="ew", padx=5)
        
        self.lbl_angle = ttk.Label(ctrl_frame, text="90°")
        self.lbl_angle.grid(row=1, column=3, padx=5)
        
        ttk.Button(ctrl_frame, text="设置角度", command=self.set_angle).grid(row=1, column=4, padx=5, pady=5)

        # 获取状态
        ttk.Button(ctrl_frame, text="获取全局状态 (GET_STATUS)", command=lambda: self.send_command("GET_STATUS")).grid(row=2, column=0, columnspan=5, pady=10)

        # 4. 日志区
        log_frame = ttk.LabelFrame(self.root, text="通信日志")
        log_frame.pack(padx=10, pady=5, fill="both", expand=True)
        
        self.txt_log = tk.Text(log_frame, height=10, state="disabled")
        self.txt_log.pack(side="left", fill="both", expand=True, padx=5, pady=5)
        
        scrollbar = ttk.Scrollbar(log_frame, command=self.txt_log.yview)
        scrollbar.pack(side="right", fill="y")
        self.txt_log['yscrollcommand'] = scrollbar.set

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        self.port_combobox['values'] = [port.device for port in ports]
        if ports:
            self.port_combobox.current(0)

    def toggle_connection(self):
        if not self.is_connected:
            port = self.port_combobox.get()
            if not port:
                messagebox.showwarning("警告", "请先选择一个串口！")
                return
            try:
                # JDY-31 默认波特率通常为 9600
                self.serial_port = serial.Serial(port, 9600, timeout=1)
                self.is_connected = True
                self.btn_connect.config(text="断开")
                self.log(f"成功连接到 {port}")
                
                # 开启接收线程
                self.read_thread = threading.Thread(target=self.read_serial_data, daemon=True)
                self.read_thread.start()
                
            except Exception as e:
                messagebox.showerror("错误", f"连接失败: {str(e)}")
        else:
            self.is_connected = False
            if self.serial_port:
                self.serial_port.close()
            self.btn_connect.config(text="连接")
            self.log("已断开连接")

    def update_angle_lbl(self, val):
        self.lbl_angle.config(text=f"{int(float(val))}°")

    def set_angle(self):
        angle = self.angle_var.get()
        self.send_command(f"SET_ANGLE={angle}")

    def send_command(self, cmd):
        if self.is_connected and self.serial_port:
            try:
                # 假设发送命令以换行符结尾
                full_cmd = f"{cmd}\r\n"
                self.serial_port.write(full_cmd.encode('utf-8'))
                self.log(f"发送: {cmd}")
            except Exception as e:
                self.log(f"发送失败: {str(e)}")
        else:
            messagebox.showwarning("警告", "请先连接蓝牙模块！")

    def read_serial_data(self):
        buffer = ''
        while self.is_connected:
            try:
                if self.serial_port.in_waiting:
                    data = self.serial_port.read(self.serial_port.in_waiting).decode('utf-8', errors='ignore')
                    buffer += data
                    
                    # 按行解析 (假设单片机加上了换行符 \n 结尾)
                    if '\n' in buffer:
                        lines = buffer.split('\n')
                        for line in lines[:-1]:
                            clean_line = line.strip()
                            if clean_line:
                                self.root.after(0, self.process_received_data, clean_line)
                        buffer = lines[-1]
            except Exception as e:
                self.root.after(0, self.log, f"读取错误: {str(e)}")
                break
            time.sleep(0.05)
            
    def process_received_data(self, data):
        self.log(f"接收: {data}")
        
        # 解析温湿度 T:24.1C H:67.0%
        # 解析 GET_STATUS 返回 LED:OFF SG90:0 T:24.1C H:61.0%
        try:
            parts = data.split()
            for part in parts:
                if part.startswith("T:"):
                    temp = part[2:]
                    self.lbl_temp.config(text=f"温度: {temp}")
                elif part.startswith("H:"):
                    hum = part[2:]
                    self.lbl_hum.config(text=f"湿度: {hum}")
        except Exception:
            pass

    def log(self, text):
        self.txt_log.config(state="normal")
        self.txt_log.insert(tk.END, f"[{time.strftime('%H:%M:%S')}] {text}\n")
        self.txt_log.see(tk.END)
        self.txt_log.config(state="disabled")

if __name__ == '__main__':
    root = tk.Tk()
    app = BluetoothApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.serial_port.close() if app.serial_port else None, root.destroy()))
    root.mainloop()
