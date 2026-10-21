#pragma once
#include "core/motion.hpp"
#include <QColor>
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QVariantMap>

class MotionPose : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap from READ from WRITE setFrom NOTIFY inputChanged)
    Q_PROPERTY(QVariantMap to READ to WRITE setTo NOTIFY inputChanged)
    Q_PROPERTY(double progress READ progress WRITE setProgress NOTIFY changed)
    Q_PROPERTY(double easedProgress READ easedProgress NOTIFY changed)
    Q_PROPERTY(QPointF position READ position NOTIFY changed)
    Q_PROPERTY(QSizeF extent READ extent NOTIFY changed)
    Q_PROPERTY(double angle READ angle NOTIFY changed)
    Q_PROPERTY(double alpha READ alpha NOTIFY changed)
    Q_PROPERTY(QColor fill READ fill NOTIFY changed)
    Q_PROPERTY(QColor ink READ ink NOTIFY changed)
  public:
    explicit MotionPose(QObject* parent = nullptr) : QObject(parent) {}
    QVariantMap from() const { return m_from; }
    QVariantMap to() const { return m_to; }
    void setFrom(const QVariantMap& v) {
        m_from = v;
        m_a = decode(v);
        emit inputChanged();
        update();
    }
    void setTo(const QVariantMap& v) {
        m_to = v;
        m_b = decode(v);
        emit inputChanged();
        update();
    }
    double progress() const { return m_progress; }
    double easedProgress() const { return m_easedProgress; }
    void setProgress(double v) {
        m_progress = flux::unit(v);
        update();
    }
    QPointF position() const { return {m_pose.position.x, m_pose.position.y}; }
    QSizeF extent() const { return {m_pose.width, m_pose.height}; }
    double angle() const { return m_pose.rotation; }
    double alpha() const { return m_pose.opacity; }
    QColor fill() const { return encode(m_pose.fill); }
    QColor ink() const { return encode(m_pose.ink); }
  signals:
    void inputChanged();
    void changed();

  private:
    QVariantMap m_from, m_to;
    flux::Pose m_a, m_b, m_pose;
    double m_progress{};
    double m_easedProgress{};
    static QColor encode(flux::Color c) { return QColor::fromRgbF(c.r, c.g, c.b, c.a); }
    static flux::Color color(const QVariant& v) {
        QColor c(v.toString());
        if (!c.isValid())
            c = Qt::transparent;
        return {c.redF(), c.greenF(), c.blueF(), c.alphaF()};
    }
    static flux::Pose decode(const QVariantMap& v) {
        return {{v.value("x").toDouble(), v.value("y").toDouble()},
                v.value("w", 100).toDouble(),
                v.value("h", 100).toDouble(),
                v.value("rotation", 0).toDouble(),
                v.value("opacity", 1).toDouble(),
                color(v.value("fill", "#172535")),
                color(v.value("color", "#edf3f4"))};
    }
    void update() {
        auto path = flux::Path::Straight;
        const auto name = m_to.value("path").toString();
        if (name == "arc")
            path = flux::Path::Arc;
        if (name == "bezier")
            path = flux::Path::Bezier;
        auto e = flux::Ease::Smooth;
        if (m_to.value("easing") == "linear")
            e = flux::Ease::Linear;
        if (m_to.value("easing") == "out")
            e = flux::Ease::CubicOut;
        m_easedProgress = flux::ease(m_progress, e);
        m_pose = flux::interpolate(m_a, m_b, m_easedProgress, path,
                                   {m_to.value("c1x", m_a.position.x).toDouble(),
                                    m_to.value("c1y", m_a.position.y - 160).toDouble()},
                                   {m_to.value("c2x", m_b.position.x).toDouble(),
                                    m_to.value("c2y", m_b.position.y - 160).toDouble()});
        emit changed();
    }
};
