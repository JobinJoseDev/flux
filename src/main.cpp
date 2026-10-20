#include "Document.hpp"
#include "MotionPose.hpp"
#include "Render.hpp"
#include "SmokeTest.hpp"
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Flux");
    QCoreApplication::setOrganizationName("Flux");
    QCoreApplication::setApplicationVersion("0.2.0");
    QGuiApplication::setDesktopFileName("io.fluxpresent.Flux");
    QQuickStyle::setStyle("Fusion");
    QCommandLineParser parser;
    parser.setApplicationDescription("Flux — offline presentations in motion");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("project", "Local .flux project to open.", "[project]");
    parser.addOption({"smoke-test", "Load the real QML application, render a frame, then quit."});
    parser.addOption({"screenshot", "Save the real editor framebuffer to a PNG.", "path"});
    parser.process(app);
    qmlRegisterType<MotionPose>("Flux.Native", 1, 0, "MotionPose");
    qmlRegisterType<ContentItem>("Flux.Native", 1, 0, "ContentItem");
    Document document;
    if (!parser.positionalArguments().isEmpty())
        document.open(QUrl::fromLocalFile(
            QFileInfo(parser.positionalArguments().first()).absoluteFilePath()));
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::quit, &app, [] { QCoreApplication::exit(0); });
    engine.rootContext()->setContextProperty("deck", &document);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    int qmlWarnings = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [&](const QList<QQmlError>& warnings) { qmlWarnings += warnings.size(); });
    engine.load(QUrl("qrc:/qt/qml/Flux/qml/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    if (parser.isSet("smoke-test"))
        runSmokeTest(engine, document, qmlWarnings);
    if (parser.isSet("screenshot")) {
        QTimer::singleShot(1000, &app, [&] {
            if (parser.isSet("screenshot")) {
                auto window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                if (!window || !window->grabWindow().save(parser.value("screenshot"))) {
                    QCoreApplication::exit(2);
                    return;
                }
            }
            QCoreApplication::exit(0);
        });
    }
    return app.exec();
}
