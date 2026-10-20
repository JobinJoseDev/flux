#include "Document.hpp"
#include "Render.hpp"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>

namespace {
constexpr qint64 MaxFile = 64 * 1024 * 1024;
const QSet<QString> kinds = {"text",     "rect",  "ellipse", "arrow", "code",
                             "equation", "chart", "table",   "image"};
QVariantMap track(QJsonObject a, QJsonObject b, double delay = 0, double duration = 1) {
    return {{"from", a.toVariantMap()},
            {"to", b.toVariantMap()},
            {"delay", delay},
            {"duration", duration},
            {"crossfade", a.value("text") != b.value("text") ||
                              a.value("source") != b.value("source") ||
                              a.value("chartStyle") != b.value("chartStyle")}};
}
bool atomicWrite(const QString& path, const QJsonObject& data) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    auto bytes = QJsonDocument(data).toJson(QJsonDocument::Indented);
    if (bytes.size() > MaxFile) {
        f.cancelWriting();
        return false;
    }
    if (f.write(bytes) != bytes.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}
} // namespace
QString Document::uid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
Document::Document(QObject* parent) : QObject(parent) {
    const QString dir =
        qEnvironmentVariable("FLUX_DATA_DIR",
                             QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)) +
        "/recovery";
    QDir().mkpath(dir);
    auto files = QDir(dir).entryInfoList({"*.flux"}, QDir::Files, QDir::Time);
    if (!files.isEmpty())
        m_recoverCandidate = files.first().absoluteFilePath();
    m_recoveryPath = dir + "/" + uid() + ".flux";
    m_autosave.setSingleShot(true);
    m_autosave.setInterval(1500);
    connect(&m_autosave, &QTimer::timeout, this, &Document::writeRecovery);
    newDeck(true);
}
QJsonObject Document::slide(int i) const {
    auto ss = m_doc.value("slides").toArray();
    return i >= 0 && i < ss.size() ? ss.at(i).toObject() : QJsonObject();
}
QVariantList Document::slides() const {
    return m_doc.value("slides").toArray().toVariantList();
}
QVariantList Document::objects() const {
    return slide(m_index).value("objects").toArray().toVariantList();
}
QVariantMap Document::currentSlide() const {
    return slide(m_index).toVariantMap();
}
QString Document::title() const {
    return m_doc.value("title").toString();
}
QVariantMap Document::selected() const {
    for (const auto& v : slide(m_index).value("objects").toArray())
        if (v.toObject().value("id").toString() == m_selected)
            return v.toObject().toVariantMap();
    return {};
}
void Document::report(const QString& text) {
    m_status = text;
    emit statusChanged();
}
void Document::select(const QString& id) {
    if (m_selected == id)
        return;
    m_selected = id;
    emit selectionChanged();
}
void Document::setSlideIndex(int i) {
    int n = m_doc.value("slides").toArray().size();
    if (i < 0 || i >= n || i == m_index)
        return;
    m_index = i;
    m_selected.clear();
    emit documentChanged();
    emit selectionChanged();
}
void Document::setTitle(const QString& value) {
    auto t = value.trimmed().left(160);
    if (t.isEmpty() || t == title())
        return;
    checkpoint();
    m_doc["title"] = t;
    changed();
}
void Document::checkpoint() {
    m_undo.push_back(state());
    if (m_undo.size() > 80)
        m_undo.erase(m_undo.begin());
    m_redo.clear();
}
void Document::changed() {
    emit documentChanged();
    emit selectionChanged();
    emit historyChanged();
    m_autosave.start();
    report("Unsaved changes · recovery pending");
}
void Document::restore(const State& s) {
    m_doc = s.doc;
    m_index = s.index;
    m_selected = s.selected;
    changed();
}
void Document::undo() {
    if (m_undo.empty())
        return;
    m_redo.push_back(state());
    auto s = m_undo.back();
    m_undo.pop_back();
    restore(s);
}
void Document::redo() {
    if (m_redo.empty())
        return;
    m_undo.push_back(state());
    auto s = m_redo.back();
    m_redo.pop_back();
    restore(s);
}
void Document::replaceSlide(QJsonObject s) {
    auto arr = m_doc["slides"].toArray();
    arr[m_index] = s;
    m_doc["slides"] = arr;
}
QJsonObject Document::makeObject(const QString& kind, double x, double y, double w, double h,
                                 QString text) {
    return {{"id", uid()},
            {"kind", kind},
            {"x", x},
            {"y", y},
            {"w", w},
            {"h", h},
            {"rotation", 0},
            {"opacity", 1},
            {"fill", kind == "code" ? "#162738" : "#ffb48d"},
            {"stroke", "#8edbd3"},
            {"strokeWidth", 0},
            {"radius", 18},
            {"color", "#edf3f2"},
            {"fontSize", kind == "code"   ? 24
                         : kind == "text" ? 44
                                          : 36},
            {"bold", false},
            {"align", "left"},
            {"text", text},
            {"path", "straight"},
            {"easing", "smooth"},
            {"animation", "none"},
            {"delay", 0},
            {"duration", 0.7},
            {"chartStyle", "bar"}};
}
QJsonObject Document::makeSlide(const QString& layout) {
    QJsonArray nodes;
    QString label = "Blank slide";
    auto txt = [&](QString s, double x, double y, double w, double h, int size, bool bold = false,
                   QString c = "#edf3f2") {
        auto n = makeObject("text", x, y, w, h, s);
        n["fontSize"] = size;
        n["bold"] = bold;
        n["color"] = c;
        nodes.append(n);
    };
    if (layout == "title") {
        label = "Title";
        txt("YOUR NEXT BIG IDEA", 88, 86, 900, 48, 20, false, "#8edbd3");
        txt("Make your\nideas move.", 88, 186, 860, 290, 92, true);
        txt("A new perspective starts here.", 92, 542, 1000, 80, 28);
        auto c = makeObject("ellipse", 1000, 180, 170, 170);
        c["fill"] = "#ffb48d";
        nodes.append(c);
    } else if (layout == "two-column") {
        label = "Two columns";
        txt("Two sides of the story", 78, 58, 1100, 100, 52, true);
        txt("THE PROBLEM", 80, 208, 480, 70, 23, true, "#ffb48d");
        txt("Describe the challenge.\nKeep it clear and focused.", 80, 294, 490, 300, 32);
        txt("THE POSSIBILITY", 700, 208, 480, 70, 23, true, "#8edbd3");
        txt("Show what changes.\nMake the outcome tangible.", 700, 294, 490, 300, 32);
    } else if (layout == "technical") {
        label = "Technical explanation";
        txt("Show the idea. Then the code.", 72, 54, 1140, 90, 52, true);
        auto n = makeObject("code", 72, 198, 680, 380,
                            "// A small idea, expressed clearly\nauto transform(Point p) {\n    "
                            "return Point{p.x + 40, p.y};\n}");
        n["fontSize"] = 25;
        nodes.append(n);
        txt("WHY IT WORKS", 820, 218, 370, 50, 21, true, "#8edbd3");
        txt("A simple transformation.\nA visible result.\nA story worth explaining.", 820, 302, 370,
            280, 30);
    } else if (layout == "data") {
        label = "Data story";
        txt("Let the evidence speak", 72, 54, 1140, 100, 52, true);
        auto n = makeObject("chart", 72, 192, 730, 420,
                            "Baseline,35\nIteration 1,52\nIteration 2,71\nFinal,92");
        nodes.append(n);
        txt("2.6×", 860, 256, 320, 120, 92, true, "#ffb48d");
        txt("A clear improvement.\nExplain the result here.", 860, 402, 320, 180, 29);
    } else if (layout == "section") {
        label = "Section divider";
        txt("02 / THE NEXT CHAPTER", 90, 120, 1000, 60, 24, false, "#ffb48d");
        txt("A fresh\nperspective.", 90, 246, 1100, 310, 98, true);
    }
    return {{"id", uid()},     {"name", label},         {"background", "#101e2c"}, {"notes", ""},
            {"duration", 1.1}, {"transition", "morph"}, {"objects", nodes}};
}
void Document::newDeck(bool demo) {
    QJsonArray ss;
    if (demo) {
        auto a = makeSlide("title");
        a["name"] = "A little motion changes everything";
        a["notes"] = "Welcome to Flux. Move to the next slide to see the same objects change "
                     "position and scale. Press Space or Right to advance. Escape ends the show.";
        auto nodes = a["objects"].toArray();
        auto card = makeObject("rect", 985, 400, 205, 130);
        card["fill"] = "#2d6e73";
        nodes.prepend(card);
        a["objects"] = nodes;
        ss.append(a);
        auto b = a;
        b["id"] = uid();
        b["name"] = "Same objects, a different story";
        auto bn = b["objects"].toArray();
        for (int i = 0; i < bn.size(); ++i) {
            auto n = bn[i].toObject();
            if (n["kind"] == "ellipse") {
                n["x"] = 160;
                n["y"] = 470;
                n["w"] = 130;
                n["h"] = 130;
                n["path"] = "arc";
            } else if (n["kind"] == "rect") {
                n["x"] = 340;
                n["y"] = 470;
                n["w"] = 300;
                n["h"] = 130;
                n["rotation"] = 0;
            } else if (n["fontSize"].toInt() == 92) {
                n["text"] = "Same idea.\nNew perspective.";
                n["fontSize"] = 74;
                n["y"] = 160;
                n["w"] = 1050;
            } else if (n["fontSize"].toInt() == 28) {
                n["text"] = "Duplicate. Rearrange. Morph.";
                n["x"] = 710;
                n["y"] = 495;
                n["w"] = 480;
            }
            bn[i] = n;
        }
        b["objects"] = bn;
        b["notes"] =
            "These objects retain their identities across slides. The circle follows an arc; the "
            "card changes position and size. Select an object and open Motion to choose a path.";
        ss.append(b);
        auto c = makeSlide("technical");
        c["notes"] = "Code is editable and highlighted offline. Double-click an object to focus "
                     "its content editor.";
        ss.append(c);
        auto d = makeSlide("data");
        d["notes"] = "Chart data is one label,value pair per line. Negative values are supported. "
                     "Choose line or bar in the inspector.";
        ss.append(d);
        auto e = makeSlide("two-column");
        e["name"] = "Equations and evidence";
        auto en = e["objects"].toArray();
        en.removeLast();
        en.append(
            makeObject("equation", 690, 300, 510, 160, "\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}"));
        auto eq = en.last().toObject();
        eq["fontSize"] = 48;
        en[en.size() - 1] = eq;
        e["objects"] = en;
        ss.append(e);
    } else
        ss.append(makeSlide("title"));
    cleanupRecovery();
    m_doc = {{"format", "flux"},
             {"version", 1},
             {"title", demo ? "A little motion" : "Untitled presentation"},
             {"width", 1280},
             {"height", 720},
             {"slides", ss}};
    m_saved = m_doc;
    m_path.clear();
    m_index = 0;
    m_selected.clear();
    m_undo.clear();
    m_redo.clear();
    emit documentChanged();
    emit selectionChanged();
    emit fileChanged();
    emit historyChanged();
    report("Ready · fully offline");
}
void Document::addObject(const QString& kind) {
    if (!kinds.contains(kind) || kind == "image")
        return;
    auto s = slide(m_index);
    auto nodes = s["objects"].toArray();
    if (nodes.size() >= 250) {
        report("This slide has reached 250 objects.");
        return;
    }
    QString text;
    double w = 440, h = 160;
    if (kind == "text")
        text = "Your idea here";
    if (kind == "code") {
        text = "// Make something worth showing\nint main() {\n    return 0;\n}";
        w = 640;
        h = 300;
    }
    if (kind == "equation") {
        text = "E = mc^2";
        w = 480;
        h = 140;
    }
    if (kind == "chart") {
        text = "First,24\nSecond,42\nThird,68\nFourth,85";
        w = 640;
        h = 380;
    }
    if (kind == "table") {
        text = "Metric\tBefore\tAfter\nLatency\t120 ms\t42 ms\nMemory\t180 MB\t76 MB";
        w = 640;
        h = 250;
    }
    if (kind == "ellipse") {
        w = 180;
        h = 180;
    }
    if (kind == "arrow") {
        w = 260;
        h = 50;
    }
    checkpoint();
    auto n = makeObject(kind, 160 + nodes.size() % 8 * 20, 180 + nodes.size() % 6 * 20, w, h, text);
    nodes.append(n);
    s["objects"] = nodes;
    replaceSlide(s);
    m_selected = n["id"].toString();
    changed();
}
void Document::updateObject(const QString& id, const QVariantMap& changes) {
    auto s = slide(m_index);
    auto nodes = s["objects"].toArray();
    static const QSet<QString> allowed = {
        "x",      "y",           "w",      "h",         "rotation", "opacity", "fill",
        "stroke", "strokeWidth", "radius", "color",     "fontSize", "bold",    "align",
        "text",   "animation",   "delay",  "duration",  "path",     "easing",  "c1x",
        "c1y",    "c2x",         "c2y",    "chartStyle"};
    for (int i = 0; i < nodes.size(); ++i) {
        auto n = nodes[i].toObject();
        if (n["id"].toString() != id)
            continue;
        auto updated = n;
        for (auto it = changes.begin(); it != changes.end(); ++it)
            if (allowed.contains(it.key()))
                updated[it.key()] = QJsonValue::fromVariant(it.value());
        for (auto key : {"x", "y", "rotation", "c1x", "c1y", "c2x", "c2y"})
            if (updated.contains(key))
                updated[key] = std::clamp(updated[key].toDouble(), -20000., 20000.);
        for (auto key : {"w", "h"})
            updated[key] = std::clamp(updated[key].toDouble(), 16., 10000.);
        updated["opacity"] = std::clamp(updated["opacity"].toDouble(1), 0., 1.);
        updated["fontSize"] = std::clamp(updated["fontSize"].toDouble(32), 8., 240.);
        updated["delay"] = std::clamp(updated["delay"].toDouble(), 0., 20.);
        updated["duration"] = std::clamp(updated["duration"].toDouble(.7), .1, 10.);
        updated["text"] = updated["text"].toString().left(12000);
        for (auto key : {"fill", "stroke", "color"})
            if (!QColor(updated[key].toString()).isValid())
                updated[key] = n[key];
        if (changes.value("path").toString() == "bezier" && !updated.contains("c1x")) {
            auto prior = n;
            for (auto pv : slide(m_index - 1)["objects"].toArray())
                if (pv.toObject()["id"] == n["id"])
                    prior = pv.toObject();
            updated["c1x"] = prior["x"];
            updated["c1y"] = prior["y"].toDouble() - 160;
            updated["c2x"] = n["x"];
            updated["c2y"] = n["y"].toDouble() - 160;
        }
        if (updated == n)
            return;
        checkpoint();
        nodes[i] = updated;
        s["objects"] = nodes;
        replaceSlide(s);
        changed();
        return;
    }
}
void Document::deleteObject() {
    auto s = slide(m_index);
    auto nodes = s["objects"].toArray();
    for (int i = 0; i < nodes.size(); ++i)
        if (nodes[i].toObject()["id"].toString() == m_selected) {
            checkpoint();
            nodes.removeAt(i);
            s["objects"] = nodes;
            replaceSlide(s);
            m_selected.clear();
            changed();
            return;
        }
}
void Document::duplicateObject() {
    auto n = QJsonObject::fromVariantMap(selected());
    if (n.isEmpty())
        return;
    auto s = slide(m_index);
    auto nodes = s["objects"].toArray();
    if (nodes.size() >= 250)
        return;
    checkpoint();
    n["id"] = uid();
    n["x"] = n["x"].toDouble() + 24;
    n["y"] = n["y"].toDouble() + 24;
    nodes.append(n);
    s["objects"] = nodes;
    replaceSlide(s);
    m_selected = n["id"].toString();
    changed();
}
void Document::layerObject(int direction) {
    auto s = slide(m_index);
    auto nodes = s["objects"].toArray();
    for (int i = 0; i < nodes.size(); ++i)
        if (nodes[i].toObject()["id"].toString() == m_selected) {
            int to = qBound(0, i + direction, int(nodes.size()) - 1);
            if (to == i)
                return;
            checkpoint();
            auto n = nodes.takeAt(i);
            nodes.insert(to, n);
            s["objects"] = nodes;
            replaceSlide(s);
            changed();
            return;
        }
}
void Document::alignObject(const QString& a) {
    auto n = selected();
    if (n.isEmpty())
        return;
    QVariantMap p;
    if (a == "left")
        p["x"] = 64;
    if (a == "center")
        p["x"] = (1280 - n["w"].toDouble()) / 2;
    if (a == "right")
        p["x"] = 1216 - n["w"].toDouble();
    if (a == "middle")
        p["y"] = (720 - n["h"].toDouble()) / 2;
    updateObject(m_selected, p);
}
void Document::unlinkObject() {
    auto s = slide(m_index);
    auto nodes = s["objects"].toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        auto n = nodes[i].toObject();
        if (n["id"].toString() == m_selected) {
            checkpoint();
            n["id"] = uid();
            nodes[i] = n;
            s["objects"] = nodes;
            replaceSlide(s);
            m_selected = n["id"].toString();
            changed();
            return;
        }
    }
}
void Document::addSlide(const QString& layout) {
    auto ss = m_doc["slides"].toArray();
    if (ss.size() >= 200)
        return;
    checkpoint();
    ss.insert(++m_index, makeSlide(layout));
    m_doc["slides"] = ss;
    m_selected.clear();
    changed();
}
void Document::duplicateSlide() {
    auto ss = m_doc["slides"].toArray();
    if (ss.size() >= 200)
        return;
    checkpoint();
    auto s = slide(m_index);
    s["id"] = uid();
    s["name"] = s["name"].toString() + " · copy";
    ss.insert(++m_index, s);
    m_doc["slides"] = ss;
    changed();
    report("Slide duplicated · object identities preserved for Morph");
}
void Document::deleteSlide() {
    auto ss = m_doc["slides"].toArray();
    if (ss.size() < 2) {
        report("Keep at least one slide in the presentation.");
        return;
    }
    checkpoint();
    ss.removeAt(m_index);
    m_index = qMin(m_index, int(ss.size()) - 1);
    m_doc["slides"] = ss;
    m_selected.clear();
    changed();
}
void Document::moveSlide(int direction) {
    auto ss = m_doc["slides"].toArray();
    int target = qBound(0, m_index + direction, int(ss.size()) - 1);
    if (target == m_index)
        return;
    checkpoint();
    auto s = ss.takeAt(m_index);
    ss.insert(target, s);
    m_index = target;
    m_doc["slides"] = ss;
    changed();
}
void Document::updateSlide(const QVariantMap& changes) {
    auto s = slide(m_index), next = s;
    for (auto it = changes.begin(); it != changes.end(); ++it)
        if (it.key() == "notes" || it.key() == "name" || it.key() == "background" ||
            it.key() == "duration" || it.key() == "transition")
            next[it.key()] = QJsonValue::fromVariant(it.value());
    next["duration"] = std::clamp(next["duration"].toDouble(1.1), .1, 10.);
    next["notes"] = next["notes"].toString().left(20000);
    if (!QColor(next["background"].toString()).isValid())
        next["background"] = s["background"];
    if (next == s)
        return;
    checkpoint();
    replaceSlide(next);
    changed();
}
bool Document::validate(const QJsonObject& doc, QString* why) {
    auto fail = [&](QString s) {
        if (why)
            *why = s;
        return false;
    };
    if (doc.value("format") != "flux" || doc.value("version").toInt() != 1)
        return fail("Unsupported Flux format or version.");
    if (doc.value("width").toInt() != 1280 || doc.value("height").toInt() != 720)
        return fail("This version supports 1280 × 720 presentations.");
    auto ss = doc.value("slides").toArray();
    if (ss.isEmpty() || ss.size() > 200)
        return fail("A presentation must have 1–200 slides.");
    QSet<QString> slideIds;
    for (auto v : ss) {
        auto s = v.toObject();
        QString sid = s["id"].toString();
        if (sid.isEmpty() || slideIds.contains(sid))
            return fail("Invalid or duplicate slide identity.");
        slideIds.insert(sid);
        if (!QColor(s["background"].toString()).isValid())
            return fail("Invalid slide background.");
        if (!s["objects"].isArray() || s["objects"].toArray().size() > 250)
            return fail("Invalid object list.");
        QSet<QString> ids;
        for (auto nv : s["objects"].toArray()) {
            auto n = nv.toObject();
            QString id = n["id"].toString();
            if (id.isEmpty() || ids.contains(id))
                return fail("Object identities must be unique within a slide.");
            ids.insert(id);
            if (!kinds.contains(n["kind"].toString()))
                return fail("Unknown object kind.");
            for (auto key : {"x", "y", "w", "h", "rotation", "opacity", "fontSize", "delay",
                             "duration", "c1x", "c1y", "c2x", "c2y", "radius", "strokeWidth"})
                if (n.contains(key)) {
                    double d = n[key].toDouble(1e30);
                    if (!std::isfinite(d) || std::abs(d) > 20000)
                        return fail("Invalid object geometry or timing.");
                }
            if (n["w"].toDouble() < 1 || n["h"].toDouble() < 1)
                return fail("Object dimensions must be positive.");
            if (n["text"].toString().size() > 12000)
                return fail("Object text exceeds 12,000 characters.");
            for (auto key : {"fill", "color"})
                if (!QColor(n[key].toString()).isValid())
                    return fail("Invalid object color.");
            if (n["kind"] == "image" &&
                !n["source"].toString().startsWith("data:image/png;base64,"))
                return fail("Images must be embedded PNG assets.");
        }
    }
    return true;
}
bool Document::loadFile(const QString& path, bool recovery) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > MaxFile) {
        report("Could not open this file (64 MiB maximum).");
        return false;
    }
    QJsonParseError error;
    auto data = QJsonDocument::fromJson(f.readAll(), &error);
    QString why;
    if (error.error != QJsonParseError::NoError || !data.isObject()) {
        report("This file is not valid JSON.");
        return false;
    }
    auto doc = data.object();
    QString original;
    if (recovery) {
        original = doc.take("recoverySource").toString();
    }
    if (!validate(doc, &why)) {
        report(why);
        return false;
    }
    cleanupRecovery();
    m_doc = doc;
    m_saved = recovery ? QJsonObject() : doc;
    m_path = recovery ? original : path;
    m_index = 0;
    m_selected.clear();
    m_undo.clear();
    m_redo.clear();
    emit documentChanged();
    emit selectionChanged();
    emit fileChanged();
    emit historyChanged();
    report(recovery ? "Recovered draft · save to keep your changes"
                    : "Opened " + QFileInfo(path).fileName());
    if (recovery)
        m_autosave.start();
    return true;
}
bool Document::open(const QUrl& url) {
    if (!url.isLocalFile()) {
        report("Choose a local .flux file.");
        return false;
    }
    return loadFile(url.toLocalFile(), false);
}
bool Document::save(const QUrl& url) {
    QString path = url.isEmpty() ? m_path : url.toLocalFile();
    if (path.isEmpty()) {
        report("Choose a filename with Save As.");
        return false;
    }
    if (!path.endsWith(".flux", Qt::CaseInsensitive))
        path += ".flux";
    if (!atomicWrite(path, m_doc)) {
        report("Could not save. Check folder permissions and available space.");
        return false;
    }
    m_saved = m_doc;
    m_path = path;
    cleanupRecovery();
    emit fileChanged();
    emit documentChanged();
    report("Saved " + QFileInfo(path).fileName());
    return true;
}
void Document::writeRecovery() {
    if (!dirty())
        return;
    auto data = m_doc;
    data["recoverySource"] = m_path;
    if (atomicWrite(m_recoveryPath, data))
        report("Recovery saved locally · save project to keep changes");
    else
        report("Recovery could not be written · save your project now");
}
void Document::cleanupRecovery() {
    m_autosave.stop();
    if (!m_recoveryPath.isEmpty())
        QFile::remove(m_recoveryPath);
}
bool Document::recover() {
    if (m_recoverCandidate.isEmpty())
        return false;
    QString p = m_recoverCandidate;
    if (!loadFile(p, true))
        return false;
    QFile::remove(p);
    m_recoverCandidate.clear();
    emit recoveryChanged();
    return true;
}
void Document::discardRecovery() {
    if (!m_recoverCandidate.isEmpty())
        QFile::remove(m_recoverCandidate);
    m_recoverCandidate.clear();
    emit recoveryChanged();
}
bool Document::importImage(const QUrl& url) {
    if (!url.isLocalFile())
        return false;
    QFileInfo info(url.toLocalFile());
    if (info.size() > 20 * 1024 * 1024) {
        report("Choose an image smaller than 20 MiB.");
        return false;
    }
    QImageReader reader(info.absoluteFilePath());
    QSize size = reader.size();
    if (!size.isValid()) {
        report("This image could not be read.");
        return false;
    }
    if (size.width() > 4096 || size.height() > 4096) {
        size.scale(4096, 4096, Qt::KeepAspectRatio);
        reader.setScaledSize(size);
    }
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        report("Image decode failed.");
        return false;
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    if (bytes.size() > 16 * 1024 * 1024) {
        report("Image is too large after decoding.");
        return false;
    }
    auto s = slide(m_index);
    auto ns = s["objects"].toArray();
    if (ns.size() >= 250)
        return false;
    QSizeF display = img.size();
    display.scale(700, 440, Qt::KeepAspectRatio);
    auto n = makeObject("image", (1280 - display.width()) / 2, (720 - display.height()) / 2,
                        display.width(), display.height());
    n["source"] = "data:image/png;base64," + QString::fromLatin1(bytes.toBase64());
    checkpoint();
    ns.append(n);
    s["objects"] = ns;
    replaceSlide(s);
    m_selected = n["id"].toString();
    changed();
    return true;
}
bool Document::exportPdf(const QUrl& url) {
    if (!url.isLocalFile())
        return false;
    QString path = url.toLocalFile();
    if (!path.endsWith(".pdf", Qt::CaseInsensitive))
        path += ".pdf";
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        report("Could not create PDF.");
        return false;
    }
    bool success = true;
    {
        QPdfWriter pdf(&out);
        pdf.setTitle(title());
        pdf.setCreator("Flux");
        pdf.setResolution(144);
        pdf.setPageSize(QPageSize(QSizeF(320, 180), QPageSize::Millimeter));
        pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter painter;
        if (!painter.begin(&pdf)) {
            out.cancelWriting();
            report("PDF renderer could not start.");
            return false;
        }
        auto ss = m_doc["slides"].toArray();
        for (int i = 0; i < ss.size(); ++i) {
            if (i && !pdf.newPage()) {
                success = false;
                break;
            }
            painter.save();
            painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            painter.scale(pdf.width() / 1280.0, pdf.height() / 720.0);
            Render::slide(painter, ss[i].toObject());
            painter.restore();
        }
        painter.end();
    }
    if (!success || !out.commit()) {
        out.cancelWriting();
        report("PDF export failed.");
        return false;
    }
    report("Exported PDF · transitions remain in the .flux project");
    return true;
}
QVariantList Document::morphTracks(int target) const {
    QVariantList result;
    auto ss = m_doc["slides"].toArray();
    if (target < 0 || target >= ss.size())
        return result;
    auto to = slide(target)["objects"].toArray();
    auto from = target > 0 ? slide(target - 1)["objects"].toArray() : QJsonArray();
    QHash<QString, QJsonObject> prior;
    for (auto v : from) {
        auto n = v.toObject();
        prior.insert(n["id"].toString(), n);
    }
    QSet<QString> used;
    bool fade = slide(target)["transition"].toString() == "fade";
    for (auto v : to) {
        auto b = v.toObject();
        QString id = b["id"].toString();
        auto a = prior.value(id);
        if (a.isEmpty() || a["kind"] != b["kind"] || fade) {
            a = b;
            a["opacity"] = 0;
        } else
            used.insert(id);
        result.append(track(a, b));
    }
    for (auto v : from) {
        auto a = v.toObject();
        if (used.contains(a["id"].toString()))
            continue;
        auto b = a;
        b["opacity"] = 0;
        result.append(track(a, b));
    }
    return result;
}
double Document::buildDuration(int index) const {
    double total = .1;
    for (auto v : slide(index)["objects"].toArray()) {
        auto n = v.toObject();
        if (n["animation"].toString("none") != "none")
            total = qMax(total, n["delay"].toDouble() + n["duration"].toDouble(.7));
    }
    return total;
}
QVariantList Document::entranceTracks(int index) const {
    QVariantList result;
    for (auto v : slide(index)["objects"].toArray()) {
        auto b = v.toObject(), a = b;
        QString kind = b["animation"].toString("none");
        if (kind != "none")
            a["opacity"] = 0;
        if (kind == "rise")
            a["y"] = b["y"].toDouble() + 90;
        if (kind == "zoom") {
            a["w"] = b["w"].toDouble() * .7;
            a["h"] = b["h"].toDouble() * .7;
            a["x"] = b["x"].toDouble() + b["w"].toDouble() * .15;
            a["y"] = b["y"].toDouble() + b["h"].toDouble() * .15;
        }
        if (kind == "path") {
            a["x"] = b["x"].toDouble() - 280;
            a["y"] = b["y"].toDouble() + 100;
        }
        result.append(track(a, b, kind == "none" ? 0 : b["delay"].toDouble(),
                            kind == "none" ? .001 : b["duration"].toDouble(.7)));
    }
    return result;
}
