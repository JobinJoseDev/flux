#pragma once
#include <QJsonObject>
#include <QPainter>
#include <QQuickPaintedItem>

namespace Render {
void content(QPainter& p, const QJsonObject& node);
void slide(QPainter& p, const QJsonObject& slide);
} // namespace Render

class ContentItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantMap node READ node WRITE setNode NOTIFY nodeChanged)
  public:
    explicit ContentItem(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {
        setAntialiasing(true);
    }
    QVariantMap node() const { return m_node.toVariantMap(); }
    void setNode(const QVariantMap& v) {
        m_node = QJsonObject::fromVariantMap(v);
        emit nodeChanged();
        update();
    }
    void paint(QPainter* p) override {
        if (m_node.isEmpty())
            return;
        p->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        p->scale(width() / qMax(1.0, m_node.value("w").toDouble(100)),
                 height() / qMax(1.0, m_node.value("h").toDouble(100)));
        Render::content(*p, m_node);
    }
  signals:
    void nodeChanged();

  private:
    QJsonObject m_node;
};
