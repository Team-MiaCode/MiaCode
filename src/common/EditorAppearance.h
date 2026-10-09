#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QList>
#include <QString>
#include <QtMath>

// Editor appearance extracted from runtime/Shared.cpp. The existing namespace
// keeps desktop callers on the same implementation without a runtime dependency.
namespace miacode::runtime::shared {
inline constexpr int kEditorTextFontSizeMin = 8;
inline constexpr int kEditorTextFontSizeMax = 28;
inline constexpr double kEditorLineSpacingFactorDefault = 1.0;
inline const QList<double> kEditorLineSpacingFactorOptions{
    1.0, 1.5, 2.0, 3.0, 5.0,
};

inline double normalizeEditorLineSpacingFactor(double factor)
{
    if (kEditorLineSpacingFactorOptions.isEmpty()) {
        return kEditorLineSpacingFactorDefault;
    }
    double best = kEditorLineSpacingFactorOptions.first();
    double bestDiff = qAbs(best - factor);
    for (double candidate : kEditorLineSpacingFactorOptions) {
        const double diff = qAbs(candidate - factor);
        if (diff < bestDiff) {
            best = candidate;
            bestDiff = diff;
        }
    }
    return best;
}

inline QString editorLineSpacingFactorLabel(double factor)
{
    if (qFuzzyCompare(factor + 1.0, 1.0)) {
        return QStringLiteral("0x");
    }
    const QString text = QString::number(factor, 'f', qFuzzyCompare(factor, qRound(factor)) ? 0 : 1);
    return text + QStringLiteral("x");
}

inline QFont editorFont(int pointSize = -1)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    static const QString bundledEditorFontFamily = []() -> QString {
        const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/maple_mono_cn.ttf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        return families.isEmpty() ? QString() : families.first();
    }();
    if (!bundledEditorFontFamily.isEmpty()) {
        font.setFamily(bundledEditorFontFamily);
    }
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    if (pointSize > 0) {
        font.setPointSize(pointSize);
    }
    font.setStyleStrategy(QFont::PreferAntialias);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

inline int blockSpacingPixelsForPointSize(int pointSize, double spacingFactor)
{
    const int baseSpacing = qBound(1, qRound(static_cast<double>(pointSize) * 0.18), 6);
    return qMax(0, qRound(static_cast<double>(baseSpacing) * qMax(0.0, spacingFactor)));
}

} // namespace miacode::runtime::shared
