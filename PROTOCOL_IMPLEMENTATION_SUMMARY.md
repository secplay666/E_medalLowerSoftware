# 可靠图像传输协议 V2 实现总结

## 问题诊断

### 原始问题
- 上位机发送完整个传输过程，但单片机一页数据都没有正确接收和存储
- 原因：上位机和单片机之间缺乏同步机制，上位机一直发送，不等待单片机的ACK响应

## 解决方案

### 总体架构
实现了一个完全同步的 **帧-响应型** 可靠传输协议：
- 上位机每发送一帧，必须等待单片机返回ACK/NAK
- 单片机接收数据后立即验证并返回响应
- 传输完成后进行完整性校验

## 关键改进

### 1. 协议响应流程 (详见PROTOCOL_V2_RELIABLE.md)

```
上位机                          单片机
   |                              |
   |-- START -------->            |
   |                              |  Flash初始化
   |<------- READY <----|          |
   |                              |
   |-- 图像头 -------->           |
   |                              |  验证CRC和校验和
   |                              |  写入Flash
   |<------ ACK/NAK ---------|      |
   |                              |
   |  (重复61次)                   |
   |-- 数据帧[i] -------->         |
   |<------ ACK/NAK ---------|      |
   |                              |
   |-- END -------->              |
   |                              |  验证所有61帧完整性
   |<---- COMPLETE/FAIL -------|  返回验证结果
   |                              |
```

### 2. 上位机端修改 (串口调试助手_V3_升级版.html)

#### 新增全局状态变量
```javascript
let lastAckFrame = -1;        // 最后一个ACK的帧号
let waitingForAck = false;    // 是否正在等待ACK
let ackPromiseResolve = null; // Promise的resolve回调
```

#### 新增协议解析函数
- `parseProtocolFrames()` - 在接收数据时自动识别ACK/NAK/COMPLETE/FAIL响应
- `handleControlFrame()` - 处理READY/COMPLETE/FAIL控制帧
- `handleResponseFrame()` - 处理ACK/NAK响应帧

#### 同步发送函数
```javascript
sendControlFrameAndWait()      // 发送控制帧并等待指定响应
sendControlFrameAndWaitResult() // 发送END并等待COMPLETE/FAIL
sendDataFrameAndWaitAck()      // 发送数据帧并等待ACK/NAK (支持重试)
```

#### 重写的发送流程
1. **发送START** → 等待READY (5秒超时)
2. **发送图像头** → 等待ACK (3秒超时)
3. **循环发送61帧数据** → 每帧等待ACK/NAK (3秒超时)
   - 收到NAK时自动重试 (最多5次)
   - 所有重试失败才报错退出
4. **发送END** → 等待COMPLETE/FAIL (5秒超时)
5. **显示最终结果** - 完整性校验成功/失败

### 3. 单片机端修改 (source/image_transfer_v2.c)

#### 命令码更新
```c
RESP_READY    = 0x03  // 准备好接收
RESP_COMPLETE = 0x04  // 验证成功
RESP_FAIL     = 0x05  // 验证失败
RESP_ACK      = 0x20  // 帧接收成功
RESP_NAK      = 0x21  // 帧接收失败
```

#### 关键改进：END命令处理

```c
// 收到END命令时执行完整性验证
if (cmd == CMD_END) {
    // 1. 检查帧数
    if (total_frames_received >= IMAGE_PAGES) {
        // 2. 检查帧位图（bitmap）
        uint64_t expected_bitmap = ((uint64_t)1 << 61) - 1;
        if ((frame_bitmap & expected_bitmap) == expected_bitmap) {
            send_ctrl_frame(RESP_COMPLETE);  // 所有帧都收到
        } else {
            send_ctrl_frame(RESP_FAIL);      // 存在缺失帧
        }
    } else {
        send_ctrl_frame(RESP_FAIL);          // 帧数不足
    }
}
```

#### 数据帧处理（保持不变，但响应立即发出）
```c
// 收到数据帧时：
// 1. 验证校验和
// 2. 验证CRC32
// 3. 写入Flash
// 4. 立即返回ACK (成功) 或 NAK (失败)
```

## 协议帧结构详解

### 应答帧 (ACK/NAK) - 6字节

```
[0x55] [0x20/0x21] [FrameNum_L] [FrameNum_H] [Checksum] [0xAA]
 HEAD     TYPE      FRAME_NUM_LOWER  FRAME_NUM_UPPER  CHECK  TAIL
```

- 总长度：6字节
- 0x20 = ACK，0x21 = NAK
- 帧号用小端序编码 (FrameNum = L | (H << 8))
- 校验和 = (HEAD + TYPE + L + H) & 0xFF

### 控制帧 (READY/COMPLETE/FAIL) - 4字节

```
[0x55] [0x03/0x04/0x05] [Checksum] [0xAA]
 HEAD    COMMAND           CHECK     TAIL
```

- 总长度：4字节
- 0x03 = READY，0x04 = COMPLETE，0x05 = FAIL
- 校验和 = (HEAD + CMD) & 0xFF

## 传输流程示例

### 成功传输
```
上位机: 📤 发送START命令
单片机: 📥 RX START → 初始化 → 📤 TX READY
上位机: 🔵 收到READY
上位机: 📤 发送图像头
单片机: 📥 RX HEADER → 验证 → 写入Flash → 📤 TX ACK
上位机: ✅ 收到Frame 0 ACK
上位机: 📤 发送数据帧 0-60 (61帧)
单片机: 📥 RX DATA[i] → 验证 → 写入Flash → 📤 TX ACK
上位机: ✅ 每帧确认
上位机: 📤 发送END命令
单片机: 📥 RX END → 验证完整性 → 📤 TX COMPLETE
上位机: ✅ 传输成功!
```

### 失败重试
```
上位机: 📤 发送数据帧 5
单片机: 📥 RX DATA[5] → CRC错误 → 📤 TX NAK
上位机: ❌ 收到NAK
上位机: 📤 重发数据帧 5 (重试1/5)
单片机: 📥 RX DATA[5] → 验证成功 → 📤 TX ACK
上位机: ✅ 收到Frame 5 ACK
```

## 完整性校验机制

### 单帧校验（已有）
- CRC32：检测载荷数据损伤
- 校验和：检测帧头尾或编码错误

### 全部帧校验（新增）
```
方法1: 帧位图 (bitmap)
- 接收端为每个成功接收的帧置位
- 完成时检查是否所有61位都被置位
- 位丢失 = 缺失帧

方法2: 帧计数
- 检查接收的总帧数是否 >= 61
- 仅作为快速检查

结果:
- 所有帧都在 → COMPLETE
- 缺失任何帧 → FAIL
```

## 超时机制

### 上位机超时处理
```
FRAME_TIMEOUT_MS = 3000      // 单帧超时
CTRL_TIMEOUT_MS  = 5000      // 控制帧超时
MAX_RETRIES      = 5         // 最大重试次数
```

### 单片机超时处理
```
TIMEOUT_FRAME    = 3000      // 帧接收超时
TIMEOUT_IDLE     = 5000      // 空闲超时（尚未实现，可选）
```

## 调试信息

### 上位机输出示例
```
📤 发送START命令
🔵 单片机已准备好接收
✅ 帧0已确认 (ACK)
📤 发送帧0 (重试 1/5)
✅ 帧0已确认 (ACK)
✅ 帧1已确认 (ACK)
...
✅ 帧60已确认 (ACK)
📤 发送END命令，等待单片机验证完整性...
✅ 单片机验证成功，所有帧完整
✅ 传输完成且验证成功！
```

### 单片机输出示例
```
[IMG_V2] RX CTRL: START (0x01)
[IMG_V2] Transfer started, waiting for header...
[IMG_V2] TX CTRL: READY (0x03)
[IMG_V2] RX DATA: frame=0 (HEADER), CRC=0xXXXXXXXX
[IMG_V2] Flash write OK: 248 bytes
[IMG_V2] TX ACK: frame=0
[IMG_V2] RX DATA: frame=0, CRC=0xXXXXXXXX
[IMG_V2] Frame 0 saved
[IMG_V2] TX ACK: frame=0
...
[IMG_V2] RX CTRL: END (0x02)
[IMG_V2] ✅ Transfer COMPLETE! All 62 frames verified
[IMG_V2] TX CTRL: COMPLETE (0x04)
```

## 技术细节

### 为什么使用 Promise/async-await?
- JavaScript 异步编程的标准模式
- 避免回调地狱 (callback hell)
- 支持超时和错误处理
- 代码可读性强

### 为什么需要帧位图 (bitmap)?
- 支持乱序接收（如果将来需要）
- 快速检查是否缺失特定帧
- 存储紧凑（64位 = 2个uint32_t）
- 适合MCU资源受限场景

### 校验和 vs CRC32
- 校验和 = 快速检错，但不可靠
- CRC32 = 更强的检错能力
- 两者结合：
  - 校验和检查帧头尾完整性
  - CRC32检查载荷数据完整性

## 测试建议

1. **正常传输测试**
   - 一次性传输完整61帧
   - 验证单片机Flash中数据正确

2. **数据损伤模拟**
   - 修改帧的部分字节
   - 验证CRC失败返回NAK
   - 验证上位机自动重试

3. **超时测试**
   - 单片机故意延迟响应
   - 验证上位机超时重试
   - 验证最终报错

4. **帧丢失模拟**
   - 单片机故意不保存某帧
   - 完成时验证返回FAIL

5. **不同Slot ID**
   - 测试多个Slot的存储
   - 验证数据不互相覆盖

## 后续改进方向

1. **滑动窗口** - 支持同时发送多帧，提高吞吐量
2. **压缩** - 对图像数据进行压缩，减少传输时间
3. **分段校验** - 分段计算完整性码，更及时发现问题
4. **弹性重试** - 根据网络状况动态调整重试参数
5. **日志记录** - 记录所有传输过程，便于故障分析

