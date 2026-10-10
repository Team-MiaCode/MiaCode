#include "app/ui/net/NetModel.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QUuid>
#include "app/ui/document/DocumentModel.h"
#include "app/services/net/NetConfiguration.h"
#include "media_tools/net/NetQueryRules.h"
#include <algorithm>

namespace miacode::ui {

NetModel::NetModel(NetService* service, QObject* parent) : QAbstractListModel(parent), service_(service)
{
    outputDirectory_ = net_configuration::value("last_net_batch_output_dir").toString();
    if (service_) {
        auto version = QCoreApplication::applicationVersion();
        if (version.isEmpty()) version = QStringLiteral("unknown");
        api_ = std::make_unique<api::ApiDispatcher>(*service_, version);
        connect(&service_->jobs(), &JobRegistry::changed, this, &NetModel::updateJob);
    }
}

net::NetCallResult NetModel::call(const QString& operation, const QJsonObject& parameters)
{
    if (!service_ || !api_) return {{}, jobError(QStringLiteral("capability.unavailable"))};
    const auto reply = api_->dispatch({principal_, {QStringLiteral("net.read"), QStringLiteral("jobs.read"),
        QStringLiteral("jobs.control"), QStringLiteral("net.download"), QStringLiteral("files.write"),
        QStringLiteral("net.preview"), QStringLiteral("document.read"), QStringLiteral("document.replace")}, {},
        QUuid::createUuid().toString(QUuid::Id128)}, operation, parameters);
    if (!reply.envelope.value(QStringLiteral("ok")).toBool()) return {{}, reply.envelope.value(QStringLiteral("error")).toObject()};
    return {reply.envelope.value(QStringLiteral("result")), {}};
}

int NetModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : charts_.size(); }

QVariant NetModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= charts_.size()) return {};
    const auto chart = charts_[index.row()].toObject();
    switch (role) {
    case ChartIdRole: return chart.value(QStringLiteral("chartId")).toString();
    case TitleRole: return chart.value(QStringLiteral("title")).toString();
    case ArtistRole: return chart.value(QStringLiteral("artist")).toString();
    case DesignerRole: return chart.value(QStringLiteral("designer")).toString();
    case UploaderRole: return chart.value(QStringLiteral("uploader")).toString();
    case LevelsRole: {
        QStringList levels;
        for (const auto& value : chart.value(QStringLiteral("levels")).toArray()) {
            if (!value.toString().trimmed().isEmpty()) levels.append(value.toString());
        }
        return levels.join(QStringLiteral(" / "));
    }
    case TagsRole: {
        QStringList tags;
        for (const auto& value : chart.value(QStringLiteral("tags")).toArray()) tags.append(value.toString());
        return tags.join(QStringLiteral(", "));
    }
    case UploadedAtRole:
        return QDateTime::fromString(chart.value(QStringLiteral("uploadedAtUtc")).toString(), Qt::ISODateWithMs)
            .toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    case SelectedRole: return selected_.contains(chart.value(QStringLiteral("chartId")).toString());
    case StateRole: return rowStates_.value(chart.value("chartId").toString(), qtTrId("net.pending_download"));
    default: return {};
    }
}

QHash<int, QByteArray> NetModel::roleNames() const
{
    return {{ChartIdRole, "chartId"}, {TitleRole, "title"}, {ArtistRole, "artist"},
        {DesignerRole, "designer"}, {UploaderRole, "uploader"}, {LevelsRole, "levels"},
        {TagsRole, "tags"}, {UploadedAtRole, "uploadedAt"}, {SelectedRole, "selected"}, {StateRole, "itemState"}};
}

int NetModel::selectedCount() const
{
    int count = 0;
    for (const auto& value : charts_) if (selected_.contains(value.toObject().value(QStringLiteral("chartId")).toString())) ++count;
    return count;
}

void NetModel::query(const QVariantMap& parameters)
{
    if (!service_ || working()) return;
    auto values = QJsonObject::fromVariantMap(parameters);
    setSort(values.value("sort").toString("uploaded_desc"));
    if (displaySort_.startsWith("status_")) values.insert("sort", "uploaded_desc");
    for (const auto& key : {QStringLiteral("startDate"), QStringLiteral("endDate")})
        if (values.value(key).toString().isEmpty()) values.remove(key);
    values.insert(QStringLiteral("providerId"), QStringLiteral("majdata"));
    const auto accepted = call(QStringLiteral("net.queries.create"), values);
    appendLog(qtTrId("net.ui.query"));
    errorText_.clear();
    if (!accepted.ok()) errorText_ = qtTrId("net.ui.error").arg(accepted.error.value(QStringLiteral("code")).toString());
    else queryJobId_ = accepted.result.toObject().value(QStringLiteral("jobId")).toString();
    emit changed();
}

void NetModel::probe()
{
    if (!service_ || working()) return;
    const auto accepted = call(QStringLiteral("net.probes.create"),
        {{QStringLiteral("providerId"), QStringLiteral("majdata")}});
    if (accepted.ok()) probeJobId_ = accepted.result.toObject().value(QStringLiteral("jobId")).toString();
    else errorText_ = qtTrId("net.ui.error").arg(accepted.error.value(QStringLiteral("code")).toString());
    emit changed();
}

void NetModel::cancelQuery() { if (busy()) call(QStringLiteral("jobs.cancel"), {{QStringLiteral("jobId"), queryJobId_}}); }
void NetModel::cancelProbe() { if (probing()) call(QStringLiteral("jobs.cancel"), {{QStringLiteral("jobId"), probeJobId_}}); }

void NetModel::setSelected(const QString& chartId, bool selected)
{
    if (selected) selected_.insert(chartId);
    else selected_.remove(chartId);
    if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {SelectedRole});
    emit changed();
}

void NetModel::selectAll(bool selected)
{
    for (const auto& value : charts_) {
        const auto chartId = value.toObject().value(QStringLiteral("chartId")).toString();
        if (selected) selected_.insert(chartId);
        else selected_.remove(chartId);
    }
    if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {SelectedRole});
    emit changed();
}

void NetModel::setSort(const QString& sort)
{
    const QStringList allowed{"uploaded_desc", "uploaded_asc", "level_desc", "level_asc", "title_asc", "title_desc", "status_asc", "status_desc"};
    if (working() || !allowed.contains(sort)) return;
    displaySort_ = sort;
    applySort();
}

void NetModel::applySort()
{
    if (charts_.size() < 2) return;
    QList<net::NetChartSummary> summaries;
    QHash<QString, QJsonObject> records;
    for (const auto& value : charts_) {
        const auto record = value.toObject();
        net::NetChartSummary chart;
        chart.id = record.value("chartId").toString();
        chart.title = record.value("title").toString();
        chart.timestampUtc = QDateTime::fromString(record.value("uploadedAtUtc").toString(), Qt::ISODateWithMs);
        for (const auto& level : record.value("levels").toArray()) chart.levels.append(level.toString());
        records.insert(chart.id, record);
        summaries.append(chart);
    }
    net::sortNetCharts(summaries, displaySort_.startsWith("status_") ? QStringLiteral("title_asc") : displaySort_);
    if (displaySort_.startsWith("status_")) {
        const QStringList states{"pending", "running", "retry_wait", "blocked", "succeeded", "failed", "cancelled", "skipped", "outcome_unknown"};
        const bool descending = displaySort_ == QLatin1String("status_desc");
        std::stable_sort(summaries.begin(), summaries.end(), [&](const auto& a, const auto& b) {
            const int left = states.indexOf(chartStates_.value(a.id, "pending"));
            const int right = states.indexOf(chartStates_.value(b.id, "pending"));
            return descending ? left > right : left < right;
        });
    }
    QJsonArray sorted;
    for (const auto& chart : summaries) sorted.append(records.value(chart.id));
    if (sorted == charts_) return;
    beginResetModel(); charts_ = sorted; endResetModel();
}

void NetModel::updateJob(const QString& jobId)
{
    if (!service_) return;
    if (jobId == downloadJobId_) { updateDownload(); return; }
    if (jobId == previewJobId_) {
        const auto job = service_->jobs().snapshot(jobId, principal_);
        const auto state = job.value("state").toString();
        if (!JobRegistry::isTerminal(state) && state != QLatin1String("blocked")) return;
        previewJobId_.clear();
        if (state == QLatin1String("succeeded") && job.value("kind") == QLatin1String("net.preview_prepare")) {
            const auto handle = job.value("result").toObject();
            previewExpected_.insert("previewRef", handle.value("previewRef"));
            const auto opened = call("net.previews.open", previewExpected_);
            if (opened.ok()) previewJobId_ = opened.result.toObject().value("jobId").toString();
            else errorText_ = qtTrId("net.ui.error").arg(opened.error.value("code").toString());
        } else if (state == QLatin1String("succeeded")) {
            statusText_ = qtTrId("net.online_preview_opened");
            rowStates_.insert(previewChartId_, statusText_);
        } else {
            errorText_ = qtTrId("net.ui.error").arg(job.value("error").toObject().value("code").toString());
            rowStates_.insert(previewChartId_, state == QLatin1String("cancelled") ? qtTrId("net.download_canceled") : errorText_);
            if (state == QLatin1String("blocked")) call("jobs.cancel", {{"jobId", jobId}});
        }
        chartStates_.insert(previewChartId_, previewJobId_.isEmpty() ? state : QStringLiteral("running"));
        if (displaySort_.startsWith("status_")) applySort();
        if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {StateRole});
        appendLog(errorText_.isEmpty() ? statusText_ : errorText_);
        emit changed();
        return;
    }
    if (jobId != queryJobId_ && jobId != probeJobId_) return;
    const auto job = service_->jobs().snapshot(jobId, principal_);
    const QString state = job.value(QStringLiteral("state")).toString();
    if (!JobRegistry::isTerminal(state) && state != QLatin1String("blocked")) { emit changed(); return; }
    if (jobId == queryJobId_) {
        queryJobId_.clear();
        if (state == QLatin1String("succeeded")) loadQuery(job.value(QStringLiteral("result")).toObject().value(QStringLiteral("queryRef")).toString());
        else if (state != QLatin1String("cancelled")) errorText_ = qtTrId("net.ui.error").arg(job.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString());
    } else {
        probeJobId_.clear();
        const auto result = job.value(QStringLiteral("result")).toObject();
        if (state == QLatin1String("succeeded")) {
            connectionText_ = qtTrId(result.value(QStringLiteral("classification")) == QLatin1String("slow")
                ? "net.ui.connection_slow" : "net.ui.connection_normal").arg(result.value(QStringLiteral("elapsedMs")).toInt());
        } else connectionText_ = qtTrId("net.ui.error").arg(job.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString());
    }
    emit changed();
}

void NetModel::loadQuery(const QString& queryRef)
{
    QJsonArray charts;
    QString cursor;
    do {
        const auto page = call(QStringLiteral("net.queries.results"),
            {{QStringLiteral("queryRef"), queryRef}, {QStringLiteral("limit"), 200}, {QStringLiteral("cursor"), cursor}});
        if (!page.ok()) {
            errorText_ = qtTrId("net.ui.error").arg(page.error.value(QStringLiteral("code")).toString());
            return;
        }
        const auto result = page.result.toObject();
        for (const auto& value : result.value(QStringLiteral("items")).toArray()) charts.append(value);
        cursor = result.value(QStringLiteral("nextCursor")).toString();
    } while (!cursor.isEmpty());
    beginResetModel();
    charts_ = std::move(charts);
    selected_.clear();
    rowStates_.clear();
    chartStates_.clear();
    for (const auto& value : charts_) selected_.insert(value.toObject().value("chartId").toString());
    statusText_ = qtTrId("net.ui.result_count").arg(charts_.size()).arg(selected_.size());
    appendLog(statusText_);
    endResetModel();
    applySort();
}

void NetModel::setHost(DocumentModel* document, UiRequestService* requests) {
    document_ = document;
    requests_ = requests;
}

void NetModel::setOutputDirectory(const QString& path) {
    const QString normalized = path.trimmed().isEmpty() ? QString{}
        : QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (normalized == outputDirectory_) return;
    outputDirectory_ = normalized;
    net_configuration::update({{"last_net_batch_output_dir", normalized}});
    emit changed();
}

void NetModel::browseOutputDirectory() {
    if (!requests_ || working()) return;
    FileRequest request;
    request.title = localizedText("net.output_directory");
    request.selectFolder = true;
    request.startPath = outputDirectory_;
    QPointer<NetModel> self(this);
    requests_->requestFile(request, [self](const QString& path) { if (self && !path.isEmpty()) self->setOutputDirectory(path); });
}

void NetModel::downloadSelected(bool includeVideo, bool createZip) {
    if (!service_ || working() || !selectedCount()) return;
    QJsonArray ids;
    downloadChartIds_.clear();
    for (const auto& value : charts_) {
        const auto id = value.toObject().value("chartId").toString();
        if (selected_.contains(id)) { ids.append(id); downloadChartIds_.append(id); }
    }
    if (QDir::isAbsolutePath(outputDirectory_)) QDir().mkpath(outputDirectory_);
    const auto grant = service_->grantDirectory(principal_, outputDirectory_);
    const auto accepted = call("net.downloads.create", {{"providerId", "majdata"}, {"chartIds", ids},
        {"destination", QJsonObject{{"kind", "directory_grant"}, {"grantRef", grant}}},
        {"includeVideo", includeVideo}, {"createZip", createZip}});
    errorText_.clear();
    if (!accepted.ok()) errorText_ = qtTrId("net.ui.error").arg(accepted.error.value("code").toString());
    else {
        downloadJobId_ = accepted.result.toObject().value("jobId").toString();
        progress_ = 0;
        progressText_.clear();
        progressResource_.clear();
        paused_ = false;
        appendLog(qtTrId("net.start_download_queue_selected_1").arg(ids.size()));
    }
    emit changed();
}

void NetModel::preview(const QString& chartId) {
    if (!service_ || !document_ || working()) return;
    document_->requestLeaveCurrentField({});
    const auto identity = document_->documentIdentity();
    previewExpected_ = {{"expectedWorkspaceId", identity.value("workspaceId")},
        {"expectedDocumentOpenGeneration", identity.value("documentOpenGeneration")}, {"expectedRevision", identity.value("revision")}};
    QJsonObject parameters{{"providerId", "majdata"}, {"chartId", chartId}};
    for (const auto& chart : charts_) if (chart.toObject().value("chartId") == chartId)
        if (chart.toObject().value("remoteVersion").isString()) parameters.insert("remoteVersion", chart.toObject().value("remoteVersion"));
    const auto accepted = call("net.previews.prepare", parameters);
    errorText_.clear();
    if (!accepted.ok()) errorText_ = qtTrId("net.ui.error").arg(accepted.error.value("code").toString());
    else {
        previewChartId_ = chartId;
        previewJobId_ = accepted.result.toObject().value("jobId").toString();
        rowStates_.insert(chartId, qtTrId("net.online_preview_loading"));
        statusText_ = qtTrId("net.online_preview_loading");
        appendLog(statusText_);
        emit dataChanged(index(0), index(charts_.size() - 1), {StateRole});
    }
    emit changed();
}

void NetModel::cancelTask() {
    for (const auto& id : {queryJobId_, probeJobId_, downloadJobId_, previewJobId_})
        if (!id.isEmpty()) call("jobs.cancel", {{"jobId", id}});
}

void NetModel::resumeDownload() {
    if (!service_ || !paused_) return;
    const auto job = service_->jobs().snapshot(downloadJobId_, principal_);
    const auto resumed = service_->call(principal_, "jobs.resume", {{"jobId", downloadJobId_}, {"expectedJobVersion", job.value("version")}});
    if (!resumed.ok()) errorText_ = qtTrId("net.ui.error").arg(resumed.error.value("code").toString());
    else { paused_ = false; errorText_.clear(); }
    emit changed();
}

void NetModel::appendLog(const QString& text) {
    if (text.isEmpty()) return;
    logs_.append(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text);
    if (logs_.size() > 1000) logs_.removeFirst();
}

void NetModel::updateDownload() {
    const auto job = service_->jobs().snapshot(downloadJobId_, principal_);
    const auto items = service_->jobs().items(downloadJobId_, principal_);
    int completed = 0;
    double fraction = 0;
    bool stateChanged = false;
    for (const auto& value : items) {
        const auto item = value.toObject();
        const int order = item.value("inputOrder").toInt();
        if (order < 0 || order >= downloadChartIds_.size()) continue;
        const auto state = item.value("state").toString();
        const auto phase = item.value("phase").toString();
        const auto resourceName = phase == QLatin1String("track") ? QStringLiteral("track.mp3")
            : phase == QLatin1String("image") ? QStringLiteral("bg.jpg") : phase == QLatin1String("chart") ? QStringLiteral("maidata.txt")
            : phase == QLatin1String("video") ? QStringLiteral("pv.mp4") : QString{};
        QString text;
        if (state == QLatin1String("succeeded")) { text = qtTrId("net.done_folder"); ++completed; }
        else if (state == QLatin1String("failed")) { text = qtTrId("net.failed_1").arg(item.value("error").toObject().value("code").toString()); ++completed; }
        else if (state == QLatin1String("cancelled")) { text = qtTrId("net.download_canceled"); ++completed; }
        else if (state == QLatin1String("blocked")) text = qtTrId("net.paused");
        else if (state == QLatin1String("retry_wait")) text = qtTrId("net.retrying_1").arg(resourceName);
        else if (state == QLatin1String("running")) {
            text = resourceName.isEmpty() ? qtTrId("net.packaging_zip") : qtTrId("net.downloading_1_2").arg(resourceName);
            const auto bytes = item.value("bytesReceived").toString().toLongLong();
            const auto total = item.value("bytesTotal").toString().toLongLong();
            const QString resourceKey = item.value("itemId").toString() + ':' + phase + ':' + QString::number(item.value("attempt").toInt());
            if (progressResource_ != resourceKey) { progressResource_ = resourceKey; resourceTimer_.start(); }
            progressText_ = total > 0 ? QStringLiteral("%1 / %2 MiB").arg(bytes / 1048576.0, 0, 'f', 1).arg(total / 1048576.0, 0, 'f', 1)
                : QStringLiteral("%1 MiB").arg(bytes / 1048576.0, 0, 'f', 1);
            const auto elapsed = resourceTimer_.elapsed();
            if (elapsed > 0 && !resourceName.isEmpty()) progressText_ += QStringLiteral(" · %1 MiB/s").arg(bytes * 1000.0 / elapsed / 1048576.0, 0, 'f', 2);
            const int resource = QStringList{"track", "image", "chart", "video", "publish"}.indexOf(phase);
            fraction = (qMax(0, resource) + (total > 0 ? qBound(0.0, double(bytes) / total, 1.0) : 0.0)) / 5.0;
        } else text = qtTrId("net.pending_download");
        const auto id = downloadChartIds_[order];
        if (chartStates_.value(id) != state) { chartStates_.insert(id, state); stateChanged = true; }
        if (rowStates_.value(id) != text) { rowStates_.insert(id, text); appendLog(id + "  " + text); }
    }
    progress_ = items.isEmpty() ? 0 : (completed + fraction) / items.size();
    paused_ = job.value("state") == QLatin1String("blocked");
    if (paused_) errorText_ = qtTrId("net.ui.error").arg(job.value("error").toObject().value("code").toString());
    if (JobRegistry::isTerminal(job.value("state").toString())) {
        const auto counts = job.value("counts").toObject();
        statusText_ = qtTrId("net.download_complete_1_succeeded_2").arg(counts.value("succeeded").toInt()).arg(counts.value("failed").toInt());
        if (job.value("state") == QLatin1String("cancelled")) statusText_ = qtTrId("net.download_canceled");
        appendLog(statusText_);
        downloadJobId_.clear();
        paused_ = false;
        progressText_.clear();
    }
    if (stateChanged && displaySort_.startsWith("status_")) applySort();
    if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {StateRole});
    emit changed();
}

} // namespace miacode::ui
