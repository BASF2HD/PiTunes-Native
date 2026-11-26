#pragma once
#include "networking/ApiClient.h"
#include <QDialog>
namespace PiTunes {
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(ApiClient *api, QWidget *parent = nullptr);
};
}
