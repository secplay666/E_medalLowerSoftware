#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
图像传输上位机测试程序
实现新的图像传输协议，支持黑白和红白图像数据传输
"""

import serial
import time
import struct
import random
from typing import List, Tuple, Optional

# 协议常量
PROTOCOL_MAGIC_HOST = 0xA5A5
PROTOCOL_MAGIC_MCU = 0x5A5A
PROTOCOL_END_HOST = 0xA5A5AFAF
PROTOCOL_END_MCU = 0x5A5A5F5F

# 命令类型
CMD_IMAGE_TRANSFER = 0xC0
CMD_IMAGE_DATA = 0xD0
CMD_TRANSFER_END = 0xC1

# 颜色类型
COLOR_TYPE_BW = 0x00
COLOR_TYPE_RED = 0x10

# MCU回复状态
MCU_STATUS_OK = 0x01
MCU_STATUS_BUSY = 0x02
MCU_STATUS_ERROR = 0xFF

# 数据帧回复状态
DATA_STATUS_OK = 0x00
DATA_STATUS_CRC_ERROR = 0x10
DATA_STATUS_TIMEOUT = 0x30

# 传输参数
IMAGE_WIDTH = 400
IMAGE_HEIGHT = 300
IMAGE_SIZE = (IMAGE_WIDTH * IMAGE_HEIGHT) // 8  # 15000字节
IMAGE_PAGES_PER_COLOR = 61
IMAGE_DATA_PER_PAGE = 248
IMAGE_LAST_PAGE_DATA_SIZE = 120
FRAME_DATA_SIZE = 54
FRAME_LAST_DATA_SIZE = 32
FRAMES_PER_PAGE = 5

# CRC32多项式
CRC32_POLYNOMIAL = 0xEDB88320

class ImageSender:
    def __init__(self, port: str, baudrate: int = 115200):
        """
        初始化图像发送器
        
        Args:
            port: 串口名称
            baudrate: 波特率
        """
        self.serial = serial.Serial(port, baudrate, timeout=5)
        self.slot = 0
        self.retry_count = 0
        self.max_retries = 3
        
    def calculate_crc32(self, data: bytes) -> int:
        """
        计算CRC32校验值
        
        Args:
            data: 输入数据
            
        Returns:
            CRC32校验值
        """
        crc = 0xFFFFFFFF
        
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ CRC32_POLYNOMIAL
                else:
                    crc >>= 1
                    
        return (~crc) & 0xFFFFFFFF
    
    def generate_test_image(self, color_type: int) -> bytes:
        """
        生成测试图像数据
        
        Args:
            color_type: 颜色类型 (0=黑白, 1=红白)
            
        Returns:
            图像数据字节
        """
        if color_type == 0:  # 黑白图像
            # 生成棋盘格图案
            data = bytearray(IMAGE_SIZE)
            for i in range(IMAGE_SIZE):
                # 创建简单的条纹图案
                if (i // 50) % 2 == 0:
                    data[i] = 0xAA  # 10101010
                else:
                    data[i] = 0x55  # 01010101
        else:  # 红白图像
            # 生成不同的图案
            data = bytearray(IMAGE_SIZE)
            for i in range(IMAGE_SIZE):
                # 创建渐变图案
                data[i] = (i * 255) // IMAGE_SIZE
                
        return bytes(data)
    
    def send_start_frame(self, slot: int, color_type: int) -> bool:
        """
        发送首帧
        
        Args:
            slot: 图像槽位 (0-15)
            color_type: 颜色类型 (0=黑白, 1=红白)
            
        Returns:
            发送成功返回True
        """
        # 构造首帧
        frame = struct.pack('<HHBBBBBB',
                           PROTOCOL_MAGIC_HOST,  # magic
                           CMD_IMAGE_TRANSFER | (slot & 0x0F),  # command + slot
                           color_type | (slot & 0x0F),  # color + slot
                           0,  # padding
                           (PROTOCOL_END_HOST >> 24) & 0xFF,
                           (PROTOCOL_END_HOST >> 16) & 0xFF,
                           (PROTOCOL_END_HOST >> 8) & 0xFF,
                           PROTOCOL_END_HOST & 0xFF)
        
        print(f"发送首帧: slot={slot}, color={'黑白' if color_type == 0 else '红白'}")
        self.serial.write(frame)
        
        # 等待回复
        return self.wait_start_reply()
    
    def wait_start_reply(self) -> bool:
        """
        等待首帧回复
        
        Returns:
            收到正确回复返回True
        """
        try:
            # 读取回复帧 (10字节)
            reply = self.serial.read(10)
            if len(reply) != 10:
                print(f"首帧回复长度错误: {len(reply)}")
                return False
            
            magic, command, slot_color, status, reserved = struct.unpack('<HHHBB', reply[:8])
            end_magic = struct.unpack('<I', reply[6:10])[0]
            
            if magic != PROTOCOL_MAGIC_MCU:
                print(f"首帧回复魔法数错误: 0x{magic:04X}")
                return False
                
            if end_magic != PROTOCOL_END_MCU:
                print(f"首帧回复结束魔法数错误: 0x{end_magic:08X}")
                return False
            
            if status == MCU_STATUS_OK:
                print("首帧回复: 成功")
                return True
            elif status == MCU_STATUS_BUSY:
                print("首帧回复: 忙碌，等待1秒后重试")
                time.sleep(1)
                return False
            else:
                print(f"首帧回复: 错误 (status=0x{status:02X})")
                return False
                
        except Exception as e:
            print(f"等待首帧回复时出错: {e}")
            return False
    
    def send_data_frames(self, page_data: bytes, page_seq: int, slot: int, color_type: int) -> bool:
        """
        发送一页的数据帧 (5帧)
        
        Args:
            page_data: 页数据 (248字节)
            page_seq: 页序列号 (1-61)
            slot: 图像槽位
            color_type: 颜色类型
            
        Returns:
            发送成功返回True
        """
        # 计算页数据的CRC32
        actual_size = IMAGE_LAST_PAGE_DATA_SIZE if page_seq == 61 else IMAGE_DATA_PER_PAGE
        page_crc = self.calculate_crc32(page_data[:actual_size])
        
        # 发送前4帧 (每帧54字节)
        for frame_seq in range(1, 5):
            start_pos = (frame_seq - 1) * FRAME_DATA_SIZE
            frame_data = page_data[start_pos:start_pos + FRAME_DATA_SIZE]
            
            if not self.send_data_frame(frame_data, page_seq, frame_seq, slot, color_type):
                return False
        
        # 发送第5帧 (32字节数据 + 4字节CRC)
        start_pos = 4 * FRAME_DATA_SIZE
        frame_data = page_data[start_pos:start_pos + FRAME_LAST_DATA_SIZE]
        crc_bytes = struct.pack('<I', page_crc)
        
        if not self.send_data_frame(frame_data + crc_bytes, page_seq, 5, slot, color_type):
            return False
        
        # 等待页完成回复
        return self.wait_data_reply(page_seq)
    
    def send_data_frame(self, data: bytes, page_seq: int, frame_seq: int, slot: int, color_type: int) -> bool:
        """
        发送单个数据帧
        
        Args:
            data: 帧数据
            page_seq: 页序列号
            frame_seq: 帧序列号
            slot: 图像槽位
            color_type: 颜色类型
            
        Returns:
            发送成功返回True
        """
        # 构造数据帧头
        frame_header = struct.pack('<HHBBBB',
                                  PROTOCOL_MAGIC_HOST,  # magic
                                  CMD_IMAGE_DATA,  # command
                                  color_type | (slot & 0x0F),  # color + slot
                                  page_seq,  # page sequence
                                  frame_seq,  # frame sequence
                                  0)  # padding
        
        # 添加数据
        frame = frame_header + data
        
        # 填充到64字节 (如果需要)
        if len(frame) < 64:
            frame += b'\x00' * (64 - len(frame))
        
        # 添加结束魔法数
        frame = frame[:-4] + struct.pack('<I', PROTOCOL_END_HOST)
        
        self.serial.write(frame)
        return True
    
    def wait_data_reply(self, page_seq: int) -> bool:
        """
        等待数据帧回复
        
        Args:
            page_seq: 页序列号
            
        Returns:
            收到正确回复返回True
        """
        try:
            # 读取回复帧 (6字节)
            reply = self.serial.read(6)
            if len(reply) != 6:
                print(f"数据帧回复长度错误: {len(reply)}")
                return False
            
            magic, command, slot_color, page, status = struct.unpack('<HHBBB', reply)
            
            if magic != PROTOCOL_MAGIC_MCU:
                print(f"数据帧回复魔法数错误: 0x{magic:04X}")
                return False
            
            if page != page_seq:
                print(f"数据帧回复页序列错误: 期望{page_seq}, 收到{page}")
                return False
            
            if status == DATA_STATUS_OK:
                print(f"页{page_seq}发送成功")
                return True
            elif status == DATA_STATUS_CRC_ERROR:
                print(f"页{page_seq}CRC错误，需要重传")
                return False
            elif (status & 0xF0) == 0x20:
                missing_frame = status & 0x0F
                print(f"页{page_seq}缺失帧{missing_frame}，需要重传")
                return False
            elif status == DATA_STATUS_TIMEOUT:
                print(f"页{page_seq}超时，需要重传")
                return False
            else:
                print(f"页{page_seq}未知错误: 0x{status:02X}")
                return False
                
        except Exception as e:
            print(f"等待数据帧回复时出错: {e}")
            return False
    
    def send_end_frame(self, slot: int) -> bool:
        """
        发送尾帧
        
        Args:
            slot: 图像槽位
            
        Returns:
            发送成功返回True
        """
        # 构造尾帧
        frame = struct.pack('<HHBBBBBB',
                           PROTOCOL_MAGIC_HOST,  # magic
                           CMD_TRANSFER_END | (slot & 0x0F),  # command + slot
                           slot & 0x0F,  # slot
                           0,  # padding
                           (PROTOCOL_END_HOST >> 24) & 0xFF,
                           (PROTOCOL_END_HOST >> 16) & 0xFF,
                           (PROTOCOL_END_HOST >> 8) & 0xFF,
                           PROTOCOL_END_HOST & 0xFF)
        
        print("发送尾帧")
        self.serial.write(frame)
        
        # 等待回复
        return self.wait_end_reply()
    
    def wait_end_reply(self) -> bool:
        """
        等待尾帧回复
        
        Returns:
            收到正确回复返回True
        """
        try:
            # 读取回复帧 (8字节)
            reply = self.serial.read(8)
            if len(reply) != 8:
                print(f"尾帧回复长度错误: {len(reply)}")
                return False
            
            magic, command, slot_color = struct.unpack('<HHH', reply[:6])
            end_magic = struct.unpack('<I', reply[4:8])[0]
            
            if magic != PROTOCOL_MAGIC_MCU:
                print(f"尾帧回复魔法数错误: 0x{magic:04X}")
                return False
                
            if end_magic != PROTOCOL_END_MCU:
                print(f"尾帧回复结束魔法数错误: 0x{end_magic:08X}")
                return False
            
            print("尾帧回复: 传输完成")
            return True
                
        except Exception as e:
            print(f"等待尾帧回复时出错: {e}")
            return False
    
    def send_image_color(self, image_data: bytes, color_type: int, slot: int) -> bool:
        """
        发送一种颜色的图像数据
        
        Args:
            image_data: 图像数据
            color_type: 颜色类型
            slot: 图像槽位
            
        Returns:
            发送成功返回True
        """
        print(f"开始发送{'黑白' if color_type == 0 else '红白'}图像数据")
        
        # 发送首帧
        retry_count = 0
        while retry_count < self.max_retries:
            if self.send_start_frame(slot, color_type):
                break
            retry_count += 1
            print(f"首帧重试 {retry_count}/{self.max_retries}")
        else:
            print("首帧发送失败")
            return False
        
        # 发送数据页
        for page_seq in range(1, IMAGE_PAGES_PER_COLOR + 1):
            # 计算页数据
            start_pos = (page_seq - 1) * IMAGE_DATA_PER_PAGE
            if page_seq == IMAGE_PAGES_PER_COLOR:  # 最后一页
                page_data = image_data[start_pos:start_pos + IMAGE_LAST_PAGE_DATA_SIZE]
                page_data += b'\x00' * (IMAGE_DATA_PER_PAGE - IMAGE_LAST_PAGE_DATA_SIZE)  # 填充0
            else:
                page_data = image_data[start_pos:start_pos + IMAGE_DATA_PER_PAGE]
            
            # 发送页数据帧
            retry_count = 0
            while retry_count < self.max_retries:
                if self.send_data_frames(page_data, page_seq, slot, color_type):
                    break
                retry_count += 1
                print(f"页{page_seq}重试 {retry_count}/{self.max_retries}")
            else:
                print(f"页{page_seq}发送失败")
                return False
            
            # 进度显示
            progress = (page_seq * 100) // IMAGE_PAGES_PER_COLOR
            print(f"{'黑白' if color_type == 0 else '红白'}图像进度: {progress}% ({page_seq}/{IMAGE_PAGES_PER_COLOR})")
        
        print(f"{'黑白' if color_type == 0 else '红白'}图像数据发送完成")
        return True
    
    def send_complete_image(self, slot: int = 0) -> bool:
        """
        发送完整图像 (黑白 + 红白)
        
        Args:
            slot: 图像槽位 (0-15)
            
        Returns:
            发送成功返回True
        """
        print(f"开始发送完整图像到槽位{slot}")
        
        # 生成测试图像数据
        bw_image = self.generate_test_image(0)  # 黑白图像
        red_image = self.generate_test_image(1)  # 红白图像
        
        # 发送黑白图像
        if not self.send_image_color(bw_image, COLOR_TYPE_BW, slot):
            print("黑白图像发送失败")
            return False
        
        # 发送红白图像
        if not self.send_image_color(red_image, COLOR_TYPE_RED, slot):
            print("红白图像发送失败")
            return False
        
        # 发送尾帧
        if not self.send_end_frame(slot):
            print("尾帧发送失败")
            return False
        
        print(f"图像发送完成! 槽位: {slot}")
        return True
    
    def close(self):
        """关闭串口连接"""
        if self.serial.is_open:
            self.serial.close()

def main():
    """主函数"""
    # 配置串口 (根据实际情况修改)
    PORT = 'COM3'  # Windows
    # PORT = '/dev/ttyUSB0'  # Linux
    BAUDRATE = 115200
    
    try:
        # 创建发送器
        sender = ImageSender(PORT, BAUDRATE)
        print(f"连接到串口: {PORT}, 波特率: {BAUDRATE}")
        
        # 等待用户输入
        input("按回车键开始发送图像...")
        
        # 发送图像到槽位0
        success = sender.send_complete_image(slot=0)
        
        if success:
            print("\n=== 图像传输成功! ===")
        else:
            print("\n=== 图像传输失败! ===")
        
    except serial.SerialException as e:
        print(f"串口错误: {e}")
    except KeyboardInterrupt:
        print("\n用户中断")
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        try:
            sender.close()
        except:
            pass
        print("程序结束")

if __name__ == '__main__':
    main()