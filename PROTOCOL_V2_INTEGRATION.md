# 新协议V2集成指南

## 📋 文件清单

### 新增文件
- `source/image_transfer_v2.c` - 下位机传输模块（新）
- `source/image_transfer_v2.h` - 下位机传输模块头文件
- `tools/串口调试助手_V3_新协议.html` - 上位机工具（新）

### 需要修改的文件
- `source/main.c` - 集成新模块

### 可选保留文件（用于对比）
- `source/image_transfer.c` - 旧协议（保留）
- 旧的HTML工具

---

## 🔧 集成步骤

### 步骤1：添加项目文件

在Keil中：
1. Project → Manage Project Items
2. 在Source Group中添加：
   - `source/image_transfer_v2.c`
   - `source/image_transfer_v2.h`

### 步骤2：修改main.c

**移除旧的image_transfer调用:**

```c
// 移除或注释
// #include "image_transfer.h"
// ImageTransfer_Init();
// ImageTransfer_Process();
```

**添加新的image_transfer_v2调用:**

```c
#include "image_transfer_v2.h"

// 在初始化部分 (main函数中):
ImageTransferV2_Init();
UARTIF_uartPrintf(0, "Image transfer V2 initialized!\n");

// 在主循环中，替换旧的:
while(1) {
    // 1ms处理一次
    if (tg5ms)  // 这个标志已经是1ms了
    {
        tg5ms = FALSE;
        ImageTransferV2_Process();  // 新协议处理
    }

    // ... 其他代码
}
```

### 步骤3：编译验证

```
Project → Build Target (F7)
确保无编译错误
```

---

## 🚀 使用流程

### 上位机操作

1. **打开新工具**
   - 使用 `串口调试助手_V3_新协议.html`

2. **连接串口**
   - 选择波特率 115200
   - 点击"连接串口"

3. **选择并预览图像**
   - 选择图像文件
   - 点击"预览"

4. **传输图像**
   - 点击"开始传输"
   - 等待完成

### 下位机行为

| 步骤 | 下位机状态 | 操作 | 日志 |
|------|----------|------|------|
| 1 | IDLE | 等待START | `[IMG_V2] RX CTRL: cmd=0x01` |
| 2 | 等待头部 | 发送READY | `[IMG_V2] TX CTRL: cmd=0x01` |
| 3 | 等待头部 | 接收并保存头 | `[IMG_V2] Header saved: slot=0` |
| 4 | 等待数据 | 接收61帧 | `[IMG_V2] Frame N saved: bitmap=...` |
| 5 | 完成 | 收到END | `[IMG_V2] Transfer complete!` |

---

## 📊 协议对比

| 特性 | 旧协议 | 新协议V2 |
|------|--------|---------|
| 帧大小 | 264字节 | 259字节 |
| 帧头/尾 | 0xA5/0x5A | 0x55/0xAA |
| 错误检测 | CRC32 | CRC32+Checksum |
| 应答机制 | 无（单向） | 有（ACK/NAK） |
| 状态机 | 复杂 | 清晰（4状态） |
| 缓冲需求 | 264字节 | 259字节 |
| 可靠性 | 低 | 高 |
| 易调试 | 困难 | 容易（清晰的START/STOP）|

---

## 🧪 测试步骤

### 快速测试

1. **编译并上传新固件**
   ```
   F7编译 → 上传
   ```

2. **打开上位机工具**
   - 使用新的V3工具

3. **连接并发送测试**
   - 连接 → 选择图像 → 传输
   - 观察日志输出

4. **验证结果**
   - 应该看到逐帧的ACK
   - 最后看到COMPLETE
   - 图像应该已保存到Flash

---

## 📈 日志示例

### 正常传输
```
[IMG_V2] Protocol V2 initialized
[IMG_V2] RX CTRL: cmd=0x01
[IMG_V2] TX CTRL: cmd=0x01
[IMG_V2] Header saved: slot=0
[IMG_V2] Frame 0 saved: bitmap=0x0000000000000001
[IMG_V2] Frame 1 saved: bitmap=0x0000000000000003
...
[IMG_V2] Frame 60 saved: bitmap=0x1FFFFFFFFFFFFFFF
[IMG_V2] RX CTRL: cmd=0x02
[IMG_V2] Transfer complete! Total frames: 62
```

### 错误情况
```
[IMG_V2] DATA checksum error
[IMG_V2] TX NAK: frame=5
```

---

## 🔍 故障排查

### 问题1：没有收到任何数据
- 检查波特率是否为115200
- 检查main.c是否调用了ImageTransferV2_Init()
- 检查ImageTransferV2_Process()是否在循环中被调用

### 问题2：收到NAK或checksum错误
- 检查UART线路质量
- 降低波特率试试（57600）
- 增大超时时间

### 问题3：传输中断
- 检查Flash操作是否成功
- 查看FM_writeData()返回的错误码
- 检查Flash容量是否足够

### 问题4：帧号不对应
- 验证帧长度是否正确（259字节）
- 检查CRC计算是否正确
- 使用新工具的日志功能追踪

---

## 💡 优化建议

### 性能优化
```c
// 可调参数（image_transfer_v2.c）
#define TIMEOUT_FRAME         3000   // 调整等待时间
#define MAX_RETRIES           5      // 调整重传次数
```

### 功能扩展
```c
// 添加自定义magic支持
uint8_t magic = (frame_type == FRAME_TYPE_IMAGE_HEADER)
                ? MAGIC_BW_IMAGE_HEADER
                : MAGIC_BW_IMAGE_DATA;

// 可改为支持多种类型
// : (imageType == RED) ? MAGIC_RED_IMAGE_DATA : MAGIC_BW_IMAGE_DATA;
```

---

## 🔐 安全考虑

1. **缓冲溢出保护** - 已实现（固定大小缓冲）
2. **校验保护** - 已实现（CRC32+Checksum）
3. **超时保护** - 已实现（3秒超时）
4. **重传限制** - 已实现（5次重试）

---

## 📝 完整main.c修改示例

```c
#include "image_transfer_v2.h"  // 新增

void timInit(void)
{
    // ... 保持不变 ...
}

void Bt0Int(void)
{
    Bt_ClearIntFlag(TIM0);
    UARTIF_passThrough();
    (void)E104_getLinkState();

    // 每1ms调用一次
    tg5ms = TRUE;

    // ... 其他定时逻辑 ...
}

int32_t main(void)
{
    // ... 初始化代码 ...

    timInit();
    UARTIF_uartInit();
    UARTIF_lpuartInit();
    W25Q32_Init();
    EPD_initGDEY042Z98();

    // 新协议初始化
    FM_init();
    ImageTransferV2_Init();  // 新增

    UARTIF_uartPrintf(0, "System ready!\n");

    while(1)
    {
        // 图像传输处理（新协议）
        if (tg5ms)
        {
            tg5ms = FALSE;
            ImageTransferV2_Process();  // 新增
        }
    }
}
```

---

## 🎯 预期效果

修改完成后：
- ✅ 串口传输稳定，无错误
- ✅ 每帧都能正确校验并保存
- ✅ 完整的61帧图像传输成功
- ✅ 日志清晰，便于调试

---

## 📞 常见问题

**Q: 新旧协议能并存吗？**
A: 可以，但建议不要同时启用。可以通过编译选项切换。

**Q: 需要修改Flash操作吗？**
A: 不需要。新协议仍使用FM_writeData()和FM_writeImageHeader()。

**Q: 能支持多个Slot同时传输吗？**
A: 目前不支持。可以修改协议支持。

**Q: 帧重传时是否需要上位机做特殊处理？**
A: 不需要。上位机会自动在收到NAK后重传。

