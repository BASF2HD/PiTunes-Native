#pragma once
#include "models/Domain.h"
#include "networking/ApiClient.h"
#include <QDialog>
namespace PiTunes {
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(ApiClient *api, CaptionFontSizes fonts, QWidget *parent = nullptr);

signals:
    void captionFontSizesChanged(PiTunes::CaptionFontSizes sizes);
};
}
