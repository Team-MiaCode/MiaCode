#include "common/IntroAssetImages.h"

#include <QQmlComponent>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class MobileIntroTextureSpec : public QObject {
    Q_OBJECT

    static QImage pattern() {
        QImage image(40, 30, QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixelColor(x, y, QColor(x * 5, y * 7, (x + y) * 3));
        return image;
    }
    static QString providerId(const QString& file) {
        return QString::fromLatin1(QUrl::toPercentEncoding(QUrl::fromLocalFile(file).toString()));
    }

private slots:
    void encodedFileAndAtlasCropPreservePixels() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath(QString::fromUtf8("曲绘 # 100%.png"));
        const QImage original = pattern();
        QVERIFY(original.save(file));
        miacode::intro::IntroAssetImages provider;
        QSize implicitSize;
        std::unique_ptr<QQuickTextureFactory> full(provider.requestTexture(providerId(file), &implicitSize, {}));
        QVERIFY(full);
        QCOMPARE(implicitSize, original.size());
        QCOMPARE(full->image().format(), QImage::Format_RGBA8888_Premultiplied);
        QCOMPARE(full->image().convertToFormat(QImage::Format_ARGB32), original);

        const QRect region(7, 9, 14, 12);
        std::unique_ptr<QQuickTextureFactory> cropped(provider.requestTexture(
            providerId(file) + "?clip=7,9,14,12", &implicitSize, {}));
        QVERIFY(cropped);
        QCOMPARE(implicitSize, original.size());
        QCOMPARE(cropped->textureSize(), region.size());
        QCOMPARE(cropped->image().convertToFormat(QImage::Format_ARGB32), original.copy(region));

        std::unique_ptr<QQuickTextureFactory> scaled(provider.requestTexture(providerId(file), &implicitSize, {20, 20}));
        QVERIFY(scaled);
        QCOMPARE(scaled->textureSize(), QSize(20, 15));
        QCOMPARE(implicitSize, original.size());
        QVERIFY(provider.requestTexture("https%3A%2F%2Fexample.invalid%2Fimage.png", nullptr, {}) == nullptr);
    }

    void qmlImageConsumesCroppedTexture() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("atlas.png");
        const QImage original = pattern();
        QVERIFY(original.save(file));
        QQmlEngine engine;
        miacode::intro::registerIntroAssetImages(&engine);
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nImage { width: 14; height: 12; smooth: false; mipmap: true; "
                          "sourceClipRect: Qt.rect(7,9,14,12) }", QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto* item = qobject_cast<QQuickItem*>(object.get());
        QVERIFY(item);
        item->setProperty("source", QUrl("image://introasset/" + providerId(file) + "?clip=7,9,14,12"));
        QTRY_COMPARE(item->property("status").toInt(), 1);

        QQuickWindow window;
        window.resize(40, 30);
        window.setColor(Qt::black);
        item->setParentItem(window.contentItem());
        item->setPosition({4, 5});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(80);
        const QImage image = window.grabWindow();
        QVERIFY(!image.isNull());
        const qreal dpr = image.devicePixelRatio();
        for (const QPoint sample : {QPoint(2, 2), QPoint(8, 5), QPoint(11, 9)}) {
            const QPoint pixel = ((QPointF(4, 5) + QPointF(sample) + QPointF(0.5, 0.5)) * dpr).toPoint();
            const QColor actual = image.pixelColor(pixel);
            const QColor expected = original.pixelColor(sample + QPoint(7, 9));
            QVERIFY2(qAbs(actual.red() - expected.red()) <= 6 && qAbs(actual.green() - expected.green()) <= 8
                && qAbs(actual.blue() - expected.blue()) <= 6, "QML must sample the selected atlas glyph, not the whole atlas");
        }
        item->setParentItem(nullptr);
    }
};

QTEST_MAIN(MobileIntroTextureSpec)
#include "MobileIntroTextureSpec.moc"
