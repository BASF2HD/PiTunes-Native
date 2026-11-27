#pragma once

#include <QListView>
#include <QScrollBar>
#include <QWheelEvent>

namespace PiTunes {
class WheelList : public QListView {
public:
    using QListView::QListView;

protected:
    void wheelEvent(QWheelEvent *event) override {
        if (!event->pixelDelta().isNull()) {
            auto *bar = verticalScrollBar();
            bar->setValue(bar->value() - event->pixelDelta().y());
        } else {
            QListView::wheelEvent(event);
        }
        // Keep the whole gesture in the list, including momentum at either end.
        event->accept();
    }
};
}
