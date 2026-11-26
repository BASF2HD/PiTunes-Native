#include "ui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFile>
#include <QTimer>

int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("PiTunes"); QCoreApplication::setApplicationName("PiTunes Native");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCommandLineParser parser; parser.setApplicationDescription("PiTunes native Qt Widgets music appliance");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"api", "API server (loopback by default)", "url", "http://127.0.0.1:8080"});
    parser.addOption({"demo", "Explicit offline UI demonstration; no audio"});
    parser.addOption({"fullscreen", "Run the appliance fullscreen"});
    parser.addOption({"reduced-motion", "Disable cover transitions and use a smaller art budget"});
    parser.addOption({"screenshot", "Write a development screenshot and exit", "path"});
    parser.addOption({"screenshot-delay", "Milliseconds before the development screenshot", "ms", "2000"});
    parser.addOption({"window-size", "Development window dimensions, for example 800x480", "WxH"});
    parser.process(app);
    const QUrl api(parser.value("api"));
    if (!api.isValid() || !QStringList({"http", "https"}).contains(api.scheme()) || api.host().isEmpty() || !api.userInfo().isEmpty())
        parser.showHelp(2);
    app.setStyle("Fusion");
    app.setStyleSheet("QWidget{background:#000;color:#ddd;font-size:14px;}"
        "QPushButton{border:1px solid #444;border-radius:5px;padding:6px 10px;}"
        "QPushButton:hover,QPushButton:focus{border-color:#ddd;}QPushButton:checked{color:white;border-color:white;}"
        "QPushButton:disabled{color:#666;}QLineEdit,QComboBox{background:#18181b;border:1px solid #555;padding:7px;}"
        "QListView{background:#101012;border:0;}QListView::item:selected{background:#34343a;}"
        "QTabBar::tab{padding:10px;}QTabBar::tab:selected{border-bottom:2px solid white;}"
        "QSlider::groove:horizontal{height:3px;background:#555;}QSlider::handle:horizontal{background:white;width:12px;margin:-5px 0;border-radius:6px;}");
    bool reduced = parser.isSet("reduced-motion");
    QFile memory("/proc/meminfo");
    if (memory.open(QIODevice::ReadOnly)) {
        const auto firstLine = memory.readLine().simplified().split(' ');
        if (firstLine.size() >= 2 && firstLine[0] == "MemTotal:") reduced = reduced || firstLine[1].toLongLong() <= 1100000;
    }
    PiTunes::MainWindow window(api, parser.isSet("demo"), reduced);
    if (parser.isSet("window-size")) {
        const auto parts = parser.value("window-size").split('x');
        if (parts.size() == 2) window.resize(qBound(320, parts[0].toInt(), 3840), qBound(320, parts[1].toInt(), 2160));
    }
    if (parser.isSet("fullscreen")) window.showFullScreen(); else window.show();
    if (parser.isSet("screenshot")) {
        const auto path = QFileInfo(parser.value("screenshot")).absoluteFilePath();
        QTimer::singleShot(qBound(0, parser.value("screenshot-delay").toInt(), 30000), &window, [&window, path, &app] {
            const bool saved = window.grab().save(path); app.exit(saved ? 0 : 1);
        });
    }
    return app.exec();
}
