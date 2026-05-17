#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace oso {

/// RGBA 颜色值，每通道 8 位，作为 OOXML 文档颜色的通用表示
struct Color {
    uint8_t r = 0;  ///< 红色通道
    uint8_t g = 0;  ///< 绿色通道
    uint8_t b = 0;  ///< 蓝色通道
    uint8_t a = 255;  ///< Alpha 通道，255 为完全不透明

    // ---- 工厂方法 ----

    static Color fromRgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, 255}; }

    static Color fromArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, a}; }

    static Color fromHex(uint32_t hex) {
        return {static_cast<uint8_t>((hex >> 16) & 0xFF), static_cast<uint8_t>((hex >> 8) & 0xFF),
                static_cast<uint8_t>(hex & 0xFF), static_cast<uint8_t>((hex >> 24) & 0xFF)};
    }

    // ---- 主题颜色 ----
    // ISO/IEC 29500-1:2016 §20.1.10.27 ST_SchemeColorVal
    enum class ThemeColor {
        Dark1,  // 深色 1（通常黑色）
        Light1,  // 浅色 1（通常白色）
        Dark2,  // 深色 2（通常深灰/蓝灰）
        Light2,  // 浅色 2（通常浅灰）
        Accent1,  // 强调色 1
        Accent2,  // 强调色 2
        Accent3,  // 强调色 3
        Accent4,  // 强调色 4
        Accent5,  // 强调色 5
        Accent6,  // 强调色 6
        Hyperlink,  // 超链接色
        FollowedHyperlink,  // 已访问超链接色
    };

    // 使用默认 Office 主题颜色值构建
    static Color fromTheme(ThemeColor tc) {
        switch (tc) {
            case ThemeColor::Dark1:
                return {0, 0, 0, 255};
            case ThemeColor::Light1:
                return {255, 255, 255, 255};
            case ThemeColor::Dark2:
                return {68, 84, 106, 255};
            case ThemeColor::Light2:
                return {242, 242, 242, 255};
            case ThemeColor::Accent1:
                return {68, 114, 196, 255};
            case ThemeColor::Accent2:
                return {237, 125, 49, 255};
            case ThemeColor::Accent3:
                return {165, 165, 165, 255};
            case ThemeColor::Accent4:
                return {255, 192, 0, 255};
            case ThemeColor::Accent5:
                return {112, 173, 71, 255};
            case ThemeColor::Accent6:
                return {91, 155, 213, 255};
            case ThemeColor::Hyperlink:
                return {5, 99, 193, 255};
            case ThemeColor::FollowedHyperlink:
                return {149, 79, 114, 255};
        }
        return {0, 0, 0, 255};
    }

    // 主题颜色 + tint（变亮）或 shade（变暗）变换
    // tint 值: 0.0 ~ 1.0 (0 = 不变亮, 1 = 完全变白)
    // shade 值: 0.0 ~ 1.0 (0 = 不变暗, 1 = 完全变黑)
    // 公式 (ISO/IEC 29500-1:2016 §20.1.10.28, §20.1.10.47):
    //   tint:   L' = L * tint + (1 - tint)
    //   shade:  L' = L * shade
    static Color fromTheme(ThemeColor tc, double tint, double shade) {
        Color base = fromTheme(tc);
        if (tint > 0.0) {
            return tintColor(base, tint);
        }
        if (shade > 0.0) {
            return shadeColor(base, shade);
        }
        return base;
    }

    // ---- 索引颜色 ----
    // ISO/IEC 29500-1:2016 §20.1.10.29 ST_IndexedColor
    // OOXML 定义 0-64 的索引调色板。
    // 索引 0-7 为系统颜色（与 Windows 调色板对应），
    // 8-63 为默认调色板，64 为自动/系统前景色。

    static Color fromIndexed(uint8_t index) {
        // 系统颜色 (0-7)
        static constexpr Color kIndexed[65] = {
            /* 0  */ {0, 0, 0, 255},  // Black
            /* 1  */ {255, 255, 255, 255},  // White
            /* 2  */ {255, 0, 0, 255},  // Red
            /* 3  */ {0, 255, 0, 255},  // Green
            /* 4  */ {0, 0, 255, 255},  // Blue
            /* 5  */ {255, 255, 0, 255},  // Yellow
            /* 6  */ {255, 0, 255, 255},  // Magenta
            /* 7  */ {0, 255, 255, 255},  // Cyan
            /* 8  */ {128, 0, 0, 255},  // Maroon
            /* 9  */ {0, 128, 0, 255},  // DarkGreen
            /* 10 */ {0, 0, 128, 255},  // Navy
            /* 11 */ {128, 128, 0, 255},  // Olive
            /* 12 */ {128, 0, 128, 255},  // Purple
            /* 13 */ {0, 128, 128, 255},  // Teal
            /* 14 */ {192, 192, 192, 255},  // Silver
            /* 15 */ {128, 128, 128, 255},  // Gray
            /* 16 */ {153, 153, 255, 255},
            /* 17 */ {153, 51, 102, 255},
            /* 18 */ {255, 255, 204, 255},
            /* 19 */ {204, 255, 255, 255},
            /* 20 */ {102, 0, 102, 255},
            /* 21 */ {255, 128, 128, 255},
            /* 22 */ {0, 102, 204, 255},
            /* 23 */ {204, 204, 255, 255},
            /* 24 */ {0, 0, 128, 255},
            /* 25 */ {255, 0, 255, 255},
            /* 26 */ {255, 255, 0, 255},
            /* 27 */ {0, 255, 255, 255},
            /* 28 */ {128, 0, 128, 255},
            /* 29 */ {128, 0, 0, 255},
            /* 30 */ {0, 128, 128, 255},
            /* 31 */ {0, 0, 255, 255},
            /* 32 */ {0, 204, 255, 255},
            /* 33 */ {204, 255, 255, 255},
            /* 34 */ {204, 255, 204, 255},
            /* 35 */ {255, 255, 153, 255},
            /* 36 */ {153, 204, 255, 255},
            /* 37 */ {255, 153, 204, 255},
            /* 38 */ {204, 153, 255, 255},
            /* 39 */ {255, 204, 153, 255},
            /* 40 */ {51, 102, 255, 255},
            /* 41 */ {51, 204, 204, 255},
            /* 42 */ {153, 204, 0, 255},
            /* 43 */ {255, 204, 0, 255},
            /* 44 */ {255, 153, 0, 255},
            /* 45 */ {255, 102, 0, 255},
            /* 46 */ {102, 102, 153, 255},
            /* 47 */ {150, 150, 150, 255},
            /* 48 */ {0, 51, 102, 255},
            /* 49 */ {51, 153, 102, 255},
            /* 50 */ {0, 51, 0, 255},
            /* 51 */ {51, 51, 0, 255},
            /* 52 */ {153, 51, 0, 255},
            /* 53 */ {153, 51, 102, 255},
            /* 54 */ {51, 51, 153, 255},
            /* 55 */ {51, 51, 51, 255},
            /* 56 */ {128, 128, 128, 255},
            /* 57 */ {255, 102, 102, 255},
            /* 58 */ {0, 153, 0, 255},
            /* 59 */ {102, 102, 0, 255},
            /* 60 */ {0, 51, 102, 255},
            /* 61 */ {0, 0, 153, 255},
            /* 62 */ {153, 51, 51, 255},
            /* 63 */ {51, 0, 0, 255},
            /* 64 */ {0, 0, 0, 255},  // Automatic (system foreground)
        };
        if (index <= 64) return kIndexed[index];
        return {0, 0, 0, 255};
    }

    // ---- 转换 ----

    uint32_t toArgb() const {
        return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    }

    uint32_t toRgb() const {
        return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }

    // HTML 样式 "#RRGGBB" 或 "#AARRGGBB"（包含 alpha）
    std::string toHexString() const {
        char buf[10];
        if (a == 255) {
            snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
        } else {
            snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", a, r, g, b);
        }
        return buf;
    }

    // ---- 比较 ----

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }

private:
    // tint: 将颜色向白色方向变换
    static Color tintColor(const Color& c, double tint) {
        auto t = [tint](uint8_t v) -> uint8_t {
            double linear = v / 255.0;
            double result = linear * tint + (1.0 - tint);
            return static_cast<uint8_t>(std::round(result * 255.0));
        };
        return {t(c.r), t(c.g), t(c.b), c.a};
    }

    // shade: 将颜色向黑色方向变换
    static Color shadeColor(const Color& c, double shade) {
        auto s = [shade](uint8_t v) -> uint8_t {
            double linear = v / 255.0;
            double result = linear * shade;
            return static_cast<uint8_t>(std::round(result * 255.0));
        };
        return {s(c.r), s(c.g), s(c.b), c.a};
    }
};

/// 常用预定义颜色常量
namespace colors {
inline constexpr Color kBlack = {0, 0, 0, 255};
inline constexpr Color kWhite = {255, 255, 255, 255};
inline constexpr Color kRed = {255, 0, 0, 255};
inline constexpr Color kGreen = {0, 255, 0, 255};
inline constexpr Color kBlue = {0, 0, 255, 255};
inline constexpr Color kYellow = {255, 255, 0, 255};
inline constexpr Color kCyan = {0, 255, 255, 255};
inline constexpr Color kMagenta = {255, 0, 255, 255};
inline constexpr Color kTransparent = {0, 0, 0, 0};
inline constexpr Color kGray = {128, 128, 128, 255};
inline constexpr Color kDarkGray = {64, 64, 64, 255};
inline constexpr Color kLightGray = {192, 192, 192, 255};
inline constexpr Color kOrange = {255, 165, 0, 255};
inline constexpr Color kPink = {255, 192, 203, 255};
inline constexpr Color kBrown = {165, 42, 42, 255};
}  // namespace colors

}  // namespace oso
