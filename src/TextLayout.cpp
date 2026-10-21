#include "TextLayout.hpp"
#include <QCryptographicHash>
#include <QFont>
#include <QHash>
#include <QPainterPath>
#include <QTextOption>
#include <QtEndian>
#include <algorithm>
#include <cmath>

namespace {
double unit(double value) {
    return std::isfinite(value) ? std::clamp(value, 0., 1.) : 0.;
}
double dimension(const QVariantMap& node, const char* key, double fallback) {
    const double value = node.value(key, fallback).toDouble();
    return std::isfinite(value) ? std::clamp(value, 1., 20000.) : fallback;
}
QByteArray faceIdentity(const QRawFont& font) {
    // The actual fallback face matters, not the requested QFont family. Font
    // tables identify the physical face independently of its current size.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const char* table : {"head", "name", "OS/2", "cmap", "maxp"}) {
        const auto data = font.fontTable(table);
        hash.addData(QByteArray::number(data.size()));
        hash.addData(":");
        hash.addData(data);
    }
    hash.addData(font.familyName().toUtf8());
    hash.addData(font.styleName().toUtf8());
    hash.addData(QByteArray::number(font.weight()));
    hash.addData(QByteArray::number(int(font.style())));
    return hash.result();
}
bool sizeDependentVariations(const QRawFont& font) {
    const auto table = font.fontTable("fvar");
    if (table.isEmpty())
        return false;
    // OpenType fvar v1 header and axis records. Changing pixel size is safe
    // for the fixed weight/style selected by QFont, but an optical-size axis
    // may change outlines even when glyph IDs and the physical face match.
    // Unknown or malformed variation data uses the complete-layout fallback.
    if (table.size() < 16)
        return true;
    auto u16 = [&](qsizetype offset) {
        return qFromBigEndian<quint16>(table.constData() + offset);
    };
    const qsizetype offset = u16(4), count = u16(8), stride = u16(10);
    if (u16(0) != 1 || offset < 16 || stride < 20 ||
        offset > table.size() || count > (table.size() - offset) / stride)
        return true;
    for (qsizetype i = 0; i < count; ++i)
        if (QByteArrayView(table).sliced(offset + i * stride, 4) == "opsz")
            return true;
    return false;
}
bool sameIdentity(const FluxText::Layout::Glyph& a, const FluxText::Layout::Glyph& b) {
    return a.sourceIndex == b.sourceIndex && a.index == b.index && a.face == b.face &&
           a.flags == b.flags;
}
bool glyphLess(const FluxText::Layout::Glyph& a, const FluxText::Layout::Glyph& b) {
    if (a.sourceIndex != b.sourceIndex)
        return a.sourceIndex < b.sourceIndex;
    if (a.face != b.face)
        return a.face < b.face;
    if (a.index != b.index)
        return a.index < b.index;
    return a.flags.toInt() < b.flags.toInt();
}
} // namespace

namespace FluxText {
Layout::Layout(const QVariantMap& node)
    : m_text(node.value("text").toString()),
      m_extent(dimension(node, "w", 100), dimension(node, "h", 100)) {
    QFont font("sans-serif");
    font.setPixelSize(qRound(dimension(node, "fontSize", 40)));
    font.setBold(node.value("bold", false).toBool());
    m_layout.setFont(font);
    // QTextLayout expects Unicode line separators for explicit line breaks.
    // Replacement keeps UTF-16 offsets intact for correspondence. CRLF is a
    // single break; replace its CR with a zero-width format character.
    QString shaped = m_text;
    for (qsizetype i = 0; i < shaped.size(); ++i) {
        if (shaped[i] == '\r')
            shaped[i] = i + 1 < shaped.size() && shaped[i + 1] == '\n' ? QChar(0x200b)
                                                                      : QChar(0x2028);
        else if (shaped[i] == '\n' || shaped[i] == QChar(0x2029))
            shaped[i] = QChar(0x2028);
    }
    m_layout.setText(shaped);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    const QString align = node.value("align").toString();
    option.setAlignment((align == "center"  ? Qt::AlignHCenter
                         : align == "right" ? Qt::AlignRight
                                            : Qt::AlignLeft) |
                        Qt::AlignAbsolute);
    m_layout.setTextOption(option);
    m_layout.setCacheEnabled(true);
    m_layout.beginLayout();
    qreal y = 0;
    while (true) {
        auto line = m_layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(m_extent.width());
        line.setPosition({0, y});
        y += line.height();
    }
    m_layout.endLayout();
    // Retrieve whole runs, never arbitrary character slices: slicing a
    // ligature can yield duplicated, partially clipped glyphs.
    const auto runs = m_layout.glyphRuns(-1, -1, QTextLayout::RetrieveGlyphIndexes |
                                                   QTextLayout::RetrieveGlyphPositions |
                                                   QTextLayout::RetrieveStringIndexes);
    QHash<QRawFont, QByteArray> faces;
    QHash<QRawFont, QHash<quint32, bool>> visibleGlyphs;
    for (const auto& run : runs) {
        const auto font = run.rawFont();
        const auto indexes = run.glyphIndexes();
        const auto sources = run.stringIndexes();
        const auto positions = run.positions();
        if (!font.isValid() || indexes.size() != sources.size() ||
            indexes.size() != positions.size() || (run.flags() & QGlyphRun::SplitLigature)) {
            m_mappable = false;
            continue;
        }
        if (!faces.contains(font))
            faces.insert(font, faceIdentity(font));
        // Preserve Qt's complete rendering for optical-size variation,
        // color layers and bitmap strikes that cannot be scaled as outlines.
        if (font.fontTable("head").isEmpty() || sizeDependentVariations(font) || !font.fontTable("COLR").isEmpty() ||
            !font.fontTable("CBDT").isEmpty() || !font.fontTable("EBDT").isEmpty() ||
            !font.fontTable("EBLC").isEmpty() || !font.fontTable("bdat").isEmpty() ||
            !font.fontTable("sbix").isEmpty() || !font.fontTable("SVG ").isEmpty())
            m_mappable = false;
        for (qsizetype i = 0; i < indexes.size(); ++i) {
            // Qt omits trailing whitespace on wrapped lines. Font-engine
            // metrics can report nonempty bounds for an inkless space at
            // some sizes (notably variable bold fonts), so inspect the
            // outline once per glyph instead. Visible missing-glyph boxes
            // remain mapped. Non-outline fonts already use the fallback.
            auto& visible = visibleGlyphs[font];
            if (!visible.contains(indexes[i]))
                visible.insert(indexes[i], !font.pathForGlyph(indexes[i]).isEmpty());
            if (!visible.value(indexes[i]))
                continue;
            if (sources[i] < 0 || sources[i] >= m_text.size() ||
                !std::isfinite(positions[i].x()) || !std::isfinite(positions[i].y()))
                m_mappable = false;
            m_glyphs.push_back(
                {sources[i], indexes[i], positions[i], font, faces.value(font), run.flags()});
        }
    }
    std::sort(m_glyphs.begin(), m_glyphs.end(), glyphLess);
    for (size_t i = 1; i < m_glyphs.size(); ++i)
        if (sameIdentity(m_glyphs[i - 1], m_glyphs[i]))
            m_mappable = false; // Ambiguous duplicate glyph inside one cluster.
    // Keep unusually large text on the bounded, whole-layout fallback path.
    if (m_glyphs.size() > 4096)
        m_mappable = false;
}

void Layout::draw(QPainter& painter, const QColor& ink) const {
    painter.save();
    painter.setClipRect(QRectF(QPointF(), m_extent), Qt::IntersectClip);
    painter.setPen(ink);
    m_layout.draw(&painter, {});
    painter.restore();
}

Morph::Morph(const QVariantMap& from, const QVariantMap& to) : m_from(from), m_to(to) {
    const auto& a = m_from.glyphs();
    const auto& b = m_to.glyphs();
    auto alignment = [](const QVariantMap& node) {
        const auto value = node.value("align").toString();
        return value == "right" || value == "center" ? value : QStringLiteral("left");
    };
    m_sameLayout = m_from.text() == m_to.text() && m_from.extent() == m_to.extent() &&
                   qRound(dimension(from, "fontSize", 40)) ==
                       qRound(dimension(to, "fontSize", 40)) &&
                   from.value("bold", false).toBool() == to.value("bold", false).toBool() &&
                   alignment(from) == alignment(to);
    if (m_from.text() != m_to.text() || !m_from.mappable() || !m_to.mappable() ||
        from.value("bold", false) != to.value("bold", false) || a.size() != b.size())
        return;
    for (size_t i = 0; i < a.size(); ++i)
        if (!sameIdentity(a[i], b[i]))
            return;
    m_spatial = true;
    for (size_t i = 0; i < a.size(); ++i) {
        auto run = std::find_if(m_runs.begin(), m_runs.end(), [&](const Run& r) {
            return r.font == a[i].font && r.endSize == b[i].font.pixelSize() &&
                   r.flags == a[i].flags;
        });
        if (run == m_runs.end()) {
            m_runs.push_back({a[i].font, b[i].font.pixelSize(), {}, {}, {}, a[i].flags});
            run = std::prev(m_runs.end());
        }
        run->indexes.append(a[i].index);
        run->from.append(a[i].position);
        run->to.append(b[i].position);
    }
}

QSizeF Morph::extent(double progress) const {
    const double p = unit(progress);
    return m_from.extent() * (1 - p) + m_to.extent() * p;
}

std::vector<QPointF> Morph::positions(double progress) const {
    if (!m_spatial)
        return {};
    const double p = unit(progress);
    std::vector<QPointF> result;
    result.reserve(m_from.glyphs().size());
    for (size_t i = 0; i < m_from.glyphs().size(); ++i)
        result.push_back(m_from.glyphs()[i].position * (1 - p) + m_to.glyphs()[i].position * p);
    return result;
}

void Morph::draw(QPainter& painter, double progress, const QColor& ink) const {
    const double p = unit(progress);
    painter.save();
    painter.setClipRect(QRectF(QPointF(), extent(p)), Qt::IntersectClip);
    painter.setPen(ink);
    if (m_sameLayout || p == 0) {
        // Pure object motion must not dim identical fallback layouts by
        // compositing two partially transparent copies over one another.
        m_from.draw(painter, ink);
    } else if (p == 1) {
        m_to.draw(painter, ink);
    } else if (m_spatial) {
        for (const auto& run : m_runs) {
            auto font = run.font;
            font.setPixelSize(font.pixelSize() * (1 - p) + run.endSize * p);
            QGlyphRun glyphs;
            glyphs.setRawFont(font);
            glyphs.setFlags(run.flags);
            glyphs.setGlyphIndexes(run.indexes);
            QList<QPointF> positions;
            positions.reserve(run.from.size());
            for (qsizetype i = 0; i < run.from.size(); ++i)
                positions.append(run.from[i] * (1 - p) + run.to[i] * p);
            glyphs.setPositions(positions);
            painter.drawGlyphRun({}, glyphs);
        }
    } else {
        // Keep complete shaped endpoints when glyph identity is ambiguous.
        // Scaling also keeps each endpoint's own clipping aligned to the
        // current box; there is no Unicode slicing or reshaping per frame.
        const auto size = extent(p);
        auto draw = [&](const Layout& layout, double alpha) {
            painter.save();
            painter.setOpacity(painter.opacity() * alpha);
            painter.scale(size.width() / layout.extent().width(),
                          size.height() / layout.extent().height());
            layout.draw(painter, ink);
            painter.restore();
        };
        draw(m_from, 1 - p);
        draw(m_to, p);
    }
    painter.restore();
}
} // namespace FluxText
