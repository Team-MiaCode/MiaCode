#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <functional>

namespace miacode {

struct JobToken {
    QString jobId;
    quint64 epoch = 0;
    bool isValid() const { return !jobId.isEmpty() && epoch != 0; }
};

// Owns task identity and observable state. UI progress is a projection of this registry.
class JobRegistry final : public QObject {
    Q_OBJECT
public:
    explicit JobRegistry(QObject* parent = nullptr);
    JobToken create(const QString& principal, const QString& kind,
                    const QJsonArray& inputs = {}, const QString& parentJobId = {});
    QJsonObject snapshot(const QString& jobId, const QString& principal) const;
    QJsonArray list(const QString& principal) const;
    QJsonArray items(const QString& jobId, const QString& principal) const;
    QJsonObject events(const QString& jobId, const QString& principal,
                       quint64 after = 0, int limit = 100) const;
    QStringList itemIds(const JobToken& token) const;
    bool setState(const JobToken& token, const QString& state, const QString& phase,
                  const QJsonObject& error = {});
    bool updateItem(const JobToken& token, const QString& itemId, const QString& state,
                    const QString& phase, const QJsonObject& error = {},
                    const QJsonObject& progress = {}, const QJsonArray& artifacts = {});
    bool finish(const JobToken& token, const QJsonValue& result = QJsonValue::Null,
                const QJsonObject& error = {});
    bool setCancelHandler(const JobToken& token, std::function<void()> cancel);
    bool cancel(const QString& jobId, const QString& principal);
    JobToken resume(const QString& jobId, const QString& principal, quint64 expectedVersion);
    void cancelAll();
    bool accepts(const JobToken& token) const;
    static bool isTerminal(const QString& state);

signals:
    void changed(const QString& jobId);
    void eventPublished(const QJsonObject& event);

private:
    struct Record {
        QString principal;
        quint64 epoch = 1;
        quint64 version = 0;
        quint64 sequence = 0;
        QJsonObject snapshot;
        QJsonArray items;
        QJsonArray events;
        std::function<void()> cancel;
    };
    Record* active(const JobToken& token);
    const Record* owned(const QString& jobId, const QString& principal) const;
    void publish(const QString& jobId, Record& record, const QString& type,
                 const QString& itemId = {});
    static QJsonObject counts(const QJsonArray& items);
    QHash<QString, Record> records_;
    QStringList order_;
};

QJsonObject jobError(const QString& code, const QString& message = {}, bool retryable = false);

} // namespace miacode
