#include "media_tools/net/NetProvider.h"
#include "media_tools/net/NetClient.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QtConcurrentRun>
#include <array>
#include <atomic>

namespace miacode::net {
namespace {
QString ref(const char* prefix) { return QString::fromLatin1(prefix) + QUuid::createUuid().toString(QUuid::Id128); }
QJsonObject error(const QString& code) {
    return {{"code", code}, {"message", code}, {"retryable", code == QLatin1String("upstream.network")}};
}
QString expires() { return QDateTime::currentDateTimeUtc().addSecs(3600).toString(Qt::ISODateWithMs); }
struct Resource { QString kind; QString endpoint; QString name; };
enum ResourceIndex { Track, Image, Chart, Video };
const QList<Resource> resources{{"track", "track", "track.mp3"}, {"image", "image?fullImage=true", "bg.jpg"},
    {"chart", "chart", "maidata.txt"}, {"video", "video", "pv.mp4"}};
constexpr int kDownloadConcurrency = 2;
constexpr int kDownloadFirstByteTimeoutMs = 15000;
constexpr int kDownloadIdleTimeoutMs = 10000;
constexpr std::array kVideoDownloadOrder{Track, Video, Image, Chart};
struct ResourceTransfer {
    NetTransportPort::Cancel cancel;
    QJsonObject entry;
    QJsonObject retryError;
    qint64 received = 0;
    qint64 total = -1;
    qint64 lastLoggedProgressMs = -1;
    int attempt = 0;
};
struct Publication { QJsonObject manifest; QString directory; QString zip; QJsonObject failure; };

bool pathOccupied(const QString& path)
{
    const QFileInfo entry(path);
    return entry.exists() || entry.isSymLink();
}
}

class NetResourceOperation final : public QObject {
public:
    NetResourceOperation(NetProvider& provider, NetTaskRequest request, NetEnginePort::Event event, NetEnginePort::Done done)
        : QObject(&provider), provider_(provider), request_(std::move(request)), event_(std::move(event)), done_(std::move(done)),
          cancelled_(std::make_shared<std::atomic_bool>(false)) {
        connect(&worker_, &QFutureWatcher<Publication>::finished, this, &NetResourceOperation::publicationFinished);
    }
    ~NetResourceOperation() override {
        done_ = {};
        cancelled_->store(true);
        cancelTransfers();
        if (worker_.isRunning()) worker_.waitForFinished();
    }
    void start() {
        if (!done_) return;
        if (cancelled_->load()) { finish(error("job.cancelled")); return; }
        preview_ = request_.operation == QLatin1String("net.previews.prepare");
        const auto previous = request_.parameters.value("previousResult").toObject();
        manifests_ = previous.value("manifests").toArray();
        directories_ = previous.value("directoryArtifacts").toArray();
        zips_ = previous.value("zipArtifacts").toArray();
        succeeded_ = previous.value("counts").toObject().value("succeeded").toInt();
        failed_ = previous.value("counts").toObject().value("failed").toInt();
        if (request_.parameters.value("providerId") != QLatin1String("majdata")) { finish(error("capability.unavailable")); return; }
        if (preview_) {
            ids_.append(request_.parameters.value("chartId"));
            cacheKey_ = request_.principal + QChar(0) + ids_.first().toString() + QChar(0) + request_.parameters.value("remoteVersion").toString()
                + QChar(0) + (request_.parameters.value("includeVideo").toBool(true) ? QStringLiteral("video") : QStringLiteral("audio"));
            const auto cached = provider_.previews_.value(provider_.previewCache_.value(cacheKey_));
            if (!request_.parameters.value("forceRefresh").toBool() && cached.expiresAt > provider_.clock_.elapsed()
                && cacheComplete(cached)) { finish({}, cached.handle); return; }
            root_ = provider_.previewRoot_->path();
        } else {
            ids_ = request_.parameters.value("chartIds").toArray();
            const auto destination = request_.parameters.value("destination").toObject();
            if (destination.value("kind") == QLatin1String("managed")) {
                root_ = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/net-downloads");
            } else {
                const auto grant = provider_.directoryGrants_.value(destination.value("grantRef").toString());
                if (grant.principal != request_.principal) { finish(error("permission.denied")); return; }
                root_ = grant.path;
            }
        }
        if (root_.isEmpty() || !QDir().mkpath(root_)) { finish(error("file.write_failed")); return; }
        nextChart();
    }
    void cancel() {
        if (!done_) return;
        cancelled_->store(true);
        if (!publishing_) stopChart(error("job.cancelled"));
    }
private:
    void logDownload(const QString& event, QJsonObject details = {}) const {
        if (preview_ || !debug_options::runtimeDebugOutputEnabled()) return;
        details.insert("event", event);
        details.insert("chartId", ids_.at(chartIndex_));
        details.insert("item", chartIndex_ + 1);
        details.insert("total", ids_.size());
        debug_log::appendLine(debug_log::Channel::Runtime, QStringLiteral("net/download"),
            QString::fromUtf8(QJsonDocument(details).toJson(QJsonDocument::Compact)));
    }
    bool cacheComplete(const NetProvider::Preview& preview) const {
        for (const auto& value : preview.handle.value("manifest").toObject().value("resources").toArray()) {
            const auto resource = value.toObject();
            if (resource.value("state") != QLatin1String("present")) continue;
            const QFileInfo file(QDir(QFileInfo(preview.path).absolutePath()).filePath(resource.value("relativeName").toString()));
            if (!file.isFile() || file.size() != resource.value("bytes").toString().toLongLong()) return false;
        }
        return QFileInfo::exists(preview.path);
    }
    void emitItem(const QString& state, const QString& phase, QJsonObject failure = {}, QJsonObject progress = {}, QJsonArray artifacts = {}) {
        if (event_ && !preview_) event_({chartIndex_, state, phase, failure, progress, artifacts});
    }
    void finish(QJsonObject failure = {}, QJsonValue result = QJsonValue::Null, bool blocked = false) {
        if (!done_) return;
        if (!preview_ && result.isNull()) {
            QJsonObject counts{{"total", ids_.size()}, {"succeeded", succeeded_}, {"failed", failed_},
                {"pending", ids_.size() - succeeded_ - failed_}, {"cancelled", 0}, {"skipped", 0}, {"unknown", 0}};
            result = QJsonObject{{"itemIds", QJsonArray::fromStringList(request_.itemIds)}, {"counts", counts},
                {"manifests", manifests_}, {"directoryArtifacts", directories_}, {"zipArtifacts", zips_}};
        }
        auto done = std::move(done_);
        done({result, failure, blocked});
        deleteLater();
    }
    void nextChart() {
        if (!done_ || cancelled_->load()) { finish(error("job.cancelled")); return; }
        ++chartIndex_;
        if (chartIndex_ >= ids_.size()) {
            QJsonObject counts{{"total", ids_.size()}, {"succeeded", succeeded_}, {"failed", failed_}, {"pending", 0},
                {"cancelled", 0}, {"skipped", 0}, {"unknown", 0}};
            finish({}, QJsonObject{{"itemIds", QJsonArray::fromStringList(request_.itemIds)}, {"counts", counts},
                {"manifests", manifests_}, {"directoryArtifacts", directories_}, {"zipArtifacts", zips_}});
            return;
        }
        if (request_.parameters.contains("pendingIndices") && !request_.parameters.value("pendingIndices").toArray().contains(chartIndex_)) {
            nextChart();
            return;
        }
        chart_ = {};
        for (const auto& query : provider_.queries_) {
            if (query.principal != request_.principal) continue;
            for (const auto& value : query.charts) if (value.toObject().value("chartId") == ids_[chartIndex_]) chart_ = value.toObject();
        }
        if (chart_.isEmpty()) chart_ = {{"chartId", ids_[chartIndex_]}, {"title", ids_[chartIndex_]}, {"remoteVersion", QJsonValue::Null}};
        if (preview_ && request_.parameters.contains("remoteVersion")) chart_.insert("remoteVersion", request_.parameters.value("remoteVersion"));
        staging_ = std::make_shared<QTemporaryDir>(QDir(root_).filePath(QStringLiteral(".miacode-net-XXXXXX")));
        if (!staging_->isValid()) { chartFailed(error("file.write_failed")); return; }
        const int count = request_.parameters.value("includeVideo").toBool(true) ? resources.size() : resources.size() - 1;
        transfers_ = QList<ResourceTransfer>(count);
        stopping_ = false;
        nextResource_ = 0;
        firstUnfinishedResource_ = 0;
        activeSlots_ = 0;
        emitItem("running", "download");
        fetchAvailable();
    }
    void chartFailed(QJsonObject failure) {
        emitItem("failed", "download", failure);
        ++failed_;
        staging_.reset();
        if (preview_) finish(failure);
        else QTimer::singleShot(0, this, [this] { nextChart(); });
    }
    void cancelTransfers() {
        for (auto& transfer : transfers_) {
            auto cancel = std::move(transfer.cancel);
            if (cancel) cancel();
        }
    }
    void stopChart(QJsonObject failure, QString blockedResource = {}) {
        if (stopping_) return;
        stopping_ = true;
        stopError_ = std::move(failure);
        blockedResource_ = std::move(blockedResource);
        // A progress handler can cancel synchronously while the reply is being read.
        QTimer::singleShot(0, this, [this] {
            cancelTransfers();
            cancelsIssued_ = true;
            finishStoppedChart();
        });
    }
    void finishStoppedChart() {
        if (!cancelsIssued_ || inFlight_ != 0) return;
        cancelsIssued_ = false;
        auto failure = std::move(stopError_);
        const auto blockedResource = std::move(blockedResource_);
        if (cancelled_->load()) {
            staging_.reset();
            finish(error("job.cancelled"));
        } else if (!blockedResource.isEmpty()) {
            staging_.reset();
            emitItem("blocked", blockedResource, failure);
            finish(failure, QJsonValue::Null, true);
        } else {
            chartFailed(failure);
        }
    }
    void fetchAvailable() {
        const int concurrency = preview_ ? 1 : kDownloadConcurrency;
        const bool prioritizeVideo = !preview_ && transfers_.size() == resources.size();
        while (done_ && !stopping_ && !cancelled_->load() && activeSlots_ < concurrency && nextResource_ < transfers_.size()) {
            const int slot = nextResource_++;
            ++activeSlots_;
            fetchResource(prioritizeVideo ? static_cast<int>(kVideoDownloadOrder[slot]) : slot);
        }
    }
    void reportResourceProgress() {
        if (preview_) return;
        qint64 received = 0;
        qint64 total = 0;
        bool totalKnown = true;
        int displayIndex = firstUnfinishedResource_;
        for (int index = 0; index < transfers_.size(); ++index) {
            const auto& transfer = transfers_[index];
            received += transfer.received;
            if (transfer.total < 0) totalKnown = false;
            else total += transfer.total;
            const auto& displayed = transfers_[displayIndex];
            if (transfer.entry.isEmpty() && transfer.attempt > 0 && transfer.retryError.isEmpty()
                && (displayed.attempt == 0 || !displayed.retryError.isEmpty())) {
                displayIndex = index;
            }
        }
        const auto& displayed = transfers_[displayIndex];
        const QJsonObject progress{{"bytesReceived", QString::number(received)},
            {"bytesTotal", totalKnown ? QJsonValue(QString::number(total)) : QJsonValue::Null},
            {"attempt", displayed.attempt}};
        emitItem(displayed.retryError.isEmpty() ? "running" : "retry_wait", resources[displayIndex].kind, displayed.retryError, progress);
    }
    void fetchResource(int index) {
        const auto& resource = resources[index];
        NetHttpRequest http;
        http.url = QUrl(provider_.baseUrl_.toString() + QStringLiteral("/maichart/")
            + QString::fromLatin1(QUrl::toPercentEncoding(ids_[chartIndex_].toString())) + '/' + resource.endpoint);
        http.outputPath = QDir(staging_->path()).filePath(resource.name);
        auto& transfer = transfers_[index];
        if (!preview_) {
            http.firstByteTimeoutMs = kDownloadFirstByteTimeoutMs;
            http.timeoutMs = kDownloadIdleTimeoutMs;
            const auto lastError = transfer.retryError.value("code").toString();
            http.freshConnection = lastError == QLatin1String("upstream.timeout") || lastError == QLatin1String("upstream.network");
        }
        http.maximumBytes = resource.kind == QLatin1String("chart") ? 16LL * 1024 * 1024 : 4LL * 1024 * 1024 * 1024;
        http.allowDeclaredEmptyChart = resource.kind == QLatin1String("chart")
            && request_.parameters.value("emptyChartPolicy").toString("allow_declared_empty") == QLatin1String("allow_declared_empty");
        http.headers = {{"Referer", "https://majdata.net/"}};
        ++transfer.attempt;
        transfer.retryError = {};
        transfer.received = 0;
        transfer.total = -1;
        transfer.lastLoggedProgressMs = -1;
        reportResourceProgress();
        if (stopping_ || cancelled_->load()) return;
        logDownload("request_started", {{"resource", resource.kind}, {"attempt", transfer.attempt},
            {"freshConnection", http.freshConnection}});
        ++inFlight_;
        QPointer<NetResourceOperation> self(this);
        transfer.cancel = provider_.transport_.request(http, [self, index](NetHttpResponse response) {
            if (self && self->done_) self->receive(index, std::move(response));
        }, [self, index](qint64 received, qint64 total) {
            if (!self || !self->done_ || self->stopping_) return;
            auto& transfer = self->transfers_[index];
            transfer.received = received;
            transfer.total = total;
            if (!self->preview_ && debug_options::runtimeDebugOutputEnabled()) {
                const auto now = self->provider_.clock_.elapsed();
                if (transfer.lastLoggedProgressMs < 0 || now - transfer.lastLoggedProgressMs >= 5000) {
                    transfer.lastLoggedProgressMs = now;
                    self->logDownload("request_progress", {{"resource", resources[index].kind}, {"attempt", transfer.attempt},
                        {"bytes", received}, {"bytesTotal", total}});
                }
            }
            self->reportResourceProgress();
        });
    }
    void receive(int index, NetHttpResponse response) {
        auto& transfer = transfers_[index];
        transfer.cancel = {};
        --inFlight_;
        const auto& resource = resources[index];
        const auto code = response.error.value("code").toString();
        logDownload("request_finished", {{"resource", resource.kind}, {"attempt", transfer.attempt},
            {"status", response.status}, {"error", code}, {"bytes", response.bytes},
            {"elapsedMs", response.elapsedMs}, {"http2", response.http2}});
        const bool blocked = code == QLatin1String("upstream.blocked") || code == QLatin1String("upstream.rate_limited");
        if (stopping_) {
            if (blocked && blockedResource_.isEmpty()) {
                stopError_ = std::move(response.error);
                blockedResource_ = resource.kind;
            }
            finishStoppedChart();
            return;
        }
        if (response.cancelled) { cancel(); return; }
        if (blocked) {
            stopChart(response.error, resource.kind);
            return;
        }
        if (resource.kind == QLatin1String("video") && response.status == 404) {
            transfer.entry = {{"kind", resource.kind}, {"state", "absent"}};
            transfer.received = 0;
            transfer.total = 0;
        } else if (!response.error.isEmpty()) {
            const bool timedOut = code == QLatin1String("upstream.timeout");
            const int maxAttempts = !preview_ && timedOut ? 2 : 3;
            if (transfer.attempt < maxAttempts && (code == QLatin1String("upstream.network") || timedOut || response.status >= 500)) {
                transfer.retryError = response.error;
                reportResourceProgress();
                QTimer::singleShot(800, this, [this, index, chart = chartIndex_] {
                    if (done_ && !stopping_ && !cancelled_->load() && chart == chartIndex_) fetchResource(index);
                });
                return;
            }
            stopChart(response.error);
            return;
        } else {
            transfer.entry = {{"kind", resource.kind}, {"state", "present"}, {"relativeName", resource.name},
                {"bytes", QString::number(response.bytes)}, {"contentType", response.contentType.isEmpty()
                    ? QStringLiteral("application/octet-stream") : QString::fromLatin1(response.contentType)},
                {"acceptedDeclaredEmpty", response.declaredEmpty}};
            transfer.received = response.bytes;
            transfer.total = response.bytes;
        }
        --activeSlots_;
        if (index == firstUnfinishedResource_) {
            while (firstUnfinishedResource_ < transfers_.size() && !transfers_[firstUnfinishedResource_].entry.isEmpty())
                ++firstUnfinishedResource_;
            if (firstUnfinishedResource_ == transfers_.size()) { publish(); return; }
        }
        reportResourceProgress();
        fetchAvailable();
    }
    void publish() {
        publishing_ = true;
        logDownload("save_started");
        emitItem("running", "publish");
        const auto staging = staging_;
        const auto cancelled = cancelled_;
        const auto parameters = request_.parameters;
        const QString destination = preview_ ? QDir(root_).filePath(ref("preview_"))
            : chartDirectoryPathForTitle(root_, chart_.value("title").toString(), ids_[chartIndex_].toString());
        QJsonArray entries;
        for (const auto& transfer : transfers_) entries.append(transfer.entry);
        QJsonObject manifest{{"providerId", "majdata"}, {"chartId", ids_[chartIndex_]},
            {"remoteVersion", chart_.value("remoteVersion")}, {"resources", entries}};
        const bool zip = !preview_ && parameters.value("createZip").toBool();
        worker_.setFuture(QtConcurrent::run([staging, cancelled, parameters, destination, manifest, zip]() mutable {
            Publication result;
            auto entries = manifest.value("resources").toArray();
            for (int i = 0; i < entries.size(); ++i) {
                auto entry = entries[i].toObject();
                if (entry.value("state") != QLatin1String("present")) continue;
                QFile file(QDir(staging->path()).filePath(entry.value("relativeName").toString()));
                QCryptographicHash hash(QCryptographicHash::Sha256);
                if (!file.open(QIODevice::ReadOnly)) { result.failure = error("file.read_failed"); return result; }
                while (!file.atEnd()) {
                    if (cancelled->load()) { result.failure = error("job.cancelled"); return result; }
                    const auto bytes = file.read(64 * 1024);
                    if (bytes.isEmpty() && file.error() != QFileDevice::NoError) { result.failure = error("file.read_failed"); return result; }
                    hash.addData(bytes);
                }
                entry.insert("sha256", QString::fromLatin1(hash.result().toHex()));
                entries[i] = entry;
            }
            manifest.insert("resources", entries);
            QSaveFile manifestFile(QDir(staging->path()).filePath("manifest.json"));
            const auto bytes = QJsonDocument(manifest).toJson();
            if (!manifestFile.open(QIODevice::WriteOnly) || manifestFile.write(bytes) != bytes.size() || !manifestFile.commit()) {
                result.failure = error("file.write_failed"); return result;
            }
            if (zip) {
                QString detail;
                if (!packNetChartFolderZip(staging->path(), QDir(staging->path()).filePath("download.zip"), nullptr, &detail,
                    parameters.value("zipMode").toString() != QLatin1String("legacy_triplet"))) {
                    result.failure = error("file.write_failed"); return result;
                }
            }
            if (cancelled->load()) { result.failure = error("job.cancelled"); return result; }
            const auto conflictPolicy = parameters.value("conflictPolicy").toString();
            QString publishedPath = destination;
            QString previous;
            if (conflictPolicy == QLatin1String("rename")) {
                int suffix = 2;
                for (;;) {
                    if (cancelled->load()) { result.failure = error("job.cancelled"); return result; }
                    if (!pathOccupied(publishedPath)) {
                        if (QDir().rename(staging->path(), publishedPath)) break;
                        if (!pathOccupied(publishedPath)) { result.failure = error("file.write_failed"); return result; }
                    }
                    publishedPath = QStringLiteral("%1 (%2)").arg(destination).arg(suffix++);
                }
            } else {
                if (pathOccupied(destination)) {
                    if (conflictPolicy != QLatin1String("replace")) { result.failure = error("file.conflict"); return result; }
                    previous = destination + '.' + ref("previous_");
                    if (!QDir().rename(destination, previous)) { result.failure = error("file.write_failed"); return result; }
                }
                if (!QDir().rename(staging->path(), destination)) {
                    if (!previous.isEmpty()) QDir().rename(previous, destination);
                    result.failure = error("file.write_failed"); return result;
                }
            }
            staging->setAutoRemove(false);
            if (!previous.isEmpty()) QDir(previous).removeRecursively();
            result.directory = publishedPath;
            result.manifest = manifest;
            if (zip) result.zip = QDir(publishedPath).filePath("download.zip");
            return result;
        }));
    }
    void publicationFinished() {
        publishing_ = false;
        const auto result = worker_.result();
        logDownload("save_finished", {{"error", result.failure.value("code")}});
        if (result.failure.value("code") == QLatin1String("job.cancelled")) {
            staging_.reset();
            finish(result.failure);
            return;
        }
        if (!result.failure.isEmpty()) { chartFailed(result.failure); return; }
        if (preview_) {
            const QString previewRef = ref("preview_");
            QJsonObject handle{{"previewRef", previewRef}, {"manifest", result.manifest}, {"expiresAt", expires()}};
            provider_.previews_.insert(previewRef, {request_.principal, QDir(result.directory).filePath("maidata.txt"), handle, provider_.clock_.elapsed() + 3600000});
            provider_.previewCache_.insert(cacheKey_, previewRef);
            finish({}, handle);
            return;
        }
        QJsonArray artifacts{provider_.registerArtifact(request_.principal, result.directory, true)};
        directories_.append(artifacts.first());
        if (!result.zip.isEmpty()) {
            const auto artifact = provider_.registerArtifact(request_.principal, result.zip, false);
            artifacts.append(artifact);
            zips_.append(artifact);
        }
        manifests_.append(result.manifest);
        ++succeeded_;
        emitItem("succeeded", "complete", {}, {}, artifacts);
        staging_.reset();
        if (cancelled_->load()) finish(error("job.cancelled"));
        else QTimer::singleShot(250, this, [this] { nextChart(); });
    }
    NetProvider& provider_;
    NetTaskRequest request_;
    NetEnginePort::Event event_;
    NetEnginePort::Done done_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    std::shared_ptr<QTemporaryDir> staging_;
    QFutureWatcher<Publication> worker_;
    QList<ResourceTransfer> transfers_;
    QJsonArray ids_, manifests_, directories_, zips_;
    QJsonObject chart_, stopError_;
    QString root_, cacheKey_, blockedResource_;
    int chartIndex_ = -1, succeeded_ = 0, failed_ = 0;
    int nextResource_ = 0, firstUnfinishedResource_ = 0, activeSlots_ = 0, inFlight_ = 0;
    bool stopping_ = false, cancelsIssued_ = false, publishing_ = false;
    bool preview_ = false;
};

NetEnginePort::Cancel NetProvider::executeResources(NetTaskRequest request, Event event, Done done) {
    const auto started = std::make_shared<bool>(false);
    const auto completed = std::make_shared<bool>(false);
    auto* operation = new NetResourceOperation(*this, std::move(request), std::move(event),
        [this, started, completed, done = std::move(done)](NetTaskResult result) {
            *completed = true;
            done(std::move(result));
            if (*started) { resourceActive_ = false; startNextResource(); }
        });
    QPointer<NetResourceOperation> weak(operation);
    resourceQueue_.enqueue([this, weak, started, completed] {
        if (*completed || !weak) { resourceActive_ = false; startNextResource(); return; }
        *started = true;
        weak->start();
    });
    startNextResource();
    return [weak] { if (weak) weak->cancel(); };
}
void NetProvider::startNextResource() {
    if (resourceActive_ || resourceQueue_.isEmpty()) return;
    resourceActive_ = true;
    QTimer::singleShot(0, this, resourceQueue_.dequeue());
}
QString NetProvider::grantDirectory(const QString& principal, const QString& path) {
    const QFileInfo directory(QDir::fromNativeSeparators(path.trimmed()));
    if (principal.isEmpty() || !directory.isAbsolute() || !directory.isDir()) return {};
    const QString grant = ref("directory_");
    directoryGrants_.insert(grant, {principal, directory.canonicalFilePath()});
    return grant;
}
QString NetProvider::previewPath(const QString& principal, const QString& reference) const {
    const auto found = previews_.constFind(reference);
    return found != previews_.cend() && found->principal == principal && found->expiresAt > clock_.elapsed() ? found->path : QString{};
}
QJsonObject NetProvider::registerArtifact(const QString& principal, const QString& path, bool directory) {
    const QString artifact = ref("artifact_");
    artifacts_.insert(artifact, {principal, path});
    return {{"artifactRef", artifact}, {"kind", directory ? "directory" : "file"},
        {"displayName", QFileInfo(path).fileName()}, {"contentType", directory ? QJsonValue::Null : QJsonValue("application/zip")},
        {"bytes", directory ? QJsonValue::Null : QJsonValue(QString::number(QFileInfo(path).size()))}, {"expiresAt", expires()}};
}
} // namespace miacode::net
