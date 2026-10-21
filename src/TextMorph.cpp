#include "TextMorph.hpp"
#include "TextLayout.hpp"
#include <QCoreApplication>
#include <QPainter>
#include <QQuickWindow>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <list>
#include <memory>

namespace {
// Qt multiplies textureSize by the window DPR. Explicitly limit both the
// physical dimensions and pixel count; oversized boxes are downsampled,
// while paint() continues to receive their original logical coordinates.
void updateTextureSize(QQuickPaintedItem& item) {
    double dpr = item.window() ? item.window()->effectiveDevicePixelRatio() : 1.;
    if (!std::isfinite(dpr) || dpr <= 0)
        dpr = 1.;
    const double w = std::isfinite(item.width()) ? std::max(1., std::ceil(item.width())) : 1.;
    const double h = std::isfinite(item.height()) ? std::max(1., std::ceil(item.height())) : 1.;
    constexpr double MaxEdge = 2048., MaxPixels = 1024. * 1024.;
    const double edge = std::max(1., std::floor(MaxEdge / dpr));
    const double pixels = std::max(1., std::floor(MaxPixels / dpr / dpr));
    const double scale = std::min({1., edge / w, edge / h, std::sqrt(pixels / w / h)});
    int width = std::max(1, int(std::floor(w * scale)));
    int height = std::max(1, int(std::floor(h * scale)));
    // Fractional DPR rounds physical dimensions upward. Correct that small
    // rounding excess without permitting a zero/default (unbounded) texture.
    while (std::ceil(width * dpr) * std::ceil(height * dpr) > MaxPixels &&
           (width > 1 || height > 1)) {
        if (width >= height && width > 1)
            --width;
        else
            --height;
    }
    item.setTextureSize({width, height});
}

QVariantMap layoutKey(const QVariantMap& node) {
    return {{"text", node.value("text")}, {"w", node.value("w", 100.)},
            {"h", node.value("h", 100.)}, {"fontSize", node.value("fontSize", 40.)},
            {"bold", node.value("bold", false)}, {"align", node.value("align")}};
}

template <typename Plan> class Cache {
    struct Entry {
        QVariantMap from, to;
        std::shared_ptr<Plan> plan;
        size_t glyphs;
    };
    std::list<Entry> m_entries;
    size_t m_glyphs{};
  public:
    template <typename Create, typename Cost>
    std::shared_ptr<const Plan> get(const QVariantMap& from, const QVariantMap& to, Create create, Cost cost) {
        auto found = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.from == from && entry.to == to;
        });
        if (found != m_entries.end()) {
            m_entries.splice(m_entries.begin(), m_entries, found);
            return m_entries.front().plan;
        }
        std::shared_ptr<Plan> plan = create();
        const size_t glyphs = cost(*plan);
        if (glyphs > 32768)
            return plan; // Oversized plans remain local to this one paint call.
        while (!m_entries.empty() &&
               (m_entries.size() >= 64 || m_glyphs + glyphs > 32768)) {
            m_glyphs -= m_entries.back().glyphs;
            m_entries.pop_back();
        }
        m_glyphs += glyphs;
        m_entries.push_front({from, to, std::move(plan), glyphs});
        return m_entries.front().plan;
    }
    void clear() {
        m_entries.clear();
        m_glyphs = 0;
    }
};

// QQuickPaintedItem paint runs with the GUI thread blocked. Immutable input
// maps are safe to read there, but QRawFont/layout lifetime must remain on
// that painting thread too. No item owns a pointer into these bounded caches.
struct PaintCaches {
    Cache<FluxText::Layout> layouts;
    Cache<FluxText::Morph> morphs;
    QMetaObject::Connection finished;
    PaintCaches() {
        finished = QObject::connect(QThread::currentThread(), &QThread::finished,
                                    QThread::currentThread(), [this] { clear(); },
                                    Qt::DirectConnection);
    }
    ~PaintCaches() { QObject::disconnect(finished); }
    void clear() {
        layouts.clear();
        morphs.clear();
    }
};
thread_local std::unique_ptr<PaintCaches> caches;
PaintCaches& paintCaches() {
    if (!caches)
        caches = std::make_unique<PaintCaches>();
    return *caches;
}
void clearLocalCaches() {
    // Direct/software painting can occur on the GUI thread. Release those
    // fonts during item destruction, before QGuiApplication is destroyed.
    // Render-thread caches are untouched and clear when that thread finishes.
    if (caches)
        caches->clear();
}
} // namespace

TextItem::TextItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    updateTextureSize(*this);
}
TextItem::~TextItem() { clearLocalCaches(); }
void TextItem::geometryChange(const QRectF& now, const QRectF& before) {
    QQuickPaintedItem::geometryChange(now, before);
    updateTextureSize(*this);
}
void TextItem::itemChange(ItemChange change, const ItemChangeData& data) {
    QQuickPaintedItem::itemChange(change, data);
    if (change == ItemSceneChange || change == ItemDevicePixelRatioHasChanged)
        updateTextureSize(*this);
}
void TextItem::setNode(const QVariantMap& value) {
    if (m_node == value)
        return;
    m_node = value;
    emit nodeChanged();
    update();
}
void TextItem::setInkColor(const QColor& value) {
    if (m_ink == value)
        return;
    m_ink = value;
    emit inkColorChanged();
    update();
}
void TextItem::paint(QPainter* painter) {
    const auto key = layoutKey(m_node);
    const auto layout = paintCaches().layouts.get(
        key, {}, [&] { return std::make_unique<FluxText::Layout>(key); },
        [](const FluxText::Layout& plan) { return plan.glyphs().size(); });
    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->scale(width() / layout->extent().width(), height() / layout->extent().height());
    layout->draw(*painter, m_ink);
    painter->restore();
}

TextMorphItem::TextMorphItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    updateTextureSize(*this);
}
TextMorphItem::~TextMorphItem() { clearLocalCaches(); }
void TextMorphItem::geometryChange(const QRectF& now, const QRectF& before) {
    QQuickPaintedItem::geometryChange(now, before);
    updateTextureSize(*this);
}
void TextMorphItem::itemChange(ItemChange change, const ItemChangeData& data) {
    QQuickPaintedItem::itemChange(change, data);
    if (change == ItemSceneChange || change == ItemDevicePixelRatioHasChanged)
        updateTextureSize(*this);
}
void TextMorphItem::setFrom(const QVariantMap& value) {
    if (m_from == value)
        return;
    m_from = value;
    emit fromChanged();
    update();
}
void TextMorphItem::setTo(const QVariantMap& value) {
    if (m_to == value)
        return;
    m_to = value;
    emit toChanged();
    update();
}
void TextMorphItem::setProgress(double value) {
    value = std::isfinite(value) ? std::clamp(value, 0., 1.) : 0.;
    if (m_progress == value)
        return;
    m_progress = value;
    emit progressChanged();
    update();
}
void TextMorphItem::setInkColor(const QColor& value) {
    if (m_ink == value)
        return;
    m_ink = value;
    emit inkColorChanged();
    update();
}
void TextMorphItem::paint(QPainter* painter) {
    const auto from = layoutKey(m_from), to = layoutKey(m_to);
    const auto morph = paintCaches().morphs.get(
        from, to, [&] { return std::make_unique<FluxText::Morph>(from, to); },
        [](const FluxText::Morph& plan) {
            return plan.from().glyphs().size() + plan.to().glyphs().size();
        });
    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);
    const auto extent = morph->extent(m_progress);
    painter->scale(width() / extent.width(), height() / extent.height());
    morph->draw(*painter, m_progress, m_ink);
    painter->restore();
}
