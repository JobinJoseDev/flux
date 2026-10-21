#pragma once
#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantMap>

// Shared static and moving text use exactly the same Qt layout and clipping.
class TextItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantMap node READ node WRITE setNode NOTIFY nodeChanged)
    Q_PROPERTY(QColor inkColor READ inkColor WRITE setInkColor NOTIFY inkColorChanged)
  public:
    explicit TextItem(QQuickItem* parent = nullptr);
    ~TextItem() override;
    QVariantMap node() const { return m_node; }
    void setNode(const QVariantMap& value);
    QColor inkColor() const { return m_ink; }
    void setInkColor(const QColor& value);
    void paint(QPainter* painter) override;
  signals:
    void nodeChanged();
    void inkColorChanged();
  protected:
    void geometryChange(const QRectF& now, const QRectF& before) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;
  private:
    QVariantMap m_node;
    QColor m_ink = QColor("#edf3f2");
};

class TextMorphItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantMap from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(QVariantMap to READ to WRITE setTo NOTIFY toChanged)
    Q_PROPERTY(double progress READ progress WRITE setProgress NOTIFY progressChanged)
    Q_PROPERTY(QColor inkColor READ inkColor WRITE setInkColor NOTIFY inkColorChanged)
  public:
    explicit TextMorphItem(QQuickItem* parent = nullptr);
    ~TextMorphItem() override;
    QVariantMap from() const { return m_from; }
    QVariantMap to() const { return m_to; }
    void setFrom(const QVariantMap& value);
    void setTo(const QVariantMap& value);
    double progress() const { return m_progress; }
    void setProgress(double value);
    QColor inkColor() const { return m_ink; }
    void setInkColor(const QColor& value);
    void paint(QPainter* painter) override;
  signals:
    void fromChanged();
    void toChanged();
    void progressChanged();
    void inkColorChanged();
  protected:
    void geometryChange(const QRectF& now, const QRectF& before) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;
  private:
    QVariantMap m_from, m_to;
    QColor m_ink = QColor("#edf3f2");
    double m_progress{};
};
