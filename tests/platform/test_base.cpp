#include "oso/base/Color.h"
#include "oso/base/DocPath.h"
#include "oso/base/ErrorCode.h"
#include "oso/base/Geometry.h"
#include "oso/base/Length.h"
#include "oso/base/Result.h"

#include <gtest/gtest.h>

using namespace oso;

// ============================================================
// ErrorCode
// ============================================================

TEST(ErrorCode, OkIsNotErr) {
    ErrorCode ok;
    EXPECT_TRUE(ok.isOk());
    EXPECT_FALSE(ok.isErr());
    EXPECT_EQ(ok.value(), ErrorCode::Ok);
}

TEST(ErrorCode, LayerEncoding) {
    // Platform (0x0000_0000 ~ 0x0FFF_FFFF)
    EXPECT_EQ(ErrorCode(ErrorCode::IOFileNotFound).raw() >> 28, 0x0);
    // Infra (0x1000_0000 ~ 0x1FFF_FFFF)
    EXPECT_EQ(ErrorCode(ErrorCode::OOXMLZipCorrupted).raw() >> 28, 0x1);
    // Core (0x2000_0000 ~ 0x2FFF_FFFF)
    EXPECT_EQ(ErrorCode(ErrorCode::DOMInvalidNodeType).raw() >> 28, 0x2);
}

TEST(ErrorCode, SubModuleEncoding) {
    // Sub-module occupies 4 bits within the upper 16 bits, at [19:16]
    uint32_t raw = ErrorCode(ErrorCode::OOXMLXmlParseError).raw();
    uint32_t subModule = (raw >> 16) & 0xF;
    EXPECT_EQ(subModule, 0x1);  // OOXML = 0x1 within Infra

    raw = ErrorCode(ErrorCode::FormulaSyntaxError).raw();
    subModule = (raw >> 16) & 0xF;
    EXPECT_EQ(subModule, 0x2);  // Formula = 0x2 within Infra
}

TEST(ErrorCode, ToString) {
    EXPECT_EQ(ErrorCode(ErrorCode::Ok).toString(), "Ok");
    EXPECT_EQ(ErrorCode(ErrorCode::IOFileNotFound).toString(), "IOFileNotFound");
    EXPECT_EQ(ErrorCode(ErrorCode::OOXMLZipCorrupted).toString(), "OOXMLZipCorrupted");
}

TEST(ErrorCode, Equality) {
    EXPECT_EQ(ErrorCode(ErrorCode::DOMInvalidNodeType), ErrorCode(ErrorCode::DOMInvalidNodeType));
    EXPECT_NE(ErrorCode(ErrorCode::Ok), ErrorCode(ErrorCode::IOReadError));
}

TEST(ErrorCode, UnknownFallsThrough) {
    ErrorCode unknown(static_cast<ErrorCode::Value>(0xDEADBEEF));
    EXPECT_TRUE(unknown.toString().find("Unknown") != std::string::npos);
}

// ============================================================
// Result<T>
// ============================================================

TEST(Result, OkInt) {
    auto r = Result<int>::ok(42);
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isErr());
    EXPECT_EQ(r.value(), 42);
}

TEST(Result, ErrInt) {
    auto r = Result<int>::err(ErrorCode::IOReadError, "cannot read");
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::IOReadError);
    EXPECT_EQ(r.message(), "cannot read");
}

TEST(Result, TakeValue) {
    auto r = Result<std::string>::ok("hello");
    auto s = r.takeValue();
    EXPECT_EQ(s, "hello");
}

TEST(Result, VoidOk) {
    auto r = Result<void>::ok();
    EXPECT_TRUE(r.isOk());
    EXPECT_FALSE(r.isErr());
}

TEST(Result, VoidErr) {
    auto r = Result<void>::err(ErrorCode::ConcurrentShutdown);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::ConcurrentShutdown);
}

static Result<int> divide(int a, int b) {
    if (b == 0) return Result<int>::err(ErrorCode::DOMInvalidOperation, "div by zero");
    return Result<int>::ok(a / b);
}

TEST(OSOTry, PropagatesError) {
    auto fn = [](int x) -> Result<int> {
        int val;
        OSO_TRY_ASSIGN(val, divide(10, x));
        return Result<int>::ok(val * 2);
    };
    auto r = fn(0);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error(), ErrorCode::DOMInvalidOperation);
    EXPECT_EQ(r.message(), "div by zero");
}

TEST(OSOTry, PassesOk) {
    auto fn = [](int x) -> Result<int> {
        int val;
        OSO_TRY_ASSIGN(val, divide(10, x));
        return Result<int>::ok(val * 2);
    };
    auto r = fn(2);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 10);  // 10/2*2
}

TEST(OSOTryAssign, AssignsValue) {
    auto fn = [](int x) -> Result<int> {
        int v;
        OSO_TRY_ASSIGN(v, divide(20, x));
        return Result<int>::ok(v + 1);
    };
    auto r = fn(4);
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(r.value(), 6);  // 20/4 + 1
}

// ============================================================
// Color
// ============================================================

TEST(Color, FromRgb) {
    auto c = Color::fromRgb(255, 128, 64);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 128);
    EXPECT_EQ(c.b, 64);
    EXPECT_EQ(c.a, 255);
}

TEST(Color, FromArgb) {
    auto c = Color::fromArgb(128, 255, 0, 0);
    EXPECT_EQ(c.a, 128);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
}

TEST(Color, FromHex) {
    auto c = Color::fromHex(0xFF336699);
    EXPECT_EQ(c.a, 255);
    EXPECT_EQ(c.r, 0x33);
    EXPECT_EQ(c.g, 0x66);
    EXPECT_EQ(c.b, 0x99);
}

TEST(Color, FromHexWithAlpha) {
    auto c = Color::fromHex(0x80FF0000);
    EXPECT_EQ(c.a, 0x80);
    EXPECT_EQ(c.r, 0xFF);
}

TEST(Color, ToArgb) {
    auto c = Color::fromRgb(0x11, 0x22, 0x33);
    EXPECT_EQ(c.toArgb(), 0xFF112233U);
}

TEST(Color, ToHexString) {
    EXPECT_EQ(Color::fromRgb(255, 255, 255).toHexString(), "#FFFFFF");
    EXPECT_EQ(Color::fromRgb(0, 0, 0).toHexString(), "#000000");
    EXPECT_EQ(Color::fromRgb(0xAB, 0xCD, 0xEF).toHexString(), "#ABCDEF");
}

TEST(Color, ToHexStringWithAlpha) {
    EXPECT_EQ(Color::fromArgb(128, 255, 255, 255).toHexString(), "#80FFFFFF");
}

TEST(Color, PredefinedColors) {
    EXPECT_EQ(colors::kBlack, Color::fromRgb(0, 0, 0));
    EXPECT_EQ(colors::kWhite, Color::fromRgb(255, 255, 255));
    EXPECT_EQ(colors::kRed, Color::fromRgb(255, 0, 0));
    EXPECT_EQ(colors::kTransparent, Color::fromArgb(0, 0, 0, 0));
}

TEST(Color, ThemeDark1) {
    auto c = Color::fromTheme(Color::ThemeColor::Dark1);
    EXPECT_EQ(c, colors::kBlack);
}

TEST(Color, ThemeLight1) {
    auto c = Color::fromTheme(Color::ThemeColor::Light1);
    EXPECT_EQ(c, colors::kWhite);
}

TEST(Color, ThemeHyperlink) {
    auto c = Color::fromTheme(Color::ThemeColor::Hyperlink);
    EXPECT_NE(c, colors::kBlack);  // should be a distinct hyperlink color
}

TEST(Color, ThemeWithTint) {
    auto base = Color::fromTheme(Color::ThemeColor::Accent1);
    auto tinted = Color::fromTheme(Color::ThemeColor::Accent1, 0.5, 0.0);
    // tinted should be lighter than base
    EXPECT_GT(tinted.r + tinted.g + tinted.b, base.r + base.g + base.b);
}

TEST(Color, ThemeWithShade) {
    auto base = Color::fromTheme(Color::ThemeColor::Accent1);
    auto shaded = Color::fromTheme(Color::ThemeColor::Accent1, 0.0, 0.5);
    // shaded should be darker than base
    EXPECT_LT(shaded.r + shaded.g + shaded.b, base.r + base.g + base.b);
}

TEST(Color, ThemeAllColorsHaveValues) {
    using TC = Color::ThemeColor;
    std::vector<TC> all = {
        TC::Dark1,   TC::Light1,  TC::Dark2,   TC::Light2,  TC::Accent1,   TC::Accent2,
        TC::Accent3, TC::Accent4, TC::Accent5, TC::Accent6, TC::Hyperlink, TC::FollowedHyperlink,
    };
    for (auto tc : all) {
        auto c = Color::fromTheme(tc);
        EXPECT_TRUE(c.a > 0);
    }
}

TEST(Color, IndexedSystemColors) {
    EXPECT_EQ(Color::fromIndexed(0), colors::kBlack);
    EXPECT_EQ(Color::fromIndexed(1), colors::kWhite);
    EXPECT_EQ(Color::fromIndexed(2), colors::kRed);
    EXPECT_EQ(Color::fromIndexed(3), colors::kGreen);
    EXPECT_EQ(Color::fromIndexed(4), colors::kBlue);
}

TEST(Color, IndexedBeyondTableReturnsBlack) {
    auto c = Color::fromIndexed(255);
    EXPECT_EQ(c, colors::kBlack);
}

TEST(Color, IndexedAllIndicesValid) {
    for (uint8_t i = 0; i <= 64; ++i) {
        auto c = Color::fromIndexed(i);
        EXPECT_LE(c.r, 255);
        EXPECT_LE(c.g, 255);
        EXPECT_LE(c.b, 255);
    }
}

// ============================================================
// Geometry
// ============================================================

TEST(Point, DefaultZero) {
    Point p;
    EXPECT_EQ(p.x, 0);
    EXPECT_EQ(p.y, 0);
}

TEST(Point, Value) {
    Point p{3.5f, -2.0f};
    EXPECT_FLOAT_EQ(p.x, 3.5f);
    EXPECT_FLOAT_EQ(p.y, -2.0f);
}

TEST(Size, DefaultZero) {
    Size s;
    EXPECT_EQ(s.width, 0);
    EXPECT_EQ(s.height, 0);
}

TEST(Size, Value) {
    Size s{100.f, 50.f};
    EXPECT_FLOAT_EQ(s.width, 100.f);
    EXPECT_FLOAT_EQ(s.height, 50.f);
}

TEST(Rect, DefaultZero) {
    Rect r;
    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 0);
    EXPECT_EQ(r.width, 0);
    EXPECT_EQ(r.height, 0);
}

TEST(Rect, Edges) {
    Rect r{10, 20, 100, 50};
    EXPECT_FLOAT_EQ(r.left(), 10);
    EXPECT_FLOAT_EQ(r.top(), 20);
    EXPECT_FLOAT_EQ(r.right(), 110);
    EXPECT_FLOAT_EQ(r.bottom(), 70);
}

TEST(Rect, IsEmpty) {
    EXPECT_TRUE((Rect{0, 0, 0, 0}.isEmpty()));
    EXPECT_TRUE((Rect{0, 0, -1, 10}.isEmpty()));
    EXPECT_FALSE((Rect{0, 0, 10, 10}.isEmpty()));
}

TEST(Rect, Contains) {
    Rect r{10, 10, 100, 100};
    EXPECT_TRUE(r.contains({10, 10}));
    EXPECT_TRUE(r.contains({110, 110}));  // boundary
    EXPECT_TRUE(r.contains({50, 50}));
    EXPECT_FALSE(r.contains({0, 0}));
    EXPECT_FALSE(r.contains({200, 200}));
}

// ============================================================
// Length
// ============================================================

TEST(Length, Zero) {
    EXPECT_EQ(Length::zero().toEmu(), 0);
    EXPECT_TRUE(Length::zero().isZero());
    EXPECT_FALSE(Length::zero().isPositive());
}

TEST(Length, FromEmu) {
    auto l = Length::fromEmu(914400);
    EXPECT_EQ(l.toEmu(), 914400);
}

TEST(Length, FromCm) {
    auto l = Length::fromCm(2.54);
    EXPECT_NEAR(l.toInch(), 1.0, 0.0001);
}

TEST(Length, FromInch) {
    auto l = Length::fromInch(1.0);
    EXPECT_NEAR(l.toCm(), 2.54, 0.0001);
    EXPECT_NEAR(l.toMm(), 25.4, 0.0001);
    EXPECT_NEAR(l.toPt(), 72.0, 0.001);
}

TEST(Length, FromPt) {
    auto l = Length::fromPt(72.0);
    EXPECT_NEAR(l.toInch(), 1.0, 0.0001);
}

TEST(Length, FromPx96Dpi) {
    auto l = Length::fromPx(96.0, 96.0);
    EXPECT_NEAR(l.toInch(), 1.0, 0.0001);
}

TEST(Length, RoundTrip) {
    auto l = Length::fromCm(5.0);
    EXPECT_NEAR(l.toCm(), 5.0, 0.001);
    EXPECT_NEAR(l.toMm(), 50.0, 0.01);
}

TEST(Length, Addition) {
    auto a = Length::fromCm(1.0);
    auto b = Length::fromCm(2.0);
    EXPECT_NEAR((a + b).toCm(), 3.0, 0.001);
}

TEST(Length, Subtraction) {
    auto a = Length::fromCm(5.0);
    auto b = Length::fromCm(3.0);
    EXPECT_NEAR((a - b).toCm(), 2.0, 0.001);
}

TEST(Length, Multiply) {
    auto a = Length::fromCm(2.0);
    EXPECT_NEAR((a * 3.0).toCm(), 6.0, 0.001);
}

TEST(Length, Divide) {
    auto a = Length::fromCm(10.0);
    EXPECT_NEAR((a / 2.0).toCm(), 5.0, 0.001);
}

TEST(Length, Comparison) {
    EXPECT_LT(Length::fromCm(1.0), Length::fromCm(2.0));
    EXPECT_GT(Length::fromCm(2.0), Length::fromCm(1.0));
    EXPECT_EQ(Length::fromCm(1.0), Length::fromMm(10.0));
}

TEST(Length, CompoundAssignment) {
    auto l = Length::fromCm(1.0);
    l += Length::fromCm(2.0);
    EXPECT_NEAR(l.toCm(), 3.0, 0.001);
    l -= Length::fromCm(1.0);
    EXPECT_NEAR(l.toCm(), 2.0, 0.001);
}

TEST(Length, A4Dimensions) {
    EXPECT_NEAR(lengths::kA4Width.toMm(), 210.0, 0.01);
    EXPECT_NEAR(lengths::kA4Height.toMm(), 297.0, 0.01);
}

TEST(Length, LetterDimensions) {
    EXPECT_NEAR(lengths::kLetterWidth.toInch(), 8.5, 0.001);
    EXPECT_NEAR(lengths::kLetterHeight.toInch(), 11.0, 0.001);
}

// ============================================================
// DocPath — PartPath
// ============================================================

TEST(PartPath, FromString) {
    auto p = PartPath::fromString("/word/document.xml");
    EXPECT_EQ(p.toString(), "/word/document.xml");
}

TEST(PartPath, AutoPrefixSlash) {
    auto p = PartPath::fromString("word/document.xml");
    EXPECT_EQ(p.toString(), "/word/document.xml");
}

TEST(PartPath, RelativePath) {
    auto p = PartPath::fromString("/xl/worksheets/sheet1.xml");
    EXPECT_EQ(p.relativePath(), "xl/worksheets/sheet1.xml");
}

TEST(PartPath, Name) {
    auto p = PartPath::fromString("/word/document.xml");
    EXPECT_EQ(p.name(), "document.xml");
}

TEST(PartPath, Directory) {
    auto p = PartPath::fromString("/xl/worksheets/sheet1.xml");
    EXPECT_EQ(p.directory(), "/xl/worksheets/");
}

TEST(PartPath, Extension) {
    EXPECT_EQ(PartPath::fromString("/word/document.xml").extension(), "xml");
    EXPECT_EQ(PartPath::fromString("/word/media/image1.png").extension(), "png");
    EXPECT_EQ(PartPath::fromString("/word/_rels/document.xml.rels").extension(), "rels");
}

TEST(PartPath, Empty) {
    PartPath p;
    EXPECT_TRUE(p.empty());
    EXPECT_TRUE(PartPath::fromString("").empty());
    EXPECT_EQ(p.directory(), "");
}

TEST(PartPath, Comparison) {
    auto a = PartPath::fromString("/word/a.xml");
    auto b = PartPath::fromString("/word/b.xml");
    EXPECT_LT(a, b);
    EXPECT_EQ(a, a);
    EXPECT_NE(a, b);
}

// ============================================================
// DocPath — RelationshipId
// ============================================================

TEST(RelationshipId, FromString) {
    auto rid = RelationshipId::fromString("rId4");
    EXPECT_EQ(rid.toString(), "rId4");
}

TEST(RelationshipId, FromIndex) {
    auto rid = RelationshipId::fromIndex(10);
    EXPECT_EQ(rid.toString(), "rId10");
}

TEST(RelationshipId, Index) {
    EXPECT_EQ(RelationshipId::fromString("rId8").index(), 8);
    EXPECT_EQ(RelationshipId::fromString("rId123").index(), 123);
}

TEST(RelationshipId, InvalidIndex) {
    EXPECT_EQ(RelationshipId::fromString("not-an-rid").index(), -1);
    EXPECT_EQ(RelationshipId::fromString("").index(), -1);
}

TEST(RelationshipId, Empty) {
    EXPECT_TRUE(RelationshipId().empty());
    EXPECT_TRUE(RelationshipId::fromString("").empty());
    EXPECT_FALSE(RelationshipId::fromString("rId1").empty());
}

TEST(RelationshipId, Comparison) {
    EXPECT_LT(RelationshipId::fromString("rId1"), RelationshipId::fromString("rId2"));
    EXPECT_EQ(RelationshipId::fromIndex(4), RelationshipId::fromString("rId4"));
}

TEST(RelationshipId, FromIndexRejectsZero) { EXPECT_TRUE(RelationshipId::fromIndex(0).empty()); }

TEST(RelationshipId, FromIndexRejectsNegative) {
    EXPECT_TRUE(RelationshipId::fromIndex(-1).empty());
}

TEST(RelationshipId, isValid) {
    EXPECT_TRUE(RelationshipId::fromString("rId1").isValid());
    EXPECT_TRUE(RelationshipId::fromString("rId999").isValid());
    EXPECT_FALSE(RelationshipId::fromString("rId0").isValid());
    EXPECT_FALSE(RelationshipId::fromString("rId").isValid());
    EXPECT_FALSE(RelationshipId::fromString("xyz").isValid());
    EXPECT_FALSE(RelationshipId().isValid());
}

TEST(RelationshipId, IndexManualParse) {
    EXPECT_EQ(RelationshipId::fromString("rId42").index(), 42);
    EXPECT_EQ(RelationshipId::fromString("rId0").index(), 0);
    EXPECT_EQ(RelationshipId::fromString("rId007").index(), 7);
    EXPECT_EQ(RelationshipId::fromString("rIdabc").index(), -1);
}

// ============================================================
// DocPath — ExternalReference
// ============================================================

TEST(ExternalReference, UrlType) {
    auto ref = ExternalReference::fromString("https://example.com/doc.docx");
    EXPECT_EQ(ref.type(), ExternalReference::Type::Url);
    EXPECT_TRUE(ref.isAbsolute());
}

TEST(ExternalReference, MailtoType) {
    auto ref = ExternalReference::fromString("mailto:user@example.com");
    EXPECT_EQ(ref.type(), ExternalReference::Type::Mailto);
}

TEST(ExternalReference, FileType) {
    auto ref = ExternalReference::fromString("file:///home/user/doc.docx");
    EXPECT_EQ(ref.type(), ExternalReference::Type::File);
}

TEST(ExternalReference, RelativePath) {
    auto ref = ExternalReference::fromString("../images/photo.png");
    EXPECT_EQ(ref.type(), ExternalReference::Type::File);
    EXPECT_TRUE(ref.isRelative());
}

TEST(ExternalReference, RelativePathDotSlash) {
    auto ref = ExternalReference::fromString("./config.json");
    EXPECT_EQ(ref.type(), ExternalReference::Type::File);
    EXPECT_TRUE(ref.isRelative());
}

TEST(ExternalReference, PlainRelativePath) {
    auto ref = ExternalReference::fromString("images/pic.png");
    EXPECT_EQ(ref.type(), ExternalReference::Type::File);
    EXPECT_TRUE(ref.isRelative());
}

TEST(ExternalReference, AbsolutePath) {
    // "/etc/config.cfg" 无 scheme，按 URI 语义不是 absolute
    auto ref = ExternalReference::fromString("/etc/config.cfg");
    EXPECT_EQ(ref.type(), ExternalReference::Type::File);
    EXPECT_TRUE(ref.isRelative());
}

TEST(ExternalReference, WindowsDrivePath) {
    auto ref = ExternalReference::fromString("C:\\Users\\data.xlsx");
    EXPECT_EQ(ref.type(), ExternalReference::Type::File);
}

TEST(ExternalReference, Empty) {
    ExternalReference ref;
    EXPECT_TRUE(ref.empty());
}

TEST(ExternalReference, Equality) {
    auto a = ExternalReference::fromString("https://example.com");
    auto b = ExternalReference::fromString("https://example.com");
    EXPECT_EQ(a, b);
}

TEST(ExternalReference, MailtoIsNotRelative) {
    auto ref = ExternalReference::fromString("mailto:user@example.com");
    EXPECT_FALSE(ref.isRelative());
    EXPECT_TRUE(ref.isAbsolute());
}

TEST(ExternalReference, HasScheme) {
    EXPECT_TRUE(ExternalReference::fromString("https://x.com").hasScheme());
    EXPECT_TRUE(ExternalReference::fromString("mailto:x@x.com").hasScheme());
    EXPECT_TRUE(ExternalReference::fromString("file:///tmp").hasScheme());
    EXPECT_TRUE(ExternalReference::fromString("urn:isbn:0-486-27557-4").hasScheme());
    EXPECT_TRUE(ExternalReference::fromString("data:text/plain,hello").hasScheme());
    EXPECT_FALSE(ExternalReference::fromString("../img.png").hasScheme());
    EXPECT_FALSE(ExternalReference::fromString("/absolute/path").hasScheme());
    EXPECT_FALSE(ExternalReference().hasScheme());
}

TEST(ExternalReference, IsAbsoluteSchemeBased) {
    // scheme-based absolute
    EXPECT_TRUE(ExternalReference::fromString("mailto:a@b.com").isAbsolute());
    EXPECT_TRUE(ExternalReference::fromString("urn:uuid:...").isAbsolute());
    // protocol-relative
    EXPECT_TRUE(ExternalReference::fromString("//cdn.example.com/lib.js").isAbsolute());
    // plain paths are not absolute
    EXPECT_FALSE(ExternalReference::fromString("/etc/hosts").isAbsolute());
    EXPECT_FALSE(ExternalReference::fromString("../img.png").isAbsolute());
}

TEST(ExternalReference, OtherSchemeType) {
    auto ref = ExternalReference::fromString("ftp://files.example.com");
    EXPECT_EQ(ref.type(), ExternalReference::Type::Other);
    EXPECT_TRUE(ref.isAbsolute());

    auto ref2 = ExternalReference::fromString("urn:isbn:0-486-27557-4");
    EXPECT_EQ(ref2.type(), ExternalReference::Type::Other);
    EXPECT_TRUE(ref2.isAbsolute());
}
