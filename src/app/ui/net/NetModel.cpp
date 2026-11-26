#include "app/ui/net/NetModel.h"

#include <QStandardPaths>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUuid>
#include "app/ui/document/DocumentModel.h"
#include "app/services/net/NetConfiguration.h"
#include "media_tools/net/NetClient.h"
#include "media_tools/net/NetQueryRules.h"
#include <algorithm>
#include <utility>

namespace miacode::ui {
namespace {

QString downloadHttpStatus(const QJsonObject& error)
{
    const int status = error.value("upstreamStatus").toInt();
    return status > 0 ? QStringLiteral(" (HTTP %1)").arg(status) : QString{};
}

} // namespace

NetModel::NetModel(NetService* service, QObject* parent) : QAbstractListModel(parent), service_(service)
{
    outputDirectory_ = net_configuration::value("last_net_batch_output_dir").toString();
    if (outputDirectory_.trimmed().isEmpty()) {
        outputDirectory_ = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (outputDirectory_.isEmpty()) outputDirectory_ = QDir::homePath();
    }
    if (service_) {
        auto version = QCoreApplication::applicationVersion();
        if (version.isEmpty()) version = QStringLiteral("unknown");
        api_ = std::make_unique<api::ApiDispatcher>(*service_, version);
        connect(&service_->jobs(), &JobRegistry::changed, this, &NetModel::updateJob);
    }
}

NetModel::~NetModel()
{
    ++suggestionGeneration_;
    if (cancelSuggestions_) cancelSuggestions_();
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
    const auto chartId = chart.value(QStringLiteral("chartId")).toString();
    switch (role) {
    case ChartIdRole: return chartId;
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
    case SelectedRole: return selected_.contains(chartId);
    case StateRole: return rowStates_.value(chartId,
        qtTrId(downloadFolders_.contains(chartId) ? "net.done_folder" : "net.pending_download"));
    case DownloadedRole: return downloadFolders_.contains(chartId);
    case DownloadInProgressRole: {
        const auto state = chartStates_.value(chartId);
        return (state == QLatin1String("running") || state == QLatin1String("retry_wait"))
            && downloading() && downloadChartIds_.contains(chartId);
    }
    default: return {};
    }
}

QHash<int, QByteArray> NetModel::roleNames() const
{
    return {{ChartIdRole, "chartId"}, {TitleRole, "title"}, {ArtistRole, "artist"},
        {DesignerRole, "designer"}, {UploaderRole, "uploader"}, {LevelsRole, "levels"},
        {TagsRole, "tags"}, {SelectedRole, "selected"}, {StateRole, "itemState"},
        {DownloadedRole, "downloaded"}, {DownloadInProgressRole, "downloadInProgress"}};
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
    clearSuggestions();
    auto values = QJsonObject::fromVariantMap(parameters);
    setSort(values.value("sort").toString("title_asc"));
    if (displaySort_.startsWith("status_")) values.insert("sort", "title_asc");
    values.insert(QStringLiteral("providerId"), QStringLiteral("majdata"));
    const auto accepted = call(QStringLiteral("net.queries.create"), values);
    errorText_.clear();
    if (!accepted.ok()) errorText_ = qtTrId("net.ui.error").arg(accepted.error.value(QStringLiteral("code")).toString());
    else queryJobId_ = accepted.result.toObject().value(QStringLiteral("jobId")).toString();
    emit changed();
}

void NetModel::requestSuggestions(const QVariantMap& parameters)
{
    clearSuggestions();
    if (!service_ || working()) return;
    auto values = QJsonObject::fromVariantMap(parameters);
    values.insert(QStringLiteral("providerId"), QStringLiteral("majdata"));
    const auto generation = suggestionGeneration_;
    const QPointer<NetModel> self(this);
    cancelSuggestions_ = service_->suggest(principal_, values, [self, generation](net::NetTaskResult result) {
        if (!self || self->suggestionGeneration_ != generation || !result.error.isEmpty()) return;
        self->suggestions_ = result.result.toArray().toVariantList();
        emit self->suggestionsChanged();
    });
}

void NetModel::clearSuggestions()
{
    ++suggestionGeneration_;
    if (auto cancel = std::exchange(cancelSuggestions_, {})) cancel();
    if (suggestions_.isEmpty()) return;
    suggestions_.clear();
    emit suggestionsChanged();
}

void NetModel::probe()
{
    if (!service_ || working()) return;
    const auto accepted = call(QStringLiteral("net.probes.create"),
        {{QStringLiteral("providerId"), QStringLiteral("majdata")}});
    if (accepted.ok()) {
        probeJobId_ = accepted.result.toObject().value(QStringLiteral("jobId")).toString();
        connectionText_ = qtTrId("net.ui.probing");
    } else connectionText_ = qtTrId("net.ui.error").arg(accepted.error.value(QStringLiteral("code")).toString());
    emit changed();
}

void NetModel::cancelQuery() { if (busy()) call(QStringLiteral("jobs.cancel"), {{QStringLiteral("jobId"), queryJobId_}}); }
void NetModel::cancelProbe() { if (probing()) call(QStringLiteral("jobs.cancel"), {{QStringLiteral("jobId"), probeJobId_}}); }

void NetModel::setSelected(const QString& chartId, bool selected)
{
    if (selected && downloadFolders_.contains(chartId)) return;
    if (selected) selected_.insert(chartId);
    else selected_.remove(chartId);
    if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {SelectedRole});
    emit changed();
}

void NetModel::selectAll(bool selected)
{
    selected_.clear();
    if (selected) {
        for (const auto& value : charts_) {
            const auto chartId = value.toObject().value(QStringLiteral("chartId")).toString();
            if (!downloadFolders_.contains(chartId)) selected_.insert(chartId);
        }
    }
    if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {SelectedRole});
    emit changed();
}

void NetModel::setSort(const QString& sort)
{
    const QStringList allowed{"level_desc", "level_asc", "title_asc", "title_desc", "status_asc", "status_desc"};
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
        for (const auto& level : record.value("levels").toArray()) chart.levels.append(level.toString());
        records.insert(chart.id, record);
        summaries.append(chart);
    }
    net::sortNetCharts(summaries, displaySort_.startsWith("status_") ? QStringLiteral("title_asc") : displaySort_);
    if (displaySort_.startsWith("status_")) {
        const QStringList states{"pending", "running", "retry_wait", "blocked", "succeeded", "failed", "cancelled", "skipped", "outcome_unknown"};
        const bool descending = displaySort_ == QLatin1String("status_desc");
        const auto rank = [&](const QString& chartId) {
            return states.indexOf(chartStates_.value(chartId,
                downloadFolders_.contains(chartId) ? QStringLiteral("succeeded") : QStringLiteral("pending")));
        };
        std::stable_sort(summaries.begin(), summaries.end(), [&](const auto& a, const auto& b) {
            const auto left = rank(a.id);
            const auto right = rank(b.id);
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
                ? "net.ui.connection_slow" : "net.ui.connection_normal");
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
    refreshDownloadFolders();
    statusText_.clear();
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
    refreshDownloadFolders();
    if (displaySort_.startsWith("status_")) applySort();
    if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {DownloadedRole, StateRole, SelectedRole});
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
    QStringList ids;
    for (const auto& value : charts_) {
        const auto id = value.toObject().value("chartId").toString();
        if (selected_.contains(id) && !downloadFolders_.contains(id)) ids.append(id);
    }
    downloadCharts(std::move(ids), includeVideo, createZip);
}

void NetModel::activateDownload(const QString& chartId, bool includeVideo, bool createZip)
{
    const auto path = downloadFolders_.value(chartId);
    if (!path.isEmpty()) {
        const QFileInfo directory(path);
        if (directory.isDir() && !directory.isSymLink() && QDesktopServices::openUrl(QUrl::fromLocalFile(path))) return;
        refreshDownloadFolders();
        errorText_ = qtTrId("net.ui.download_folder_unavailable");
        if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {DownloadedRole, StateRole, SelectedRole});
        emit changed();
        return;
    }
    if (std::any_of(charts_.cbegin(), charts_.cend(), [&chartId](const auto& value) {
        return value.toObject().value(QStringLiteral("chartId")) == chartId;
    })) downloadCharts({chartId}, includeVideo, createZip);
}

void NetModel::refreshDownloadFolders()
{
    QHash<QString, QString> titlesById;
    for (const auto& value : charts_) {
        const auto chart = value.toObject();
        titlesById.insert(chart.value(QStringLiteral("chartId")).toString(), chart.value(QStringLiteral("title")).toString());
    }
    downloadFolders_ = net::findDownloadedChartFolders(outputDirectory_, titlesById);
    for (auto it = downloadFolders_.cbegin(); it != downloadFolders_.cend(); ++it) selected_.remove(it.key());
}

void NetModel::downloadCharts(QStringList ids, bool includeVideo, bool createZip)
{
    if (!service_ || working() || ids.isEmpty()) return;
    if (QDir::isAbsolutePath(outputDirectory_)) QDir().mkpath(outputDirectory_);
    const auto grant = service_->grantDirectory(principal_, outputDirectory_);
    const auto accepted = call("net.downloads.create", {{"providerId", "majdata"}, {"chartIds", QJsonArray::fromStringList(ids)},
        {"destination", QJsonObject{{"kind", "directory_grant"}, {"grantRef", grant}}},
        {"includeVideo", includeVideo}, {"createZip", createZip}, {"conflictPolicy", "rename"}});
    errorText_.clear();
    if (!accepted.ok()) errorText_ = qtTrId("net.ui.error").arg(accepted.error.value("code").toString());
    else {
        downloadJobId_ = accepted.result.toObject().value("jobId").toString();
        downloadChartIds_ = std::move(ids);
        downloadCreatesZip_ = createZip;
        paused_ = false;
        statusText_.clear();
        updateDownload();
        return;
    }
    emit changed();
}

void NetModel::preview(const QString& chartId, bool includeVideo) {
    if (!service_ || !document_ || working()) return;
    document_->requestLeaveCurrentField({});
    const auto identity = document_->documentIdentity();
    previewExpected_ = {{"expectedWorkspaceId", identity.value("workspaceId")},
        {"expectedDocumentOpenGeneration", identity.value("documentOpenGeneration")}, {"expectedRevision", identity.value("revision")}};
    QJsonObject parameters{{"providerId", "majdata"}, {"chartId", chartId}, {"includeVideo", includeVideo}};
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

void NetModel::updateDownload() {
    const auto job = service_->jobs().snapshot(downloadJobId_, principal_);
    const auto items = service_->jobs().items(downloadJobId_, principal_);
    const auto counts = job.value(QStringLiteral("counts")).toObject();
    const int completed = counts.value(QStringLiteral("succeeded")).toInt() + counts.value(QStringLiteral("failed")).toInt();
    progressText_ = QStringLiteral("%1/%2").arg(completed).arg(downloadChartIds_.size());
    progress_ = -1;
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
        if (state == QLatin1String("succeeded")) text = qtTrId("net.done_folder");
        else if (state == QLatin1String("failed")) {
            const auto error = item.value("error").toObject();
            text = qtTrId("net.failed_1").arg(error.value("code").toString()) + downloadHttpStatus(error);
        }
        else if (state == QLatin1String("cancelled")) text = qtTrId("net.download_canceled");
        else if (state == QLatin1String("blocked")) text = qtTrId("net.paused") + downloadHttpStatus(item.value("error").toObject());
        else if (state == QLatin1String("retry_wait")) {
            const auto error = item.value("error").toObject();
            text = qtTrId("net.retrying_1").arg(resourceName);
            const auto code = error.value("code").toString();
            if (!code.isEmpty())
                text += QStringLiteral(" (%1)").arg(code);
            text += downloadHttpStatus(error);
        }
        else if (state == QLatin1String("running")) {
            if (!resourceName.isEmpty()) text = qtTrId("net.downloading_1_2").arg(resourceName);
            else if (phase == QLatin1String("publish")) {
                text = downloadCreatesZip_ ? qtTrId("net.packaging_zip") : qtTrId("net.saving_download");
            } else text = qtTrId("net.preparing_download");
            const auto file = item.value(QStringLiteral("fileProgress")).toObject();
            const auto total = file.value(QStringLiteral("bytesTotal")).toString().toLongLong();
            if (total > 0) {
                const auto bytes = file.value(QStringLiteral("bytesReceived")).toString().toLongLong();
                progress_ = qBound(0.0, static_cast<double>(bytes) / total, 1.0);
            }
        } else text = qtTrId("net.pending_download");
        const auto id = downloadChartIds_[order];
        if (state == QLatin1String("succeeded") && chartStates_.value(id) != state) {
            for (const auto& value : item.value(QStringLiteral("artifacts")).toArray()) {
                const auto artifact = value.toObject();
                if (artifact.value(QStringLiteral("kind")) != QLatin1String("directory")) continue;
                const auto path = service_->directoryArtifactPath(principal_, artifact.value(QStringLiteral("artifactRef")).toString());
                if (!path.isEmpty() && QFileInfo(path).absolutePath() == QFileInfo(outputDirectory_).canonicalFilePath()) {
                    downloadFolders_.insert(id, path);
                    selected_.remove(id);
                }
                break;
            }
        }
        if (chartStates_.value(id) != state) { chartStates_.insert(id, state); stateChanged = true; }
        if (rowStates_.value(id) != text) rowStates_.insert(id, text);
    }
    const auto state = job.value(QStringLiteral("state")).toString();
    const auto error = job.value(QStringLiteral("error")).toObject();
    paused_ = state == QLatin1String("blocked");
    if (paused_) {
        errorText_ = qtTrId("net.ui.error").arg(error.value("code").toString()) + downloadHttpStatus(error);
    }
    if (JobRegistry::isTerminal(state)) {
        QString summary;
        if (state == QLatin1String("cancelled")) summary = qtTrId("net.download_canceled");
        else if (!error.isEmpty()) summary = qtTrId("net.ui.error").arg(error.value("code").toString()) + downloadHttpStatus(error);
        else summary = qtTrId("net.download_complete_1_succeeded_2")
            .arg(counts.value("succeeded").toInt()).arg(counts.value("failed").toInt());
        downloadJobId_.clear();
        paused_ = false;
        progress_ = -1;
        progressText_.clear();
        errorText_.clear();
        emit downloadFinished(summary, state == QLatin1String("failed") || state == QLatin1String("partial"));
    }
    if (stateChanged && displaySort_.startsWith("status_")) applySort();
    if (!charts_.isEmpty()) emit dataChanged(index(0), index(charts_.size() - 1), {StateRole, DownloadedRole, DownloadInProgressRole, SelectedRole});
    emit changed();
}

} // namespace miacode::ui
