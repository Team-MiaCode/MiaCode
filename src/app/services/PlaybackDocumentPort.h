#pragma once

#include <QtGlobal>

namespace miacode {

// 播放协调器通过此端口提交文档编辑、请求导航并读取已应用的文档版本。
class PlaybackDocumentPort
{
public:
    virtual ~PlaybackDocumentPort() = default;

    virtual void updateDirtyState() = 0;
    virtual bool requestEditorNavigation(int line, int column, int endLine, int endColumn,
                                         bool selectToken, bool focusEditor, bool centerView) = 0;
    virtual quint64 appliedWorkspaceRevision() const = 0;
};

}  // namespace miacode
