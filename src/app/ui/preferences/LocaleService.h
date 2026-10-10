#pragma once

#include <QObject>
#include <QString>

class QQmlEngine;
class QTranslator;

namespace miacode {

// Owns the process-wide QTranslator install for ID-based catalogs
// (qtTrId / qsTrId). Language preference persistence stays on PreferenceDocument;
// this service only loads .qm files and notifies the registered QML engines and UI models.
class LocaleService final : public QObject
{
    Q_OBJECT

public:
    static LocaleService& instance();

    // Install the catalog matching PreferenceDocument::resolvedLanguageToken(). Safe to
    // call before any QML engine exists.
    void applyResolvedLanguage();
    // Release the installed catalog while the application still exists.
    void clearTranslator();

    // Persist token via PreferenceDocument, reload translators, then retranslate.
    void setLanguageToken(const QString& token);

    QString activeLanguageToken() const { return activeToken_; }

    void setQmlEngine(QQmlEngine* engine);

signals:
    void languageChanged(const QString& token);

private:
    explicit LocaleService(QObject* parent = nullptr);
    ~LocaleService() override;

    bool loadTranslatorForToken(const QString& token);

    QTranslator* translator_ = nullptr;
    QString activeToken_;
};

}  // namespace miacode
