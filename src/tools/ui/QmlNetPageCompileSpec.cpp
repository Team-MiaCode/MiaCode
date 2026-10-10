#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTextStream>
#include <QUrl>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    QTextStream err(stderr);
    const QStringList pages = {
        QStringLiteral("NetDownloadPage"),
        QStringLiteral("NetUploadPage"),
        QStringLiteral("NetResultTable"),
    };
    bool passed = true;
    for (const QString& page : pages) {
        QQmlComponent component(&engine,
            QUrl::fromLocalFile(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT "/MiaCode/UI/")
                + page + QStringLiteral(".qml")),
            QQmlComponent::PreferSynchronous);
        if (component.isReady()) {
            continue;
        }
        passed = false;
        err << "FAIL: " << page << Qt::endl;
        for (const QQmlError& error : component.errors()) {
            err << error.toString() << Qt::endl;
        }
    }
    if (passed) {
        QTextStream(stdout) << "qml_net_page_compile_spec ok" << Qt::endl;
    }
    return passed ? 0 : 1;
}
