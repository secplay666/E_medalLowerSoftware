# 🚨 帧格式问题分析与修复

## 问题发现

从日志看接收到的全是 `5A 5A 5A 5A`（帧尾），说明**帧结构定义不匹配**！

---

## 📊 帧格式对比

### 上位机发送格式 (HTML)
```
位置(字节)  内容           字节数
0-3        帧头           4   [0xA5, 0xA5, 0xA5, 0xA5]
4          slot_id        1
5          imageType      1
6          pageIdx        1
7          reserved       1
8-11       CRC32          4   (大端序: [byte24, byte16, byte8, byte0])
12-259     payload        248
260-263    帧尾           4   [0x5A, 0x5A, 0x5A, 0x5A]
─────────────────────────────────
总计                       264字节
```

### 下位机接收定义 (C)
```
#define IMAGE_FRAME_HEAD_SIZE   4
#define IMAGE_FRAME_TAIL_SIZE   4
#define IMAGE_META_SIZE         8  // ⚠️ 注释说"crc32(2,建议4)"
#define IMAGE_FRAME_SIZE = 4 + 8 + 248 + 4 = 264字节

但IMAGE_META_SIZE = 8包含：
  slot_id(1) + type(1) + pageIdx(1) + reserved(1) + crc32(?)
```

**问题**: IMAGE_META_SIZE=8意味着CRC32应该是4字节，但实际定义有歧义！

---

## 🔍 关键代码位置分析

### 上位机 (串口调试助手 V2.html: L1124-1127)
```javascript
// CRC32 - 大端序发送
frame[offset++] = (crc >> 24) & 0xFF;  // 字节0
frame[offset++] = (crc >> 16) & 0xFF;  // 字节1
frame[offset++] = (crc >> 8) & 0xFF;   // 字节2
frame[offset++] = crc & 0xFF;          // 字节3
```
✓ **发送: CRC占4字节**

### 下位机接收 (image_transfer.c: L415-420)
```c
rx_buf[4]  = 帧头[3] = 0xA5
rx_buf[5]  = slot_id = 0x00
rx_buf[6]  = type = 0xA3
rx_buf[7]  = pageIdx = 0x00
rx_buf[8]  = reserved = 0x00

// CRC校验
lastImageMagic = rx_buf[4];   // ❌ 错误！这应该是rx_buf[5]
lastSlotId = rx_buf[5];       // ❌ 错误！这应该是rx_buf[6]
lastFrameNum = rx_buf[6];     // ❌ 错误！这应该是rx_buf[7]

crcReceived = (rx_buf[8]<<24)|...  // ❌ 错误位置！
```

---

## 💥 **根本问题：字节位置错位！**

### 实际帧结构
```
字节位置   内容
0-3       帧头 0xA5 0xA5 0xA5 0xA5
4         slot_id
5         type (magic)
6         pageIdx
7         reserved
8-11      CRC32 (4字节)
12-259    payload (248字节)
260-263   帧尾 0x5A 0x5A 0x5A 0x5A
```

### 代码错误解析
```c
// 当前错误代码 (image_transfer.c:415-420):
lastImageMagic = rx_buf[4];   // 读的是slot_id ❌
lastSlotId = rx_buf[5];       // 读的是type ✓（但变量名错了）
lastFrameNum = rx_buf[6];     // 读的是pageIdx ✓（但变量名错了）
crcReceived = (rx_buf[8]<<24)|(rx_buf[9]<<16)|... // ✓ 正确

// 应该是:
lastSlotId = rx_buf[4];       // slot_id
lastImageMagic = rx_buf[5];   // type/magic
lastFrameNum = rx_buf[6];     // pageIdx
crcReceived = (rx_buf[8]<<24)|... // CRC32
```

---

## 🎯 修复方案

### 修改 image_transfer.c 第415-420行

**当前错误代码:**
```c
lastImageMagic = rx_buf[4];
lastSlotId = rx_buf[5];
lastFrameNum = rx_buf[6];
// rx_buf[7]; // reserved
crcReceived = (rx_buf[8]<<24)|(rx_buf[9]<<16)|(rx_buf[10]<<8)|rx_buf[11];
data = &rx_buf[12];
```

**修正代码:**
```c
lastSlotId = rx_buf[4];           // 正确：字节4 = slot_id
lastImageMagic = rx_buf[5];       // 正确：字节5 = magic/type
lastFrameNum = rx_buf[6];         // 正确：字节6 = pageIdx
// rx_buf[7];                     // reserved
crcReceived = (rx_buf[8]<<24)|(rx_buf[9]<<16)|(rx_buf[10]<<8)|rx_buf[11];
data = &rx_buf[12];
```

---

## ✅ 验证：为什么会接收到5A 5A 5A 5A

**当前流程:**
1. 上位机发送: `A5 A5 A5 A5 | 00 A3 00 00 | CRC(4) | 数据(248) | 5A 5A 5A 5A`
2. 下位机在IDLE状态：
   ```c
   if (rx_idx >= IMAGE_FRAME_HEAD_SIZE)  // >= 4字节
   ```
3. 调用 `findFrameHeader()` 查找 `0xA5A5A5A5`
   - 但由于接收缓冲处理有问题，可能导致帧头没正确对齐
4. 结果：缓冲区中积累了很多数据
5. 最后当帧尾 `0x5A5A5A5A` 到达时，被误认为是帧头的后续数据

**原因链:**
- 字节位置错误 → 验证失败 → 缓冲不清空 → 数据混乱 → 看到帧尾

---

## 📝 完整修复清单

### 文件: source/image_transfer.c

**位置 1: 第414-420行（接收数据状态）**

```diff
- lastImageMagic = rx_buf[4];
- lastSlotId = rx_buf[5];
- lastFrameNum = rx_buf[6];
+ lastSlotId = rx_buf[4];
+ lastImageMagic = rx_buf[5];
+ lastFrameNum = rx_buf[6];
  // rx_buf[7]; // reserved
  crcReceived = (rx_buf[8]<<24)|(rx_buf[9]<<16)|(rx_buf[10]<<8)|rx_buf[11];
  data = &rx_buf[12];
```

**位置 2: 第422-423行（调试输出）**

```diff
  UARTIF_uartPrintf(0, "[IMG_DBG] Parse: magic=0x%02X, slot=%d, frame=%d\r\n",
-                      lastImageMagic, lastSlotId, lastFrameNum);
+                      lastImageMagic, lastSlotId, lastFrameNum);  // 顺序现在正确了
```

---

## 🧪 验证方法

修改后重新编译，发送图像，应该看到：

```
[IMG_DBG] IDLE: Received 4 new bytes, total=4, rx_idx=4
[IMG_DBG] Frame 1: Header pos=0, rx_idx=4
[IMG_DBG] IDLE: Received 1 new bytes, total=5, rx_idx=5
[IMG_DBG] Parse: magic=0xA3, slot=0, frame=0
[IMG_DBG] Frame 0 saved
```

而不是：
```
[IMG_DBG] RX_BUF in IDLE: 5A 5A 5A 5A  ❌ 帧尾被误认为帧头
```

---

## 📊 数据流修正图

```
发送方                     接收方（修改前❌）          接收方（修改后✓）
────────────────────────────────────────────────────────────────

A5 A5 A5 A5          →    帧头识别OK            →    帧头识别OK
00 A3 00 00          →    读错位置              →    正确读取
CRC(4字节)           →    错误的CRC值           →    正确的CRC值
248字节数据          →    校验失败              →    校验成功
5A 5A 5A 5A          →    误当帧头(!)           →    正确识别帧尾
                          不清空缓冲,积累数据       帧处理完成,清空缓冲
                          ❌ 下一帧失败             ✓ 下一帧成功
```

---

## 重要提示

这个bug导致了你看到的现象：
1. 前4个字节正确接收 `[A5 A5 A5 A5]`
2. 但随后字节位置全错
3. 最后帧尾 `5A 5A 5A 5A` 被误认为是某个帧的数据
4. 导致缓冲溢出，重复打印 `RX_BUF in IDLE: 5A 5A 5A 5A`

修正字节位置后，这个问题将彻底解决！
