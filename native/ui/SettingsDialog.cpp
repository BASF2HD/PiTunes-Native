#include "SettingsDialog.h"
#include "PerformanceMonitor.h"
#include "PlayerIcons.h"
#include "TouchKeyboard.h"
#include "models/Domain.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
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
void chooseMusicFolder(ApiClient *api, QLineEdit *field, QWidget *parent) {
    if (api->isStandalone()) {
        const auto folder = QFileDialog::getExistingDirectory(parent, "Choose music folder", field->text());
        if (!folder.isEmpty()) field->setText(folder);
        return;
    }
    auto *dialog = new QDialog(parent);
    dialog->setWindowTitle("Choose music folder");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(510, 395);
    auto *layout = new QVBoxLayout(dialog);
    auto *path = new QLabel(dialog);
    path->setTextFormat(Qt::PlainText);
    path->setWordWrap(true);
    layout->addWidget(path);
    auto *folders = new QListWidget(dialog);
    layout->addWidget(folders, 1);
    auto *status = new QLabel(dialog);
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto *row = new QHBoxLayout;
    auto *up = new QPushButton("Up", dialog);
    auto *choose = new QPushButton("Choose this folder", dialog);
    auto *cancel = new QPushButton("Cancel", dialog);
    row->addWidget(up); row->addStretch(); row->addWidget(choose); row->addWidget(cancel);
    layout->addLayout(row);
    const QPointer<QDialog> guard(dialog);
    const auto load = [=](const QString &folder) {
        up->setEnabled(false);
        choose->setEnabled(false);
        folders->clear();
        status->setText("Loading folders…");
        api->get("/api/filesystem/browse?path=" + QString::fromUtf8(QUrl::toPercentEncoding(folder)),
            [=](QJsonObject response, QString error) {
                if (!guard) return;
                status->setText(error);
                if (!error.isEmpty()) {
                    up->setProperty("parentPath", QString());
                    up->setEnabled(!folder.isEmpty());
                    return;
                }
                path->setText(textValue(response["path"]));
                const auto parentPath = textValue(response["parent"]);
                up->setProperty("parentPath", parentPath);
                up->setEnabled(!parentPath.isEmpty());
                choose->setEnabled(!path->text().isEmpty());
                for (const auto &value : response["entries"].toArray()) {
                    const auto entry = value.toObject();
                    auto *item = new QListWidgetItem(textValue(entry["name"]), folders);
                    item->setData(Qt::UserRole, textValue(entry["path"]));
                    if (!boolValue(entry["readable"])) item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                }
            });
    };
    QObject::connect(up, &QPushButton::clicked, dialog, [=] { load(up->property("parentPath").toString()); });
    QObject::connect(folders, &QListWidget::itemActivated, dialog, [=](QListWidgetItem *item) {
        const auto next = item->data(Qt::UserRole).toString();
        load(next);
    });
    QObject::connect(choose, &QPushButton::clicked, dialog, [=] { field->setText(path->text()); dialog->accept(); });
    QObject::connect(cancel, &QPushButton::clicked, dialog, &QDialog::reject);
    load(field->text().trimmed());
    dialog->open();
}

class SettingsComboBox : public QComboBox {
public:
    SettingsComboBox(QTabBar *tabs, QWidget *parent) : QComboBox(parent), m_tabs(tabs) {}

    void showPopup() override {
        auto *popup = view()->window();
        popup->installEventFilter(this);
        m_pressedTab = -1;
        QComboBox::showPopup();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            auto *popup = view()->window();
            if (watched == popup && mouse->button() == Qt::LeftButton) {
                const int tab = popup->rect().contains(popup->mapFromGlobal(mouse->globalPos()))
                    ? -1 : m_tabs->tabAt(m_tabs->mapFromGlobal(mouse->globalPos()));
                if (tab >= 0 && m_tabs->isTabEnabled(tab)) {
                    if (event->type() == QEvent::MouseButtonPress) m_pressedTab = tab;
                    else if (m_pressedTab == tab) {
                        popup->setAttribute(Qt::WA_NoMouseReplay);
                        hidePopup();
                        m_tabs->setCurrentIndex(tab);
                    }
                    return true;
                }
                m_pressedTab = -1;
            }
        }
        return QComboBox::eventFilter(watched, event);
    }

private:
    QTabBar *m_tabs;
    int m_pressedTab = -1;
};

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

SettingsDialog::SettingsDialog(ApiClient *api, CaptionFontSizes fonts, QWidget *parent,
                               AnimationMode animation, PerformanceMonitor *monitor, bool forceReducedMotion, int animationFrameRate)
    : QDialog(parent) {
    setWindowTitle("Application settings"); resize(650, 490);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName("applicationSettings");
    setStyleSheet(
        "QDialog#applicationSettings { background: transparent; }"
        "QFrame#settingsSurface { background: #12151d; border: 1px solid #343b49; border-radius: 4px; }"
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
        "QLineEdit, QSpinBox, QComboBox, QPushButton { color: white; background: #202631; padding: 4px; "
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
        "QPushButton:disabled { color: #8993a5; }");
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
    const auto comboBox = [tabs](QFormLayout *form) { return new SettingsComboBox(tabs->tabBar(), form->parentWidget()); };
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
    auto button = [this](QFormLayout *form, const QString &text, std::function<void()> action, QHBoxLayout *row = nullptr) {
        auto *b = new QPushButton(text, this);
        b->setMinimumHeight(30);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        if (row) row->addWidget(b);
        else form->addRow(b);
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
    const auto setupDisplaySettings = [=](QFormLayout *system) {
        auto *modes = comboBox(system);
        modes->addItems({"Full", "Lightweight", "Off"});
        modes->setCurrentIndex(int(animation));
        modes->setEnabled(!forceReducedMotion);
        system->addRow("CoverFlow animation", modes);
        connect(modes, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
            emit animationModeChanged(AnimationMode(index));
        });
        auto *rate = comboBox(system);
        for (const int fps : {15, 20, 30}) rate->addItem(QString("%1 FPS").arg(fps), fps);
        rate->setCurrentIndex(rate->findData(animationFrameRate));
        rate->setEnabled(!forceReducedMotion && animation != AnimationMode::Off);
        system->addRow("CoverFlow frame limit", rate);
        connect(rate, QOverload<int>::of(&QComboBox::activated), this, [this, rate](int index) {
            emit animationFrameRateChanged(rate->itemData(index).toInt());
        });
        connect(modes, QOverload<int>::of(&QComboBox::currentIndexChanged), rate, [rate, forceReducedMotion](int index) {
            rate->setEnabled(!forceReducedMotion && index != int(AnimationMode::Off));
        });
        if (!monitor) return;
        auto *display = comboBox(system);
        display->addItems({"Off", "Compact overlay", "Detailed monitor"});
        display->setCurrentIndex(int(monitor->mode()));
        system->addRow("Performance Monitor (F12)", display);
        connect(display, QOverload<int>::of(&QComboBox::activated), monitor, [monitor](int index) {
            monitor->setMode(MonitorMode(index));
        });
        connect(monitor, &PerformanceMonitor::modeChanged, display, &QComboBox::setCurrentIndex);
        auto *record = button(system, "Record benchmark CSV…", [=] {
            if (monitor->recording()) {
                monitor->stopRecording();
                return;
            }
            const auto path = QFileDialog::getSaveFileName(this, "Record benchmark CSV", "pitunes-performance.csv", "CSV (*.csv)");
            monitor->startRecording(path);
        });
        const auto labelRecord = [record](bool active) {
            record->setText(active ? "Stop and save benchmark" : "Record benchmark CSV…");
        };
        labelRecord(monitor->recording());
        connect(monitor, &PerformanceMonitor::recordingChanged, record, labelRecord);
        connect(monitor, &PerformanceMonitor::recordingSaved, this, [status](const QString &path, bool saved) {
            status->setText(saved ? "Saved " + QFileInfo(path).fileName() : "Could not save the benchmark CSV.");
        });
    };
    if (!api) {
        setupDisplaySettings(page("System"));
        return;
    }

    auto *library = page("Library");
    auto *musicRow = new QHBoxLayout;
    auto *music = new QLineEdit;
    keyboard->watch(music);
    auto *browse = new QPushButton("Browse…", this);
    musicRow->addWidget(music, 1);
    musicRow->addWidget(browse);
    library->addRow("Music folder", musicRow);
    connect(browse, &QPushButton::clicked, this, [=] { chooseMusicFolder(api, music, this); });
    auto *source = comboBox(library);
    source->addItem("USB / local folder", "local"); source->addItem("Internal library", "internal");
    library->addRow("Source", source);
    if (api->isStandalone()) { library->labelForField(source)->hide(); source->hide(); }
    api->get("/api/settings", [guard, music, source](QJsonObject o, QString error) {
        if (!guard || !error.isEmpty()) return;
        const auto settings = o["settings"].toObject(); music->setText(textValue(settings["music_directory"]));
        const int i = source->findData(textValue(settings["storage_source"])); if (i >= 0) source->setCurrentIndex(i);
    });
    auto *libraryActions = new QHBoxLayout;
    library->addRow(libraryActions);
    button(library, "Save source", [=] {
        api->post("/api/settings", {{"music_directory", music->text()}, {"storage_source", source->currentData().toString()}}, result, 90000);
    }, libraryActions);
    button(library, "Rescan library", [=] { api->post("/api/library/rescan", {}, result); }, libraryActions);
    libraryActions->addStretch();
    if (!api->isStandalone()) button(library, "Import browser data…", [=] {
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
        auto *route = comboBox(audio); auto *device = comboBox(audio); auto *hat = comboBox(audio);
        device->setObjectName("applianceAudioDevice");
        auto *mixer = comboBox(audio); mixer->addItems({"software", "hardware", "none"});
        route->setObjectName("applianceAudioRoute");
        audio->addRow("Output", route);
        auto *deviceLabel = new QLabel("ALSA device");
        audio->addRow(deviceLabel, device);
        audio->addRow("Pi DAC HAT", hat);
        audio->addRow("Mixer", mixer);
        auto *mixerControl = comboBox(audio);
        auto *mixerLabel = new QLabel("Volume control");
        audio->addRow(mixerLabel, mixerControl);
        auto *ports = new QLabel;
        ports->setWordWrap(true);
        audio->addRow(ports);
        mixerLabel->hide(); mixerControl->hide(); ports->hide();
        auto *portRow = new QWidget;
        auto *portLayout = new QHBoxLayout(portRow);
        portLayout->setContentsMargins(0, 0, 0, 0);
        auto *portControl = comboBox(audio);
        auto *portState = comboBox(audio);
        auto *portVolume = new QSpinBox(portRow);
        portVolume->setRange(0, 100);
        portVolume->setSuffix("%");
        auto *applyPort = new QPushButton("Set", portRow);
        portLayout->addWidget(portControl, 1);
        portLayout->addWidget(portState);
        portLayout->addWidget(portVolume);
        portLayout->addWidget(applyPort);
        auto *portLabel = new QLabel("Speaker / headphone");
        audio->addRow(portLabel, portRow);
        portLabel->hide(); portRow->hide();
        const auto showPort = [=] {
            const auto info = portControl->currentData().toJsonObject();
            const auto state = textValue(info["state"]);
            const bool automatic = textValue(info["name"]) == "Auto-Mute Mode";
            portState->clear();
            if (automatic) {
                portState->addItems({"Enabled", "Disabled"});
                portState->setCurrentText(state);
            } else {
                portState->addItems({"Unmuted", "Muted"});
                portState->setCurrentIndex(state == "off" ? 1 : 0);
            }
            const bool haveVolume = boolValue(info["volume"]) && info["percent"].isDouble();
            portVolume->setVisible(haveVolume);
            if (haveVolume) portVolume->setValue(info["percent"].toInt());
        };
        connect(portControl, QOverload<int>::of(&QComboBox::currentIndexChanged), this, showPort);
        connect(applyPort, &QPushButton::clicked, this, [=] {
            const auto info = portControl->currentData().toJsonObject();
            const auto name = textValue(info["name"]);
            const QString state = name == "Auto-Mute Mode" ? portState->currentText()
                : portState->currentIndex() == 0 ? "unmute" : "mute";
            QJsonObject command{{"alsa", device->currentData().toString()}, {"control", name}, {"state", state}};
            if (!portVolume->isHidden()) command.insert("percent", portVolume->value());
            api->post("/api/audio/mixer", command, result);
        });
        auto *testOutput = new QPushButton("Test Audio…", this);
        audio->addRow(testOutput);
        testOutput->hide();
        connect(testOutput, &QPushButton::clicked, this, [=] {
            if (QMessageBox::question(this, "Test Audio",
                    "A short tone will play on the selected device. Lower the speaker or headphone volume first.")
                    != QMessageBox::Yes) return;
            api->post("/api/audio/test", {{"alsa", device->currentData().toString()}}, result);
        });
        const auto refreshControls = [=] {
            const auto previous = mixerControl->currentData().toString();
            mixerControl->clear();
            mixerControl->addItem("Automatic", "");
            QStringList status;
            const auto previousPort = textValue(portControl->currentData().toJsonObject()["name"]);
            portControl->clear();
            for (const auto &entry : device->currentData(Qt::UserRole + 1).toJsonArray()) {
                const auto info = entry.toObject();
                const auto name = textValue(info["name"]);
                if (boolValue(info["volume"])) mixerControl->addItem(name, name);
                if (boolValue(info["port"]) || name == "Auto-Mute Mode") portControl->addItem(name, info);
                const auto state = textValue(info["state"]);
                if (!state.isEmpty()) status << name + ": " + state;
            }
            const int selected = mixerControl->findData(previous);
            if (selected >= 0) mixerControl->setCurrentIndex(selected);
            const bool pc = route->currentData().toString() == "pc-alsa";
            const int chosenPort = portControl->findText(previousPort);
            if (chosenPort >= 0) portControl->setCurrentIndex(chosenPort);
            const bool showPorts = !preview && pc && device->currentData(Qt::UserRole + 2).toString() == "analog"
                && portControl->count() > 0;
            portLabel->setVisible(showPorts);
            portRow->setVisible(showPorts);
            testOutput->setVisible(!preview && pc);
            testOutput->setEnabled(device->currentIndex() > 0);
            showPort();
            const bool showControl = pc && mixer->currentText() == "hardware" && mixerControl->count() > 1;
            mixerControl->setVisible(showControl);
            mixerLabel->setVisible(showControl);
            ports->setText(status.join(" · "));
            ports->setVisible(pc && !status.isEmpty());
        };
        connect(device, QOverload<int>::of(&QComboBox::currentIndexChanged), this, refreshControls);
        connect(mixer, &QComboBox::currentTextChanged, this, refreshControls);
        auto *applyOutput = button(audio, "Apply output", [=] {
            api->post("/api/audio/output", {{"output", route->currentData().toString()}, {"alsa", device->currentData().toString()},
                {"dac_hat", hat->currentData().toString()}, {"mixer", mixer->currentText()},
                {"mixer_control", mixerControl->isHidden() ? QString() : mixerControl->currentData().toString()}},
                [guard, result](QJsonObject o, QString error) {
                    result(o, error);
                    if (guard && error.isEmpty() && boolValue(o["reboot_required"]))
                        QMessageBox::information(guard, "Audio output", "Output saved. Restart the appliance to complete this change.");
                }, 90000);
        });
        applyOutput->setEnabled(false);
        auto *refreshOutputs = new QPushButton("Refresh outputs", this);
        audio->addRow(refreshOutputs);
        const auto readOutputs = [=] {
            refreshOutputs->setEnabled(false);
            api->get("/api/audio/devices", [=](QJsonObject o, QString error) {
            if (!guard) return;
            refreshOutputs->setEnabled(true);
            if (!error.isEmpty()) { result(o, error); return; }
            route->clear(); device->clear(); hat->clear();
            const bool pc = textValue(o["platform"]) == "pc";
            const bool windows = textValue(o["platform"]) == "windows";
            audio->labelForField(route)->setVisible(!pc && !windows);
            route->setVisible(!pc && !windows);
            audio->labelForField(hat)->setVisible(!pc && !windows);
            hat->setVisible(!pc && !windows);
            audio->labelForField(mixer)->setVisible(!windows);
            mixer->setVisible(!windows);
            deviceLabel->setText(pc || windows ? "Output device" : "ALSA device");
            if (pc) route->addItem("ALSA playback", "pc-alsa");
            for (const auto &v : pc ? QJsonArray{} : o["routes"].toArray()) {
                const auto r = v.toObject();
                route->addItem(r.isEmpty() ? textValue(v) : textValue(r["label"]), r.isEmpty() ? textValue(v) : textValue(r["id"]));
            }
            device->addItem(windows ? "Windows default output" : pc ? "Automatic (prefer built-in audio)" : "ALSA default", "default");
            for (const auto &v : o["devices"].toArray()) {
                const auto d = v.toObject();
                const auto id = textValue(d["alsa"]);
                if (id == "default") continue;
                device->addItem(textValue(d["label"]), id);
                device->setItemData(device->count() - 1, d["controls"].toArray(), Qt::UserRole + 1);
                device->setItemData(device->count() - 1, textValue(d["kind"]), Qt::UserRole + 2);
            }
            if (pc && device->count() == 1) result({}, "No ALSA playback devices detected. Check the hardware report.");
            for (const auto &v : o["hats"].toArray()) { const auto h = v.toObject(); hat->addItem(textValue(h["label"]), textValue(h["id"])); }
            hat->setEnabled(hat->count() > 0);
            const auto current = o["current"].toObject();
            for (auto pair : {qMakePair(route, QString("route")), qMakePair(device, QString("device")), qMakePair(hat, QString("dac_hat"))}) {
                const int i = pair.first->findData(textValue(current[pair.second])); if (i >= 0) pair.first->setCurrentIndex(i);
            }
            if (pc && device->currentIndex() == 0) {
                const auto oldDevice = textValue(current["device"]);
                const auto match = QRegularExpression("^(?:plug)?hw:(\\d+),(\\d+)$").match(oldDevice);
                if (match.hasMatch()) {
                    for (const auto &value : o["devices"].toArray()) {
                        const auto d = value.toObject();
                        if (d["card"].toInt(-1) == match.captured(1).toInt() && d["device"].toInt(-1) == match.captured(2).toInt())
                            device->setCurrentIndex(device->findData(textValue(d["alsa"])));
                    }
                }
            }
            mixer->setCurrentText(textValue(current["mixer"]));
            refreshControls();
            const int control = mixerControl->findData(textValue(current["mixer_control"]));
            if (control >= 0) mixerControl->setCurrentIndex(control);
            applyOutput->setEnabled(!preview && route->count() > 0);
            }, 20000);
        };
        connect(refreshOutputs, &QPushButton::clicked, this, readOutputs);
        readOutputs();
    };
#ifdef Q_OS_MACOS
    const auto host = api->baseUrl().host();
    if (!api->isStandalone() && (host == "localhost" || QHostAddress(host).isLoopback())) {
        auto *checking = new QLabel("Checking sound output…");
        audio->addRow(checking);
        api->get("/api/system/info", [=](QJsonObject info, QString error) {
            if (!guard) return;
            audio->removeRow(checking);
            if (!error.isEmpty() || textValue(info["os"].toObject()["name"]) != "Darwin") {
                setupApplianceAudio(false);
                return;
            }
            auto *outputs = comboBox(audio);
            outputs->setObjectName("systemSoundOutput");
            audio->addRow("System sound output", outputs);
            auto *description = new QLabel("Playback on this computer follows the system sound output.");
            description->setWordWrap(true);
            audio->addRow(description);
            populateMacOutputs(outputs);
            auto *apply = button(audio, "Set system output", [=] {
                const auto device = AudioDeviceID(outputs->currentData().toUInt());
                const auto error = AudioObjectSetPropertyData(kAudioObjectSystemObject, &defaultOutput, 0, nullptr,
                                                               sizeof(device), &device);
                populateMacOutputs(outputs);
                result({}, error == noErr ? QString() : QString("Could not change the system sound output."));
            });
            apply->setEnabled(outputs->currentIndex() >= 0);
            connect(outputs, QOverload<int>::of(&QComboBox::currentIndexChanged), apply, [apply](int row) { apply->setEnabled(row >= 0); });
            setupApplianceAudio(true);
        });
    } else
#endif
    setupApplianceAudio(false);

    if (api->isStandalone()) {
        auto *system = page("System");
        setupDisplaySettings(system);
#ifdef Q_OS_WIN
        const QString runKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        auto *startup = new QCheckBox("Start PiTunes Native when I sign in");
        const QString value = '"' + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + '"';
        startup->setChecked(QSettings(runKey, QSettings::NativeFormat).value("PiTunes Native").toString() == value);
        system->addRow(startup);
        connect(startup, &QCheckBox::toggled, this, [=](bool checked) {
            QSettings run(runKey, QSettings::NativeFormat);
            if (checked) run.setValue("PiTunes Native", value);
            else if (run.value("PiTunes Native").toString() == value) run.remove("PiTunes Native");
            run.sync();
            if (run.status() != QSettings::NoError) status->setText("Could not update Windows startup.");
        });
#endif
        return;
    }

    auto *network = page("Network");
    auto *netStatus = new QLabel; netStatus->setWordWrap(true); network->addRow(netStatus);
    auto *ssid = field(network, "Wi-Fi name"); auto *password = field(network, "Wi-Fi password", true);
    auto *country = field(network, "Country code"); country->setText("GB");
    const auto refreshWifi = [=] {
        api->get("/api/network/wifi/status", [guard, netStatus](QJsonObject o, QString error) {
            if (!guard) return;
            if (!error.isEmpty()) { netStatus->setText(error); return; }
            const auto diagnostics = o["diagnostics"].toObject();
            const auto iface = textValue(diagnostics["interface"]);
            const auto ip = textValue(o["ip"]);
            QString status = "Network: " + textValue(o["mode"]);
            if (!ip.isEmpty()) status += " · " + ip;
            if (!diagnostics.isEmpty()) {
                status += iface.isEmpty() ? "\nNo Wi-Fi interface detected." : "\nWi-Fi interface: " + iface;
                const auto driver = textValue(diagnostics["driver"]);
                if (!driver.isEmpty()) status += " · " + driver;
                const auto radio = textValue(diagnostics["radio"]);
                if (!radio.isEmpty()) status += " · Radio " + radio;
                if (boolValue(diagnostics["hard_blocked"])) status += "\nWi-Fi blocked by a hardware switch.";
                else if (boolValue(diagnostics["soft_blocked"])) status += "\nWi-Fi is software-blocked.";
                for (const auto &device : diagnostics["pci_devices"].toArray()) {
                    const auto observation = device.toObject();
                    const auto id = textValue(observation["id"]);
                    const auto bound = textValue(observation["driver"]);
                    if (!id.isEmpty()) status += "\nWireless PCI ID: " + id + (bound.isEmpty() ? " · no driver" : " · " + bound);
                }
                if (boolValue(diagnostics["b43_loaded"]) && !boolValue(diagnostics["b43_ucode15_present"]))
                    status += "\nb43 firmware missing. See the Atom hardware guide for offline installation.";
            }
            netStatus->setText(status);
        }, 30000);
    };
    button(network, "Refresh network status", refreshWifi);
    auto *networks = comboBox(network); network->addRow("Available networks", networks);
    connect(networks, QOverload<int>::of(&QComboBox::activated), this, [=](int) { ssid->setText(networks->currentText()); });
    auto *wifiActions = new QHBoxLayout;
    network->addRow(wifiActions);
    button(network, "Scan Wi-Fi", [=] {
        api->get("/api/network/wifi/scan", [guard, networks, result](QJsonObject o, QString error) {
            if (!guard) return;
            result(o, error);
            if (!error.isEmpty()) return;
            networks->clear(); for (const auto &v : o["networks"].toArray()) networks->addItem(textValue(v.toObject()["ssid"]));
        }, 45000);
    }, wifiActions);
    button(network, "Connect Wi-Fi", [=] {
        api->post("/api/network/wifi/connect", {{"ssid", ssid->text()}, {"password", password->text()}, {"country", country->text()}}, result);
        password->clear(); keyboard->hide();
    }, wifiActions);
    wifiActions->addStretch();
    button(network, "Start recovery hotspot", [=] { api->post("/api/network/hotspot/start", {}, result); });
    button(network, "Stop recovery hotspot", [=] { api->post("/api/network/hotspot/stop", {}, result); });

    connect(tabs, &QTabWidget::currentChanged, this, [tabs, network, refreshWifi](int index) {
        if (tabs->widget(index) == network->parentWidget()) refreshWifi();
    });
    auto *nas = page("NAS");
    auto *protocol = comboBox(nas); protocol->addItems({"smb", "nfs"}); nas->addRow("Protocol", protocol);
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
    setupDisplaySettings(system);
    for (const auto &name : {QString("ssh"), QString("bluetooth"), QString("airplay")}) {
        auto *choice = comboBox(system); choice->addItems({"Stopped", "Enabled"}); system->addRow(name, choice);
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
