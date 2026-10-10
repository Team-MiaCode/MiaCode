#pragma once

#include "app/services/PreferenceDocument.h"
#include "app/services/PreferenceVersionGuard.h"
#include "export/cover_export/CoverCompositionState.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <functional>
#include <utility>

namespace miacode::app_preferences {
struct CoverUserPreset {
    QString name;
    QJsonObject composition;
};

// Application-owned persistence; tests inject an isolated in-memory store.
class CoverExportPreferences {
public:
    using Reader = std::function<QJsonObject()>;
    using Writer = std::function<bool(const QJsonObject&)>;
    CoverExportPreferences(Reader reader, Writer writer)
        : reader_(std::move(reader)), writer_(std::move(writer)) {}
    QJsonObject loadPreferences();
    bool savePreferences(const QJsonObject& root);
    QStringList loadRecentFiles();
    void pushRecentFile(const QString& path);
    void clearRecentFiles();
    QList<CoverUserPreset> loadUserPresets();
    void saveUserPreset(const QString& name, const QJsonObject& composition);
    void removeUserPreset(const QString& name);
    void renameUserPreset(const QString& oldName, const QString& newName);
private:
    static QString normalizedPresetName(QString name) { return name.trimmed(); }
    bool writable(const QJsonObject& section) const
    {
        return allowVersionWrite(section, QStringLiteral("version"), cover_export::CoverCompositionState::kCurrentVersion, QStringLiteral("cover-export"));
    }
    Reader reader_;
    Writer writer_;
};

inline QJsonObject CoverExportPreferences::loadPreferences()
{
    return reader_();
}

inline bool CoverExportPreferences::savePreferences(const QJsonObject& preferences)
{
    const QJsonObject existing =
        reader_();
    if (!writable(existing) || !writable(preferences)) return false;
    QJsonObject merged = existing;
    for (const char* ownedKey : {"kind", "version", "size", "background", "card", "layout", "output", "outputFile", "chartFrame"}) {
        merged.remove(QString::fromLatin1(ownedKey));
    }
    for (auto it = preferences.constBegin(); it != preferences.constEnd(); ++it) {
        merged.insert(it.key(), it.value());
    }
    return writer_(merged);
}

inline QStringList CoverExportPreferences::loadRecentFiles()
{
    const QJsonObject cover = loadPreferences();
    QStringList out;
    const QJsonArray arr = cover.value(QStringLiteral("recentFiles")).toArray();
    for (const QJsonValue& value : arr) {
        const QString path = value.toString();
        if (!path.isEmpty()) {
            out.append(path);
        }
    }
    return out;
}

inline void CoverExportPreferences::pushRecentFile(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QJsonObject cover = reader_();
    if (!writable(cover)) return;

    QStringList list;
    list.append(trimmed);
    const QJsonArray prior = cover.value(QStringLiteral("recentFiles")).toArray();
    for (const QJsonValue& value : prior) {
        const QString p = value.toString();
        if (!p.isEmpty() && p != trimmed && list.size() < 8) {
            list.append(p);
        }
    }
    QJsonArray arr;
    for (const QString& p : list) {
        arr.append(p);
    }
    cover.insert(QStringLiteral("recentFiles"), arr);
    writer_(cover);
}

inline void CoverExportPreferences::clearRecentFiles()
{
    QJsonObject cover = reader_();
    if (!writable(cover)) return;
    cover.insert(QStringLiteral("recentFiles"), QJsonArray());
    writer_(cover);
}

inline QList<CoverUserPreset> CoverExportPreferences::loadUserPresets()
{
    const QJsonObject cover = loadPreferences();
    QList<CoverUserPreset> out;
    const QJsonArray arr = cover.value(QStringLiteral("presets")).toArray();
    for (const QJsonValue& value : arr) {
        const QJsonObject obj = value.toObject();
        const QString name = normalizedPresetName(obj.value(QStringLiteral("name")).toString());
        const QJsonObject composition = obj.value(QStringLiteral("composition")).toObject();
        if (!name.isEmpty() && !composition.isEmpty()) {
            out.append(CoverUserPreset{name, composition});
        }
    }
    return out;
}

inline void CoverExportPreferences::saveUserPreset(const QString& name, const QJsonObject& composition)
{
    const QString trimmed = normalizedPresetName(name);
    if (!writable(composition)) return;
    if (trimmed.isEmpty() || composition.isEmpty()) {
        return;
    }

    QJsonObject cover = reader_();
    if (!writable(cover)) return;

    const QJsonArray prior = cover.value(QStringLiteral("presets")).toArray();
    for (const QJsonValue& value : prior) {
        const QJsonObject existing = value.toObject();
        if (normalizedPresetName(existing.value(QStringLiteral("name")).toString()) == trimmed
            && (!allowVersionWrite(existing, QStringLiteral("version"), 1, QStringLiteral("cover-preset wrapper"))
                || !writable(existing.value(QStringLiteral("composition")).toObject()))) {
            return;
        }
    }
    QJsonArray arr;
    QJsonObject saved;
    saved.insert(QStringLiteral("name"), trimmed);
    saved.insert(QStringLiteral("version"), 1);
    saved.insert(QStringLiteral("composition"), composition);
    arr.append(saved);

    for (const QJsonValue& value : prior) {
        const QJsonObject obj = value.toObject();
        const QString existingName = normalizedPresetName(obj.value(QStringLiteral("name")).toString());
        if (!existingName.isEmpty() && existingName != trimmed) {
            arr.append(obj);
        }
    }

    cover.insert(QStringLiteral("presets"), arr);
    writer_(cover);
}

inline void CoverExportPreferences::removeUserPreset(const QString& name)
{
    const QString trimmed = normalizedPresetName(name);
    if (trimmed.isEmpty()) {
        return;
    }

    QJsonObject cover = reader_();
    if (!writable(cover)) return;
    QJsonArray arr;
    const QJsonArray prior = cover.value(QStringLiteral("presets")).toArray();
    for (const QJsonValue& value : prior) {
        const QJsonObject obj = value.toObject();
        if (normalizedPresetName(obj.value(QStringLiteral("name")).toString()) != trimmed) {
            arr.append(obj);
        }
    }
    cover.insert(QStringLiteral("presets"), arr);
    writer_(cover);
}

inline void CoverExportPreferences::renameUserPreset(const QString& oldName, const QString& newName)
{
    const QString oldTrimmed = normalizedPresetName(oldName);
    const QString newTrimmed = normalizedPresetName(newName);
    if (oldTrimmed.isEmpty() || newTrimmed.isEmpty()) {
        return;
    }

    QJsonObject cover = reader_();
    if (!writable(cover)) return;
    QJsonArray arr;
    const QJsonArray prior = cover.value(QStringLiteral("presets")).toArray();
    for (const QJsonValue& value : prior) {
        QJsonObject obj = value.toObject();
        const QString existingName = normalizedPresetName(obj.value(QStringLiteral("name")).toString());
        if (existingName == oldTrimmed) {
            obj.insert(QStringLiteral("name"), newTrimmed);
            arr.append(obj);
        } else if (existingName != newTrimmed) {
            arr.append(obj);
        }
    }
    cover.insert(QStringLiteral("presets"), arr);
    writer_(cover);
}


inline CoverExportPreferences& coverExportPreferences()
{
    static CoverExportPreferences preferences(
        [] { return PreferenceDocument::loadPreferencesObject().value(QStringLiteral("app")).toObject()
                        .value(QStringLiteral("cover_export")).toObject(); },
        [](const QJsonObject& section) {
            QJsonObject root = PreferenceDocument::loadPreferencesObject();
            QJsonObject app = root.value(QStringLiteral("app")).toObject();
            app.insert(QStringLiteral("cover_export"), section);
            root.insert(QStringLiteral("app"), app);
            return PreferenceDocument::savePreferencesObject(root);
        });
    return preferences;
}

} // namespace miacode::app_preferences
