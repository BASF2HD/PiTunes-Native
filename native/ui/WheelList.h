#pragma once

#include <QListView>
#include <QScrollBar>
#include <QScroller>
#include <QWheelEvent>

namespace PiTunes {
class WheelList : public QListView {
public:
    using QListView::QListView;

protected:
    void wheelEvent(QWheelEvent *event) override {
        // A native wheel gesture owns the list. Stop a previous touchscreen
        // fling before it can overwrite the scroll position between packets.
        if (QScroller::hasScroller(viewport())) QScroller::scroller(viewport())->stop();
        if (!event->pixelDelta().isNull()) {
            auto *bar = verticalScrollBar();
            bar->setValue(bar->value() - event->pixelDelta().y());
        } else if (event->phase() == Qt::NoScrollPhase || event->phase() == Qt::ScrollUpdate) {
            QListView::wheelEvent(event);
        }
        // Keep the whole gesture in the list, including momentum at either end.
        event->accept();
    }
};
}
