#pragma once

#include <QObject>
#include <QtGlobal>

// Single decision surface for v2 window chrome / menu placement.
// QML and Bootstrap both read this object from applicationContext.
namespace miacode::ui {

class PlatformChrome final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool nativeMenuBar READ nativeMenuBar CONSTANT)
    Q_PROPERTY(bool embeddedMenuInTitleBar READ embeddedMenuInTitleBar CONSTANT)
    Q_PROPERTY(bool captionButtons READ captionButtons CONSTANT)

public:
    explicit PlatformChrome(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    bool nativeMenuBar() const
    {
#ifdef Q_OS_MACOS
        return true;
#else
        return false;
#endif
    }

    bool embeddedMenuInTitleBar() const { return !nativeMenuBar(); }

    bool captionButtons() const
    {
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
        return true;
#else
        return false;
#endif
    }
};
} // namespace miacode::ui
