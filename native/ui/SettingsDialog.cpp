#include "SettingsDialog.h"
#include "TouchKeyboard.h"
#include "models/Domain.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace PiTunes {
SettingsDialog::SettingsDialog(ApiClient *api, CaptionFontSizes fonts, QWidget *parent) : QDialog(parent) {
    setWindowTitle("PiTunes Settings"); resize(760, 620);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this); layout->addWidget(tabs, 1);
    auto *status = new QLabel(this); status->setWordWrap(true); layout->addWidget(status);
    auto *keyboard = new TouchKeyboard(this); layout->addWidget(keyboard);
    QPointer<SettingsDialog> guard(this);
    const auto result = [guard, status](QJsonObject object, QString error) {
        if (!guard) return;
        status->setText(error.isEmpty() ? textValue(object["message"]).isEmpty() ? "Done" : textValue(object["message"]) : error);
    };
    auto page = [tabs](const QString &title) {
        auto *widget = new QWidget(tabs); tabs->addTab(widget, title);
        auto *form = new QFormLayout(widget); form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); return form;
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
        auto *size = new QSpinBox(this);
        size->setObjectName(name);
        size->setAccessibleName(label + " font size");
        size->setRange(CaptionFontSizes::minimum, CaptionFontSizes::maximum);
        size->setFixedWidth(105);
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
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (!api) {
        layout->addWidget(close);
        return;
    }

    auto *library = page("Library");
    auto *music = field(library, "Music folder");
    auto *source = new QComboBox; source->addItem("USB / local folder", "local"); source->addItem("Internal library", "internal");
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
    auto *route = new QComboBox; auto *device = new QComboBox; auto *hat = new QComboBox;
    auto *mixer = new QComboBox; mixer->addItems({"software", "hardware", "none"});
    audio->addRow("Output", route); audio->addRow("ALSA device", device); audio->addRow("Pi DAC HAT", hat); audio->addRow("Mixer", mixer);
    api->get("/api/audio/devices", [guard, route, device, hat, mixer, result](QJsonObject o, QString error) {
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
    });
    button(audio, "Apply output", [=] {
        api->post("/api/audio/output", {{"output", route->currentData().toString()}, {"alsa", device->currentData().toString()},
            {"dac_hat", hat->currentData().toString()}, {"mixer", mixer->currentText()}},
            [guard, result](QJsonObject o, QString error) {
                result(o, error);
                if (guard && error.isEmpty() && boolValue(o["reboot_required"]))
                    QMessageBox::information(guard, "Audio output", "Output saved. Restart the appliance to complete this change.");
            }, 90000);
    });

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
    auto *networks = new QComboBox; network->addRow("Available networks", networks);
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
    auto *protocol = new QComboBox; protocol->addItems({"smb", "nfs"}); nas->addRow("Protocol", protocol);
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
        auto *choice = new QComboBox; choice->addItems({"Stopped", "Enabled"}); system->addRow(name, choice);
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
    layout->addWidget(close);
}
}
