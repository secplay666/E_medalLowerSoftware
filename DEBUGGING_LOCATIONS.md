# 调试代码位置速查表

## 已修改的源文件

### 1. source/main.c

#### 修改位置: 第446-462行

**原代码**:
```c
    // 初始化image transfer模块
    ImageTransferV2_Init();
    UARTIF_uartPrintf(0, "image_transfer init completely!\n");
    // 立即进入while循环
```

**新代码** (添加了调试信息):
```c
    // 初始化image transfer模块
    ImageTransferV2_Init();
    UARTIF_uartPrintf(0, "image_transfer init completely!\n");

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

**作用**: 系统启动后立即显示调试信息，提醒用户检查配置

---

### 2. source/image_transfer_v2.c

#### 修改位置1: 第261-268行 (ImageTransferV2_Init 函数)

**原代码**:
```c
void ImageTransferV2_Init(void)
{
    memset(&rx_ctx, 0, sizeof(rx_context_t));
    rx_ctx.state = RX_STATE_IDLE;
    UARTIF_uartPrintf(0, "[IMG_V2] Protocol V2 initialized\r\n");
}
```

**新代码** (添加了参数显示):
```c
void ImageTransferV2_Init(void)
{
    memset(&rx_ctx, 0, sizeof(rx_context_t));
    rx_ctx.state = RX_STATE_IDLE;
    UARTIF_uartPrintf(0, "[IMG_V2] Protocol V2 initialized\r\n");
    UARTIF_uartPrintf(0, "[IMG_V2] MAX_RETRIES=%d, IMAGE_PAGES=%d\r\n", MAX_RETRIES, IMAGE_PAGES);
    UARTIF_uartPrintf(0, "[IMG_V2] FRAME_PAYLOAD_SIZE=%d, TIMEOUT_FRAME=%dms\r\n", FRAME_PAYLOAD_SIZE, TIMEOUT_FRAME);
}
```

**作用**: 显示协议参数配置，便于验证

---

#### 修改位置2: 第282-304行 (ImageTransferV2_Process 函数开头)

**原代码**:
```c
    // Try to fetch data from UART queue
    temp_idx = 0;
    bytes_fetched = UARTIF_fetchDataFromUart(temp_buf, &temp_idx);

    if (bytes_fetched == 0) {
        rx_ctx.timeout_counter++;
        if (rx_ctx.timeout_counter > TIMEOUT_FRAME && rx_ctx.state != RX_STATE_IDLE) {
            UARTIF_uartPrintf(0, "[IMG_V2] Timeout in state %d\r\n", rx_ctx.state);
            rx_ctx.timeout_counter = 0;
        }
        return;
    }

    // Process fetched bytes
```

**新代码** (添加了数据接收显示):
```c
    // Try to fetch data from UART queue
    temp_idx = 0;
    bytes_fetched = UARTIF_fetchDataFromUart(temp_buf, &temp_idx);

    if (bytes_fetched == 0) {
        rx_ctx.timeout_counter++;
        if (rx_ctx.timeout_counter > TIMEOUT_FRAME && rx_ctx.state != RX_STATE_IDLE) {
            UARTIF_uartPrintf(0, "[IMG_V2] ⏱ Timeout in state %d (counter=%u)\r\n",
                            rx_ctx.state, rx_ctx.timeout_counter);
            rx_ctx.timeout_counter = 0;
        }
        return;
    }

    // ==================== 调试：输出接收到的数据 ====================
    UARTIF_uartPrintf(0, "[IMG_V2] 📥 RX %d bytes: ", bytes_fetched);
    for (i = 0; i < temp_idx && i < 20; i++) {
        UARTIF_uartPrintf(0, "%02X ", temp_buf[i]);
    }
    if (temp_idx > 20) {
        UARTIF_uartPrintf(0, "...");
    }
    UARTIF_uartPrintf(0, " [state=%d]\r\n", rx_ctx.state);

    // Process fetched bytes
```

**作用**: 显示接收到的原始数据和当前状态，便于诊断波特率问题

---

#### 修改位置3: 第147-173行 (process_ctrl_frame 函数)

**原代码**:
```c
static uint8_t process_ctrl_frame(void)
{
    uint8_t command;
    uint8_t checksum;
    uint8_t expected_checksum;

    // Expected: [0x55, CMD, CHECKSUM, 0xAA]
    if (rx_ctx.frame_idx < 4) {
        return 0; // Not complete
    }

    command = rx_ctx.frame_buf[1];
    checksum = rx_ctx.frame_buf[2];
    expected_checksum = calc_checksum(&rx_ctx.frame_buf[0], 2);

    if (checksum != expected_checksum) {
        UARTIF_uartPrintf(0, "[IMG_V2] CTRL checksum error\r\n");
        return 0;
    }

    UARTIF_uartPrintf(0, "[IMG_V2] RX CTRL: cmd=0x%02X\r\n", command);
    return command;
}
```

**新代码** (改进了错误信息):
```c
static uint8_t process_ctrl_frame(void)
{
    uint8_t command;
    uint8_t checksum;
    uint8_t expected_checksum;

    // Expected: [0x55, CMD, CHECKSUM, 0xAA]
    if (rx_ctx.frame_idx < 4) {
        UARTIF_uartPrintf(0, "[IMG_V2_DEBUG] CTRL frame incomplete: idx=%d/4\r\n", rx_ctx.frame_idx);
        return 0; // Not complete
    }

    command = rx_ctx.frame_buf[1];
    checksum = rx_ctx.frame_buf[2];
    expected_checksum = calc_checksum(&rx_ctx.frame_buf[0], 2);

    UARTIF_uartPrintf(0, "[IMG_V2_DEBUG] CTRL frame check: cmd=0x%02X, checksum=%02X (expected=%02X)\r\n",
                    command, checksum, expected_checksum);

    if (checksum != expected_checksum) {
        UARTIF_uartPrintf(0, "[IMG_V2] ❌ CTRL checksum error: got %02X, expected %02X\r\n", checksum, expected_checksum);
        return 0;
    }

    UARTIF_uartPrintf(0, "[IMG_V2] ✅ RX CTRL: cmd=0x%02X\r\n", command);
    return command;
}
```

**作用**: 显示控制帧的详细处理过程，包括完整性检查和校验和验证

---

## 修改文件列表

| 文件 | 修改位置 | 行数 | 功能 |
|------|---------|------|------|
| source/main.c | 450-462 | 13行 | 初始化调试提示 |
| source/image_transfer_v2.c | 261-268 | 2行 | 参数显示 |
| source/image_transfer_v2.c | 282-304 | 22行 | 数据接收显示 |
| source/image_transfer_v2.c | 147-173 | 改进 | 帧处理调试 |

**总计修改**: 4个位置，约37行代码

---

## 调试代码的输出顺序

启动单片机后，调试输出按以下顺序出现：

```
1. 系统启动信息
   ↓
2. Flash初始化信息
   ↓
3. Image Transfer初始化 + 协议参数
   ↓
4. 调试模式启动提示
   ↓
5. (等待上位机START...)
   ↓
6. 接收数据 + 帧处理 (反复)
   ↓
7. 完成或错误信息
```

---

## 如何查找修改位置

### 使用编辑器查找功能

#### 搜索 main.c 中的调试代码：
```
搜索: "调试：测试串口和协议"
结果: 第450行左右
```

#### 搜索 image_transfer_v2.c 中的调试代码：
```
搜索1: "[IMG_V2] MAX_RETRIES"
结果: 第267行 (参数显示)

搜索2: "📥 RX"
结果: 第297行 (数据接收显示)

搜索3: "CTRL frame check"
结果: 第163行 (帧处理调试)
```

---

## 编译验证

修改完成后，编译时应该没有错误或警告。

### Keil µVision 编译步骤：

1. 打开 HC32L110C6UA.uvprojx
2. 选择 Project → Rebuild all target files (F7)
3. 如果编译成功，应该看到：
   ```
   Rebuild started: Project: HC32L110C6UA
   ...
   ".\output\release\HC32L110C6UA.hex" - 0 Error(s), 0 Warning(s)
   ```

4. 如果有错误，检查：
   - 是否有语法错误
   - 是否正确保存文件
   - 是否混淆了行号

---

## 回退修改（如需要）

如果需要回退调试代码，可以注释掉添加的printf语句：

### main.c - 注释调试信息：
```c
// UARTIF_uartPrintf(0, "[DEBUG] 单片机已就绪...\n");
// UARTIF_uartPrintf(0, "[DEBUG] UART波特率: 9600 baud\n");
// 等等...
```

### image_transfer_v2.c - 注释调试信息：
```c
// UARTIF_uartPrintf(0, "[IMG_V2] MAX_RETRIES=%d...\n", ...);
// UARTIF_uartPrintf(0, "[IMG_V2] 📥 RX %d bytes...\n", ...);
// 等等...
```

---

## 调试代码的优化建议

### 1. 减少频繁输出的信息

数据接收显示 (第297-304行) 会在每次有数据时输出，在高速传输时可能很频繁。

**优化方案** (可选):
```c
// 添加计数器，每N条消息才输出一次
static uint32_t debug_count = 0;
if ((debug_count++ % 10) == 0) {
    UARTIF_uartPrintf(0, "[IMG_V2] 📥 RX %d bytes...\n", bytes_fetched);
}
```

### 2. 条件编译调试代码

使用预编译指令可以轻松启用/禁用调试：

```c
#define DEBUG_IMAGE_TRANSFER 1

#if DEBUG_IMAGE_TRANSFER
    UARTIF_uartPrintf(0, "[IMG_V2] Debug info...\n");
#endif
```

### 3. 性能测量

添加时间戳可以测量传输速度：

```c
static uint32_t start_time = 0;
if (frame_num == 0) {
    start_time = get_system_time(); // 需要实现
}
if (frame_num == 60) {
    uint32_t elapsed = get_system_time() - start_time;
    UARTIF_uartPrintf(0, "[IMG_V2] Transfer time: %u ms\r\n", elapsed);
}
```

---

## 相关文档

| 文档 | 用途 |
|------|------|
| DEBUG_CODE_ADDED.md | 详细的代码修改说明 |
| DEBUG_GUIDE.md | 完整的调试步骤指南 |
| QUICK_DEBUG_CHECKLIST.txt | 快速排查清单 |
| DEBUGGING_SUMMARY.txt | 调试总结和问题诊断 |

---

## 下一步

1. ✅ 查看本文件确认修改位置
2. ✅ 在代码编辑器中验证修改
3. ✅ 编译新固件
4. ✅ 烧写到单片机
5. ✅ 观察调试输出
6. ✅ 根据输出诊断问题

---

**调试代码已全部添加！** 🎉

现在应该能看到详细的调试信息，可以快速定位任何通信问题。

