#pragma once

#include <QVector>

#include <algorithm>
#include <limits>
#include <utility>

#include "common/TaskCancellation.h"

namespace miacode::muri::detail {

struct MuriTimeInterval {
    double start = 0.0;
    double end = 0.0;
    int sourceIndex = 0;
    double subtreeEnd = 0.0;
};

// Balanced interval tree over a sorted array. Queries retain source order.
class MuriTimeIndex {
public:
    MuriTimeIndex() = default;
    explicit MuriTimeIndex(QVector<MuriTimeInterval> intervals)
        : intervals_(std::move(intervals))
    {
        std::sort(intervals_.begin(), intervals_.end(), [](const auto& left, const auto& right) {
            miacode::task::CancellationScope::checkpoint();
            if (left.start != right.start) return left.start < right.start;
            return left.sourceIndex < right.sourceIndex;
        });
        build(0, intervals_.size());
    }

    QVector<int> intersecting(double start, double end) const
    {
        QVector<int> result;
        collect(0, intervals_.size(), start, end, result);
        std::sort(result.begin(), result.end());
        return result;
    }

    int firstEndingAfter(double second) const
    {
        return firstEndingAfter(0, intervals_.size(), second);
    }

private:
    int firstEndingAfter(int begin, int end, double second) const
    {
        if (begin == end) return -1;
        miacode::task::CancellationScope::checkpoint();
        const int middle = begin + (end - begin) / 2;
        const auto& interval = intervals_.at(middle);
        if (interval.subtreeEnd < second) return -1;
        const int left = firstEndingAfter(begin, middle, second);
        if (left >= 0) return left;
        if (interval.end >= second) return interval.sourceIndex;
        return firstEndingAfter(middle + 1, end, second);
    }

    double build(int begin, int end)
    {
        if (begin == end) return -std::numeric_limits<double>::infinity();
        miacode::task::CancellationScope::checkpoint();
        const int middle = begin + (end - begin) / 2;
        const double leftEnd = build(begin, middle);
        const double rightEnd = build(middle + 1, end);
        auto& interval = intervals_[middle];
        interval.subtreeEnd = std::max({interval.end, leftEnd, rightEnd});
        return interval.subtreeEnd;
    }

    void collect(int begin, int end, double start, double finish, QVector<int>& result) const
    {
        if (begin == end) return;
        miacode::task::CancellationScope::checkpoint();
        const int middle = begin + (end - begin) / 2;
        const auto& interval = intervals_.at(middle);
        if (interval.subtreeEnd < start) return;
        collect(begin, middle, start, finish, result);
        if (interval.start > finish) return;
        if (interval.end >= start) result.append(interval.sourceIndex);
        collect(middle + 1, end, start, finish, result);
    }

    QVector<MuriTimeInterval> intervals_;
};

} // namespace miacode::muri::detail
