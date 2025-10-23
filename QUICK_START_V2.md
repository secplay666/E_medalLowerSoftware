# Protocol V2 快速启动指南

## 🚀 5分钟快速集成

### 步骤1：添加新文件到Keil项目（2分钟）

1. 打开 **HC32L110C6UA.uvprojx** 在Keil µVision 5中
2. 右键点击 **Source Group** → **Add Existing Files to Group**
3. 添加以下文件：
   - `source/image_transfer_v2.c`
   - `source/image_transfer_v2.h`
4. 点击 **Add** → **Close**

### 步骤2：修改main.c（2分钟）

打开 [source/main.c](source/main.c)，找到main函数，做以下修改：

#### 2.1 添加头文件

在文件顶部添加（大约第10-20行）：
```c
#include "image_transfer_v2.h"  // 添加这一行
```

#### 2.2 注释掉旧协议（如果存在）

如果main.c中有以下代码，请注释掉：
```c
// #include "image_transfer.h"     // 注释掉
// ImageTransfer_Init();           // 注释掉
// ImageTransfer_Process();        // 注释掉
```

#### 2.3 初始化新协议

在main函数中，**FM_init()之后**添加：
```c
int32_t main(void)
{
    // ... 现有的初始化代码 ...

    timInit();
    UARTIF_uartInit();
    UARTIF_lpuartInit();
    W25Q32_Init();
    EPD_initGDEY042Z98();

    FM_init();                      // 必须在这之前
    ImageTransferV2_Init();         // ← 添加这一行

    UARTIF_uartPrintf(0, "Protocol V2 ready!\n");  // ← 可选的调试输出

    while(1)
    {
        // ... 其他代码 ...
    }
}
```

#### 2.4 在主循环中调用Process函数

在main函数的while(1)循环中，**tg5ms标志处**添加：
```c
while(1)
{
    // 图像传输处理（新协议V2）
    if (tg5ms)  // 这个标志现在是1ms触发
    {
        tg5ms = FALSE;
        ImageTransferV2_Process();  // ← 添加这一行
    }

    // ... 其他代码 ...
}
```

### 步骤3：编译项目（30秒）

1. 点击菜单 **Project** → **Build Target** (或按 **F7**)
2. 确保编译成功，0 errors

**预期输出：**
```
Build target 'Target 1'
compiling image_transfer_v2.c...
compiling main.c...
linking...
Program Size: Code=XXXXX RO-data=XXXX RW-data=XXX ZI-data=XXX
".\output\release\HC32L110C6UA.axf" - 0 Error(s), 0 Warning(s).
```

### 步骤4：上传固件（30秒）

1. 连接调试器到MCU
2. 点击 **Flash** → **Download** (或按 **F8**)
3. 等待上传完成

### 步骤5：测试传输（1分钟）

#### 5.1 打开上位机工具

用浏览器（Chrome/Edge推荐）打开：
```
tools/串口调试助手_V3_新协议.html
```

#### 5.2 连接串口

1. 选择波特率：**115200**
2. 点击 **"连接串口"**
3. 选择正确的COM口

#### 5.3 发送测试图像

1. 点击 **"选择图像"** → 选择任意图像文件
2. 点击 **"预览"** → 验证图像已加载
3. 点击 **"开始传输"**
4. 观察：
   - 进度条应该逐渐增加（0% → 100%）
   - 日志窗口应该显示每一帧的发送和ACK

**预期日志（上位机）：**
```
[2025-XX-XX XX:XX:XX] 发送START控制帧
[2025-XX-XX XX:XX:XX] 收到READY应答
[2025-XX-XX XX:XX:XX] 发送帧 0/61
[2025-XX-XX XX:XX:XX] 收到ACK: 帧0
[2025-XX-XX XX:XX:XX] 发送帧 1/61
[2025-XX-XX XX:XX:XX] 收到ACK: 帧1
...
[2025-XX-XX XX:XX:XX] 发送帧 60/61
[2025-XX-XX XX:XX:XX] 收到ACK: 帧60
[2025-XX-XX XX:XX:XX] 发送END控制帧
[2025-XX-XX XX:XX:XX] 收到COMPLETE应答
[2025-XX-XX XX:XX:XX] ✓ 传输成功！
```

**预期日志（下位机串口）：**
```
[IMG_V2] Protocol V2 initialized
[IMG_V2] RX CTRL: cmd=0x01
[IMG_V2] TX CTRL: cmd=0x01
[IMG_V2] Header received
[IMG_V2] Header saved: slot=0
[IMG_V2] Frame 0 received
[IMG_V2] Frame 0 saved: bitmap=0x0000000000000001
[IMG_V2] TX ACK: frame=0
[IMG_V2] Frame 1 received
[IMG_V2] Frame 1 saved: bitmap=0x0000000000000003
[IMG_V2] TX ACK: frame=1
...
[IMG_V2] Frame 60 received
[IMG_V2] Frame 60 saved: bitmap=0x1FFFFFFFFFFFFFFF
[IMG_V2] TX ACK: frame=60
[IMG_V2] RX CTRL: cmd=0x02
[IMG_V2] Transfer complete! Total frames: 62
[IMG_V2] TX CTRL: cmd=0x02
```

---

## ✅ 成功标志

传输成功的关键标志：
- ✓ 上位机显示 "传输成功！"
- ✓ 下位机输出 "Transfer complete! Total frames: 62"
- ✓ 没有NAK或CRC错误
- ✓ frameReceivedBitmap = 0x1FFFFFFFFFFFFFFF（所有61帧都收到）

---

## ❌ 故障排查

### 问题1：没有收到任何响应

**症状：** 上位机发送START后一直等待，没有READY

**检查清单：**
- [ ] MCU是否正确上传了新固件？
- [ ] main.c中是否调用了 `ImageTransferV2_Init()`？
- [ ] main.c中的while循环是否调用了 `ImageTransferV2_Process()`？
- [ ] 串口波特率是否为115200？
- [ ] 串口是否连接正确？

**解决方案：**
```c
// 在main函数中添加调试输出
UARTIF_uartPrintf(0, "ImageTransferV2_Init called\n");
ImageTransferV2_Init();
UARTIF_uartPrintf(0, "ImageTransferV2_Init done\n");

// 在循环中添加调试输出（仅在开发阶段）
static uint32_t debugCounter = 0;
if (tg5ms) {
    tg5ms = FALSE;
    ImageTransferV2_Process();

    if (++debugCounter % 1000 == 0) {  // 每1秒输出一次
        UARTIF_uartPrintf(0, "Process() alive\n");
    }
}
```

### 问题2：收到NAK或CRC错误

**症状：** 上位机日志显示 "收到NAK: 帧X"

**可能原因：**
1. 串口线路质量差
2. 波特率不匹配
3. 数据丢失

**解决方案：**

**选项A：降低波特率**
```javascript
// 在HTML工具中修改（大约第150行）
const BAUD_RATE = 57600;  // 从115200改为57600
```

**选项B：增加重传延迟**
```javascript
// 在HTML工具中修改sendDataFrame函数（大约第450行）
await new Promise(resolve => setTimeout(resolve, 100));  // 从50ms改为100ms
```

**选项C：检查CRC计算**
```c
// 在image_transfer_v2.c中添加调试输出（第XXX行）
UARTIF_uartPrintf(0, "[IMG_V2] CRC: rx=0x%08X, calc=0x%08X\n",
    rx_crc, calc_crc);
```

### 问题3：传输中断

**症状：** 传到某一帧就停止了

**检查：**
1. 查看下位机日志，是否有Flash写入错误
2. 检查frameReceivedBitmap的值

**解决方案：**
```c
// 在main.c中添加诊断调用（传输完成后）
image_transfer_stats_t stats;
ImageTransferV2_GetStats(&stats);
UARTIF_uartPrintf(0, "=== Stats ===\n");
UARTIF_uartPrintf(0, "State: %d\n", stats.state);
UARTIF_uartPrintf(0, "Frames RX: %d\n", stats.frames_received);
UARTIF_uartPrintf(0, "Bitmap: 0x%016llX\n", stats.frame_bitmap);
UARTIF_uartPrintf(0, "Errors: %d\n", stats.error_count);
```

### 问题4：Flash写入失败

**症状：** 下位机日志显示 "Flash write failed"

**检查：**
1. FM_init()是否在ImageTransferV2_Init()之前调用？
2. Flash是否已满？

**解决方案：**
```c
// 手动触发GC（在main.c的某处调用）
FM_forceGarbageCollect();
UARTIF_uartPrintf(0, "GC done\n");
```

---

## 🔧 高级配置

### 调整超时时间

如果传输速度慢，可以增加超时时间：

**文件：** `source/image_transfer_v2.c` 第40-45行
```c
// 原配置
#define TIMEOUT_FRAME         3000   // 3秒

// 可以改为
#define TIMEOUT_FRAME         5000   // 5秒
```

### 调整重传次数

**文件：** `tools/串口调试助手_V3_新协议.html` 第500行
```javascript
// 原配置
const MAX_RETRIES = 5;

// 可以改为
const MAX_RETRIES = 10;  // 增加重传次数
```

### 启用详细调试

**文件：** `source/image_transfer_v2.c` 第30行
```c
// 取消注释以启用详细调试
#define DEBUG_VERBOSE 1

#ifdef DEBUG_VERBOSE
#define DEBUG_V(...) UARTIF_uartPrintf(0, __VA_ARGS__)
#else
#define DEBUG_V(...)
#endif
```

---

## 📊 性能参数

### 正常传输时间

- **61帧数据** = 61 × 248 字节 = 15,128 字节
- **完整帧** = 61 × 259 字节 = 15,799 字节（包含帧头尾等）
- **传输速度** @ 115200 baud ≈ 11.5 KB/s
- **预期时间** ≈ 15,799 ÷ 11,500 ≈ **1.4秒**

加上ACK等待和处理时间：
- **实际传输时间** ≈ **2-3秒**

如果超过5秒还没完成，说明有问题。

---

## 📝 完整的main.c示例

```c
#include "ddl.h"
#include "uart.h"
#include "bt.h"
#include "lpm.h"
#include "gpio.h"
#include "flash.h"

// 应用层头文件
#include "flash_manager.h"
#include "uart_interface.h"
#include "e104.h"
#include "epd.h"
#include "w25q32.h"
#include "image_transfer_v2.h"  // ← 新协议头文件

// 全局变量
volatile boolean_t tg5ms = FALSE;

// Timer中断处理
void Bt0Int(void)
{
    Bt_ClearIntFlag(TIM0);
    UARTIF_passThrough();
    (void)E104_getLinkState();

    // 每1ms触发一次
    tg5ms = TRUE;
}

// Timer初始化
void timInit(void)
{
    stc_bt_config_t stcConfig;

    Bt_Stop(TIM0);

    DDL_ZERO_STRUCT(stcConfig);
    stcConfig.enGateP = BtPositive;
    stcConfig.enGate = BtGateDisable;
    stcConfig.enPRS = BtPCLKDiv8;
    stcConfig.enTog = BtTogDisable;
    stcConfig.enCT = BtTimer;
    stcConfig.enMD = BtMode2;

    Bt_Init(TIM0, &stcConfig);
    Bt_ARRSet(TIM0, 0x03E7);    // 1ms中断
    Bt_Cnt16Set(TIM0, 0x03E7);

    Bt_ClearIntFlag(TIM0);
    EnableNvic(TIM0_IRQn, 3, TRUE);
    Bt_Run(TIM0);
}

// 主函数
int32_t main(void)
{
    // 系统初始化
    Sysctrl_SetRCHTrim(SysctrlRchFreq24MHz);
    Sysctrl_ClkSourceEnable(SysctrlClkRCH, TRUE);

    // 外设初始化
    timInit();
    UARTIF_uartInit();
    UARTIF_lpuartInit();
    W25Q32_Init();
    EPD_initGDEY042Z98();

    // Flash管理器初始化（必须在图像传输之前）
    FM_init();

    // 新协议V2初始化
    ImageTransferV2_Init();

    UARTIF_uartPrintf(0, "\n=== System Ready ===\n");
    UARTIF_uartPrintf(0, "Protocol V2 initialized\n");
    UARTIF_uartPrintf(0, "Waiting for image...\n");

    // 主循环
    while(1)
    {
        // 图像传输处理（每1ms调用）
        if (tg5ms)
        {
            tg5ms = FALSE;
            ImageTransferV2_Process();
        }

        // 其他任务...
    }
}
```

---

## 🎯 下一步

完成以上步骤后，你应该能够：
1. ✅ 成功编译新固件
2. ✅ 成功上传到MCU
3. ✅ 使用新工具传输图像
4. ✅ 看到完整的61帧成功接收

如果一切正常，你可以：
- 删除旧的 `image_transfer.c/h`（可选）
- 删除旧的HTML工具（可选）
- 开始使用新协议进行生产开发

**祝测试顺利！如果遇到问题，请参考故障排查部分或查看详细文档。**
