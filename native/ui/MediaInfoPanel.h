#pragma once

#include "FrostedSurface.h"
#include "models/Domain.h"

class QGridLayout;

namespace PiTunes {
class MediaInfoPanel : public FrostedPanel {
    Q_OBJECT
public:
    MediaInfoPanel(CoverFlowWidget *stage, const QString &eyebrow, const QString &title);
    void setTrack(const Track &track, int row);
    void setAlbum(const Album &album, const QVector<Track> *tracks = nullptr);

signals:
    void dismissRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setRows(const QVector<QPair<QString, QString>> &rows);
    QGridLayout *m_fields;
};
} // namespace PiTunes
