#include "SettingsDialog.h"
#include "FrostedSurface.h"
#include "PlayerIcons.h"
#include "TouchKeyboard.h"
#include "models/Domain.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyleOptionSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <cstddef>
#ifdef Q_OS_MACOS
#include <CoreAudio/CoreAudio.h>
#include <QHostAddress>
#endif

namespace PiTunes {
namespace {
class FontSizeSpinBox : public QSpinBox {
public:
    using QSpinBox::QSpinBox;

protected:
    void paintEvent(QPaintEvent *event) override {
        QSpinBox::paintEvent(event);
        QStyleOptionSpinBox option;
        initStyleOption(&option);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        for (const auto button : {QStyle::SC_SpinBoxUp, QStyle::SC_SpinBoxDown}) {
            const bool up = button == QStyle::SC_SpinBoxUp;
            const auto available = up ? StepUpEnabled : StepDownEnabled;
            const auto center = style()->subControlRect(QStyle::CC_SpinBox, &option, button, this).center();
            const int direction = up ? 1 : -1;
            painter.setBrush(isEnabled() && option.stepEnabled.testFlag(available) ? QColor("#f4f5fa") : QColor("#707888"));
            painter.drawPolygon(QPolygon({center + QPoint(-4, 2 * direction), center + QPoint(4, 2 * direction),
                                          center + QPoint(0, -2 * direction)}));
        }
    }
};

class SettingsTitleBar : public QWidget {
public:
    explicit SettingsTitleBar(QDialog *dialog) : QWidget(dialog), m_dialog(dialog) {
        setObjectName("settingsTitleBar");
        setCursor(Qt::SizeAllCursor);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) return;
        m_offset = event->globalPos() - m_dialog->pos();
        auto *window = m_dialog->windowHandle();
        m_manualMove = !window || !window->startSystemMove();
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_manualMove && (event->buttons() & Qt::LeftButton)) m_dialog->move(event->globalPos() - m_offset);
    }

private:
    QDialog *m_dialog;
    QPoint m_offset;
    bool m_manualMove = false;
};

#ifdef Q_OS_MACOS
constexpr AudioObjectPropertyAddress defaultOutput = {
    kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
};

void populateMacOutputs(QComboBox *choice) {
    choice->clear();
    constexpr AudioObjectID system = kAudioObjectSystemObject;
    constexpr AudioObjectPropertyAddress devices = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    UInt32 bytes = 0;
    if (AudioObjectGetPropertyDataSize(system, &devices, 0, nullptr, &bytes) != noErr ||
        bytes == 0 || bytes > 4096) return;
    QVector<AudioDeviceID> ids(bytes / sizeof(AudioDeviceID));
    if (AudioObjectGetPropertyData(system, &devices, 0, nullptr, &bytes, ids.data()) != noErr) return;
    AudioDeviceID current = kAudioObjectUnknown;
    UInt32 currentSize = sizeof(current);
    AudioObjectGetPropertyData(system, &defaultOutput, 0, nullptr, &currentSize, &current);

    constexpr AudioObjectPropertyAddress configuration = {
        kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain
    };
    constexpr AudioObjectPropertyAddress name = {
        kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    for (const auto id : ids) {
        UInt32 streamBytes = 0;
        if (AudioObjectGetPropertyDataSize(id, &configuration, 0, nullptr, &streamBytes) != noErr ||
            streamBytes < sizeof(AudioBufferList) || streamBytes > 65536) continue;
        QByteArray storage(int(streamBytes), '\0');
        auto *buffers = reinterpret_cast<AudioBufferList *>(storage.data());
        if (AudioObjectGetPropertyData(id, &configuration, 0, nullptr, &streamBytes, buffers) != noErr ||
            streamBytes < offsetof(AudioBufferList, mBuffers)) continue;
        const auto count = (streamBytes - offsetof(AudioBufferList, mBuffers)) / sizeof(AudioBuffer);
        if (buffers->mNumberBuffers > count) continue;
        bool hasOutput = false;
        for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i) hasOutput |= buffers->mBuffers[i].mNumberChannels > 0;
        if (!hasOutput) continue;
        CFStringRef label = nullptr;
        UInt32 labelSize = sizeof(label);
        if (AudioObjectGetPropertyData(id, &name, 0, nullptr, &labelSize, &label) != noErr || !label) continue;
        QByteArray utf8(int(CFStringGetMaximumSizeForEncoding(CFStringGetLength(label), kCFStringEncodingUTF8)) + 1, '\0');
        if (CFStringGetCString(label, utf8.data(), utf8.size(), kCFStringEncodingUTF8))
            choice->addItem(QString::fromUtf8(utf8.constData()), uint(id));
        CFRelease(label);
    }
    choice->setCurrentIndex(choice->findData(uint(current)));
}
#endif
}

SettingsDialog::SettingsDialog(ApiClient *api, CaptionFontSizes fonts, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Application settings"); resize(650, 490);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName("applicationSettings");
    setStyleSheet(QString(
        "QDialog#applicationSettings { background: transparent; }"
        "QFrame#settingsSurface { background: #12151d; border: 1px solid #343b49; border-radius: %1px; }"
        "QWidget#settingsTitleBar, QLabel, QWidget#settingsPage { background: transparent; }"
        "QLabel#settingsHeading { color: #f2f4fb; font-size: 17px; font-weight: 600; }"
        "QTabWidget, QTabBar, QStackedWidget { background: transparent; border: 0; }"
        "QTabWidget::pane { border: 0; background: transparent; }"
        "QTabBar::tab { color: #aeb6c6; background: transparent; border: 0; border-bottom: 2px solid transparent;"
        "padding: 7px 11px; margin-right: 4px; }"
        "QTabBar::tab:selected { color: white; border-bottom-color: #e0e3ee; }"
        "QTabBar::tab:hover:!selected { color: white; }"
        "QToolButton#settingsClose { background: #212734; border: 0; border-radius: 14px; }"
        "QToolButton#settingsClose:hover, QToolButton#settingsClose:focus { background: #414a5c; }"
        "QSpinBox, QComboBox, QPushButton { color: white; background: #202631; padding: 4px; "
        "border: 1px solid #404a5c; border-radius: 5px; }"
        "QSpinBox { padding-right: 24px; }"
        "QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin: padding; width: 21px; border: 0; }"
        "QSpinBox::up-button { subcontrol-position: top right; }"
        "QSpinBox::down-button { subcontrol-position: bottom right; }"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #3a4559; }"
        "QSpinBox::up-arrow, QSpinBox::down-arrow { image: none; }"
        "QComboBox { combobox-popup: 0; }"
        "QComboBox QAbstractItemView { color: #f4f5fa; background: #202631; border: 1px solid #404a5c;"
        "selection-color: white; selection-background-color: #343d50; outline: 0; padding: 0; }"
        "QComboBox QAbstractItemView::item { min-height: 24px; padding: 0 6px; }"
        "QPushButton:hover, QPushButton:focus { border-color: #8393b0; }"
        "QPushButton:disabled { color: #8993a5; }").arg(popupCornerRadius));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *surface = new QFrame(this);
    surface->setObjectName("settingsSurface");
    outer->addWidget(surface);
    auto *layout = new QVBoxLayout(surface);
    layout->setContentsMargins(15, 11, 15, 15);
    auto *titleBar = new SettingsTitleBar(this);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 3);
    auto *titleText = new QLabel(windowTitle(), titleBar);
    titleText->setObjectName("settingsHeading");
    titleText->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLayout->addWidget(titleText, 1);
    auto *close = new QToolButton(titleBar);
    close->setObjectName("settingsClose");
    close->setAccessibleName("Close settings");
    close->setToolTip(close->accessibleName());
    close->setIcon(playerIcon(PlayerIcon::Close));
    close->setFixedSize(28, 28);
    close->setCursor(Qt::ArrowCursor);
    titleLayout->addWidget(close);
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    layout->addWidget(titleBar);
    auto *tabs = new QTabWidget(surface);
    tabs->setDocumentMode(true);
    tabs->tabBar()->setDrawBase(false);
    layout->addWidget(tabs, 1);
    auto *status = new QLabel(this); status->setWordWrap(true); layout->addWidget(status);
    auto *keyboard = new TouchKeyboard(this); layout->addWidget(keyboard);
    QPointer<SettingsDialog> guard(this);
    const auto result = [guard, status](QJsonObject object, QString error) {
        if (!guard) return;
        status->setText(error.isEmpty() ? textValue(object["message"]).isEmpty() ? "Done" : textValue(object["message"]) : error);
    };
    auto page = [tabs](const QString &title) {
        auto *widget = new QWidget(tabs); tabs->addTab(widget, title);
        widget->setObjectName("settingsPage");
        auto *form = new QFormLayout(widget);
        form->setContentsMargins(3, 10, 3, 3);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        return form;
    };
    auto field = [keyboard](QFormLayout *form, const QString &label, bool secret = false) {
        auto *edit = new QLineEdit; if (secret) edit->setEchoMode(QLineEdit::Password);
        form->addRow(label, edit); keyboard->watch(edit); return edit;
    };
    auto button = [this](QFormLayout *form, const QString &text, std::function<void()> action) {
        auto *b = new QPushButton(text, this); b->setMinimumHeight(38); form->addRow(b);
        connect(b, &QPushButton::clicked, this, std::move(action)); return b;
    };
    auto *font = page("Font");
    auto *heading = new QLabel("Album information panel", this);
    auto headingFont = heading->font();
    headingFont.setWeight(QFont::DemiBold);
    heading->setFont(headingFont);
    font->addRow(heading);
    const auto sizeControl = [font, this](const QString &label, const QString &name, int value) {
        auto *size = new FontSizeSpinBox(this);
        size->setObjectName(name);
        size->setAccessibleName(label + " font size");
        size->setRange(CaptionFontSizes::minimum, CaptionFontSizes::maximum);
        size->setFixedWidth(105);
        size->setMinimumHeight(30);
        size->setSuffix(" px");
        size->setValue(value);
        font->addRow(label, size);
        return size;
    };
    auto *title = sizeControl("Title", "captionTitleSize", fonts.title);
    auto *artist = sizeControl("Artist / singer", "captionArtistSize", fonts.artist);
    auto *album = sizeControl("Album", "captionAlbumSize", fonts.album);
    const auto preview = [this, title, artist, album] {
        emit captionFontSizesChanged({title->value(), artist->value(), album->value()});
    };
    for (auto *control : {title, artist, album}) {
        connect(control, QOverload<int>::of(&QSpinBox::valueChanged), this, preview);
    }
    auto *reset = button(font, "Reset font sizes", [title, artist, album, preview] {
        const CaptionFontSizes defaults;
        const QSignalBlocker preventTitle(title), preventArtist(artist), preventAlbum(album);
        title->setValue(defaults.title);
        artist->setValue(defaults.artist);
        album->setValue(defaults.album);
        preview();
    });
    reset->setMaximumWidth(170);
    if (!api) return;

    auto *library = page("Library");
    auto *music = field(library, "Music folder");
    auto *source = new QComboBox(library->parentWidget());
    source->addItem("USB / local folder", "local"); source->addItem("Internal library", "internal");
    library->addRow("Source", source);
    api->get("/api/settings", [guard, music, source](QJsonObject o, QString error) {
        if (!guard || !error.isEmpty()) return;
        const auto settings = o["settings"].toObject(); music->setText(textValue(settings["music_directory"]));
        const int i = source->findData(textValue(settings["storage_source"])); if (i >= 0) source->setCurrentIndex(i);
    });
    button(library, "Save source", [=] {
        api->post("/api/settings", {{"music_directory", music->text()}, {"storage_source", source->currentData().toString()}}, result, 90000);
    });
    button(library, "Rescan library", [=] { api->post("/api/library/rescan", {}, result); });
    button(library, "Import browser data…", [=] {
        const auto path = QFileDialog::getOpenFileName(this, "Import PiTunes browser data", {}, "PiTunes browser data (*.json)");
        if (path.isEmpty()) return;
        QFile file(path);
        if (file.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly)) { status->setText("Cannot read the export or it exceeds 1 MiB."); return; }
        QJsonParseError error; const auto doc = QJsonDocument::fromJson(file.read(1024 * 1024 + 1), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) { status->setText("This file is not a valid PiTunes browser export."); return; }
        api->post("/api/library/smart-playlists/import", doc.object(), result);
    });
    button(library, "Rebuild artwork cache…", [=] {
        if (QMessageBox::question(this, "Rebuild artwork", "Rebuild cached artwork and rescan the library?") == QMessageBox::Yes)
            api->post("/api/library/rebuild-cache", {}, result, 90000);
    });

    auto *audio = page("Audio");
    const auto setupApplianceAudio = [=](bool preview) {
        if (preview) {
            auto *description = new QLabel("Appliance output options are applied on a PiTunes device.");
            description->setWordWrap(true);
            audio->addRow(description);
        }
        auto *parent = audio->parentWidget();
        auto *route = new QComboBox(parent); auto *device = new QComboBox(parent); auto *hat = new QComboBox(parent);
        auto *mixer = new QComboBox(parent); mixer->addItems({"software", "hardware", "none"});
        route->setObjectName("applianceAudioRoute");
        audio->addRow("Output", route); audio->addRow("ALSA device", device); audio->addRow("Pi DAC HAT", hat); audio->addRow("Mixer", mixer);
        auto *applyOutput = button(audio, "Apply output", [=] {
            api->post("/api/audio/output", {{"output", route->currentData().toString()}, {"alsa", device->currentData().toString()},
                {"dac_hat", hat->currentData().toString()}, {"mixer", mixer->currentText()}},
                [guard, result](QJsonObject o, QString error) {
                    result(o, error);
                    if (guard && error.isEmpty() && boolValue(o["reboot_required"]))
                        QMessageBox::information(guard, "Audio output", "Output saved. Restart the appliance to complete this change.");
                }, 90000);
        });
        applyOutput->setEnabled(false);
        api->get("/api/audio/devices", [=](QJsonObject o, QString error) {
            if (!guard) return;
            if (!error.isEmpty()) { result(o, error); return; }
            for (const auto &v : o["routes"].toArray()) {
                const auto r = v.toObject();
                route->addItem(r.isEmpty() ? textValue(v) : textValue(r["label"]), r.isEmpty() ? textValue(v) : textValue(r["id"]));
            }
            device->addItem("ALSA default", "default");
            for (const auto &v : o["devices"].toArray()) { const auto d = v.toObject(); device->addItem(textValue(d["label"]), textValue(d["alsa"])); }
            for (const auto &v : o["hats"].toArray()) { const auto h = v.toObject(); hat->addItem(textValue(h["label"]), textValue(h["id"])); }
            hat->setEnabled(hat->count() > 0);
            const auto current = o["current"].toObject();
            for (auto pair : {qMakePair(route, QString("route")), qMakePair(device, QString("device")), qMakePair(hat, QString("dac_hat"))}) {
                const int i = pair.first->findData(textValue(current[pair.second])); if (i >= 0) pair.first->setCurrentIndex(i);
            }
            mixer->setCurrentText(textValue(current["mixer"]));
            applyOutput->setEnabled(!preview && route->count() > 0);
        });
    };
#ifdef Q_OS_MACOS
    const auto host = api->baseUrl().host();
    if (host == "localhost" || QHostAddress(host).isLoopback()) {
        auto *checking = new QLabel("Checking sound output…");
        audio->addRow(checking);
        api->get("/api/system/info", [=](QJsonObject info, QString error) {
            if (!guard) return;
            audio->removeRow(checking);
            if (!error.isEmpty() || textValue(info["os"].toObject()["name"]) != "Darwin") {
                setupApplianceAudio(false);
                return;
            }
            auto *outputs = new QComboBox(audio->parentWidget());
            outputs->setObjectName("macSoundOutput");
            audio->addRow("Mac sound output", outputs);
            auto *description = new QLabel("Preview playback follows the Mac’s system sound output.");
            description->setWordWrap(true);
            audio->addRow(description);
            populateMacOutputs(outputs);
            auto *apply = button(audio, "Use as Mac sound output", [=] {
                const auto device = AudioDeviceID(outputs->currentData().toUInt());
                const auto error = AudioObjectSetPropertyData(kAudioObjectSystemObject, &defaultOutput, 0, nullptr,
                                                               sizeof(device), &device);
                populateMacOutputs(outputs);
                result({}, error == noErr ? QString() : QString("Could not change the Mac sound output."));
            });
            apply->setEnabled(outputs->currentIndex() >= 0);
            connect(outputs, QOverload<int>::of(&QComboBox::currentIndexChanged), apply, [apply](int row) { apply->setEnabled(row >= 0); });
            setupApplianceAudio(true);
        });
    } else
#endif
    setupApplianceAudio(false);

    auto *network = page("Network");
    auto *netStatus = new QLabel; netStatus->setWordWrap(true); network->addRow(netStatus);
    auto *ssid = field(network, "Wi-Fi name"); auto *password = field(network, "Wi-Fi password", true);
    auto *country = field(network, "Country code"); country->setText("GB");
    const auto refreshWifi = [=] {
        api->get("/api/network/wifi/status", [guard, netStatus](QJsonObject o, QString error) {
            if (!guard) return;
            netStatus->setText(error.isEmpty() ? "Network: " + textValue(o["mode"]) + " · " + textValue(o["ip"]) : error);
        }, 30000);
    };
    button(network, "Refresh network status", refreshWifi);
    auto *networks = new QComboBox(network->parentWidget()); network->addRow("Available networks", networks);
    connect(networks, QOverload<int>::of(&QComboBox::activated), this, [=](int) { ssid->setText(networks->currentText()); });
    button(network, "Scan Wi-Fi", [=] {
        api->get("/api/network/wifi/scan", [guard, networks, result](QJsonObject o, QString error) {
            if (!guard) return;
            result(o, error);
            if (!error.isEmpty()) return;
            networks->clear(); for (const auto &v : o["networks"].toArray()) networks->addItem(textValue(v.toObject()["ssid"]));
        }, 45000);
    });
    button(network, "Connect Wi-Fi", [=] {
        api->post("/api/network/wifi/connect", {{"ssid", ssid->text()}, {"password", password->text()}, {"country", country->text()}}, result);
        password->clear(); keyboard->hide();
    });
    button(network, "Start recovery hotspot", [=] { api->post("/api/network/hotspot/start", {}, result); });
    button(network, "Stop recovery hotspot", [=] { api->post("/api/network/hotspot/stop", {}, result); });

    auto *nas = page("NAS");
    auto *protocol = new QComboBox(nas->parentWidget()); protocol->addItems({"smb", "nfs"}); nas->addRow("Protocol", protocol);
    auto *server = field(nas, "Server"); auto *share = field(nas, "Share / export");
    auto *user = field(nas, "Username"); auto *secret = field(nas, "Password", true);
    button(nas, "Mount music source", [=] {
        api->post("/api/storage/network/configure", {{"protocol", protocol->currentText()}, {"server", server->text()},
            {"share", share->text()}, {"username", user->text()}, {"password", secret->text()}}, result, 90000);
        secret->clear(); keyboard->hide();
    });
    button(nas, "Check mount", [=] {
        api->get("/api/storage/network/status", [guard, result](QJsonObject o, QString error) {
            if (!guard) return;
            if (error.isEmpty()) o["message"] = boolValue(o["mounted"]) ? "Network music mounted" : "Network music is unavailable";
            result(o, error);
        }, 45000);
    });

    auto *system = page("System");
    for (const auto &name : {QString("ssh"), QString("bluetooth"), QString("airplay")}) {
        auto *choice = new QComboBox(system->parentWidget()); choice->addItems({"Stopped", "Enabled"}); system->addRow(name, choice);
        connect(choice, QOverload<int>::of(&QComboBox::activated), this, [=](int i) {
            api->post("/api/services/control", {{"service", name}, {"action", i == 1 ? "start" : "stop"}}, result, 60000);
        });
        api->get("/api/services", [guard, choice, name](QJsonObject o, QString error) {
            if (!guard || !error.isEmpty()) return;
            const auto components = o["services"].toObject()[name].toArray();
            bool active = false; for (const auto &v : components) if (textValue(v.toObject()["active"]) == "active") active = true;
            choice->setCurrentIndex(active ? 1 : 0);
        }, 60000);
    }
    auto *about = new QLabel; about->setWordWrap(true); system->addRow(about);
    button(system, "About this system", [=] {
        api->get("/api/system/info", [guard, about](QJsonObject o, QString error) {
            if (!guard) return;
            const auto version = o["pitunes"].toObject();
            about->setText(error.isEmpty() ? "PiTunes Native · " + textValue(version["version"]) + "\n" + textValue(o["hostname"]) : error);
        }, 30000);
    });
    system->addRow(new QLabel("Native software updates are disabled pending installation and rollback validation."));
    for (const auto &action : {QString("reboot"), QString("shutdown")}) {
        button(system, action == "reboot" ? "Restart…" : "Shut down…", [=] {
            if (QMessageBox::question(this, "PiTunes", "Confirm " + action + "?") == QMessageBox::Yes)
                api->post("/api/system/control", {{"action", action}}, result);
        });
    }
}
}
