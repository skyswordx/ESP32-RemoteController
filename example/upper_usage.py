#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-RemoteController 夹爪控制上位机脚本
用于在Ubuntu小主机上与ESP32进行串口通信，控制夹爪舵机
作者: AI Assistant
日期: 2025-09-14
"""

import serial
import time
import threading
import sys
import os
import re
from typing import Dict, List, Optional, Tuple

class ESP32GripperController:
    """ESP32夹爪控制器类"""
    
    def __init__(self, port: str = '/dev/ttyUSB0', baudrate: int = 115200, servo_id: int = 1):
        """
        初始化ESP32夹爪控制器
        
        Args:
            port: 串口设备路径，通常为 /dev/ttyUSB0 或 /dev/ttyACM0
            baudrate: 波特率，默认115200
            servo_id: 舵机ID，默认为1
        """
        self.port = port
        self.baudrate = baudrate
        self.servo_id = servo_id
        self.serial_conn: Optional[serial.Serial] = None
        self.is_connected = False
        self.read_thread: Optional[threading.Thread] = None
        self.running = False
        self.response_buffer = []
        self.response_lock = threading.Lock()
        
        # 夹爪物理参数
        self.angle_min = 101.00  # 完全张开角度
        self.angle_max = 147.00  # 完全闭合角度
        self.default_move_time = 2000  # 默认运动时间(ms)

    def connect(self) -> bool:
        """
        连接到ESP32设备
        
        Returns:
            bool: 连接成功返回True，失败返回False
        """
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1,
                write_timeout=1
            )
            self.is_connected = True
            self.running = True
            
            # 启动读取线程
            self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
            self.read_thread.start()
            
            print(f"✅ 成功连接到 {self.port}")
            print("等待ESP32响应...")
            time.sleep(2)  # 等待ESP32启动完成
            
            # 测试连接
            if self._test_connection():
                print("🔗 ESP32连接验证成功")
                return True
            else:
                print("⚠️  ESP32连接验证失败，但串口已连接")
                return True  # 仍然返回True，因为串口连接成功
                
        except serial.SerialException as e:
            print(f"❌ 连接失败: {e}")
            return False

    def disconnect(self):
        """断开连接"""
        self.running = False
        self.is_connected = False
        
        if self.read_thread:
            self.read_thread.join(timeout=1)
            
        if self.serial_conn:
            self.serial_conn.close()
            print("🔌 已断开连接")

    def _read_loop(self):
        """串口读取循环线程"""
        while self.running and self.serial_conn:
            try:
                if self.serial_conn.in_waiting > 0:
                    data = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                    if data:
                        with self.response_lock:
                            self.response_buffer.append(data)
                        print(f"📥 ESP32: {data}")
                        
            except Exception as e:
                if self.running:
                    print(f"⚠️  读取错误: {e}")
                break
                
        print("📖 读取线程已停止")

    def _send_command(self, command: str) -> bool:
        """
        发送命令到ESP32
        
        Args:
            command: 要发送的命令字符串
            
        Returns:
            bool: 发送成功返回True，失败返回False
        """
        if not self.is_connected or not self.serial_conn:
            print("❌ 设备未连接")
            return False
            
        try:
            # 确保命令以换行符结尾
            if not command.endswith('\n'):
                command += '\n'
                
            self.serial_conn.write(command.encode('utf-8'))
            self.serial_conn.flush()
            
            print(f"📤 发送: {command.strip()}")
            return True
            
        except Exception as e:
            print(f"❌ 发送失败: {e}")
            return False

    def _wait_for_response(self, keyword: str = None, timeout: float = 10.0) -> List[str]:
        """
        等待ESP32响应
        
        Args:
            keyword: 等待包含特定关键字的响应
            timeout: 超时时间(秒)
            
        Returns:
            List[str]: 接收到的响应列表
        """
        start_time = time.time()
        responses = []
        
        while time.time() - start_time < timeout:
            with self.response_lock:
                if self.response_buffer:
                    response = self.response_buffer.pop(0)
                    responses.append(response)
                    
                    if keyword and keyword in response:
                        break
                        
            time.sleep(0.01)  # 短暂休眠避免CPU占用过高
            
        return responses

    def _test_connection(self) -> bool:
        """测试ESP32连接"""
        # 清空响应缓冲区
        with self.response_lock:
            self.response_buffer.clear()
            
        # 发送help命令测试
        if self._send_command("help"):
            responses = self._wait_for_response("Available commands:", timeout=3.0)
            return len(responses) > 0
        return False

    def get_servo_status(self) -> Optional[Dict[str, float]]:
        """
        获取舵机状态信息
        
        Returns:
            Dict[str, float]: 包含角度、温度、电压的字典，失败返回None
        """
        # 清空响应缓冲区
        with self.response_lock:
            self.response_buffer.clear()
            
        command = f"servo_status {self.servo_id}"
        if not self._send_command(command):
            return None
            
        responses = self._wait_for_response("状态:", timeout=5.0)
        
        for response in responses:
            if "状态:" in response:
                # 解析响应: "Servo 1 状态: 角度=123.45°, 温度=35°C, 电压=6.12V"
                match = re.search(r'角度=([\d.]+)°.*温度=(\d+)°C.*电压=([\d.]+)V', response)
                if match:
                    return {
                        'angle': float(match.group(1)),
                        'temperature': int(match.group(2)),
                        'voltage': float(match.group(3))
                    }
        
        print("⚠️  无法解析舵机状态响应")
        return None

    def read_current_position(self) -> Optional[float]:
        """
        读取舵机当前位置
        
        Returns:
            float: 当前角度，失败返回None
        """
        # 清空响应缓冲区
        with self.response_lock:
            self.response_buffer.clear()
            
        command = f"servo_read_now_position {self.servo_id}"
        if not self._send_command(command):
            return None
            
        responses = self._wait_for_response("实时位置:", timeout=5.0)
        
        for response in responses:
            if "实时位置:" in response:
                # 解析响应: "Servo 1 实时位置: 角度=123.45°"
                match = re.search(r'角度=([\d.]+)°', response)
                if match:
                    return float(match.group(1))
        
        print("⚠️  无法解析舵机位置响应")
        return None

    def control_gripper(self, normalized_value: float, move_time: int = None) -> Optional[float]:
        """
        控制夹爪到指定位置
        
        Args:
            normalized_value: 归一化值 (0.0=完全张开, 1.0=完全闭合)
            move_time: 运动时间(毫秒)，默认使用类属性值
            
        Returns:
            float: 实际到达的归一化位置，失败返回None
        """
        if not (0.0 <= normalized_value <= 1.0):
            print(f"❌ 归一化值必须在0.0~1.0范围内，当前值: {normalized_value}")
            return None
            
        if move_time is None:
            move_time = self.default_move_time
            
        print(f"🎯 控制夹爪: {normalized_value:.3f} ({'闭合' if normalized_value > 0.5 else '张开'})")
        
        # 清空响应缓冲区
        with self.response_lock:
            self.response_buffer.clear()
            
        command = f"gripper_control {self.servo_id} {normalized_value:.3f} {move_time}"
        if not self._send_command(command):
            return None
            
        # 等待控制完成，超时时间为运动时间的2倍 + 5秒
        timeout = (move_time / 1000.0) * 2 + 5
        responses = self._wait_for_response("GRIPPER_RESULT:", timeout=timeout)
        
        # 解析结果
        for response in responses:
            if "GRIPPER_RESULT:" in response:
                # 解析响应: "GRIPPER_RESULT:0.123"
                match = re.search(r'GRIPPER_RESULT:([\d.]+)', response)
                if match:
                    actual_normalized = float(match.group(1))
                    print(f"✅ 夹爪控制完成: {actual_normalized:.3f}")
                    return actual_normalized
                    
        # 检查是否有错误响应
        for response in responses:
            if "ERROR:" in response:
                print(f"❌ 夹爪控制失败: {response}")
                return None
                
        print("⚠️  夹爪控制超时或无响应")
        return None

    def calibrate_gripper(self) -> Tuple[Optional[float], Optional[float]]:
        """
        夹爪校准：分别移动到完全张开和完全闭合位置
        
        Returns:
            Tuple[float, float]: (张开位置的归一化值, 闭合位置的归一化值)
        """
        print("🔧 开始夹爪校准...")
        
        # 校准张开位置 (0.0)
        print("📐 校准张开位置...")
        open_result = self.control_gripper(0.0, 3000)  # 使用较长时间确保到位
        
        time.sleep(1)  # 稳定等待
        
        # 校准闭合位置 (1.0)
        print("📐 校准闭合位置...")
        close_result = self.control_gripper(1.0, 3000)  # 使用较长时间确保到位
        
        print(f"📋 校准结果: 张开={open_result}, 闭合={close_result}")
        return open_result, close_result

    def test_gripper_sequence(self, steps: int = 5) -> List[Tuple[float, Optional[float]]]:
        """
        测试夹爪连续控制序列
        
        Args:
            steps: 测试步数
            
        Returns:
            List[Tuple[float, float]]: [(目标值, 实际值), ...]
        """
        print(f"🧪 开始夹爪序列测试 ({steps}步)...")
        results = []
        
        for i in range(steps):
            target = i / (steps - 1)  # 0.0 到 1.0 的均匀分布
            print(f"\n📍 步骤 {i+1}/{steps}: 目标位置 {target:.3f}")
            
            actual = self.control_gripper(target, 2000)
            results.append((target, actual))
            
            if actual is not None:
                error = abs(actual - target)
                print(f"   误差: {error:.3f}")
            else:
                print("   ❌ 控制失败")
                
            time.sleep(0.5)  # 步骤间等待
            
        return results

    def interactive_mode(self):
        """交互模式"""
        print("\n🚀 进入夹爪控制交互模式")
        print("=" * 60)
        print("命令:")
        print("  <数字>     - 控制夹爪 (0.0=张开, 1.0=闭合)")
        print("  status     - 查看舵机状态")
        print("  position   - 读取当前位置")
        print("  calibrate  - 夹爪校准")
        print("  test       - 序列测试")
        print("  help       - ESP32帮助")
        print("  quit       - 退出")
        print("=" * 60)
        
        while True:
            try:
                user_input = input("Gripper> ").strip()
                
                if not user_input:
                    continue
                    
                if user_input.lower() in ['quit', 'exit', 'q']:
                    break
                elif user_input.lower() == 'status':
                    status = self.get_servo_status()
                    if status:
                        print(f"📊 舵机状态: {status}")
                    else:
                        print("❌ 获取状态失败")
                elif user_input.lower() == 'position':
                    pos = self.read_current_position()
                    if pos is not None:
                        # 转换为归一化值显示
                        normalized = (pos - self.angle_min) / (self.angle_max - self.angle_min)
                        normalized = max(0.0, min(1.0, normalized))
                        print(f"📍 当前位置: {pos:.2f}° (归一化: {normalized:.3f})")
                    else:
                        print("❌ 读取位置失败")
                elif user_input.lower() == 'calibrate':
                    self.calibrate_gripper()
                elif user_input.lower() == 'test':
                    results = self.test_gripper_sequence()
                    print("\n📈 测试结果总结:")
                    for i, (target, actual) in enumerate(results):
                        if actual is not None:
                            error = abs(actual - target)
                            print(f"  {i+1}: {target:.3f} -> {actual:.3f} (误差: {error:.3f})")
                        else:
                            print(f"  {i+1}: {target:.3f} -> 失败")
                elif user_input.lower() == 'help':
                    self._send_command("help")
                    time.sleep(1)  # 等待响应显示
                else:
                    # 尝试解析为数字（归一化值）
                    try:
                        value = float(user_input)
                        if 0.0 <= value <= 1.0:
                            result = self.control_gripper(value)
                            if result is not None:
                                error = abs(result - value)
                                print(f"✅ 控制结果: {value:.3f} -> {result:.3f} (误差: {error:.3f})")
                        else:
                            print("❌ 值必须在0.0~1.0范围内")
                    except ValueError:
                        print(f"❌ 未知命令: {user_input}")
                        print("输入 'help' 查看可用命令")
                        
            except KeyboardInterrupt:
                print("\n👋 用户中断")
                break
            except EOFError:
                print("\n👋 输入结束")
                break

def scan_serial_ports() -> List[str]:
    """扫描可用的串口设备"""
    import glob
    ports = []
    
    # 常见的Linux串口设备路径
    patterns = [
        '/dev/ttyUSB*',
        '/dev/ttyACM*',
        '/dev/ttyS*'
    ]
    
    for pattern in patterns:
        ports.extend(glob.glob(pattern))
    
    return sorted(ports)

def main():
    """主函数"""
    print("🤖 ESP32夹爪控制器")
    print("=" * 50)
    
    # 扫描可用串口
    ports = scan_serial_ports()
    if not ports:
        print("❌ 未找到可用的串口设备")
        print("请检查:")
        print("1. ESP32是否已连接到电脑")
        print("2. 串口驱动是否已安装")
        print("3. 用户是否有串口访问权限 (sudo usermod -a -G dialout $USER)")
        return
    
    print(f"📡 发现可用串口: {', '.join(ports)}")
    
    # 选择串口
    if len(ports) == 1:
        selected_port = ports[0]
        print(f"🔌 自动选择串口: {selected_port}")
    else:
        print("\n请选择串口:")
        for i, port in enumerate(ports):
            print(f"  {i+1}. {port}")
        
        while True:
            try:
                choice = int(input("请输入选择 (1-{}): ".format(len(ports))))
                if 1 <= choice <= len(ports):
                    selected_port = ports[choice-1]
                    break
                else:
                    print("❌ 无效选择，请重新输入")
            except ValueError:
                print("❌ 请输入数字")
    
    # 获取舵机ID
    while True:
        try:
            servo_id = int(input("请输入舵机ID (默认1): ") or "1")
            if servo_id > 0:
                break
            else:
                print("❌ 舵机ID必须大于0")
        except ValueError:
            print("❌ 请输入有效数字")
    
    # 创建夹爪控制器
    controller = ESP32GripperController(port=selected_port, servo_id=servo_id)
    
    try:
        # 连接设备
        if not controller.connect():
            return
            
        # 进入交互模式
        controller.interactive_mode()
        
    except KeyboardInterrupt:
        print("\n👋 程序被用户中断")
    finally:
        controller.disconnect()

if __name__ == "__main__":
    # 检查pySerial是否已安装
    try:
        import serial
    except ImportError:
        print("❌ pySerial 未安装")
        print("请运行以下命令安装:")
        print("conda activate gestureDR")
        print("pip install pyserial")
        sys.exit(1)
    
    main()
