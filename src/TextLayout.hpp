#pragma once
#include <QColor>
#include <QGlyphRun>
#include <QPainter>
#include <QSizeF>
#include <QTextLayout>
#include <QVariantMap>
#include <memory>
#include <vector>

namespace FluxText {
// A complete Qt-shaped layout. Construct, use and destroy it on one thread:
// the QRawFonts retained by glyph runs are local to that thread.
class Layout {
  public:
    struct Glyph {
        qsizetype sourceIndex{};
        quint32 index{};
        QPointF position;
        QRawFont font;
        QByteArray face;
        QGlyphRun::GlyphRunFlags flags;
    };
    explicit Layout(const QVariantMap& node);
    const QString& text() const { return m_text; }
    QSizeF extent() const { return m_extent; }
    int lineCount() const { return m_layout.lineCount(); }
    const std::vector<Glyph>& glyphs() const { return m_glyphs; }
    bool mappable() const { return m_mappable; }
    void draw(QPainter& painter, const QColor& ink) const;

  private:
    QString m_text;
    QSizeF m_extent;
    QTextLayout m_layout;
    std::vector<Glyph> m_glyphs;
    bool m_mappable = true;
};

// Endpoint layouts and correspondence are immutable during playback. Seeking
// or reversing uses the same plan and changes only the scalar progress.
class Morph {
  public:
    Morph(const QVariantMap& from, const QVariantMap& to);
    bool spatial() const { return m_spatial; }
    const Layout& from() const { return m_from; }
    const Layout& to() const { return m_to; }
    QSizeF extent(double progress) const;
    std::vector<QPointF> positions(double progress) const;
    void draw(QPainter& painter, double progress, const QColor& ink) const;

  private:
    struct Run {
        QRawFont font;
        qreal endSize{};
        QList<quint32> indexes;
        QList<QPointF> from, to;
        QGlyphRun::GlyphRunFlags flags;
    };
    Layout m_from, m_to;
    std::vector<Run> m_runs;
    bool m_spatial = false;
    bool m_sameLayout = false;
};
} // namespace FluxText
