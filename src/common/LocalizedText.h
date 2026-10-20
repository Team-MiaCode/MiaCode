#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <tuple>
#include <utility>

namespace miacode {

// Retains a translation and its arguments until the presentation is read.
// Literal text represents user content, paths, or external diagnostics.
class LocalizedText
{
public:
    LocalizedText() = default;
    LocalizedText(const QString& literal) : literal_(literal) {}

    static LocalizedText translated(const char* id)
    {
        const QByteArray key(id);
        return LocalizedText([key] { return qtTrId(key.constData()); });
    }

    QString text() const { return render_ ? render_() : literal_; }
    operator QString() const { return text(); }
    bool isNull() const { return !render_ && literal_.isNull(); }
    bool isEmpty() const { return text().isEmpty(); }
    void clear() { *this = LocalizedText(); }
    QVariant variant() const;

    template<typename... Args>
    LocalizedText arg(Args... args) const
    {
        return LocalizedText([source = *this, values = std::make_tuple(std::move(args)...)] {
            return std::apply([&source](const auto&... value) {
                return source.text().arg(formatArgument(value)...);
            }, values);
        });
    }

    friend LocalizedText operator+(const LocalizedText& left, const LocalizedText& right)
    {
        return LocalizedText([left, right] { return left.text() + right.text(); });
    }

    friend LocalizedText operator+(const QString& left, const LocalizedText& right)
    { return LocalizedText(left) + right; }
    friend LocalizedText operator+(const LocalizedText& left, const QString& right)
    { return left + LocalizedText(right); }

    static QString resolve(const QVariant& value);

private:
    explicit LocalizedText(std::function<QString()> render) : render_(std::move(render)) {}
    static QString formatArgument(const LocalizedText& value) { return value.text(); }
    template<typename T>
    static const T& formatArgument(const T& value) { return value; }

    QString literal_;
    std::function<QString()> render_;
};

// Accepts existing literal filter lists as well as deferred translations.
class LocalizedTextList : public QList<LocalizedText>
{
public:
    using QList<LocalizedText>::QList;
    using QList<LocalizedText>::operator=;
    LocalizedTextList() = default;
    LocalizedTextList(const QStringList& values)
    {
        for (const auto& value : values) append(value);
    }
    LocalizedTextList& operator=(const QStringList& values)
    {
        clear();
        for (const auto& value : values) append(value);
        return *this;
    }
    LocalizedTextList& operator=(std::initializer_list<LocalizedText> values)
    {
        QList<LocalizedText>::operator=(QList<LocalizedText>(values));
        return *this;
    }
};

inline LocalizedText localizedText(const char* id) { return LocalizedText::translated(id); }

} // namespace miacode

Q_DECLARE_METATYPE(miacode::LocalizedText)

inline QVariant miacode::LocalizedText::variant() const { return QVariant::fromValue(*this); }
inline QString miacode::LocalizedText::resolve(const QVariant& value)
{
    return value.metaType() == QMetaType::fromType<LocalizedText>()
        ? value.value<LocalizedText>().text() : value.toString();
}
