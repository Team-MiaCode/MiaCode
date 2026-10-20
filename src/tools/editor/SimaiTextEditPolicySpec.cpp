#include "editor/SimaiTextEditPolicy.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

using miacode::editor::SimaiCompletionSession;
using miacode::editor::SimaiTextEditRequest;
using miacode::editor::SimaiTextEditResult;
using miacode::editor::SimaiTextEditTransaction;

struct ExpectedTransaction {
    QString text;
    int anchor = 0;
    int position = 0;
    bool hasEdit = false;
    int replacementStart = 0;
    int replacementEnd = 0;
    QString replacementText;
};

struct ExpectedResult {
    bool consumed = false;
    ExpectedTransaction transaction;
    SimaiCompletionSession completion;
};

struct PolicyCase {
    QString label;
    SimaiTextEditRequest request;
    ExpectedResult expected;
};

bool expect(bool condition, const QString& message, QTextStream& out, int* failed)
{
    if (condition) {
        out << "[PASS] " << message << '\n';
        return true;
    }
    out << "[FAIL] " << message << '\n';
    ++(*failed);
    return false;
}

void expectResult(const QString& input, const SimaiTextEditResult& actual, const ExpectedResult& expected,
                  const QString& label, QTextStream& out, int* failed)
{
    expect(actual.consumed == expected.consumed, label + QStringLiteral(" consumed"), out, failed);
    expect(actual.transaction.hasEdit == expected.transaction.hasEdit,
           label + QStringLiteral(" transaction hasEdit"), out, failed);
    QString applied = input;
    if (actual.transaction.hasEdit) applied.replace(actual.transaction.replacementStart,
        actual.transaction.replacementEnd - actual.transaction.replacementStart, actual.transaction.replacementText);
    expect(applied == expected.transaction.text,
           label + QStringLiteral(" transaction text"), out, failed);
    expect(actual.transaction.anchor == expected.transaction.anchor
               && actual.transaction.position == expected.transaction.position,
           label + QStringLiteral(" transaction selection/caret"), out, failed);
    expect(actual.transaction.replacementStart == expected.transaction.replacementStart
               && actual.transaction.replacementEnd == expected.transaction.replacementEnd
               && actual.transaction.replacementText == expected.transaction.replacementText,
           label + QStringLiteral(" transaction local replacement span"), out, failed);
    expect(actual.completion.active == expected.completion.active
               && actual.completion.opening == expected.completion.opening
               && actual.completion.closingPresent == expected.completion.closingPresent
               && actual.completion.startPosition == expected.completion.startPosition
               && actual.completion.candidates == expected.completion.candidates,
           label + QStringLiteral(" completion session"), out, failed);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);
    QTextStream out(stdout);
    int failed = 0;

    // Each case describes the input, replacement span and completion result.
    const QList<PolicyCase> cases = {
        // Half-width normalization, carried over from the Widgets editor's spec:
        // that editor had its own copy of the conversion, and the policy — the
        // one v2 actually runs — had no coverage for these characters.
        {QStringLiteral("ideographic comma normalizes to a slash separator"),
         {QString(), 0, 0, QStringLiteral("、"), Qt::Key_unknown, Qt::NoModifier,
          true, false, true, QString()},
         {true, {QStringLiteral("/"), 1, 1, true, 0, 0, QStringLiteral("/")}, {}}},
        {QStringLiteral("full-width comma normalizes to the beat separator"),
         {QString(), 0, 0, QStringLiteral("，"), Qt::Key_unknown, Qt::NoModifier,
          true, false, true, QString()},
         {true, {QStringLiteral(","), 1, 1, true, 0, 0, QStringLiteral(",")}, {}}},
        {QStringLiteral("full-width slash, hash and colon convert to half width"),
         {QString(), 0, 0, QStringLiteral("／＃："), Qt::Key_unknown, Qt::NoModifier,
          true, false, true, QString()},
         {true, {QStringLiteral("/#:"), 3, 3, true, 0, 0, QStringLiteral("/#:")}, {}}},
        {QStringLiteral("shift-6 forces a caret regardless of the IME's glyph"),
         {QString(), 0, 0, QStringLiteral("＾"), Qt::Key_6, Qt::ShiftModifier,
          true, false, true, QString()},
         {true, {QStringLiteral("^"), 1, 1, true, 0, 0, QStringLiteral("^")}, {}}},
        {QStringLiteral("half-width conversion is off when the preference is off"),
         {QString(), 0, 0, QStringLiteral("，"), Qt::Key_unknown, Qt::NoModifier,
          false, false, true, QString()},
         {true, {QStringLiteral("，"), 1, 1, true, 0, 0, QStringLiteral("，")}, {}}},
        {QStringLiteral("IME half-width bracket conversion"),
         {QString(), 0, 0, QStringLiteral("【"), Qt::Key_BracketLeft, Qt::NoModifier,
          true, false, true, QString()},
         {true, {QStringLiteral("[]"), 1, 1, true, 0, 0, QStringLiteral("[]")},
          {true, QLatin1Char('['), true, 1,
           {QStringLiteral("8:1]"), QStringLiteral("4:1]"), QStringLiteral("16:3]"),
            QStringLiteral("384:1]")}}}},
        {QStringLiteral("selection replacement"),
         {QStringLiteral("abc"), 1, 2, QStringLiteral("["), Qt::Key_BracketLeft,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QStringLiteral("a[]c"), 2, 2, true, 1, 2, QStringLiteral("[]")},
          {true, QLatin1Char('['), true, 2,
           {QStringLiteral("8:1]"), QStringLiteral("4:1]"), QStringLiteral("16:3]"),
            QStringLiteral("384:1]")}}}},
        {QStringLiteral("repeated text replacement stays local"),
         {QStringLiteral("xx"), 1, 2, QStringLiteral("["), Qt::Key_BracketLeft,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QStringLiteral("x[]"), 2, 2, true, 1, 2, QStringLiteral("[]")},
          {true, QLatin1Char('['), true, 2,
           {QStringLiteral("8:1]"), QStringLiteral("4:1]"), QStringLiteral("16:3]"),
            QStringLiteral("384:1]")}}}},
        {QStringLiteral("bracket pair insert"),
         {QStringLiteral("x"), 1, 1, QStringLiteral("{"), Qt::Key_BraceLeft,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QStringLiteral("x{}"), 2, 2, true, 1, 1, QStringLiteral("{}")},
          {true, QLatin1Char('{'), true, 2,
           {QStringLiteral("16}"), QStringLiteral("24}"), QStringLiteral("32}")}}}},
        {QStringLiteral("whole BPM completion entry"),
         {QString(), 0, 0, QStringLiteral("("), 0, Qt::NoModifier,
          true, false, true, QStringLiteral("180")},
         {true, {QStringLiteral("()"), 1, 1, true, 0, 0, QStringLiteral("()")},
          {true, QLatin1Char('('), true, 1, {QStringLiteral("180)")}}}},
        {QStringLiteral("existing left bracket advances"),
         {QStringLiteral("[8:1]"), 0, 0, QStringLiteral("["), Qt::Key_BracketLeft,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QStringLiteral("[8:1]"), 1, 1, false, 0, 0, QString()},
          {true, QLatin1Char('['), false, 1,
           {QStringLiteral("8:1]"), QStringLiteral("4:1]"), QStringLiteral("16:3]"),
            QStringLiteral("384:1]")}}}},
        {QStringLiteral("right bracket skips"),
         {QStringLiteral("[]"), 1, 1, QStringLiteral("]"), Qt::Key_BracketRight,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QStringLiteral("[]"), 2, 2, false, 0, 0, QString()}, {}}},
        {QStringLiteral("empty pair single delete"),
         {QStringLiteral("[]"), 1, 1, QStringLiteral("\b"), Qt::Key_Backspace,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QString(), 0, 0, true, 0, 2, QString()}, {}}},
        {QStringLiteral("ordinary Backspace stays native"),
         {QStringLiteral("abc"), 2, 2, QStringLiteral("\b"), Qt::Key_Backspace,
          Qt::NoModifier, true, false, true, QString()},
         {false, {QStringLiteral("abc"), 2, 2, false, 0, 0, QString()}, {}}},
        {QStringLiteral("ordinary Delete stays native"),
         {QStringLiteral("abc"), 1, 1, QStringLiteral("\x7f"), Qt::Key_Delete,
          Qt::NoModifier, true, false, true, QString()},
         {false, {QStringLiteral("abc"), 1, 1, false, 0, 0, QString()}, {}}},
        {QStringLiteral("h hold completion entry"),
         {QString(), 0, 0, QStringLiteral("h"), Qt::Key_H,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QStringLiteral("h"), 1, 1, true, 0, 0, QStringLiteral("h")},
          {true, QLatin1Char('['), false, 1,
           {QStringLiteral("[8:1]"), QStringLiteral("[4:1]"), QStringLiteral("[16:3]"),
            QStringLiteral("[384:1]")}}}},
        {QStringLiteral("active bracket completion treats h as a filter character"),
         {QStringLiteral("[]"), 1, 1, QStringLiteral("h"), Qt::Key_H,
          Qt::NoModifier, true, false, true, QString(), miacode::editor::defaultCompletionConfig(), true},
         {true, {QStringLiteral("[h]"), 2, 2, true, 1, 1, QStringLiteral("h")}, {}}},
        {QStringLiteral("custom durations populate bracket completion in configured order"),
         SimaiTextEditRequest{
             .input = QStringLiteral("["),
             .completionConfig = {{QStringLiteral("12:1"), QStringLiteral("6:1")},
                                  {QStringLiteral("12"), QStringLiteral("48")}}},
         {true, {QStringLiteral("[]"), 1, 1, true, 0, 0, QStringLiteral("[]")},
          {true, QLatin1Char('['), true, 1, {QStringLiteral("12:1]"), QStringLiteral("6:1]")}}}},
        {QStringLiteral("custom divisions populate brace completion in configured order"),
         SimaiTextEditRequest{
             .input = QStringLiteral("{"),
             .completionConfig = {{QStringLiteral("12:1"), QStringLiteral("6:1")},
                                  {QStringLiteral("12"), QStringLiteral("48")}}},
         {true, {QStringLiteral("{}"), 1, 1, true, 0, 0, QStringLiteral("{}")},
          {true, QLatin1Char('{'), true, 1, {QStringLiteral("12}"), QStringLiteral("48}")}}}},
        {QStringLiteral("Enter line break"),
         {QStringLiteral("x"), 1, 1, QString(), Qt::Key_Return,
          Qt::NoModifier, true, false, true, QString()},
         {true, {QStringLiteral("x\n"), 2, 2, true, 1, 1, QStringLiteral("\n")}, {}}},
        {QStringLiteral("Ctrl+Enter line break"),
         {QStringLiteral("x"), 1, 1, QString(), Qt::Key_Return,
          Qt::ControlModifier, true, false, true, QString()},
         {true, {QStringLiteral("x\n"), 2, 2, true, 1, 1, QStringLiteral("\n")}, {}}},
        {QStringLiteral("overwrite mode falls through"),
         {QStringLiteral("x"), 1, 1, QStringLiteral("["), Qt::Key_BracketLeft,
          Qt::NoModifier, true, true, true, QString()},
         {false, {QStringLiteral("x"), 1, 1, false, 0, 0, QString()}, {}}},
    };

    for (const PolicyCase& policyCase : cases) {
        expectResult(policyCase.request.text, miacode::editor::applySimaiTextEditPolicy(policyCase.request),
                     policyCase.expected, policyCase.label, out, &failed);
    }

    // A refused command-modified key must tell its adapter whether Qt's text
    // control would still type the literal character. Qt drops Ctrl-modified
    // characters itself, and Alt/Option is a real composition modifier, but a
    // Meta/Super modifier (macOS 物理 Control) reaches the raw insert path.
    struct FallbackCase {
        QString label;
        QString input;
        int key;
        Qt::KeyboardModifiers modifiers;
        bool suppress;
    };
    const FallbackCase fallbackCases[] = {
        {QStringLiteral("Meta+Z is suppressed instead of typing z"), QStringLiteral("z"),
         Qt::Key_Z, Qt::MetaModifier, true},
        {QStringLiteral("Meta+C is suppressed instead of typing c"), QStringLiteral("c"),
         Qt::Key_C, Qt::MetaModifier, true},
        {QStringLiteral("Ctrl+Z stays a native shortcut"), QStringLiteral("z"),
         Qt::Key_Z, Qt::ControlModifier, false},
        {QStringLiteral("Ctrl+Meta+Z stays a native shortcut"), QStringLiteral("z"),
         Qt::Key_Z, Qt::ControlModifier | Qt::MetaModifier, false},
        {QStringLiteral("Alt composition still types its character"), QStringLiteral("\u00f8"),
         Qt::Key_O, Qt::AltModifier, false},
        {QStringLiteral("Meta with no text is not suppressed"), QString(),
         Qt::Key_Left, Qt::MetaModifier, false},
    };
    for (const FallbackCase& fallbackCase : fallbackCases) {
        miacode::editor::SimaiTextEditRequest request;
        request.text = QStringLiteral("abc");
        request.anchor = request.position = 3;
        request.input = fallbackCase.input;
        request.key = fallbackCase.key;
        request.modifiers = fallbackCase.modifiers;
        const auto result = miacode::editor::applySimaiTextEditPolicy(request);
        const bool ok = !result.consumed
            && result.suppressFallbackInsert == fallbackCase.suppress
            && !result.transaction.hasEdit;
        out << (ok ? "[PASS] " : "[FAIL] ") << fallbackCase.label << '\n';
        if (!ok) ++failed;
    }

    // Keys whose text() is a control character. The policy's insert fallback
    // used to gate on "is there text", so Escape typed U+001B into the chart —
    // a character Qt's own controls would have refused. Declining leaves the
    // key to Qt, which refuses it too.
    struct NonTextKeyCase {
        QString label;
        QString input;
        int key;
    };
    const NonTextKeyCase nonTextCases[] = {
        {QStringLiteral("Escape types nothing"), QStringLiteral("\u001B"), Qt::Key_Escape},
        {QStringLiteral("Backspace types nothing"), QStringLiteral("\b"), Qt::Key_Backspace},
        {QStringLiteral("Delete types nothing"), QStringLiteral("\u007F"), Qt::Key_Delete},
        {QStringLiteral("Cancel types nothing"), QStringLiteral("\u0018"), Qt::Key_Cancel},
    };
    for (const NonTextKeyCase& nonTextCase : nonTextCases) {
        miacode::editor::SimaiTextEditRequest request;
        request.text = QStringLiteral("abc");
        request.anchor = request.position = 3;
        request.input = nonTextCase.input;
        request.key = nonTextCase.key;
        const auto result = miacode::editor::applySimaiTextEditPolicy(request);
        const bool ok = !result.consumed && !result.transaction.hasEdit;
        out << (ok ? "[PASS] " : "[FAIL] ") << nonTextCase.label << '\n';
        if (!ok) ++failed;
    }

    // The other half of the same rule: real text still gets typed, including a
    // tab and a zero-width joiner, which are not printable but are content.
    struct TextKeyCase {
        QString label;
        QString input;
        int key;
        QString expected;
    };
    const TextKeyCase textCases[] = {
        {QStringLiteral("a letter still types"), QStringLiteral("d"), Qt::Key_D,
         QStringLiteral("abcd")},
        {QStringLiteral("a tab still types"), QStringLiteral("\t"), Qt::Key_Tab,
         QStringLiteral("abc\t")},
        {QStringLiteral("a zero-width joiner still types"), QStringLiteral("\u200D"), 0,
         QStringLiteral("abc\u200D")},
    };
    for (const TextKeyCase& textCase : textCases) {
        miacode::editor::SimaiTextEditRequest request;
        request.text = QStringLiteral("abc");
        request.anchor = request.position = 3;
        request.input = textCase.input;
        request.key = textCase.key;
        const auto result = miacode::editor::applySimaiTextEditPolicy(request);
        QString applied = request.text;
        if (result.transaction.hasEdit) applied.replace(result.transaction.replacementStart,
            result.transaction.replacementEnd - result.transaction.replacementStart, result.transaction.replacementText);
        const bool ok = result.consumed && applied == textCase.expected;
        out << (ok ? "[PASS] " : "[FAIL] ") << textCase.label << '\n';
        if (!ok) ++failed;
    }

    if (failed != 0) {
        out << "SimaiTextEditPolicy spec failed: " << failed << '\n';
        return 1;
    }
    out << "SimaiTextEditPolicy spec passed.\n";
    return 0;
}
