#pragma once

#include "app/services/ChartWorkspace.h"
#include <QJsonObject>
#include <QTimer>
#include <QMap>
#include <QVariantList>

namespace miacode::android {

// Owns platform persistence, never a second writable copy of the chart model.
// An I/O request has one captured save point; edits may continue while it runs.
class AndroidDocumentSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString chartText READ chartText WRITE setChartText NOTIFY chartTextChanged)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString sourceUri READ sourceUri NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY changed)
    Q_PROPERTY(int activeDifficulty READ activeDifficulty NOTIFY changed)
    Q_PROPERTY(quint64 documentGeneration READ documentGeneration NOTIFY changed)
    Q_PROPERTY(QVariantList difficulties READ difficulties NOTIFY changed)
    Q_PROPERTY(QVariantList assets READ assets NOTIFY changed)
    Q_PROPERTY(QString editorFont READ editorFont NOTIFY changed)
    Q_PROPERTY(bool backgroundExportAllowed READ backgroundExportAllowed WRITE setBackgroundExportAllowed NOTIFY changed)
    Q_PROPERTY(int currentDifficultyId READ activeDifficulty NOTIFY documentStateChanged)
    Q_PROPERTY(quint64 documentOpenGeneration READ documentGeneration NOTIFY documentStateChanged)
    Q_PROPERTY(quint64 documentRevision READ documentRevision NOTIFY documentStateChanged)
    Q_PROPERTY(quint64 validationRevision READ documentRevision NOTIFY documentStateChanged)
    Q_PROPERTY(quint64 bookmarkGeneration READ documentRevision NOTIFY documentStateChanged)
    Q_PROPERTY(bool validationPending READ validationPending CONSTANT)
    Q_PROPERTY(QVariantList syntaxIssues READ syntaxIssues NOTIFY documentStateChanged)
    Q_PROPERTY(int syntaxErrorCount READ syntaxErrorCount NOTIFY documentStateChanged)
    Q_PROPERTY(int syntaxWarningCount READ syntaxWarningCount NOTIFY documentStateChanged)
    Q_PROPERTY(QVariantList availableDifficulties READ availableDifficulties NOTIFY changed)
    Q_PROPERTY(QStringList dirtyEditorKeys READ dirtyEditorKeys NOTIFY changed)
    Q_PROPERTY(QString currentFileName READ currentFileName NOTIFY changed)
    Q_PROPERTY(QString currentFilePath READ currentFilePath NOTIFY changed)
    Q_PROPERTY(QString currentDifficultyLevel READ currentDifficultyLevel WRITE setCurrentDifficultyLevel NOTIFY changed)
    Q_PROPERTY(QString currentDifficultyLabel READ currentDifficultyLabel NOTIFY changed)
    Q_PROPERTY(QString currentDifficultyDesigner READ currentDifficultyDesigner WRITE setCurrentDifficultyDesigner NOTIFY changed)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentStateChanged)
    Q_PROPERTY(QString metadataTitle READ title WRITE setTitle NOTIFY metadataChanged)
    Q_PROPERTY(QString metadataArtist READ metadataArtist WRITE setMetadataArtist NOTIFY metadataChanged)
    Q_PROPERTY(QString metadataFirst READ metadataFirst WRITE setMetadataFirst NOTIFY metadataChanged)
    Q_PROPERTY(QString metadataDesigner READ metadataDesigner WRITE setMetadataDesigner NOTIFY metadataChanged)
    Q_PROPERTY(QString metadataClockCount READ metadataClockCount WRITE setMetadataClockCount NOTIFY metadataChanged)
    Q_PROPERTY(QString metadataExtraText READ metadataExtraText WRITE setMetadataExtraText NOTIFY metadataChanged)
    Q_PROPERTY(bool metadataHasVideo READ metadataHasVideo NOTIFY mediaAssetsChanged)
    Q_PROPERTY(bool metadataNeedsAttention READ metadataNeedsAttention NOTIFY metadataChanged)
    Q_PROPERTY(QString metadataAttentionText READ metadataAttentionText NOTIFY metadataChanged)
    Q_PROPERTY(QString currentDifficultyOffset READ metadataFirst WRITE setMetadataFirst NOTIFY metadataChanged)
    Q_PROPERTY(QVariantList designerSlots READ designerSlots NOTIFY metadataChanged)
    Q_PROPERTY(bool unifiedDesignerEnabled READ unifiedDesignerEnabled NOTIFY metadataChanged)
public:
    QString currentDifficultyLabel() const;
    explicit AndroidDocumentSession(QString storageRoot, QObject* parent = nullptr);
    QString chartText() const;
    QString title() const { return workspace_.document().title; }
    QString sourceUri() const { return sourceUri_; }
    bool dirty() const { return workspace_.snapshot().dirty; }
    bool busy() const { return !pendingKind_.isEmpty(); }
    QString status() const { return status_; }
    bool recoveryAvailable() const { return recoveryAvailable_; }
    int activeDifficulty() const { return workspace_.snapshot().activeDifficultyId; }
    quint64 documentGeneration() const { return workspace_.snapshot().documentOpenGeneration; }
    QVariantList difficulties() const;
    QVariantList assets() const { return assets_; }
    QString editorFont() const { return editorFont_; }
    bool backgroundExportAllowed() const { return backgroundExportAllowed_; }
    Q_INVOKABLE void newProject();
    Q_INVOKABLE void openProject();
    Q_INVOKABLE void openProjectFolder();
    Q_INVOKABLE void save(bool saveAs = false);
    Q_INVOKABLE void setChartText(const QString& text);
    Q_INVOKABLE void setTitle(const QString& text);
    Q_INVOKABLE void selectDifficulty(int id);
    Q_INVOKABLE void addDifficulty(int id);
    Q_INVOKABLE void importAsset(const QString& kind);
    Q_INVOKABLE void share();
    Q_INVOKABLE bool recover();
    Q_INVOKABLE bool flushRecovery();
    Q_INVOKABLE void runMediaProbe();
    Q_INVOKABLE void exportProbeResults();
    void setBackgroundExportAllowed(bool allowed);
    void completeIo(const QJsonObject& result);
    const ChartWorkspace& workspace() const { return workspace_; }
    ChartWorkspace& workspace() { return workspace_; }
#ifndef Q_OS_ANDROID
    bool loadHostFixture(const QString& path);
#endif
    quint64 documentRevision() const { return workspace_.snapshot().revision; }
    bool validationPending() const { return false; }
    QVariantList syntaxIssues() const;
    int syntaxErrorCount() const;
    int syntaxWarningCount() const;
    QVariantList availableDifficulties() const;
    QStringList dirtyEditorKeys() const;
    QString currentFileName() const { return QStringLiteral("maidata.txt"); }
    QString currentFilePath() const { return workspace_.snapshot().filePath; }
    QString previewAssetPath(const QString& kind) const;
    bool hasDocument() const { return workspace_.snapshot().hasDocument; }
    QString metadataArtist() const { return workspace_.document().artist; }
    QString metadataFirst() const { return workspace_.document().first; }
    QString metadataDesigner() const { return workspace_.document().designer; }
    QString metadataClockCount() const;
    QString metadataExtraText() const;
    bool metadataHasVideo() const { return !previewAssetPath("video").isEmpty(); }
    bool metadataNeedsAttention() const { return !metadataAttentionText().isEmpty(); }
    QString metadataAttentionText() const;
    QVariantList designerSlots() const;
    bool unifiedDesignerEnabled() const { return workspace_.unifiedDesignerEnabled(); }
    void setMetadataArtist(const QString& value);
    void setMetadataFirst(const QString& value);
    void setMetadataDesigner(const QString& value);
    void setMetadataClockCount(const QString& value);
    void setMetadataExtraText(const QString& value);
    Q_INVOKABLE void applyDesignerSlots(const QVariantList& slotValues, bool unified, const QString& name);
    Q_INVOKABLE void readTitleFromAudioFile();
    Q_INVOKABLE void readArtistFromAudioFile();
    Q_INVOKABLE void extractCoverFromAudioFile();
    Q_INVOKABLE void importChartBackgroundImage() { importAsset("image"); }
    Q_INVOKABLE void importChartBackgroundVideo() { importAsset("video"); }
    Q_INVOKABLE void removeChartPv();
    QString currentDifficultyLevel() const;
    QString currentDifficultyDesigner() const;
    void setCurrentDifficultyLevel(const QString& value);
    void setCurrentDifficultyDesigner(const QString& value);
    Q_INVOKABLE void removeDifficulty(int id) { workspace_.removeDifficulty(id); }
    Q_INVOKABLE void requestCloseDifficulty(int id) { emit difficultyCloseRequested(id); emit difficultyCloseAccepted(id); }
    Q_INVOKABLE QVariantList bookmarksForDifficulty(int id) const;
    Q_INVOKABLE void navigateToBookmark(int id, int line) { selectDifficulty(id); emit bookmarkNavigationRequested(id, line); }
    Q_INVOKABLE int chartPosition(int line, int column) const;
    Q_INVOKABLE void logEditorDocumentState(const QString&, int, quint64, int) const {}
    Q_INVOKABLE QVariantList chartTransformMenu() const;
    Q_INVOKABLE QString chartTransformMoreLabel() const;
    Q_INVOKABLE QVariantList normalizeGridOptions() const;
    Q_INVOKABLE QVariantList normalizeSectionOptions() const;
    Q_INVOKABLE QVariantList normalizeSyntaxOptions() const;
    Q_INVOKABLE QVariantList recentDocuments() const { return {}; }
    Q_INVOKABLE QVariantList backupDocuments() const { return {}; }
    Q_INVOKABLE QVariantMap transformChartSelection(const QString& text, int anchor, int position, const QString& opId) const;
    Q_INVOKABLE QVariantMap selectionBeatSummary(const QString& text, int anchor, int position) const;
    Q_INVOKABLE QVariantMap normalizeChartSelection(const QString& text, int anchor, int position, const QVariantMap& options) const;
signals:
    void chartTextChanged();
    void documentStateChanged();
    void metadataChanged();
    void editingFinishedRequested();
    void documentReplaced();
    void mediaAssetsChanged();
    void difficultyCloseRequested(int id);
    void difficultyCloseAccepted(int id);
    void bookmarkNavigationRequested(int id, int line);
    void changed();
    void ioRequested(const QString& kind, const QString& uri, const QString& payload);
private:
    bool beginIo(const QString& kind, const QString& uri = {}, const QString& payload = {});
    bool writeState();
    bool readState(QJsonObject* state);
    void loadFonts();
    ChartWorkspace workspace_;
    QString storageRoot_;
    QString sourceUri_;
    QString savedSource_;
    QString pendingKind_;
    QString pendingSaveSource_;
    QString status_;
    QString editorFont_;
    QMap<QString, int> fontIds_;
    QVariantList assets_;
    bool recoveryAvailable_ = false;
    bool backgroundExportAllowed_ = false;
    bool videoDisabled_ = false;
    QTimer recoveryTimer_;
    quint64 publishedGeneration_ = 0;
};
}
