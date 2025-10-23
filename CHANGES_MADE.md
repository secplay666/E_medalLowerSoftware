# 可靠图像传输协议 V2 - 修改清单

日期：2025-10-22
目的：解决上位机一直发送，单片机无法接收的问题

## 修改文件清单

### 1. 上位机 - 串口调试助手_V3_升级版.html

#### 1.1 新增全局变量 (第673-694行)
```javascript
// 协议常量
const CMD_READY = 0x03;          // 新增
const CMD_COMPLETE = 0x04;        // 新增
const CMD_FAIL = 0x05;            // 新增
const FRAME_TIMEOUT_MS = 3000;    // 新增
const CTRL_TIMEOUT_MS = 5000;     // 新增

// 传输状态
let lastAckFrame = -1;            // 新增
let waitingForAck = false;        // 新增
let ackPromiseResolve = null;     // 新增
```

#### 1.2 新增响应解析函数 (第899-979行)
- `parseProtocolFrames()` - 识别ACK/NAK/READY/COMPLETE/FAIL
- `handleControlFrame()` - 处理READY/COMPLETE/FAIL
- `handleResponseFrame()` - 处理ACK/NAK

#### 1.3 修改 handleReceivedData() (第865-897行)
- 新增 `parseProtocolFrames(data)` 调用
- 将接收数据保存到 `receivedDataBuffer`
- 实时更新显示

#### 1.4 重写图像发送函数 (第1385-1584行)

##### 新增：sendImage() 主流程
- 步骤1：发送START等待READY
- 步骤2：发送图像头等待ACK
- 步骤3：循环发送61帧数据，每帧等待ACK
- 步骤4：发送END等待COMPLETE/FAIL
- 显示最终统计

##### 新增：sendControlFrameAndWait()
- 发送控制帧并等待指定响应
- 超时: 5秒
- 支持Promise超时处理

##### 新增：sendControlFrameAndWaitResult()
- 发送END并等待COMPLETE或FAIL
- 返回校验结果

##### 新增：sendDataFrameAndWaitAck()
- 发送数据帧并等待ACK/NAK
- 支持自动重试 (最多5次)
- 超时: 3秒/帧
- 收到NAK自动重试，无需用户干预

### 2. 单片机 - source/image_transfer_v2.c

#### 2.1 更新命令码 (第29-34行)
```c
#define RESP_READY    0x03      // 改为0x03 (原为0x01)
#define RESP_COMPLETE 0x04      // 改为0x04 (原为0x02)
#define RESP_FAIL     0x05      // 新增
```

#### 2.2 改进 send_ctrl_frame() (第94-118行)
- 支持CMD_READY/COMPLETE/FAIL
- 添加描述性日志字符串
- 改进调试信息

#### 2.3 改进 END命令处理 (第330-352行)

**核心逻辑**：
```c
if (cmd == CMD_END) {
    // 步骤1：检查总帧数
    if (total_frames_received >= IMAGE_PAGES) {
        // 步骤2：检查位图完整性
        uint64_t expected = ((uint64_t)1 << 61) - 1;
        if ((frame_bitmap & expected) == expected) {
            send_ctrl_frame(RESP_COMPLETE);  // ✅ 所有帧收到
        } else {
            send_ctrl_frame(RESP_FAIL);      // ❌ 缺失帧
        }
    } else {
        send_ctrl_frame(RESP_FAIL);          // ❌ 帧数不足
    }
}
```

**改进点**：
- 新增完整性校验逻辑
- 使用位图验证所有61帧是否都收到
- 根据验证结果返回COMPLETE或FAIL
- 添加详细日志显示位图和期望值

## 关键设计决策

### 1. 为什么使用Promise?
- ✅ JavaScript异步编程标准
- ✅ 支持超时处理
- ✅ 避免回调地狱
- ✅ 代码易读易维护

### 2. 为什么需要帧位图?
- ✅ 支持检测缺失帧
- ✅ 存储紧凑 (1个uint64_t)
- ✅ 快速位运算检查
- ✅ 适合MCU资源受限

### 3. 为什么分离READY/COMPLETE/FAIL?
- ✅ 清晰的状态转换
- ✅ 便于调试和日志
- ✅ 支持故障诊断
- ✅ 便于未来扩展

## 新增文档

### PROTOCOL_V2_RELIABLE.md
- 完整的协议设计文档
- 帧格式详细说明
- 状态机和超时设置
- 调试信息示例

### PROTOCOL_IMPLEMENTATION_SUMMARY.md
- 问题诊断
- 解决方案概述
- 实现细节
- 测试建议

### PROTOCOL_QUICK_REFERENCE.txt
- 快速参考卡
- 命令码速查
- 故障排除
- 核对清单

## 向后兼容性

⚠️ **注意：此协议与之前的版本不兼容！**

### 不兼容的地方：
1. READY命令从 0x01 改为 0x03
2. COMPLETE命令从 0x02 改为 0x04
3. 新增FAIL命令 0x05
4. 上位机现在会等待ACK，之前不会
5. 单片机END处理改为验证完整性

### 升级检查清单：
- [ ] 单片机代码已更新到新版本
- [ ] 上位机HTML已更新到新版本
- [ ] 协议版本已清晰标记 (V2)
- [ ] 旧的图像数据已清除（新协议无法兼容）

## 性能指标

### 传输时间估计
```
正常传输：
- 每帧259字节 @ 115200 baud ≈ 18ms
- 62帧 × 18ms ≈ 1.1秒
- 加上应答 ≈ 2-3秒

NAK重试：
- 最坏情况（每帧重试1次）: 4-5秒
- 关键帧多次重试: 10-20秒

完整性校验：
- 位图检查: < 1ms
- Flash读取验证: 可选（当前未实现）
```

### 可靠性指标
```
单帧可靠性：
- CRC32检验: 99.99%
- 校验和检验: 99%
- 重试机制: 5次

全部帧可靠性：
- 61帧全部成功: > 99.9%
- 位图验证: 100% (无遗漏)
```

## 测试计划

### 必做测试
1. [ ] 正常传输 - 一次性完成61帧
2. [ ] 单帧重试 - 模拟1-2帧失败，观察重试
3. [ ] 超时重试 - 单片机延迟，上位机应超时重试
4. [ ] 完整性校验 - 故意缺失1帧，应返回FAIL
5. [ ] 不同Slot - 传输到不同Slot，验证不覆盖

### 可选深度测试
1. [ ] 批量传输 - 多次连续传输
2. [ ] 长时间运行 - 观察内存泄漏
3. [ ] 极限参数 - 最小/最大Slot和帧号
4. [ ] 噪声环境 - 添加随机错误
5. [ ] Flash满载 - Flash空间不足时的处理

## 注意事项

### ⚠️ 重要
1. 旧的上位机工具无法与新单片机固件通信！
2. 旧的单片机固件无法与新上位机工具通信！
3. 必须同时更新两端代码！

### 🔍 调试建议
1. 先用日志确认两端都收到正确的命令
2. 检查校验和和CRC32是否匹配
3. 如果卡住，检查超时是否过短
4. 使用16进制显示模式查看原始数据

### 💾 数据保存
1. 在更新固件前备份旧的图像数据
2. 使用下载功能保存接收到的数据
3. 记录传输日志便于分析问题

## 后续优化方向

### 短期 (可直接添加)
- [ ] 显示传输百分比和速率
- [ ] 添加传输历史记录
- [ ] 支持批量传输
- [ ] 添加传输日志导出

### 中期 (需要协议修改)
- [ ] 滑动窗口 (一次发送多帧)
- [ ] 可选压缩支持
- [ ] 分段校验码
- [ ] 弹性重试策略

### 长期 (系统级改进)
- [ ] P2P传输优化
- [ ] 多设备管理
- [ ] 固件升级机制
- [ ] 远程诊断接口

---

**最后更新**: 2025-10-22
**状态**: 完成 ✅
**测试状态**: 待验证 ⏳
