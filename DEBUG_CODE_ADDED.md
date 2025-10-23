# 调试代码添加完成 ✅

## 已添加的调试代码

### 1. main.c - 初始化调试信息

**位置**: `source/main.c` 第450-462行

**添加内容**:
```c
// ==================== 调试：测试串口和协议 ====================
UARTIF_uartPrintf(0, "\n");
UARTIF_uartPrintf(0, "═══════════════════════════════════════════════════\n");
UARTIF_uartPrintf(0, "   图像传输协议 V2 - 调试模式已启动\n");
UARTIF_uartPrintf(0, "═══════════════════════════════════════════════════\n");
UARTIF_uartPrintf(0, "[DEBUG] 单片机已就绪，等待上位机START命令...\n");
UARTIF_uartPrintf(0, "[DEBUG] UART波特率: 9600 baud\n");
UARTIF_uartPrintf(0, "[DEBUG] Flash Manager: %s\n", (FM_init() == FLASH_OK) ? "OK" : "FAIL");
UARTIF_uartPrintf(0, "[DEBUG] 如果没有看到START命令，请检查:\n");
UARTIF_uartPrintf(0, "  1. 上位机和单片机的UART连接\n");
UARTIF_uartPrintf(0, "  2. 波特率设置是否一致\n");
UARTIF_uartPrintf(0, "  3. 上位机是否正确发送了START\n");
UARTIF_uartPrintf(0, "═══════════════════════════════════════════════════\n\n");
```

**功能**: 显示初始化完成，提醒检查配置

---

### 2. image_transfer_v2.c - 协议初始化调试

**位置**: `source/image_transfer_v2.c` 第261-268行

**添加内容**:
```c
void ImageTransferV2_Init(void)
{
    memset(&rx_ctx, 0, sizeof(rx_context_t));
    rx_ctx.state = RX_STATE_IDLE;
    UARTIF_uartPrintf(0, "[IMG_V2] Protocol V2 initialized\r\n");
    // 新增调试信息:
    UARTIF_uartPrintf(0, "[IMG_V2] MAX_RETRIES=%d, IMAGE_PAGES=%d\r\n", MAX_RETRIES, IMAGE_PAGES);
    UARTIF_uartPrintf(0, "[IMG_V2] FRAME_PAYLOAD_SIZE=%d, TIMEOUT_FRAME=%dms\r\n", FRAME_PAYLOAD_SIZE, TIMEOUT_FRAME);
}
```

**功能**: 显示协议参数配置

---

### 3. image_transfer_v2.c - 数据接收调试

**位置**: `source/image_transfer_v2.c` 第296-304行

**添加内容**:
```c
// ==================== 调试：输出接收到的数据 ====================
UARTIF_uartPrintf(0, "[IMG_V2] 📥 RX %d bytes: ", bytes_fetched);
for (i = 0; i < temp_idx && i < 20; i++) {
    UARTIF_uartPrintf(0, "%02X ", temp_buf[i]);
}
if (temp_idx > 20) {
    UARTIF_uartPrintf(0, "...");
}
UARTIF_uartPrintf(0, " [state=%d]\r\n", rx_ctx.state);
```

**功能**: 显示接收到的原始数据和当前状态

**输出示例**:
```
[IMG_V2] 📥 RX 4 bytes: 55 01 56 AA [state=0]
[IMG_V2] 📥 RX 259 bytes: 55 11 00 00 00 ... AA [state=2]
```

---

### 4. image_transfer_v2.c - 控制帧解析调试

**位置**: `source/image_transfer_v2.c` 第147-173行

**添加内容**:
```c
static uint8_t process_ctrl_frame(void)
{
    // ...
    if (rx_ctx.frame_idx < 4) {
        // 新增: 显示帧不完整的信息
        UARTIF_uartPrintf(0, "[IMG_V2_DEBUG] CTRL frame incomplete: idx=%d/4\r\n", rx_ctx.frame_idx);
        return 0;
    }

    // 新增: 显示校验和检查详情
    UARTIF_uartPrintf(0, "[IMG_V2_DEBUG] CTRL frame check: cmd=0x%02X, checksum=%02X (expected=%02X)\r\n",
                    command, checksum, expected_checksum);

    if (checksum != expected_checksum) {
        // 改进: 显示更详细的错误信息
        UARTIF_uartPrintf(0, "[IMG_V2] ❌ CTRL checksum error: got %02X, expected %02X\r\n",
                        checksum, expected_checksum);
        return 0;
    }

    // 改进: 显示成功标记
    UARTIF_uartPrintf(0, "[IMG_V2] ✅ RX CTRL: cmd=0x%02X\r\n", command);
    return command;
}
```

**功能**: 详细显示控制帧的接收和验证过程

**输出示例**:
```
[IMG_V2_DEBUG] CTRL frame check: cmd=0x01, checksum=56 (expected=56)
[IMG_V2] ✅ RX CTRL: cmd=0x01
```

---

## 调试输出的完整流程

### 正常情况下的输出顺序：

```
1️⃣ 初始化阶段:
───────────────
image_transfer init completely!
[IMG_V2] Protocol V2 initialized
[IMG_V2] MAX_RETRIES=5, IMAGE_PAGES=61
[IMG_V2] FRAME_PAYLOAD_SIZE=248, TIMEOUT_FRAME=3000ms

═════════════════════════════════════════════════════
   图像传输协议 V2 - 调试模式已启动
═════════════════════════════════════════════════════
[DEBUG] 单片机已就绪，等待上位机START命令...
[DEBUG] UART波特率: 9600 baud
[DEBUG] Flash Manager: OK

2️⃣ 接收START命令:
──────────────
[IMG_V2] 📥 RX 4 bytes: 55 01 56 AA [state=0]
[IMG_V2_DEBUG] CTRL frame check: cmd=0x01, checksum=56 (expected=56)
[IMG_V2] ✅ RX CTRL: cmd=0x01
[IMG_V2] Transfer started, waiting for header...
[IMG_V2] TX CTRL: READY (0x03)

3️⃣ 接收图像头:
──────────
[IMG_V2] 📥 RX 259 bytes: 55 11 00 00 00 ... AA [state=1]
[IMG_V2] Header saved: slot=0
[IMG_V2] TX ACK: frame=0

4️⃣ 接收数据帧 (重复61次):
────────────────────
[IMG_V2] 📥 RX 259 bytes: 55 10 00 00 00 ... AA [state=2]
[IMG_V2] Frame 0 saved
[IMG_V2] TX ACK: frame=0
[IMG_V2] 📥 RX 259 bytes: 55 10 01 00 00 ... AA [state=2]
[IMG_V2] Frame 1 saved
[IMG_V2] TX ACK: frame=1
... (repeated 59 more times)

5️⃣ 接收END命令:
─────────────
[IMG_V2] 📥 RX 4 bytes: 55 02 57 AA [state=2]
[IMG_V2_DEBUG] CTRL frame check: cmd=0x02, checksum=57 (expected=57)
[IMG_V2] ✅ RX CTRL: cmd=0x02
[IMG_V2] ✅ Transfer COMPLETE! All 62 frames verified
[IMG_V2] TX CTRL: COMPLETE (0x04)
```

## 如何使用调试信息

### 1. 诊断波特率问题

**情况A - 波特率正确 (9600=9600)**:
```
[IMG_V2] 📥 RX 4 bytes: 55 01 56 AA [state=0]  ✅ 清晰的数据
[IMG_V2_DEBUG] CTRL frame check: cmd=0x01     ✅ 命令正确解析
```

**情况B - 波特率错误 (115200!=9600)**:
```
[IMG_V2] 📥 RX 48 bytes: FE FF FE FF ...       ❌ 数据乱码
[IMG_V2_DEBUG] CTRL frame incomplete: idx=4/4 ❌ 无法解析
```

### 2. 诊断通信故障

**完全没有RX信息**:
- 可能: UART线未连接、固件未烧写、波特率过低
- 检查: UART线、固件、波特率

**数据乱码**:
- 可能: 波特率不匹配
- 检查: 改上位机波特率为9600

### 3. 诊断帧处理故障

**校验和错误**:
```
[IMG_V2] ❌ CTRL checksum error: got 55, expected 56
```
- 可能: 数据传输错误、波特率不匹配
- 检查: UART线、波特率

**帧不完整**:
```
[IMG_V2_DEBUG] CTRL frame incomplete: idx=2/4
```
- 可能: 正在接收中，稍等片刻
- 通常不是问题

## 编译注意事项

修改的文件：
- ✅ `source/main.c` - 添加初始化调试
- ✅ `source/image_transfer_v2.c` - 添加详细调试

需要重新编译并烧写固件！

## 性能影响

添加的调试代码会：
- ⚠️ **增加UART输出** - 更多printf调用
- ⚠️ **轻微增加延迟** - 输出文本需要时间
- ✅ **不影响核心逻辑** - 只是信息输出

**建议**: 调试完后可以注释掉某些频繁的调试信息以提升性能。

## 下一步

1. ✅ 重新编译 `source/main.c` 和 `source/image_transfer_v2.c`
2. ✅ 烧写新固件
3. ✅ 打开串口监视器
4. ✅ 等待看到初始化信息
5. ✅ 在上位机改波特率为9600
6. ✅ 点击"开始传输"
7. ✅ 观察单片机输出的调试信息
8. ✅ 根据信息判断问题所在

---

**调试代码添加完成！** 🎉

现在烧写新固件后应该能看到详细的调试信息，可以快速定位问题！
