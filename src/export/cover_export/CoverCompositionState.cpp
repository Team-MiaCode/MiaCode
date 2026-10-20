#include "export/cover_export/CoverCompositionState.h"


#include <QDir>
#include <QJsonArray>

namespace miacode::cover_export {
namespace {

constexpr char kCompositionKind[] = "miacode-cover-composition";

QJsonObject migrateLayoutV1ToV2(const QJsonObject& root)
{
    QJsonObject layout = root.value(QStringLiteral("layout")).toObject();
    QJsonArray layers = layout.value(QStringLiteral("layers")).toArray();
    const QJsonObject legacyFrame = root.value(QStringLiteral("chartFrame")).toObject();
    if (layers.isEmpty() && root.contains(QStringLiteral("chartFrame"))) {
        QJsonObject card;
        card.insert(QStringLiteral("key"), QStringLiteral("card"));
        card.insert(QStringLiteral("kind"), QStringLiteral("card"));
        layers.append(card);

        QJsonObject layer;
        layer.insert(QStringLiteral("key"), QStringLiteral("chartFrame"));
        layer.insert(QStringLiteral("kind"), QStringLiteral("chartFrame"));
        layer.insert(QStringLiteral("visible"), true);
        layers.append(layer);
    }

    for (QJsonValueRef value : layers) {
        QJsonObject layer = value.toObject();
        const QString key = layer.value(QStringLiteral("key")).toString();
        if (!layer.contains(QStringLiteral("kind"))) {
            layer.insert(QStringLiteral("kind"), key == QStringLiteral("card")
                ? QStringLiteral("card")
                : QStringLiteral("chartFrame"));
        }
        if (key == QStringLiteral("chartFrame")) {
            layer.insert(QStringLiteral("frameBgEnabled"),
                         legacyFrame.value(QStringLiteral("innerBackground")).toBool(true));
            layer.insert(QStringLiteral("frameBgBrightness"),
                         legacyFrame.value(QStringLiteral("innerBrightness")).toDouble(0.8));
        }
        if (!layer.contains(QStringLiteral("opacity"))) {
            layer.insert(QStringLiteral("opacity"), 1.0);
        }
        value = layer;
    }

    layout.insert(QStringLiteral("layers"), layers);
    return layout;
}

QJsonObject migrateLayoutV2ToV3(const QJsonObject& root)
{
    QJsonObject layout = root.value(QStringLiteral("layout")).toObject();
    QJsonArray layers = layout.value(QStringLiteral("layers")).toArray();
    for (QJsonValueRef value : layers) {
        QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("kind")).toString() == QStringLiteral("chartFrame")
            || layer.value(QStringLiteral("key")).toString().startsWith(QStringLiteral("chartFrame"))) {
            if (!layer.contains(QStringLiteral("frameBgMode"))) {
                const bool enabled = layer.value(QStringLiteral("frameBgEnabled")).toBool(true);
                layer.insert(QStringLiteral("frameBgMode"),
                             enabled ? QStringLiteral("image") : QStringLiteral("transparent"));
                if (!enabled && !layer.contains(QStringLiteral("frameBgTransparency"))) {
                    layer.insert(QStringLiteral("frameBgTransparency"), 1.0);
                }
            }
            if (!layer.contains(QStringLiteral("frameBgTransparency"))) {
                layer.insert(QStringLiteral("frameBgTransparency"), 0.5);
            }
        }
        value = layer;
    }
    layout.insert(QStringLiteral("layers"), layers);
    return layout;
}

}  // namespace

QJsonObject CoverCompositionState::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("kind"), QString::fromLatin1(kCompositionKind));
    root.insert(QStringLiteral("version"), kCurrentVersion);

    QJsonObject sizeObject;
    sizeObject.insert(QStringLiteral("w"), size.width());
    sizeObject.insert(QStringLiteral("h"), size.height());
    root.insert(QStringLiteral("size"), sizeObject);
    root.insert(QStringLiteral("background"), background);
    root.insert(QStringLiteral("card"), card);
    root.insert(QStringLiteral("layout"), layout);
    if (!outputFile.isEmpty()) {
        root.insert(QStringLiteral("outputFile"), outputFile);
    }
    return root;
}

bool CoverCompositionState::supportsVersion(const QJsonObject& root)
{
    if (!root.contains(QStringLiteral("version"))) return true;
    const QJsonValue value = root.value(QStringLiteral("version"));
    const double version = value.toDouble(-1);
    return value.isDouble() && version >= 1 && version <= kCurrentVersion && version == int(version);
}

bool CoverCompositionState::fromJson(const QJsonObject& root, CoverCompositionState* out, QString* errorMessage)
{
    if (root.value(QStringLiteral("kind")).toString() != QString::fromLatin1(kCompositionKind)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("not a MiaCode cover composition");
        }
        return false;
    }

    if (!supportsVersion(root)) {
        if (errorMessage != nullptr) *errorMessage = QStringLiteral("unsupported cover composition version");
        return false;
    }
    const QJsonObject migrated = migrateToCurrent(root);
    if (out != nullptr) {
        const QJsonObject sizeObject = migrated.value(QStringLiteral("size")).toObject();
        out->size = QSize(sizeObject.value(QStringLiteral("w")).toInt(),
                          sizeObject.value(QStringLiteral("h")).toInt());
        out->background = migrated.value(QStringLiteral("background")).toObject();
        out->card = migrated.value(QStringLiteral("card")).toObject();
        out->layout = migrated.value(QStringLiteral("layout")).toObject();
        out->outputFile = migrated.value(QStringLiteral("outputFile")).toString();
        if (out->outputFile.isEmpty()) {
            // Saved before the field named a file: "output" was the folder, and
            // the cover was always written into it as card.jpg.
            const QString legacyFolder = migrated.value(QStringLiteral("output")).toString();
            if (!legacyFolder.isEmpty()) {
                out->outputFile = QDir(legacyFolder).filePath(QString::fromLatin1(kDefaultOutputFile));
            }
        }
    }
    return true;
}

QJsonObject CoverCompositionState::migrateToCurrent(const QJsonObject& root)
{
    if (!supportsVersion(root)) return root;
    QJsonObject migrated = root;
    const int version = migrated.value(QStringLiteral("version")).toInt(1);
    if (version < 2) {
        migrated.insert(QStringLiteral("layout"), migrateLayoutV1ToV2(migrated));
        migrated.remove(QStringLiteral("chartFrame"));
    }
    if (version < 3) {
        migrated.insert(QStringLiteral("layout"), migrateLayoutV2ToV3(migrated));
    }
    migrated.insert(QStringLiteral("version"), kCurrentVersion);
    return migrated;
}

}  // namespace miacode::cover_export
