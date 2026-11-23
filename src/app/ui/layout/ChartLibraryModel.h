#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace miacode::ui {

class ChartLibraryModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QUrl directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(QString directoryName READ directoryName NOTIFY directoryChanged)
    Q_PROPERTY(QVariantList songs READ songs NOTIFY songsChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool directoryMissing READ directoryMissing NOTIFY songsChanged)

public:
    explicit ChartLibraryModel(QObject* parent = nullptr);
    QUrl directory() const { return directory_; }
    QString directoryName() const;
    QVariantList songs() const { return songs_; }
    bool scanning() const { return scanning_; }
    bool directoryMissing() const { return directoryMissing_; }

    Q_INVOKABLE void setDirectory(const QUrl& directory);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE static QString chartPathKey(const QString& path);
    Q_INVOKABLE QUrl pickerDirectory(const QString& chartPath) const;

signals:
    void directoryChanged();
    void songsChanged();
    void scanningChanged();

private:
    QUrl directory_;
    QVariantList songs_;
    quint64 scanRevision_ = 0;
    bool scanning_ = false;
    bool directoryMissing_ = false;
};

} // namespace miacode::ui
