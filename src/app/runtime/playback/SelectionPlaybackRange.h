#pragma once
#include <QtGlobal>
#include <QtMath>

namespace miacode::runtime {
// One audition owns one immutable chart interval. Highlight changes are not
// transport commands and have no path into this state.
class SelectionPlaybackRange {
public:
    bool begin(int difficulty, quint64 revision, double start, double end) {
        if (difficulty <= 0 || !qIsFinite(start) || !qIsFinite(end) || end <= start) return false;
        difficulty_ = difficulty; revision_ = revision; start_ = qMax(0.0,start); end_ = end;
        active_ = end_ > start_;
        return active_;
    }
    void cancel() { active_ = false; }
    bool active() const { return active_; }
    bool matches(int difficulty, quint64 revision) const {
        return active_ && difficulty_ == difficulty && revision_ == revision;
    }
    double startSecond() const { return start_; }
    double endSecond() const { return end_; }
    bool finished(double clockSecond, double tolerance = 1e-6) const {
        return active_ && qIsFinite(clockSecond) && clockSecond + tolerance >= end_;
    }
private:
    bool active_ = false;
    int difficulty_ = -1;
    quint64 revision_ = 0;
    double start_ = 0.0, end_ = 0.0;
};
}
