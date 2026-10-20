#include "app/ui/editor/ScintillaDslStyler.h"
#include <QColor>
#include "editor/BookmarkCommentSyntax.h"
#include <QGuiApplication>
#include <QScreen>
#include <QQuickWindow>

namespace miacode::ui {
namespace {
constexpr int errorIndicator = 8;
constexpr int warningIndicator = 9;
constexpr int muriIndicator = 10;
constexpr int followIndicator = 11;
constexpr int bookmarkMarker = 1;
int color(const QVariantMap& palette, const char* key) { return scintillaquick::rgb_from_color(palette.value(QLatin1String(key)).value<QColor>()); }
}
ScintillaDslStyler::ScintillaDslStyler(ScintillaQuick_item& editor, ScintillaDocumentAdapter& document)
    : editor_(editor), document_(document)
{
    editor_.send(SCI_SETILEXER, 0, 0);
    editor_.set_auto_line_number_margin(0, 12);
    editor_.send(SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);
    editor_.send(SCI_SETMARGINMASKN, 1, (1 << bookmarkMarker));
    editor_.send(SCI_SETMARGINWIDTHN, 1, 6);
    editor_.send(SCI_SETMARGINLEFT, 0, 6);
    editor_.send(SCI_SETMARGINRIGHT, 0, 12);
    editor_.send(SCI_SETMARGINSENSITIVEN, 1, 1);
    editor_.send(SCI_MARKERDEFINE, bookmarkMarker, SC_MARK_LEFTRECT);
    for (int indicator : {errorIndicator, warningIndicator, muriIndicator})
        editor_.send(SCI_INDICSETSTYLE, indicator, INDIC_SQUIGGLE);
    editor_.send(SCI_INDICSETSTYLE, followIndicator, INDIC_FULLBOX);
    editor_.send(SCI_INDICSETUNDER, followIndicator, 1);
    editor_.send(SCI_INDICSETOUTLINEALPHA, followIndicator, 0);
    editor_.send(SCI_SETSELECTIONLAYER, SC_LAYER_UNDER_TEXT);
    editor_.send(SCI_SETCARETLINELAYER, SC_LAYER_UNDER_TEXT);
    editor_.send(SCI_SETCARETLINEVISIBLEALWAYS, 1);
    editor_.send(SCI_SETCARETWIDTH, 2);
    editor_.send(SCI_SETWRAPMODE, SC_WRAP_WORD);
}
void ScintillaDslStyler::setAppearance(const QFont& font, const QVariantMap& palette)
{
    const QByteArray family = font.family().toUtf8();
    const QScreen* screen = editor_.window() ? editor_.window()->screen() : QGuiApplication::primaryScreen();
    const qreal pointSize = font.pointSizeF() > 0 ? font.pointSizeF()
        : font.pixelSize() * 72.0 / screen->logicalDotsPerInchY();
    editor_.sends(SCI_STYLESETFONT, STYLE_DEFAULT, family.constData());
    editor_.send(SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT, qRound(pointSize * SC_FONT_SIZE_MULTIPLIER));
    editor_.send(SCI_STYLESETWEIGHT, STYLE_DEFAULT, int(font.weight()));
    editor_.send(SCI_STYLESETITALIC, STYLE_DEFAULT, font.italic());
    editor_.send(SCI_STYLESETUNDERLINE, STYLE_DEFAULT, font.underline());
    editor_.send(SCI_STYLESETFORE, STYLE_DEFAULT, color(palette, "text"));
    editor_.send(SCI_STYLESETBACK, STYLE_DEFAULT, color(palette, "background"));
    editor_.send(SCI_STYLECLEARALL);
    editor_.send(SCI_STYLESETFORE, 1, color(palette, "keyword"));
    editor_.send(SCI_STYLESETFORE, 2, color(palette, "duration"));
    editor_.send(SCI_STYLESETFORE, 3, color(palette, "comment"));
    editor_.send(SCI_STYLESETFORE, STYLE_LINENUMBER, color(palette, "lineNumber"));
    editor_.send(SCI_STYLESETBACK, STYLE_LINENUMBER, color(palette, "background"));
    editor_.send(SCI_SETFOLDMARGINCOLOUR, 1, color(palette, "background"));
    editor_.send(SCI_SETFOLDMARGINHICOLOUR, 1, color(palette, "background"));
    editor_.send(SCI_SETCARETFORE, color(palette, "text"));
    auto rgba = [&palette](const char* key) {
        const QColor value = palette.value(QLatin1String(key)).value<QColor>();
        return quint32(scintillaquick::rgb_from_color(value)) | (quint32(value.alpha()) << 24);
    };
    if (editor_.send(SCI_GETSELECTIONEMPTY))
        editor_.send(SCI_SETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK, rgba("currentLine"));
    else
        editor_.send(SCI_RESETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK);
    for (int element : {SC_ELEMENT_SELECTION_BACK, SC_ELEMENT_SELECTION_INACTIVE_BACK})
        editor_.send(SCI_SETELEMENTCOLOUR, element, rgba("selection"));
    for (int element : {SC_ELEMENT_SELECTION_TEXT, SC_ELEMENT_SELECTION_INACTIVE_TEXT})
        editor_.send(SCI_RESETELEMENTCOLOUR, element);
    editor_.send(SCI_MARKERSETFORE, bookmarkMarker, color(palette, "accent"));
    editor_.send(SCI_MARKERSETBACK, bookmarkMarker, color(palette, "accent"));
    editor_.send(SCI_INDICSETFORE, errorIndicator, color(palette, "error"));
    editor_.send(SCI_INDICSETFORE, warningIndicator, color(palette, "warning"));
    editor_.send(SCI_INDICSETFORE, muriIndicator, color(palette, "error"));
    editor_.send(SCI_INDICSETFORE, followIndicator, color(palette, "follow"));
    editor_.send(SCI_INDICSETALPHA, followIndicator, qRound(palette.value(QStringLiteral("followOpacity")).toDouble() * 255));
}
void ScintillaDslStyler::reset()
{
    lines_ = QVector<LineState>();
    bookmarks_.clear();
    dirtyLine_ = 0;
    dirtyThrough_ = document_.lineCount() - 1;
    validation_.clear();
    muri_.clear();
    followActive_ = false;
    validationDirty_ = muriDirty_ = true;
    editor_.send(SCI_MARKERDELETEALL, bookmarkMarker);
    for (int indicator : {errorIndicator, warningIndicator, muriIndicator, followIndicator}) {
        editor_.send(SCI_SETINDICATORCURRENT, indicator);
        editor_.send(SCI_INDICATORCLEARRANGE, 0, editor_.send(SCI_GETLENGTH));
    }
}
void ScintillaDslStyler::invalidate(int line, int linesAdded)
{
    dirtyLine_ = qMin(dirtyLine_, line);
    dirtyThrough_ = qMax(line + qMax(0, linesAdded), dirtyThrough_ + linesAdded);
    if (linesAdded) {
        QMap<int, QString> shifted;
        for (auto it = bookmarks_.cbegin(); it != bookmarks_.cend(); ++it) {
            if (it.key() <= line) shifted.insert(it.key(), it.value());
            else if (linesAdded > 0 || it.key() > line - linesAdded)
                shifted.insert(it.key() + linesAdded, it.value());
        }
        bookmarks_ = std::move(shifted);
    }
    validationDirty_ = muriDirty_ = true;
    followStart_ = -1;
    if (line < lines_.size()) {
        if (linesAdded > 0) lines_.insert(line + 1, linesAdded, LineState{});
        else if (linesAdded < 0) lines_.remove(line + 1, qMin(-linesAdded, int(lines_.size()) - line - 1));
    }
}
void ScintillaDslStyler::style()
{
    lines_.resize(document_.lineCount());
    QByteArray stack = dirtyLine_ > 0 ? lines_[dirtyLine_ - 1].stack : QByteArray{};
    for (int line = dirtyLine_; line < document_.lineCount(); ++line) {
        const int begin = document_.lineStart(line);
        const int end = line + 1 < document_.lineCount() ? document_.lineStart(line + 1) : document_.text().size();
        const QStringView source = QStringView(document_.text()).mid(begin, end - begin);
        const QByteArray text = source.toUtf8();
        auto& cached = lines_[line];
        if (line > dirtyThrough_ && cached.valid && cached.text == text && cached.inputStack == stack) break;
        const QByteArray inputStack = stack;
        QByteArray styles(text.size(), 0);
        bool comment = false;
        for (int i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (ch == '\n' || ch == '\r') comment = false;
            if (ch == '|' && i + 1 < text.size() && text[i + 1] == '|') comment = true;
            if (comment) { styles[i] = 3; continue; }
            styles[i] = stack.isEmpty() ? 0 : stack.back() == ']' ? 2 : 1;
            if (ch == '<' && text.mid(i + 1, 3) == "HS*") {
                const int close = text.indexOf('>', i + 1);
                const int newline = text.indexOf('\n', i + 1);
                if (close >= 0 && (newline < 0 || close < newline)) {
                    for (; i <= close; ++i) styles[i] = 1;
                    --i;
                    continue;
                }
            }
            if (ch == '(' || ch == '{' || ch == '[') {
                stack.append(ch == '(' ? ')' : ch == '{' ? '}' : ']');
                styles[i] = ch == '[' ? 2 : 1;
            } else if (ch == ')' || ch == '}' || ch == ']') {
                styles[i] = ch == ']' ? 2 : 1;
                const int matching = stack.lastIndexOf(ch);
                if (matching >= 0) stack.truncate(matching);
            }
        }
        const auto bookmark = miacode::editor::parseBookmarkComment(source.toString());
        const QString title = bookmark && !bookmark->control ? bookmark->title : QString{};
        if (!cached.valid || cached.bookmark != title) {
            editor_.send(SCI_MARKERDELETE, line, bookmarkMarker);
            bookmarks_.remove(line);
            if (!title.isNull()) {
                editor_.send(SCI_MARKERADD, line, bookmarkMarker);
                bookmarks_.insert(line, title);
            }
        }
        cached = {text, stack, title, true, inputStack};
        editor_.send(SCI_STARTSTYLING, document_.bytePosition(begin));
        editor_.sends(SCI_SETSTYLINGEX, styles.size(), styles.constData());
    }
    // Cached suffix styles remain valid after propagation reaches an unchanged input state.
    editor_.send(SCI_STARTSTYLING, editor_.send(SCI_GETLENGTH));
    dirtyLine_ = document_.lineCount();
    dirtyThrough_ = -1;
}
QVariantList ScintillaDslStyler::bookmarks() const
{
    QVariantList result;
    for (auto it = bookmarks_.cbegin(); it != bookmarks_.cend(); ++it)
        result.append(QVariantMap{{QStringLiteral("line"), it.key() + 1}, {QStringLiteral("title"), it.value()}});
    return result;
}
void ScintillaDslStyler::fill(int indicator, int start, int end)
{
    const int first = document_.bytePosition(start);
    const int last = document_.bytePosition(end);
    editor_.send(SCI_SETINDICATORCURRENT, indicator);
    editor_.send(SCI_INDICATORFILLRANGE, first, last - first);
}
void ScintillaDslStyler::diagnostics(const QVariantList& rows, bool isMuri)
{
    auto& cached = isMuri ? muri_ : validation_;
    auto& dirty = isMuri ? muriDirty_ : validationDirty_;
    if (!dirty && cached == rows) return;
    cached = rows;
    dirty = false;
    for (int indicator : isMuri ? QList<int>{muriIndicator} : QList<int>{errorIndicator, warningIndicator}) {
        editor_.send(SCI_SETINDICATORCURRENT, indicator);
        editor_.send(SCI_INDICATORCLEARRANGE, 0, editor_.send(SCI_GETLENGTH));
    }
    for (const auto& value : rows) {
        const auto row = value.toMap();
        const int line = row.value(QStringLiteral("line")).toInt() - 1;
        if (line < 0 || line >= editor_.send(SCI_GETLINECOUNT)) continue;
        const int lineStart = document_.utf16Position(editor_.send(SCI_POSITIONFROMLINE, line));
        const int lineEnd = document_.utf16Position(editor_.send(SCI_GETLINEENDPOSITION, line));
        const int start = qBound(lineStart, lineStart + qMax(0, row.value(QStringLiteral("column")).toInt() - 1), lineEnd);
        const int end = qBound(start, lineStart + qMax(row.value(QStringLiteral("endColumn")).toInt(), row.value(QStringLiteral("column")).toInt()), lineEnd);
        const int indicator = isMuri ? muriIndicator : row.value(QStringLiteral("severity")) == QStringLiteral("warning") ? warningIndicator : errorIndicator;
        fill(indicator, start, end);
    }
}
void ScintillaDslStyler::follow(bool active, int start, int end)
{
    if (active == followActive_ && (!active || (start == followStart_ && end == followEnd_))) return;
    editor_.send(SCI_SETINDICATORCURRENT, followIndicator);
    if (followActive_) {
        editor_.send(SCI_INDICATORCLEARRANGE, 0, editor_.send(SCI_GETLENGTH));
    }
    if (active) fill(followIndicator, start, end);
    followActive_ = active;
    followStart_ = start;
    followEnd_ = end;
}
}
