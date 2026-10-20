#include "app/ui/preferences/LocaleService.h"

#include "app/services/PreferenceDocument.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QTranslator>

namespace miacode {

namespace {

QString qmResourcePathForToken(const QString& token)
{
    if (token.startsWith(QStringLiteral("zh"))) {
        return QStringLiteral(":/i18n/zh_CN.qm");
    }
    if (token.startsWith(QStringLiteral("ja"))) {
        return QStringLiteral(":/i18n/ja_JP.qm");
    }
    return QStringLiteral(":/i18n/en_US.qm");
}

}  // namespace

LocaleService& LocaleService::instance()
{
    static LocaleService service;
    return service;
}

LocaleService::LocaleService(QObject* parent)
    : QObject(parent)
{
}

LocaleService::~LocaleService()
{
    clearTranslator();
}

void LocaleService::clearTranslator()
{
    if (translator_ != nullptr) {
        QCoreApplication::removeTranslator(translator_);
        delete translator_;
        translator_ = nullptr;
    }
}

void LocaleService::setQmlEngine(QQmlEngine* engine)
{
    connect(this, &LocaleService::languageChanged,
            engine, &QQmlEngine::retranslate, Qt::UniqueConnection);
}

void LocaleService::applyResolvedLanguage()
{
    loadTranslatorForToken(PreferenceDocument::resolvedLanguageToken());
}

void LocaleService::setLanguageToken(const QString& token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty()) {
        return;
    }
    PreferenceDocument::setPreferredLanguageToken(normalized);
    const QString resolved = PreferenceDocument::resolvedLanguageToken();
    if (!loadTranslatorForToken(resolved)) {
        return;
    }
    emit languageChanged(resolved);
}

bool LocaleService::loadTranslatorForToken(const QString& token)
{
    const QString path = qmResourcePathForToken(token);
    auto* next = new QTranslator(this);
    if (!next->load(path)) {
        delete next;
        return false;
    }

    clearTranslator();

    if (!QCoreApplication::installTranslator(next)) {
        delete next;
        return false;
    }

    translator_ = next;
    activeToken_ = token.startsWith(QStringLiteral("zh"))
        ? QStringLiteral("zh")
        : (token.startsWith(QStringLiteral("ja")) ? QStringLiteral("ja")
                                                  : QStringLiteral("en"));
    return true;
}

}  // namespace miacode
