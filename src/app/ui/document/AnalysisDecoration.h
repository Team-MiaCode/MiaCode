#pragma once

namespace miacode::ui {

struct AnalysisDecoration {
    int line = 1;
    int column = 1;
    int endColumn = 1;
    bool warning = false;

    bool operator==(const AnalysisDecoration& other) const
    {
        return line == other.line && column == other.column
            && endColumn == other.endColumn && warning == other.warning;
    }
};

} // namespace miacode::ui
