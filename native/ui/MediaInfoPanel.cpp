#include "MediaInfoPanel.h"
#include "PlayerIcons.h"
#include "coverflow/CoverFlowWidget.h"
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QScroller>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

namespace PiTunes {
namespace {
QString displayDuration(double duration) {
    if (duration <= 0.) return {};
    const int seconds = int(qMin(duration, 864000.));
    if (seconds < 3600) return timeLabel(seconds);
    return QString::number(seconds / 3600) + ":" + timeLabel(seconds % 3600).rightJustified(5, '0');
}

QLabel *infoLabel(const QString &text, const QString &name, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    return label;
}
}

MediaInfoPanel::MediaInfoPanel(CoverFlowWidget *stage, const QString &eyebrow, const QString &title)
    : FrostedPanel(stage, QColor(14, 16, 26, 185), 16) {
    setObjectName("mediaInfo");
    setAccessibleName(eyebrow);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(
        "QFrame#mediaInfo, QWidget#mediaInfoRows, QScrollArea#mediaInfoScroll { background: transparent; border: 0; }"
        "QLabel { background: transparent; }"
        "QLabel#detailKey { color: #b6c2e3; font-size: 11px; font-weight: 600; }"
        "QLabel#detailTitle { color: #f5f6fc; font-size: 16px; font-weight: 600; }"
        "QLabel#detailValue { color: #f0f3fc; font-size: 13px; }"
        "QToolButton#closeMediaInfo { background: rgba(10,12,20,94);"
        "border: 1px solid rgba(225,232,245,58); border-radius: 15px; }"
        "QToolButton#closeMediaInfo:hover, QToolButton#closeMediaInfo:focus { background: rgba(225,232,245,49); }"
        "QScrollBar:vertical { width: 8px; background: transparent; }"
        "QScrollBar::handle:vertical { min-height: 20px; border-radius: 3px; background: rgba(255,255,255,58); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(infoLabel(eyebrow.toUpper(), "detailKey", this));
    auto *titleLabel = infoLabel(title, "detailTitle", this);
    titleLabel->setWordWrap(true);
    titles->addWidget(titleLabel);
    header->addLayout(titles, 1);
    auto *close = new QToolButton(this);
    close->setObjectName("closeMediaInfo");
    close->setAccessibleName("Close details");
    close->setToolTip(close->accessibleName());
    close->setFixedSize(30, 30);
    close->setIcon(playerIcon(PlayerIcon::Close));
    header->addWidget(close);
    layout->addLayout(header);
    connect(close, &QToolButton::clicked, this, &MediaInfoPanel::dismissRequested);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &MediaInfoPanel::dismissRequested);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("mediaInfoScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    auto *fields = new QWidget;
    fields->setObjectName("mediaInfoRows");
    m_fields = new QGridLayout(fields);
    m_fields->setContentsMargins(0, 4, 3, 0);
    m_fields->setHorizontalSpacing(12);
    m_fields->setVerticalSpacing(6);
    m_fields->setAlignment(Qt::AlignTop);
    m_fields->setColumnStretch(1, 1);
    scroll->setWidget(fields);
    layout->addWidget(scroll, 1);
}

void MediaInfoPanel::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        emit dismissRequested();
        return;
    }
    FrostedPanel::keyPressEvent(event);
}

void MediaInfoPanel::setRows(const QVector<QPair<QString, QString>> &rows) {
    while (auto *item = m_fields->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    auto *parent = m_fields->parentWidget();
    for (int row = 0; row < rows.size(); ++row) {
        const auto &[name, value] = rows[row];
        auto *key = infoLabel(name.toUpper(), "detailKey", parent);
        auto *text = infoLabel(value.isEmpty() ? QString("Unknown") : value, "detailValue", parent);
        text->setAccessibleName(name);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        text->setWordWrap(true);
        text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        m_fields->addWidget(key, row, 0, Qt::AlignTop);
        m_fields->addWidget(text, row, 1);
    }
}

void MediaInfoPanel::setTrack(const Track &track, int row) {
    const QString number = track.trackNumber.isEmpty() && row >= 0 ? QString::number(row + 1) : track.trackNumber;
    const QString codec = track.codec.isEmpty() ? QFileInfo(track.file).suffix().toUpper() : track.codec.toUpper();
    const QString singer = track.singer.isEmpty() && !track.artist.isEmpty()
        ? track.artist + " (Artist)" : track.singer;
    setRows({
        {"Nr", number}, {"Title", track.title}, {"Artist", track.artist},
        {"Album Artists", track.albumArtist}, {"Singer", singer}, {"Composer", track.composer},
        {"Album", track.album}, {"Duration", displayDuration(track.duration)}, {"Codec", codec},
        {"Bitrate", track.bitRate > 0. ? QString::number(qRound(track.bitRate)) + " kbps" : QString()},
        {"Genre", track.genre}, {"Date", track.year}, {"Path", track.file}
    });
}

void MediaInfoPanel::setAlbum(const Album &album, const QVector<Track> *tracks) {
    QStringList artists, singers, composers, albumArtists;
    double duration = 0.;
    if (tracks) {
        for (const auto &track : *tracks) {
            if (!track.artist.isEmpty() && !artists.contains(track.artist)) artists.append(track.artist);
            if (!track.singer.isEmpty() && !singers.contains(track.singer)) singers.append(track.singer);
            if (!track.composer.isEmpty() && !composers.contains(track.composer)) composers.append(track.composer);
            if (!track.albumArtist.isEmpty() && !albumArtists.contains(track.albumArtist)) albumArtists.append(track.albumArtist);
            duration += qMax(0., track.duration);
        }
    }
    const QString albumArtist = albumArtists.isEmpty() ? album.artist : albumArtists.join(", ");
    const QString composer = composers.isEmpty() && !albumArtist.isEmpty()
        ? albumArtist + " (Album Artist)" : composers.join(", ");
    setRows({
        {"Title", album.title}, {"Album Artists", albumArtist}, {"Artist", artists.join(", ")},
        {"Singer", singers.join(", ")}, {"Composer", composer},
        {"Songs", tracks ? QString::number(tracks->size()) : QString()},
        {"Duration", displayDuration(duration)}, {"Genre", album.genre}, {"Date", album.year}
    });
}
} // namespace PiTunes
