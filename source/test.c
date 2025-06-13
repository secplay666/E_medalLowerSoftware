void DRAW_CharZ(int x, int y, char c, int width, int height, const unsigned char *fontData, int fontSize, int color)
{
    int index = c - '0';  // 获取字符的索引

    unsigned char lineByte;
    int row, bit;
    // int screenRow, screenCol;
    int byteIndex, bitIndex;
    int originalRow;
    int originalCol;
    int rotatedRow;
    int rotatedCol;
    int fy,fx;
    if (index < 0 || index > 9) return;  // 目前只支持数字字符0-9，如果需要其他字符，需要更新索引处理

    // 遍历字符的每一行，每行包含 width / 8 字节
    for (row = 0; row < height; row++) {
        for (byteIndex = 0; byteIndex < (width / 8); byteIndex++) {
            // 重新访问字库中的字符数据，并按行进行访问
            lineByte = fontData[index * height * (width / 8) + row * (width / 8) + byteIndex];

            // 按位逐个检查字节中的位，确保从低位到高位
            for (bit = 0; bit < 8; bit++) {
                if ((lineByte >> bit) & 0x01) { // 注意：这里从低位到高位检查每一位
                    // 根据缩放的 fontSize，决定每个像素放大多少
                    for (fy = 0; fy < fontSize; fy++) {
                        for (fx = 0; fx < fontSize; fx++) {
                            // 将像素放大，保持字形不变
                            originalRow = y + row * fontSize + fy;  // 沿Y轴放大
                            originalCol = x + (width - 1 - bit) * fontSize + fx;  // 沿X轴放大

                            // 逆时针旋转坐标
                            // originalRow = screenRow;
                            // originalCol = screenCol;
                            rotatedRow = WIDTH - 1 - originalCol;  // 新行
                            rotatedCol = originalRow;             // 新列

                            // 检查坐标是否有效
                            if (rotatedRow < 0 || rotatedRow >= HEIGHT || rotatedCol < 0 || rotatedCol >= WIDTH) {
                                continue; // 超出屏幕范围，跳过
                            }

                            byteIndex = rotatedCol / 8;          // 字节索引
                            bitIndex = 7 - (rotatedCol % 8);     // 位索引

                            // 根据颜色参数设置屏幕上的像素
                            if (color == 1) {
                                screen[rotatedRow][byteIndex] |= (1 << bitIndex); // 设为白色
                            } else {
                                screen[rotatedRow][byteIndex] &= ~(1 << bitIndex); // 设为黑色
                            }
                        }
                    }
                }
            }
        }
    }
}

void DRAW_StringZ(int x, int y, const char *str, int width, int height, const unsigned char *fontData, int fontSize, int color)
{
    int cursorX = x;  // 当前字符的 X 坐标
    int cursorY = y;  // 当前字符的 Y 坐标

    // 遍历字符串中的每个字符
    while (*str) {
        // 调用 DRAW_Char 函数逐个绘制字符
        DRAW_CharZ(cursorX, cursorY, *str, width, height, fontData, fontSize, color);
        
        // 更新 cursorX，使下一个字符显示在当前字符的右侧
        cursorX += (width * fontSize);  // 字符宽度 * fontSize（如果字体放大）
        
        // 移动到下一个字符
        str++;
    }
}
