#include "app/ui/net/NetUploadModel.h"
#include "app/services/net/NetAccountStore.h"
#include "app/services/net/NetConfiguration.h"
#include "common/DebugLog.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include <QUuid>

namespace miacode::ui {
NetUploadModel::NetUploadModel(NetService* service, UiRequestService& requests, QObject* parent)
    : QAbstractListModel(parent), service_(service), requests_(requests) {
    remember_ = net_configuration::value("net_upload_remember_credentials").toBool(false);
    username_ = net_configuration::value("net_upload_username").toString();
    rootDirectory_ = net_configuration::value("last_net_batch_upload_dir").toString();
    if (net_configuration::enabled() && remember_) {
        const auto stored = NetAccountStore::load("desktop");
        if (stored.first == username_) password_ = stored.second;
    }
    if (service_) {
        api_ = std::make_unique<api::ApiDispatcher>(*service_, QCoreApplication::applicationVersion());
        connect(&service_->jobs(), &JobRegistry::changed, this, &NetUploadModel::updateJob);
    }
    retryTimer_.setInterval(1000);
    connect(&retryTimer_, &QTimer::timeout, this, [this] { if (!uploadJob_.isEmpty()) updateJob(uploadJob_); });
}
net::NetCallResult NetUploadModel::call(const QString& operation, const QJsonObject& parameters) {
    if (!api_) return {{}, jobError("capability.unavailable")};
    const auto reply = api_->dispatch({"desktop", {"net.upload", "net.accounts.manage", "net.accounts.use", "files.read", "jobs.read", "jobs.control"},
        {}, QUuid::createUuid().toString(QUuid::Id128)}, operation, parameters);
    return reply.envelope.value("ok").toBool() ? net::NetCallResult{reply.envelope.value("result"), {}}
        : net::NetCallResult{{}, reply.envelope.value("error").toObject()};
}
int NetUploadModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : rows_.size(); }
QVariant NetUploadModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
    const auto row = rows_[index.row()].toObject();
    switch (role) {
    case NameRole: return row.value("displayName").toString();
    case PathRole: return row.value("path").toString();
    case StateRole: return row.value("status").toString(qtTrId("net.ui.pending_upload"));
    case SelectedRole: return selected_.contains(row.value("itemId").toString());
    case FilesRole: {
        QStringList names;
        for (const auto& value : row.value("files").toArray()) names.append(value.toObject().value("displayName").toString());
        return names.join(" / ");
    }
    default: return {};
    }
}
QHash<int, QByteArray> NetUploadModel::roleNames() const { return {{NameRole, "displayName"}, {FilesRole, "fileNames"}, {PathRole, "path"}, {StateRole, "itemState"}, {SelectedRole, "selected"}}; }
bool NetUploadModel::secureStorageAvailable() const { return NetAccountStore::available(); }
QString NetUploadModel::logText() const {
    return qtTrId("net.upload_log_saved_1").arg(debug_log::logPath(debug_log::Channel::NetUpload)) + '\n' + logs_.join('\n');
}
bool NetUploadModel::retryAvailable() const {
    if (lastUploadJob_.isEmpty()) return false;
    for (const auto& value : rows_) {
        const auto state = value.toObject().value("state").toString();
        if (state == QLatin1String("failed") || state == QLatin1String("cancelled") || state == QLatin1String("blocked")) return true;
    }
    return false;
}
void NetUploadModel::setUsername(const QString& value) { if (busy() || username_ == value) return; if (loggedIn()) logout(); username_ = value; emit changed(); }
void NetUploadModel::setPassword(const QString& value) { if (busy() || password_ == value) return; if (loggedIn()) logout(); password_ = value; emit changed(); }
void NetUploadModel::setRemember(bool value) {
    if (busy() || remember_ == value) return;
    remember_ = value && secureStorageAvailable();
    if (!remember_) NetAccountStore::erase("desktop");
    net_configuration::update({{"net_upload_remember_credentials", remember_},
        {"net_upload_username", remember_ ? QJsonValue(username_) : QJsonValue(QJsonValue::Null)},
        {"net_upload_credential_ref", QJsonValue(QJsonValue::Null)}});
    emit changed();
}
void NetUploadModel::setRootDirectory(const QString& value) {
    if (busy()) return;
    rootDirectory_ = value.trimmed().isEmpty() ? QString{}
        : QDir::cleanPath(QDir::fromNativeSeparators(value.trimmed()));
    net_configuration::update({{"last_net_batch_upload_dir", rootDirectory_}});
    emit changed();
}
void NetUploadModel::report(const QString& text) {
    statusText_ = text;
    logs_.append(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text);
    if (logs_.size() > 1000) logs_.removeFirst();
    emit changed();
}
void NetUploadModel::browse() {
    if (busy()) return;
    FileRequest request;
    request.title = localizedText("net.ui.chart_root"); request.selectFolder = true; request.startPath = rootDirectory_;
    requests_.requestFile(request, [weak = QPointer<NetUploadModel>(this)](const QString& path) { if (weak && !path.isEmpty()) weak->setRootDirectory(path); });
}
void NetUploadModel::addDirectory() {
    if (!service_ || busy()) return;
    QJsonObject parameters{{"rootGrantRef", service_->grantDirectory("desktop", rootDirectory_)}};
    if (!planRef_.isEmpty()) {
        parameters.insert("uploadPlanRef", planRef_);
        QJsonArray retained;
        for (const auto& value : rows_) retained.append(value.toObject().value("itemId"));
        parameters.insert("retainedItemIds", retained);
    }
    const auto accepted = call("net.uploads.scan", parameters);
    if (accepted.ok()) { scanJob_ = accepted.result.toObject().value("jobId").toString(); report(qtTrId("net.ui.scanning")); }
    else report(qtTrId("net.ui.error").arg(accepted.error.value("code").toString()));
}
void NetUploadModel::selectRow(int row) {
    if (busy() || row < 0 || row >= rows_.size()) return;
    const auto id = rows_[row].toObject().value("itemId").toString();
    if (selected_.contains(id)) selected_.remove(id); else selected_.insert(id);
    emit dataChanged(index(row), index(row), {SelectedRole});
}
void NetUploadModel::removeSelected() {
    if (busy()) return;
    beginResetModel();
    QJsonArray retained;
    for (const auto& row : rows_) {
        const auto id = row.toObject().value("itemId").toString();
        if (!selected_.contains(id)) retained.append(row);
    }
    rows_ = retained; selected_.clear(); endResetModel(); emit changed();
}
void NetUploadModel::clearQueue() { if (busy()) return; beginResetModel(); rows_ = {}; selected_.clear(); planRef_.clear(); endResetModel(); emit changed(); }
void NetUploadModel::moveSelected(int direction) {
    if (busy() || (direction != -1 && direction != 1)) return;
    beginResetModel();
    for (int i = direction < 0 ? 1 : rows_.size() - 2; direction < 0 ? i < rows_.size() : i >= 0; i -= direction) {
        const int neighbor = i + direction;
        if (selected_.contains(rows_[i].toObject().value("itemId").toString()) && !selected_.contains(rows_[neighbor].toObject().value("itemId").toString())) {
            const QJsonValue value = rows_[i]; rows_[i] = rows_[neighbor]; rows_[neighbor] = value;
        }
    }
    endResetModel();
}
void NetUploadModel::moveSelectedTo(int row) {
    if (busy() || row < 0 || row > rows_.size() || selected_.isEmpty()) return;
    QJsonArray moved, retained;
    int insertion = row;
    for (int i = 0; i < rows_.size(); ++i) {
        if (selected_.contains(rows_[i].toObject().value("itemId").toString())) {
            moved.append(rows_[i]);
            if (i < row) --insertion;
        } else retained.append(rows_[i]);
    }
    QJsonArray result;
    for (int i = 0; i <= retained.size(); ++i) {
        if (i == insertion) for (const auto& value : moved) result.append(value);
        if (i < retained.size()) result.append(retained[i]);
    }
    beginResetModel(); rows_ = result; endResetModel();
}
void NetUploadModel::startRowDrag(int row) {
    if (busy() || row < 0 || row >= rows_.size()) return;
    const auto id = rows_[row].toObject().value("itemId").toString();
    if (selected_.contains(id)) return;
    selected_ = {id};
    emit dataChanged(index(0), index(rows_.size() - 1), {SelectedRole});
}
void NetUploadModel::login() {
    if (busy()) return;
    if (loggedIn()) logout();
    const auto accepted = call("net.accounts.login", {{"providerId", "majdata"}, {"username", username_}, {"password", password_}, {"remember", remember_}});
    if (accepted.ok()) { loginJob_ = accepted.result.toObject().value("jobId").toString(); report(qtTrId("net.upload_logging_in")); }
    else { uploadAfterLogin_ = false; report(qtTrId("net.ui.error").arg(accepted.error.value("code").toString())); }
}
void NetUploadModel::logout() {
    if (busy() || accountRef_.isEmpty()) return;
    const auto result = call("net.accounts.logout", {{"accountRef", accountRef_}});
    if (result.ok()) { accountRef_.clear(); if (!remember_) password_.clear(); report(qtTrId("net.ui.logged_out")); }
    else report(qtTrId("net.ui.error").arg(result.error.value("code").toString()));
}
void NetUploadModel::upload() {
    if (busy() || rows_.isEmpty()) return;
    if (!loggedIn()) { uploadAfterLogin_ = true; login(); return; }
    QJsonArray ids;
    uploadIds_.clear();
    for (const auto& value : rows_) {
        const auto state = value.toObject().value("state").toString();
        if (state == QLatin1String("succeeded") || state == QLatin1String("outcome_unknown")) continue;
        const auto id = value.toObject().value("itemId").toString(); ids.append(id); uploadIds_.append(id);
    }
    if (ids.isEmpty()) return;
    QJsonObject parameters{{"uploadPlanRef", planRef_}, {"accountRef", accountRef_}, {"orderedItemIds", ids}};
    QSet<QString> previous;
    if (!lastUploadJob_.isEmpty()) {
        for (const auto& value : service_->jobs().items(lastUploadJob_, "desktop")) previous.insert(value.toObject().value("materialRef").toString());
        bool inherited = true;
        for (const auto& id : ids) if (!previous.contains(id.toString())) inherited = false;
        if (inherited) parameters.insert("parentJobId", lastUploadJob_);
    }
    const auto accepted = call("net.uploads.create", parameters);
    if (accepted.ok()) { uploadJob_ = accepted.result.toObject().value("jobId").toString(); progress_ = 0; report(qtTrId("net.upload_uploading")); }
    else report(qtTrId("net.ui.error").arg(accepted.error.value("code").toString()));
}
void NetUploadModel::cancel() {
    uploadAfterLogin_ = false;
    if (!remember_) password_.clear();
    for (const auto& job : {loginJob_, scanJob_, uploadJob_}) if (!job.isEmpty()) call("jobs.cancel", {{"jobId", job}});
}
void NetUploadModel::updateJob(const QString& jobId) {
    if (!service_ || (jobId != loginJob_ && jobId != scanJob_ && jobId != uploadJob_)) return;
    const auto job = service_->jobs().snapshot(jobId, "desktop");
    const auto state = job.value("state").toString();
    if (jobId == uploadJob_) {
        int completed = 0;
        double fraction = 0;
        bool waiting = false;
        QHash<QString, int> rowIndices;
        for (int i = 0; i < rows_.size(); ++i) rowIndices.insert(rows_[i].toObject().value("itemId").toString(), i);
        for (const auto& value : service_->jobs().items(jobId, "desktop")) {
            const auto item = value.toObject(); const int order = item.value("inputOrder").toInt();
            if (order < 0 || order >= uploadIds_.size()) continue;
            const auto itemState = item.value("state").toString();
            QString label = itemState == QLatin1String("succeeded") ? qtTrId("net.upload_done") : itemState == QLatin1String("running") ? qtTrId("net.upload_uploading")
                : itemState == QLatin1String("retry_wait") ? qtTrId("net.ui.retry_wait") : itemState == QLatin1String("pending") ? qtTrId("net.ui.pending_upload")
                : itemState == QLatin1String("cancelled") ? qtTrId("net.ui.cancelled") : itemState == QLatin1String("blocked") ? qtTrId("net.paused")
                : itemState == QLatin1String("outcome_unknown") ? qtTrId("net.ui.outcome_unknown") : qtTrId("net.failed_1").arg(item.value("error").toObject().value("code").toString());
            if (itemState == QLatin1String("retry_wait")) {
                const auto retryAt = QDateTime::fromString(item.value("retryAt").toString(), Qt::ISODateWithMs);
                const qint64 seconds = qMax<qint64>(0, (QDateTime::currentDateTimeUtc().msecsTo(retryAt) + 999) / 1000);
                if (retryAt.isValid()) label = item.value("phase") == QLatin1String("upload_spacing")
                    ? qtTrId("net.upload_waiting_before_next_1").arg(seconds) : qtTrId("net.upload_rate_limited_retrying_1").arg(seconds);
                waiting |= seconds > 0;
            }
            if (itemState == QLatin1String("succeeded") || itemState == QLatin1String("failed") || itemState == QLatin1String("cancelled") || itemState == QLatin1String("outcome_unknown")) ++completed;
            if (itemState == QLatin1String("running")) {
                const auto total = item.value("bytesTotal").toString().toLongLong();
                if (total > 0) fraction += qBound(0.0, double(item.value("bytesReceived").toString().toLongLong()) / total, 1.0);
            }
            const int i = rowIndices.value(uploadIds_[order], -1);
            if (i >= 0) {
                auto row = rows_[i].toObject();
                if (row.value("status") != label && !(row.value("state") == QLatin1String("retry_wait") && itemState == QLatin1String("retry_wait"))) {
                    const auto detail = item.value("error").toObject();
                    const auto line = row.value("displayName").toString() + "  " + label
                        + (detail.contains("upstreamStatus") ? QStringLiteral(" (HTTP %1)").arg(detail.value("upstreamStatus").toInt()) : QString{});
                    logs_.append(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + line);
                    if (logs_.size() > 1000) logs_.removeFirst();
                }
                row.insert("status", label);
                row.insert("state", itemState);
                rows_[i] = row;
            }
        }
        progress_ = uploadIds_.isEmpty() ? 0 : (completed + fraction) / uploadIds_.size();
        if (waiting && !retryTimer_.isActive()) retryTimer_.start();
        if (!waiting) retryTimer_.stop();
        if (!rows_.isEmpty()) emit dataChanged(index(0), index(rows_.size() - 1), {StateRole});
    }
    if (!JobRegistry::isTerminal(state) && state != QLatin1String("blocked")) { emit changed(); return; }
    if (jobId == loginJob_) {
        loginJob_.clear();
        if (!remember_) password_.clear();
        if (state == QLatin1String("succeeded")) {
            const auto result = job.value("result").toObject();
            accountRef_ = result.value("accountRef").toString();
            net_configuration::update({{"net_upload_remember_credentials", result.value("remembered").toBool()},
                {"net_upload_username", result.value("remembered").toBool()
                    ? QJsonValue(username_) : QJsonValue(QJsonValue::Null)}});
            report(qtTrId("net.ui.logged_in").arg(username_));
            if (job.value("result").toObject().contains("storageError")) report(qtTrId("net.ui.error").arg(job.value("result").toObject().value("storageError").toString()));
            if (uploadAfterLogin_) { uploadAfterLogin_ = false; QTimer::singleShot(0, this, [this] { upload(); }); }
        } else { uploadAfterLogin_ = false; report(qtTrId("net.ui.error").arg(job.value("error").toObject().value("code").toString())); }
    } else if (jobId == scanJob_) {
        scanJob_.clear();
        if (state == QLatin1String("succeeded")) {
            const auto plan = job.value("result").toObject(); planRef_ = plan.value("uploadPlanRef").toString();
            beginResetModel();
            QJsonArray rows = rows_;
            QSet<QString> retained;
            for (const auto& value : rows_) retained.insert(value.toObject().value("itemId").toString());
            for (auto value : plan.value("items").toArray()) {
                auto row = value.toObject(); const auto id = row.value("itemId").toString();
                if (retained.contains(id)) continue;
                row.insert("path", service_->uploadMaterialPath("desktop", planRef_, id));
                rows.append(row);
            }
            rows_ = rows; endResetModel(); report(qtTrId("net.ui.queue_count").arg(rows_.size()));
            for (const auto& value : plan.value("rejectedEntries").toArray()) {
                const auto entry = value.toObject();
                logs_.append(entry.value("displayName").toString() + "  " + qtTrId("net.ui.error").arg(entry.value("reason").toString()));
            }
            while (logs_.size() > 1000) logs_.removeFirst();
        } else report(qtTrId("net.ui.error").arg(job.value("error").toObject().value("code").toString()));
    } else {
        lastUploadJob_ = uploadJob_;
        retryTimer_.stop();
        uploadJob_.clear();
        if (job.value("error").toObject().value("code") == QLatin1String("upstream.auth_required")) accountRef_.clear();
        if (state == QLatin1String("blocked")) call("jobs.cancel", {{"jobId", jobId}});
        const auto counts = job.value("counts").toObject();
        report(qtTrId("net.ui.upload_summary").arg(counts.value("succeeded").toInt()).arg(counts.value("failed").toInt()).arg(counts.value("unknown").toInt()));
    }
    emit changed();
}
}
