#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QLabel>
#include <QString>
#include <QTimer>
#include <memory>

namespace PiTunes {
class CoverFlowWidget;

enum class MonitorMode { Off, Compact, Detailed };

class PerformanceMonitor : public QLabel {
    Q_OBJECT
public:
    PerformanceMonitor(CoverFlowWidget *flow, QWidget *parent);
    ~PerformanceMonitor() override;
    MonitorMode mode() const { return m_mode; }
    void setMode(MonitorMode mode);
    bool recording() const { return !m_csvPath.isEmpty(); }
    void startRecording(const QString &path);
    void stopRecording();

signals:
    void modeChanged(int mode);
    void recordingChanged(bool recording);
    void recordingSaved(QString path, bool success);

private:
    void refresh();
    void saveRecording();
    struct Sampler;
    std::unique_ptr<Sampler> m_sampler;
    CoverFlowWidget *m_flow;
    QTimer m_timer;
    QElapsedTimer m_interval;
    MonitorMode m_mode = MonitorMode::Off;
    QString m_csvPath;
    QByteArray m_csv;
    int m_rows = 0;
};
} // namespace PiTunes
