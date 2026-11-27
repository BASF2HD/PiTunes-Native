#pragma once

#include <QFrame>
#include <QImage>
#include <QMenu>
#include <QPointer>
#include <QTimer>

namespace PiTunes {
class CoverFlowWidget;

class FrostedPanel : public QFrame {
public:
    FrostedPanel(CoverFlowWidget *stage, const QColor &tint, int edgeAlpha = 10, int radius = 0);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPointer<CoverFlowWidget> m_stage;
    QColor m_tint;
    int m_edgeAlpha;
    int m_radius;
    QImage m_backdrop;
    QTimer m_refresh;
};

class FrostedMenu : public QMenu {
public:
    FrostedMenu(CoverFlowWidget *stage, QWidget *parent);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPointer<CoverFlowWidget> m_stage;
    QImage m_backdrop;
    QTimer m_refresh;
};
} // namespace PiTunes
