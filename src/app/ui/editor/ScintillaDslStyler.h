#pragma once
#include <QVariantMap>
#include <QMap>
#include "app/ui/editor/ScintillaDocumentAdapter.h"

namespace miacode::ui {
class ScintillaDslStyler
{
public:
    ScintillaDslStyler(ScintillaQuick_item& editor, ScintillaDocumentAdapter& document);
    void setAppearance(const QFont& font, const QVariantMap& palette);
    void style();
    void reset();
    void invalidate(int line, int linesAdded);
    void diagnostics(const QVariantList& rows, bool isMuri);
    QVariantList bookmarks() const;
    void follow(bool active, int start, int end);
    bool following() const { return followActive_; }
private:
    struct LineState { QByteArray text; QByteArray stack; QString bookmark; bool valid = false; QByteArray inputStack; };
    QVector<LineState> lines_;
    QMap<int, QString> bookmarks_;
    int dirtyLine_ = 0;
    int dirtyThrough_ = 0;
    QVariantList validation_, muri_;
    bool followActive_ = false;
    bool validationDirty_ = true, muriDirty_ = true;
    int followStart_ = 0, followEnd_ = 0;
    void fill(int indicator, int start, int end);
    ScintillaQuick_item& editor_;
    ScintillaDocumentAdapter& document_;
};
}
