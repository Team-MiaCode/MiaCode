#pragma once

#include "app/services/ShortcutRegistry.h"

#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <functional>
#include <utility>

namespace miacode::runtime {

// Shared by the workbench event filter and its event-driven regression spec.
class PlaybackSpaceFilter {
public:
    PlaybackSpaceFilter(const ui::ShortcutRegistry& shortcuts,
                        std::function<void()> togglePlayback,
                        std::function<void()> stopOrPlay)
        : shortcuts_(shortcuts), togglePlayback_(std::move(togglePlayback)),
          stopOrPlay_(std::move(stopOrPlay)) {}

    bool handle(QEvent* event, QQuickWindow* window, bool focusOnWorkbench) const
    {
        if (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress
            && event->type() != QEvent::KeyRelease) return false;
        const auto* key = static_cast<QKeyEvent*>(event);
        if (!focusOnWorkbench || window == nullptr || key->key() != Qt::Key_Space
            || key->modifiers() != Qt::NoModifier
            || !window->property("playbackCommandsEnabled").toBool()
            || window->property("sourceEditorFocused").toBool()) return false;
        if (reservesSpace(QGuiApplication::focusObject())
            || reservesSpace(window->activeFocusItem())) {
            // Block application shortcuts while leaving key presses to the control.
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
                return true;
            }
            return false;
        }
        // Let explicit bindings own Space, including the first key of a chord.
        const QKeyCombination plainSpace(Qt::NoModifier, Qt::Key_Space);
        bool stopOrPlay = false;
        bool assignedElsewhere = false;
        for (const auto& definition : shortcuts_.editableShortcuts()) {
            const auto sequence = shortcuts_.sequence(definition.id);
            if (sequence.isEmpty() || sequence[0] != plainSpace) continue;
            if (sequence.count() == 1 && definition.id == QLatin1String("preview.stop_or_play")) {
                stopOrPlay = true;
            } else if (sequence.count() != 1 || definition.id != QLatin1String("preview.play_pause_global")) {
                assignedElsewhere = true;
            }
        }
        if (assignedElsewhere) return false;
        if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
            if (stopOrPlay) stopOrPlay_();
            else togglePlayback_(); // Preserve the unassigned default Space gesture.
        }
        event->accept();
        return true;
    }

private:
    static bool reservesSpace(QObject* object)
    {
        for (QObject* current = object; current != nullptr; current = current->parent()) {
            if (current->property("reservesPlainSpace").toBool()) return true;
        }
        for (auto* current = qobject_cast<QQuickItem*>(object); current != nullptr;
             current = current->parentItem()) {
            if (current->property("reservesPlainSpace").toBool()) return true;
        }
        return false;
    }

    const ui::ShortcutRegistry& shortcuts_;
    std::function<void()> togglePlayback_;
    std::function<void()> stopOrPlay_;
};

} // namespace miacode::runtime
