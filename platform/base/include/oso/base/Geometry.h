#pragma once

#include <cstdint>

namespace oso {

/// 二维尺寸，用于表示宽度和高度
struct Size {
    float width = 0;  ///< 宽度
    float height = 0;  ///< 高度
};

/// 二维坐标点
struct Point {
    float x = 0;  ///< X 坐标
    float y = 0;  ///< Y 坐标
};

/// 矩形区域
struct Rect {
    float x = 0;  ///< 左上角 X 坐标
    float y = 0;  ///< 左上角 Y 坐标
    float width = 0;  ///< 宽度
    float height = 0;  ///< 高度

    float left() const { return x; }  ///< 左边界 X 坐标
    float right() const { return x + width; }  ///< 右边界 X 坐标
    float top() const { return y; }  ///< 上边界 Y 坐标
    float bottom() const { return y + height; }  ///< 下边界 Y 坐标

    bool isEmpty() const { return width <= 0 || height <= 0; }  ///< 是否为空矩形
    bool contains(Point p) const {  ///< 是否包含指定点
        return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom();
    }
};

}  // namespace oso
