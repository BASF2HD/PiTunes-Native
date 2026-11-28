#pragma once
#include "coverflow/CoverFlowWidget.h"
#include "networking/ApiClient.h"
#include <QDialog>
namespace PiTunes {
class PerformanceMonitor;
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(ApiClient *api, CaptionFontSizes fonts, QWidget *parent = nullptr,
                   AnimationMode animation = AnimationMode::Full, PerformanceMonitor *monitor = nullptr,
                   bool forceReducedMotion = false);

signals:
    void animationModeChanged(AnimationMode mode);
    void captionFontSizesChanged(PiTunes::CaptionFontSizes sizes);
};
}
