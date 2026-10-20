#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <functional>

namespace miacode::update {

// 取 manifest 的端口。生产实现是唯一使用 QNetworkAccessManager 的地方；
// spec 注入一个同步的假实现。
class UpdateFetcher
{
public:
    // reason 只在 ok == false 时有意义，写日志用，不展示给用户。
    //
    // 契约：每次 fetch() 必须让传入的 callback 恰好被调用一次。UpdateService
    // 的整台状态机都假设了这一点——调用两次会让一次检查被重复记录、
    // manualCheckFinished 被重复发出去；一次都不调用则让服务卡在
    // inFlight_ == true，用户在重启之前都没法再发起下一次检查。
    using Callback = std::function<void(bool ok, QByteArray payload, QString reason)>;

    virtual ~UpdateFetcher() = default;

    virtual void fetch(const QUrl& url, Callback callback) = 0;
};

} // namespace miacode::update
