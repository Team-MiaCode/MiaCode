#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QStringList>

namespace miacode::editor_typography {

inline QFont resolveFont(const QString& selectedFamily, int pointSize = -1)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    static const QString bundledFamily = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/maple_mono_cn.ttf"));
        return QFontDatabase::applicationFontFamilies(id).value(0);
    }();
    if (!bundledFamily.isEmpty()) font.setFamily(bundledFamily);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    const QString family = selectedFamily.trimmed();
    if (!family.isEmpty() && QFontDatabase::families().contains(family, Qt::CaseInsensitive)) {
        font.setFamily(family);
        const bool fixed = QFontDatabase::isFixedPitch(family);
        font.setStyleHint(fixed ? QFont::Monospace : QFont::AnyStyle);
        font.setFixedPitch(fixed);
    }
    if (pointSize > 0) font.setPointSize(pointSize);
    font.setStyleStrategy(QFont::PreferAntialias);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

} // namespace miacode::editor_typography
