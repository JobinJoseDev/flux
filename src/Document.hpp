#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <optional>
#include <vector>

class Document final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList slides READ slides NOTIFY documentChanged)
    Q_PROPERTY(QVariantList objects READ objects NOTIFY documentChanged)
    Q_PROPERTY(QVariantMap currentSlide READ currentSlide NOTIFY documentChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectionBounds READ selectionBounds NOTIFY selectionChanged)
    Q_PROPERTY(bool canGroup READ canGroup NOTIFY selectionChanged)
    Q_PROPERTY(bool canUngroup READ canUngroup NOTIFY selectionChanged)
    Q_PROPERTY(bool groupedSelection READ groupedSelection NOTIFY selectionChanged)
    Q_PROPERTY(int slideIndex READ slideIndex WRITE setSlideIndex NOTIFY documentChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY documentChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY documentChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY recoveryChanged)
  public:
    explicit Document(QObject* parent = nullptr);
    QVariantList slides() const;
    QVariantList objects() const;
    QVariantMap currentSlide() const;
    QVariantMap selected() const;
    QString selectedId() const { return m_selected.isEmpty() ? QString() : m_selected.last(); }
    QStringList selectedIds() const { return m_selected; }
    int selectionCount() const { return m_selected.size(); }
    QVariantMap selectionBounds() const;
    bool canGroup() const;
    bool canUngroup() const;
    bool groupedSelection() const;
    int slideIndex() const { return m_index; }
    void setSlideIndex(int i);
    QString title() const;
    void setTitle(const QString& title);
    QString filePath() const { return m_path; }
    bool dirty() const { return m_doc != m_saved; }
    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    bool canPaste() const;
    QString status() const { return m_status; }
    bool recoveryAvailable() const { return !m_recoverCandidate.isEmpty(); }
    Q_INVOKABLE void select(const QString& id);
    Q_INVOKABLE void selectMany(const QStringList& ids);
    Q_INVOKABLE void toggleSelection(const QString& id);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectRectangle(double x, double y, double w, double h,
                                     const QStringList& base = {});
    Q_INVOKABLE void translateSelection(double dx, double dy);
    Q_INVOKABLE void groupSelection();
    Q_INVOKABLE void ungroupSelection();
    Q_INVOKABLE double constrainSelectionScale(double factor, double anchorX, double anchorY) const;
    Q_INVOKABLE void scaleSelection(double factor, double anchorX, double anchorY);
    Q_INVOKABLE void addObject(const QString& kind);
    Q_INVOKABLE void addObjectAt(const QString& kind, double x, double y);
    Q_INVOKABLE void updateObject(const QString& id, const QVariantMap& changes);
    Q_INVOKABLE void deleteObject();
    Q_INVOKABLE void duplicateObject();
    Q_INVOKABLE bool copyObject();
    Q_INVOKABLE void cutObject();
    Q_INVOKABLE bool pasteObject();
    Q_INVOKABLE bool pasteObjectAt(double x, double y);
    Q_INVOKABLE void layerObject(int direction);
    Q_INVOKABLE void alignObject(const QString& alignment);
    Q_INVOKABLE void addSlide(const QString& layout = "blank");
    Q_INVOKABLE void duplicateSlide();
    Q_INVOKABLE void deleteSlide();
    Q_INVOKABLE void moveSlide(int direction);
    Q_INVOKABLE void updateSlide(const QVariantMap& changes);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void newDeck(bool demo = false);
    Q_INVOKABLE bool open(const QUrl& url);
    Q_INVOKABLE bool save(const QUrl& url = QUrl());
    Q_INVOKABLE bool exportPdf(const QUrl& url);
    Q_INVOKABLE bool importImage(const QUrl& url);
    Q_INVOKABLE QVariantList morphTracks(int target) const;
    Q_INVOKABLE QVariantList entranceTracks(int index) const;
    Q_INVOKABLE double buildDuration(int index) const;
    Q_INVOKABLE void unlinkObject();
    Q_INVOKABLE bool recover();
    Q_INVOKABLE void discardRecovery();
    Q_INVOKABLE void cleanupRecovery();
    Q_INVOKABLE void report(const QString& text);
    QJsonObject json() const { return m_doc; }
    static bool validate(const QJsonObject& doc, QString* why = nullptr);
  signals:
    void documentChanged();
    void selectionChanged();
    void fileChanged();
    void historyChanged();
    void statusChanged();
    void recoveryChanged();
    void clipboardChanged();

  private:
    struct State {
        QJsonObject doc;
        int index{};
        QStringList selected;
    };
    QJsonObject m_doc, m_saved;
    int m_index{};
    int m_pasteCount{};
    QStringList m_selected;
    QString m_path, m_status = "Ready";
    std::vector<State> m_undo, m_redo;
    QTimer m_autosave;
    QString m_recoveryPath, m_recoverCandidate;
    State state() const { return {m_doc, m_index, m_selected}; }
    void restore(const State& s);
    void checkpoint();
    void changed();
    void replaceSlide(QJsonObject slide);
    bool insertObject(QJsonObject node);
    bool insertObjects(QJsonArray nodes);
    QJsonArray selectedObjects() const;
    bool pasteObjectImpl(std::optional<QPointF> position);
    QJsonObject slide(int i) const;
    void writeRecovery();
    bool loadFile(const QString& path, bool recovery);
    static QJsonObject makeSlide(const QString& layout);
    static QJsonObject makeObject(const QString& kind, double x = 100, double y = 130,
                                  double w = 400, double h = 200, QString text = {});
    static QString uid();
};
