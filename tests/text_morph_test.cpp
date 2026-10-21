#include "TextLayout.hpp"
#include "TextMorph.hpp"
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQuickWindow>
#include <QThread>
#include <QtTest>
#include <cmath>
#include <limits>

namespace {
QVariantMap node(QString text = QStringLiteral("Hello world with wrapping")) {
    return {{"kind", "text"}, {"text", text}, {"w", 420.}, {"h", 180.},
            {"fontSize", 32.}, {"bold", false}, {"align", "left"}};
}
QImage image(const FluxText::Layout& layout) {
    QImage result(640, 480, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::TextAntialiasing);
    layout.draw(painter, Qt::white);
    return result;
}
QImage image(const FluxText::Morph& morph, double progress) {
    QImage result(640, 480, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::TextAntialiasing);
    morph.draw(painter, progress, Qt::white);
    return result;
}
bool hasInk(const QImage& image) {
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)))
                return true;
    return false;
}
} // namespace

class TextMorphTest : public QObject {
    Q_OBJECT
  private slots:
    void spatialWrapFontAlignment() {
        auto a = node(), b = a;
        b["w"] = 210.;
        b["h"] = 260.;
        b["fontSize"] = 46.;
        b["align"] = "right";
        FluxText::Morph morph(a, b);
        QVERIFY(morph.spatial());
        QVERIFY(morph.to().lineCount() > morph.from().lineCount());
        QVERIFY(!morph.from().glyphs().empty());
        const auto start = morph.positions(0), middle = morph.positions(.5), end = morph.positions(1);
        QCOMPARE(start.size(), end.size());
        bool changedLine = false;
        for (size_t i = 0; i < start.size(); ++i) {
            QCOMPARE(middle[i], (start[i] + end[i]) / 2);
            changedLine |= std::abs(start[i].y() - end[i].y()) > 50;
        }
        QVERIFY(changedLine);
        QCOMPARE(image(morph, 0), image(morph.from()));
        QCOMPARE(image(morph, 1), image(morph.to()));
        QVERIFY(image(morph, .5) != image(morph, 0));
        QVERIFY(image(morph, .5) != image(morph, 1));
    }
    void fixedStyleDefaultFontSupportsSpatialResize() {
        // Fedora's default sans-serif is commonly a weight-variable Noto
        // font. A blanket fvar rejection would disable the feature there.
        for (bool bold : {false, true}) {
            auto a = node("Variable font weight remains fixed while these words wrap."), b = a;
            a["bold"] = bold;
            b["bold"] = bold;
            b["fontSize"] = 48.;
            b["w"] = 210.;
            b["align"] = "center";
            FluxText::Morph morph(a, b);
            QVERIFY(morph.spatial());
            QVERIFY(morph.to().lineCount() > morph.from().lineCount());
            QCOMPARE(image(morph, 0), image(morph.from()));
            QCOMPARE(image(morph, 1), image(morph.to()));
        }
    }
    void explicitLineBreaks_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("lf") << QString("A\nB");
        QTest::newRow("crlf") << QString("A\r\nB");
        QTest::newRow("cr") << QString("A\rB");
        QTest::newRow("unicode-line") << QString::fromUtf8("A\u2028B");
        QTest::newRow("unicode-paragraph") << QString::fromUtf8("A\u2029B");
    }
    void explicitLineBreaks() {
        QFETCH(QString, text);
        auto a = node(text), b = a;
        b["fontSize"] = 44.;
        FluxText::Morph morph(a, b);
        QCOMPARE(morph.from().lineCount(), 2);
        QCOMPARE(morph.to().lineCount(), 2);
        QCOMPARE(morph.from().text(), text);
        QVERIFY(morph.spatial());
        QCOMPARE(image(morph, 0), image(morph.from()));
        QCOMPARE(image(morph, 1), image(morph.to()));
    }
    void ligatureHardWrapKeepsWholeShaping() {
        auto a = node("office affinity ffi");
        for (double width : {1., 18., 34., 65., 120.}) {
            auto b = a;
            b["w"] = width;
            b["h"] = 400.;
            FluxText::Morph morph(a, b);
            QCOMPARE(image(morph, 0), image(morph.from()));
            QCOMPARE(image(morph, 1), image(morph.to()));
            const auto& before = morph.from().glyphs();
            const auto& after = morph.to().glyphs();
            bool identical = before.size() == after.size();
            for (size_t i = 0; identical && i < before.size(); ++i)
                identical = before[i].sourceIndex == after[i].sourceIndex &&
                            before[i].index == after[i].index &&
                            before[i].face == after[i].face &&
                            before[i].flags == after[i].flags;
            // Qt may keep a ligature whole even when it overflows a tiny
            // line. If wrapping changes shaping, use the complete fallback.
            if (!identical)
                QVERIFY(!morph.spatial());
            if (morph.spatial()) {
                QVERIFY(identical);
                QCOMPARE(morph.positions(.5).size(), before.size());
            }
        }
    }
    void alignmentMovesGlyphs() {
        auto a = node("Flux"), b = a;
        b["align"] = "center";
        FluxText::Morph centered(a, b);
        b["align"] = "right";
        FluxText::Morph right(a, b);
        QVERIFY(centered.spatial());
        QVERIFY(right.spatial());
        const auto p = centered.positions(0), c = centered.positions(1), r = right.positions(1);
        for (size_t i = 0; i < p.size(); ++i) {
            QVERIFY(c[i].x() > p[i].x());
            QVERIFY(std::abs((r[i].x() - p[i].x()) - 2 * (c[i].x() - p[i].x())) < .01);
            QCOMPARE(c[i].y(), p[i].y());
        }
    }
    void reverseAndSeekingAreStateless() {
        auto a = node(), b = a;
        b["w"] = 170.;
        b["fontSize"] = 44.;
        FluxText::Morph morph(a, b);
        const auto pose = morph.positions(.37);
        const auto frame = image(morph, .37);
        for (double p : {.37, .6, .9, 1., .6, .37, .1, 0., .37}) {
            QVERIFY(hasInk(image(morph, p)));
            if (p == .37) {
                QCOMPARE(morph.positions(p), pose);
                QCOMPARE(image(morph, p), frame);
            }
        }
        QCOMPARE(image(morph, -1), image(morph, 0));
        QCOMPARE(image(morph, 2), image(morph, 1));
        QCOMPARE(image(morph, std::numeric_limits<double>::quiet_NaN()), image(morph, 0));
    }
    void hiddenGlyphsCanEnterClippingBox() {
        auto a = node("First line\nSecond line\nThird line"), b = a;
        a["h"] = 20.;
        b["h"] = 160.;
        b["fontSize"] = 24.;
        FluxText::Morph morph(a, b);
        QVERIFY(morph.spatial());
        QCOMPARE(morph.from().glyphs().size(), morph.to().glyphs().size());
        QVERIFY(morph.from().lineCount() >= 3);
        bool hidden = false;
        for (const auto& glyph : morph.from().glyphs())
            hidden |= glyph.position.y() > a["h"].toDouble();
        QVERIFY(hidden);
        const double p = .4;
        const auto frame = image(morph, p);
        const auto extent = morph.extent(p);
        for (int y = 0; y < frame.height(); ++y)
            for (int x = 0; x < frame.width(); ++x)
                if (x >= std::ceil(extent.width()) || y >= std::ceil(extent.height()))
                    QCOMPARE(qAlpha(frame.pixel(x, y)), 0);
        QVERIFY(hasInk(frame));
    }
    void unicodeShaping_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("combining") << QString::fromUtf8("Cafe\u0301 and A\u030a\u0323");
        QTest::newRow("ligatures") << QString::fromUtf8("office affinity ffi");
        QTest::newRow("arabic-bidi") << QString::fromUtf8("Hello مرحبا بالعالم 123 שלום");
        QTest::newRow("indic") << QString::fromUtf8("नमस्ते दुनिया മലയാളം");
        QTest::newRow("emoji-zwj") << QString::fromUtf8("Family 👨‍👩‍👧‍👦 flag 🇮🇳 skin 👋🏽");
        QTest::newRow("multiline") << QString::fromUtf8("Latin\nمرحبا\n\nThird\r\nFourth");
    }
    void unicodeShaping() {
        QFETCH(QString, text);
        auto a = node(text), b = a;
        b["w"] = 180.;
        b["h"] = 340.;
        b["fontSize"] = 40.;
        b["align"] = "center";
        FluxText::Morph morph(a, b);
        QCOMPARE(morph.from().text(), text);
        QCOMPARE(morph.to().text(), text);
        QCOMPARE(image(morph, 0), image(morph.from()));
        QCOMPARE(image(morph, 1), image(morph.to()));
        QVERIFY(hasInk(image(morph, .5)));
        if (morph.spatial()) {
            const auto& start = morph.from().glyphs();
            const auto& end = morph.to().glyphs();
            QCOMPARE(start.size(), end.size());
            for (size_t i = 0; i < start.size(); ++i) {
                QCOMPARE(start[i].sourceIndex, end[i].sourceIndex);
                QCOMPARE(start[i].index, end[i].index);
                QCOMPARE(start[i].face, end[i].face);
            }
        }
    }
    void fallbackForChangedTextAndBold() {
        auto a = node(), b = a;
        b["bold"] = true;
        b["w"] = 170.;
        b["h"] = 75.;
        FluxText::Morph bold(a, b);
        QVERIFY(!bold.spatial());
        QCOMPARE(image(bold, 0), image(bold.from()));
        QCOMPARE(image(bold, 1), image(bold.to()));
        const auto middle = image(bold, .5);
        QVERIFY(hasInk(middle));
        const auto size = bold.extent(.5);
        for (int y = std::ceil(size.height()); y < middle.height(); ++y)
            for (int x = 0; x < middle.width(); ++x)
                QCOMPARE(qAlpha(middle.pixel(x, y)), 0);
        b = a;
        b["text"] = "Changed words";
        FluxText::Morph changed(a, b);
        QVERIFY(!changed.spatial());
        QCOMPARE(image(changed, 0), image(changed.from()));
        QCOMPARE(image(changed, 1), image(changed.to()));
    }
    void emptyAndLongText() {
        auto a = node(""), b = a;
        b["w"] = 160.;
        FluxText::Morph empty(a, b);
        QVERIFY(empty.spatial());
        QVERIFY(!hasInk(image(empty, .5)));
        a = node(QString(12000, 'a'));
        b = a;
        b["w"] = 160.;
        FluxText::Morph longText(a, b);
        QVERIFY(!longText.spatial());
        QCOMPARE(longText.from().text().size(), 12000);
        QVERIFY(hasInk(image(longText, .5)));
    }
    void identicalFallbackLayoutDoesNotDim() {
        auto a = node(QString(12000, 'a')), b = a;
        // Positions belong to MotionPose and do not change text layout.
        b["x"] = 400.;
        b["y"] = 100.;
        FluxText::Morph morph(a, b);
        QVERIFY(!morph.spatial());
        const auto expected = image(morph.from());
        QVERIFY(hasInk(expected));
        for (double p : {0., .25, .5, .75, 1.})
            QCOMPARE(image(morph, p), expected);
    }
    void paintedItemsShareEndpointsAndUpdate() {
        auto a = node(), b = a;
        b["w"] = 220.;
        b["fontSize"] = 48.;
        TextItem still;
        TextMorphItem moving;
        moving.setFrom(a);
        moving.setTo(b);
        auto render = [](QQuickPaintedItem& item, const QVariantMap& value) {
            item.setWidth(value["w"].toDouble());
            item.setHeight(value["h"].toDouble());
            QImage result(640, 480, QImage::Format_ARGB32_Premultiplied);
            result.fill(Qt::transparent);
            QPainter painter(&result);
            item.paint(&painter);
            return result;
        };
        for (double p : {0., 1., 0., 1.}) {
            const auto value = p == 0 ? a : b;
            still.setNode(value);
            moving.setProgress(p);
            QCOMPARE(render(still, value), render(moving, value));
        }
        b["text"] = "Updated endpoint";
        moving.setTo(b);
        still.setNode(b);
        QCOMPARE(render(still, b), render(moving, b));
        moving.setInkColor(Qt::red);
        still.setInkColor(Qt::red);
        QCOMPARE(render(still, b), render(moving, b));
    }
    void paintedTextureBudget() {
        QQuickWindow window;
        TextItem still(window.contentItem());
        TextMorphItem moving(window.contentItem());
        const double dpr = window.effectiveDevicePixelRatio();
        for (const auto size : {QSizeF(20000, 20000), QSizeF(10000, 10000),
                                QSizeF(20000, 16), QSizeF(16, 20000),
                                QSizeF(1280, 720), QSizeF(470, 132), QSizeF(0, 0)}) {
            for (QQuickPaintedItem* item : {static_cast<QQuickPaintedItem*>(&still),
                                          static_cast<QQuickPaintedItem*>(&moving)}) {
                item->setSize(size);
                const auto texture = item->textureSize();
                QVERIFY(texture.width() >= 1 && texture.height() >= 1);
                const double w = std::ceil(texture.width() * dpr);
                const double h = std::ceil(texture.height() * dpr);
                QVERIFY(w <= 2048 && h <= 2048);
                QVERIFY(w * h <= 1024. * 1024.);
                QCOMPARE(item->size(), size); // Budget does not change document geometry.
            }
        }
    }
    void paintCachesStayOnPaintingThread() {
        TextItem still;
        TextMorphItem moving;
        const auto a = node("Thread-owned shaped text"), b = node("Updated text");
        still.setNode(a);
        still.setSize({420, 180});
        moving.setFrom(a);
        moving.setTo(b);
        moving.setProgress(.5);
        moving.setSize({420, 180});
        QImage result[2];
        for (int pass = 0; pass < 2; ++pass) {
            auto thread = std::unique_ptr<QThread>(QThread::create([&, pass] {
                result[pass] = QImage(420, 180, QImage::Format_ARGB32_Premultiplied);
                result[pass].fill(Qt::transparent);
                QPainter painter(&result[pass]);
                still.paint(&painter);
                moving.paint(&painter);
            }));
            thread->start();
            // The GUI is blocked, as it is during Qt Quick paint synchronization.
            QVERIFY(thread->wait(10000));
        } // Each thread owns and destroys its own cache before item destruction.
        QVERIFY(hasInk(result[0]));
        QCOMPARE(result[0], result[1]);
    }
};
QTEST_MAIN(TextMorphTest)
#include "text_morph_test.moc"
