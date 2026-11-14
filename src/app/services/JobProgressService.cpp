#include "app/services/JobProgressService.h"

#include <algorithm>

namespace miacode {

JobProgressService::JobProgressService(QObject* parent)
    : QObject(parent)
{
}

bool JobProgressService::chartExport() const
{
    return taskType_ == TaskType::ChartExport;
}

quint64 JobProgressService::begin(const LocalizedText& title,
                                  const LocalizedText& label,
                                  bool cancellable,
                                  TaskType taskType,
                                  Presentation presentation)
{
    active_ = true;
    ++token_;
    indeterminate_ = false;
    cancellable_ = cancellable;
    taskType_ = taskType;
    presentation_ = presentation;
    // A new job always starts uncancelled: a stale flag from the previous job
    // would abort this one at its first checkpoint.
    cancelRequested_ = false;
    percent_ = 0;
    title_ = title;
    label_ = label;
    emit changed();
    return token_;
}

void JobProgressService::report(int percent, const LocalizedText& label)
{
    if (!active_) {
        return;
    }
    const int clamped = std::clamp(percent, 0, 100);
    if (clamped == percent_ && label.text() == label_.text() && !indeterminate_) {
        return;
    }
    indeterminate_ = false;
    percent_ = clamped;
    label_ = label;
    emit changed();
}

void JobProgressService::reportIndeterminate(const LocalizedText& label)
{
    if (!active_) {
        return;
    }
    if (indeterminate_ && label.text() == label_.text()) {
        return;
    }
    indeterminate_ = true;
    label_ = label;
    emit changed();
}

void JobProgressService::retranslate()
{
    if (active_) emit changed();
}

void JobProgressService::end(const LocalizedText& completionMessage)
{
    if (!active_) {
        return;
    }
    const quint64 finishedToken = token_;
    const QString message = completionMessage.text();
    active_ = false;
    cancellable_ = false;
    cancelRequested_ = false;
    indeterminate_ = false;
    percent_ = 0;
    taskType_ = TaskType::Generic;
    presentation_ = Presentation::Dialog;
    title_.clear();
    label_.clear();
    emit changed();
    emit finished(finishedToken, message);
}

void JobProgressService::requestCancel()
{
    if (!active_ || !cancellable_ || cancelRequested_) {
        return;
    }
    cancelRequested_ = true;
    emit changed();
    emit cancellationRequested(token_);
}

}  // namespace miacode
