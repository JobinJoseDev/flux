#include "Render.hpp"
#include "TextLayout.hpp"
#include <QBuffer>
#include <QFontMetricsF>
#include <QImage>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTextLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {
QColor color(const QJsonObject& n, const char* key, const char* fallback) {
    return QColor(n.value(key).toString(fallback));
}
QFont font(double size, bool mono = false, bool bold = false) {
    QFont f(mono ? "monospace" : "sans-serif");
    f.setPixelSize(qMax(8, qRound(size)));
    f.setBold(bold);
    return f;
}
struct Box {
    qreal w{}, h{}, base{};
    std::function<void(QPainter&, qreal, qreal)> draw;
};
Box glyph(QString s, double size) {
    QFont f("serif");
    f.setPixelSize(qMax(8, qRound(size)));
    QFontMetricsF fm(f);
    qreal w = fm.horizontalAdvance(s), h = fm.height(), b = fm.ascent();
    return {w, h, b, [s, f, b](QPainter& p, qreal x, qreal y) {
                p.setFont(f);
                p.drawText(QPointF(x, y + b), s);
            }};
}
Box sequence(std::vector<Box> items) {
    qreal w = 0, b = 0, d = 0;
    for (const auto& a : items) {
        w += a.w;
        b = qMax(b, a.base);
        d = qMax(d, a.h - a.base);
    }
    return {w, b + d, b, [items, b](QPainter& p, qreal x, qreal y) {
                qreal dx = 0;
                for (const auto& a : items) {
                    a.draw(p, x + dx, y + b - a.base);
                    dx += a.w;
                }
            }};
}
// Deliberately bounded TeX subset. No shell, TeX process, network, or executable macros.
class Math {
    QString text;
    qsizetype pos = 0;
    int depth = 0;
    Box group(double size) {
        if (pos < text.size() && text[pos] == '{') {
            ++pos;
            ++depth;
            auto b = parse(size);
            --depth;
            if (pos < text.size() && text[pos] == '}')
                ++pos;
            return b;
        }
        return atom(size);
    }
    Box atom(double size) {
        if (pos >= text.size())
            return glyph("", size);
        auto ch = text[pos++];
        if (ch == '{') {
            --pos;
            return group(size);
        }
        if (ch != '\\')
            return glyph(QString(ch), size);
        QString cmd;
        while (pos < text.size() && text[pos].isLetter())
            cmd += text[pos++];
        if (cmd.isEmpty() && pos < text.size())
            return glyph(QString(text[pos++]), size);
        if (cmd == "frac") {
            Box a = group(size * .78), b = group(size * .78);
            qreal w = qMax(a.w, b.w) + 12, h = a.h + b.h + 10, base = a.h + 6 + size * .22;
            return {w, h, base, [a, b, w](QPainter& p, qreal x, qreal y) {
                        a.draw(p, x + (w - a.w) / 2, y);
                        p.drawLine(QPointF(x + 2, y + a.h + 4), QPointF(x + w - 2, y + a.h + 4));
                        b.draw(p, x + (w - b.w) / 2, y + a.h + 10);
                    }};
        }
        if (cmd == "sqrt") {
            Box a = group(size), r = glyph(QString::fromUtf8("√"), size * 1.15);
            qreal h = qMax(r.h, a.h + 5), base = qMax(r.base, a.base + 5);
            return {r.w + a.w + 4, h, base, [a, r, base](QPainter& p, qreal x, qreal y) {
                        r.draw(p, x, y + base - r.base);
                        a.draw(p, x + r.w, y + base - a.base);
                        p.drawLine(QPointF(x + r.w - 2, y + 1), QPointF(x + r.w + a.w + 3, y + 1));
                    }};
        }
        if (cmd == "text" || cmd == "mathrm" || cmd == "mathbf")
            return group(size);
        static const QHash<QString, QString> symbols = {
            {"alpha", "α"},   {"beta", "β"},  {"gamma", "γ"},  {"delta", "δ"},  {"theta", "θ"},
            {"lambda", "λ"},  {"mu", "μ"},    {"pi", "π"},     {"sigma", "σ"},  {"phi", "φ"},
            {"omega", "ω"},   {"Delta", "Δ"}, {"Sigma", "Σ"},  {"sum", "∑"},    {"prod", "∏"},
            {"int", "∫"},     {"infty", "∞"}, {"cdot", "·"},   {"times", "×"},  {"pm", "±"},
            {"leq", "≤"},     {"geq", "≥"},   {"neq", "≠"},    {"to", "→"},     {"in", "∈"},
            {"partial", "∂"}, {"nabla", "∇"}, {"approx", "≈"}, {"forall", "∀"}, {"exists", "∃"}};
        return glyph(symbols.value(cmd, "\\" + cmd), size);
    }

  public:
    explicit Math(QString value) : text(value.left(2048)) {}
    Box parse(double size) {
        std::vector<Box> items;
        while (pos < text.size() && text[pos] != '}' && depth < 24) {
            auto a = atom(size);
            while (pos < text.size() && (text[pos] == '^' || text[pos] == '_')) {
                bool sup = text[pos++] == '^';
                auto s = group(size * .62);
                auto old = a;
                qreal raise = sup ? s.h * .65 : 0, base = old.base + raise,
                      h = qMax(old.h + raise, sup ? s.h : old.base + s.h * .85);
                a = {old.w + s.w, h, base, [old, s, sup, raise](QPainter& p, qreal x, qreal y) {
                         old.draw(p, x, y + raise);
                         s.draw(p, x + old.w, y + (sup ? 0 : old.base - s.base * .2));
                     }};
            }
            items.push_back(a);
        }
        return sequence(std::move(items));
    }
};
void code(QPainter& p, const QJsonObject& n, QRectF r) {
    p.setPen(Qt::NoPen);
    p.setBrush(color(n, "fill", "#142131"));
    p.drawRoundedRect(r, 14, 14);
    const double size = n.value("fontSize").toDouble(24);
    QFont f = font(size, true);
    QFontMetricsF fm(f);
    auto lines = n.value("text").toString().split('\n');
    const double lineHeight = fm.height() * 1.25;
    QRegularExpression rx(QStringLiteral(
        "(//"
        "[^\\n]*|#[^\\n]*|\\\"[^\\\"]*\\\"|'[^']*'|\\b(?:int|double|float|auto|const|class|struct|"
        "return|if|else|for|while|void|bool|def|import|from|fn|let|mut|pub|use|print|true|false|"
        "None|nullptr)\\b|\\b[0-9]+(?:\\.[0-9]+)?\\b)"));
    p.save();
    p.setClipRect(r.adjusted(12, 12, -12, -12));
    for (int i = 0; i < lines.size(); ++i) {
        double y = 18 + i * lineHeight;
        if (y > r.height())
            break;
        p.setFont(font(size * .78, true));
        p.setPen(QColor("#60748a"));
        p.drawText(QRectF(10, y, 36, lineHeight), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(i + 1));
        QTextLayout layout(lines[i], f);
        QList<QTextLayout::FormatRange> ranges;
        auto matches = rx.globalMatch(lines[i]);
        while (matches.hasNext()) {
            auto m = matches.next();
            QTextCharFormat fmt;
            auto t = m.captured();
            QColor tokenColor("#abafff");
            if (t.startsWith("//") || t.startsWith('#'))
                tokenColor = QColor("#79968d");
            else if (t.startsWith('"') || t.startsWith(QChar(39)))
                tokenColor = QColor("#b6d78e");
            else if (t[0].isDigit())
                tokenColor = QColor("#f3bf81");
            fmt.setForeground(tokenColor);
            ranges.push_back({int(m.capturedStart()), int(m.capturedLength()), fmt});
        }
        layout.setFormats(ranges);
        layout.beginLayout();
        auto line = layout.createLine();
        line.setLineWidth(qMax(1.0, r.width() - 76));
        layout.endLayout();
        p.setPen(color(n, "color", "#e0e8f0"));
        layout.draw(&p, {64, y});
    }
    p.restore();
}
void chart(QPainter& p, const QJsonObject& n, QRectF r) {
    QStringList names;
    QList<double> values;
    for (const auto& line : n.value("text").toString().split('\n', Qt::SkipEmptyParts)) {
        int comma = line.lastIndexOf(',');
        bool ok = false;
        double v = line.mid(comma + 1).trimmed().toDouble(&ok);
        if (comma > 0 && ok && std::isfinite(v)) {
            names << line.left(comma).trimmed();
            values << v;
            if (values.size() == 32)
                break;
        }
    }
    if (values.isEmpty())
        return;
    double lo = qMin(0.0, *std::min_element(values.begin(), values.end())),
           hi = qMax(1.0, *std::max_element(values.begin(), values.end()));
    QRectF plot = r.adjusted(56, 24, -24, -50);
    double zero = plot.bottom() + lo / (hi - lo) * plot.height();
    p.setFont(font(17));
    p.setPen(QPen(QColor("#344759"), 1));
    for (int i = 0; i <= 4; ++i) {
        double y = plot.top() + i * plot.height() / 4;
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(color(n, "color", "#cad5df"));
        p.drawText(QRectF(0, y - 12, 46, 24), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(hi - i * (hi - lo) / 4, 'g', 3));
        p.setPen(QPen(QColor("#344759"), 1));
    }
    double slot = plot.width() / values.size();
    QPolygonF points;
    for (int i = 0; i < values.size(); ++i) {
        double y = plot.bottom() - (values[i] - lo) / (hi - lo) * plot.height();
        double x = plot.left() + slot * (i + .5);
        points << QPointF(x, y);
        p.setPen(Qt::NoPen);
        p.setBrush(color(n, "fill", "#ffb086"));
        if (n.value("chartStyle").toString() != "line")
            p.drawRoundedRect(
                QRectF(x - slot * .3, qMin(y, zero), slot * .6, qMax(1.0, std::abs(y - zero))), 4,
                4);
        p.setPen(color(n, "color", "#cad5df"));
        p.drawText(QRectF(x - slot / 2, plot.bottom() + 10, slot, 32),
                   Qt::AlignHCenter | Qt::AlignTop, names[i]);
    }
    if (n.value("chartStyle").toString() == "line") {
        p.setPen(QPen(color(n, "fill", "#ffb086"), 4));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(points);
        p.setBrush(color(n, "fill", "#ffb086"));
        for (auto pt : points)
            p.drawEllipse(pt, 5, 5);
    }
}
void table(QPainter& p, const QJsonObject& n, QRectF r) {
    QList<QStringList> rows;
    int cols = 1;
    for (const auto& line : n.value("text").toString().split('\n')) {
        auto cells = line.split('\t');
        cols = qMax(cols, int(cells.size()));
        rows << cells;
        if (rows.size() == 40)
            break;
    }
    cols = qMin(cols, 16);
    double h = r.height() / qMax(1, rows.size()), w = r.width() / cols;
    double size = n.value("fontSize").toDouble(24);
    for (int y = 0; y < rows.size(); ++y)
        for (int x = 0; x < cols; ++x) {
            QRectF cell(x * w, y * h, w, h);
            p.setPen(QPen(QColor("#405164"), 1));
            p.setBrush(y == 0 ? color(n, "fill", "#285252")
                              : QColor(y % 2 ? "#162536" : "#1d2e40"));
            p.drawRect(cell);
            p.setFont(font(size, false, y == 0));
            p.setPen(color(n, "color", "#edf3f4"));
            p.drawText(cell.adjusted(12, 6, -10, -6),
                       Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, rows[y].value(x));
        }
}
} // namespace

void Render::content(QPainter& p, const QJsonObject& n) {
    QRectF r(0, 0, n.value("w").toDouble(100), n.value("h").toDouble(100));
    QString kind = n.value("kind").toString();
    p.save();
    p.setClipRect(r.adjusted(-2, -2, 2, 2));
    p.setPen(color(n, "color", "#edf3f4"));
    if (kind == "code")
        code(p, n, r);
    else if (kind == "chart")
        chart(p, n, r);
    else if (kind == "table")
        table(p, n, r);
    else if (kind == "equation") {
        Math parser(n.value("text").toString());
        auto box = parser.parse(n.value("fontSize").toDouble(42));
        if (box.w > 0 && box.h > 0) {
            double s = qMin(1.0, qMin(r.width() / box.w, r.height() / box.h));
            p.translate((r.width() - box.w * s) / 2, (r.height() - box.h * s) / 2);
            p.scale(s, s);
            box.draw(p, 0, 0);
        }
    } else if (kind == "text") {
        FluxText::Layout(n.toVariantMap()).draw(p, color(n, "color", "#edf3f2"));
    } else if (kind == "image") {
        QString src = n.value("source").toString();
        int comma = src.indexOf(',');
        QImage image;
        image.loadFromData(QByteArray::fromBase64(src.mid(comma + 1).toLatin1()));
        if (!image.isNull()) {
            QSizeF s = image.size();
            s.scale(r.size(), Qt::KeepAspectRatio);
            p.drawImage(QRectF((r.width() - s.width()) / 2, (r.height() - s.height()) / 2,
                               s.width(), s.height()),
                        image);
        }
    } else if (kind == "arrow") {
        p.setPen(QPen(color(n, "fill", "#ffb086"), qMax(2., n.value("strokeWidth").toDouble(5)),
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(4, r.height() / 2), QPointF(r.width() - 8, r.height() / 2));
        p.drawLine(QPointF(r.width() - 8, r.height() / 2),
                   QPointF(r.width() - 25, r.height() / 2 - 15));
        p.drawLine(QPointF(r.width() - 8, r.height() / 2),
                   QPointF(r.width() - 25, r.height() / 2 + 15));
    } else {
        p.setPen(n.value("strokeWidth").toDouble() > 0
                     ? QPen(color(n, "stroke", "#ffb086"), n.value("strokeWidth").toDouble())
                     : QPen(Qt::NoPen));
        p.setBrush(color(n, "fill", "#2a5661"));
        if (kind == "ellipse")
            p.drawEllipse(r);
        else
            p.drawRoundedRect(r, n.value("radius").toDouble(18), n.value("radius").toDouble(18));
    }
    p.restore();
}
void Render::slide(QPainter& p, const QJsonObject& s) {
    p.fillRect(QRectF(0, 0, 1280, 720), QColor(s.value("background").toString("#101e2c")));
    for (const auto& value : s.value("objects").toArray()) {
        auto n = value.toObject();
        p.save();
        double w = n.value("w").toDouble(), h = n.value("h").toDouble();
        p.setOpacity(n.value("opacity").toDouble(1));
        p.translate(n.value("x").toDouble() + w / 2, n.value("y").toDouble() + h / 2);
        p.rotate(n.value("rotation").toDouble());
        p.translate(-w / 2, -h / 2);
        content(p, n);
        p.restore();
    }
}
