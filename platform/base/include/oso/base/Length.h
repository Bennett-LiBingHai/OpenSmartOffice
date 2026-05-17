#pragma once

#include <cmath>
#include <cstdint>

namespace oso {

// OOXML 以 EMU (English Metric Unit) 为内部标准单位。
// 转换常量基于 ISO/IEC 29500-1:2016 定义。
//
//   1 inch  = 914400 EMU
//   1 cm    = 360000 EMU
//   1 mm    = 36000 EMU
//   1 pt    = 12700 EMU
//   1 px    = 取决于 DPI(dot per inch)，默认 96 DPI → 9525 EMU

/// OOXML 长度值，以 EMU 为内部存储单位，支持多种单位互转
class Length {
public:
    // ---- 工厂方法（从各种单位构造）----

    static Length fromEmu(int64_t emu) { return Length(emu); }

    static Length fromCm(double cm) {
        return Length(static_cast<int64_t>(std::round(cm * kEmuPerCm)));
    }

    static Length fromMm(double mm) {
        return Length(static_cast<int64_t>(std::round(mm * kEmuPerMm)));
    }

    static Length fromInch(double inch) {
        return Length(static_cast<int64_t>(std::round(inch * kEmuPerInch)));
    }

    static Length fromPt(double pt) {
        return Length(static_cast<int64_t>(std::round(pt * kEmuPerPt)));
    }

    static Length fromPx(double px, double dpi = 96.0) {
        // 打印场景1px=1dot
        return Length(static_cast<int64_t>(std::round(px / dpi * kEmuPerInch)));
    }

    // ---- 转换为各种单位 ----

    int64_t toEmu() const { return m_emu; }

    double toCm() const { return static_cast<double>(m_emu) / kEmuPerCm; }

    double toMm() const { return static_cast<double>(m_emu) / kEmuPerMm; }

    double toInch() const { return static_cast<double>(m_emu) / kEmuPerInch; }

    double toPt() const { return static_cast<double>(m_emu) / kEmuPerPt; }

    double toPx(double dpi = 96.0) const { return static_cast<double>(m_emu) / kEmuPerInch * dpi; }

    // ---- 算术运算 ----

    Length operator+(Length o) const { return Length(m_emu + o.m_emu); }
    Length operator-(Length o) const { return Length(m_emu - o.m_emu); }
    Length operator*(double factor) const {
        return Length(static_cast<int64_t>(std::round(m_emu * factor)));
    }
    Length operator/(double divisor) const {
        return Length(static_cast<int64_t>(std::round(m_emu / divisor)));
    }

    Length& operator+=(Length o) {
        m_emu += o.m_emu;
        return *this;
    }
    Length& operator-=(Length o) {
        m_emu -= o.m_emu;
        return *this;
    }

    // ---- 比较 ----

    bool operator==(Length o) const { return m_emu == o.m_emu; }
    bool operator!=(Length o) const { return m_emu != o.m_emu; }
    bool operator<(Length o) const { return m_emu < o.m_emu; }
    bool operator<=(Length o) const { return m_emu <= o.m_emu; }
    bool operator>(Length o) const { return m_emu > o.m_emu; }
    bool operator>=(Length o) const { return m_emu >= o.m_emu; }

    // 零长度
    static Length zero() { return Length(0); }

    // 是否为 0 或负值
    bool isZero() const { return m_emu == 0; }
    bool isPositive() const { return m_emu > 0; }

private:
    explicit Length(int64_t emu) : m_emu(emu) {}  ///< 从 EMU 值直接构造，仅内部使用

    int64_t m_emu;  ///< EMU (English Metric Unit) 值，1 inch = 914400 EMU

    // 转换常数 (基于 ISO/IEC 29500-1:2016)
    static constexpr int64_t kEmuPerCm = 360000;  ///< 1 厘米 = 360000 EMU
    static constexpr int64_t kEmuPerMm = 36000;  ///< 1 毫米 = 36000 EMU
    static constexpr int64_t kEmuPerInch = 914400;  ///< 1 英寸 = 914400 EMU
    static constexpr int64_t kEmuPerPt = 12700;  ///< 1 磅 = 12700 EMU
};

/// 常用长度常量
namespace lengths {
inline const Length kZero = Length::fromEmu(0);  ///< 零长度
inline const Length kOneInch = Length::fromInch(1.0);  ///< 1 英寸
inline const Length kOneCm = Length::fromCm(1.0);  ///< 1 厘米
inline const Length kOneMm = Length::fromMm(1.0);  ///< 1 毫米
inline const Length kOnePt = Length::fromPt(1.0);  ///< 1 磅
inline const Length kA4Width = Length::fromMm(210.0);  ///< A4 纸宽度
inline const Length kA4Height = Length::fromMm(297.0);  ///< A4 纸高度
inline const Length kLetterWidth = Length::fromInch(8.5);  ///< Letter 纸宽度
inline const Length kLetterHeight = Length::fromInch(11.0);  ///< Letter 纸高度
}  // namespace lengths

}  // namespace oso
