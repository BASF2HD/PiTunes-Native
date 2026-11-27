#include "TrackDrawer.h"
#include "PlayerIcons.h"
#include "coverflow/CoverFlowWidget.h"
#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QScroller>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace PiTunes {
namespace {
enum TrackRole { SubtitleRole = Qt::UserRole + 1, NumberRole, DurationRole, FileRole, FavouriteRole };
constexpr int rowHeight = 45;
constexpr int numberWidth = 34;
constexpr int durationWidth = 56;
constexpr int favouriteWidth = 32;
constexpr int actionsWidth = 36;
constexpr int textInset = 8;

QRect favouriteRect(const QRect &row) {
    return {row.right() + 1 - actionsWidth - durationWidth - favouriteWidth, row.top(), favouriteWidth, row.height()};
}

class TrackList : public QListView {
public:
    using QListView::QListView;
    std::function<void(int)> playRow;
    std::function<void(const QModelIndex &)> showRowActions;
    std::function<void(int)> favouriteRow;
    std::function<void()> dismiss;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        m_pressIndex = indexAt(event->pos());
        m_pressPoint = event->pos();
        QListView::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        const auto index = indexAt(event->pos());
        const bool clicked = index.isValid() && event->button() == Qt::LeftButton && index == m_pressIndex
            && (event->pos() - m_pressPoint).manhattanLength() < QApplication::startDragDistance();
        QListView::mouseReleaseEvent(event);
        if (!clicked) return;
        const QRect row = visualRect(index);
        if (event->pos().x() >= row.right() - actionsWidth + 1) showRowActions(index);
        else if (favouriteRect(row).contains(event->pos())) favouriteRow(index.row());
        else playRow(index.row());
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        // The first click already opened the track. Ignore its second release.
        m_pressIndex = {};
        event->accept();
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            dismiss();
            return;
        }
        if (currentIndex().isValid()) {
            if (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier) {
                favouriteRow(currentIndex().row());
                return;
            }
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                playRow(currentIndex().row());
                return;
            }
            if (event->key() == Qt::Key_Menu || (event->key() == Qt::Key_F10 && event->modifiers() == Qt::ShiftModifier)) {
                showRowActions(currentIndex());
                return;
            }
        }
        QListView::keyPressEvent(event);
    }

private:
    QModelIndex m_pressIndex;
    QPoint m_pressPoint;
};

QLabel *plainLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    return label;
}
}

class TrackRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    bool setPlayback(const PlayerState &state) {
        const bool playing = state.state == "play";
        const auto file = playing || state.state == "pause" ? state.track.file : QString();
        if (file == m_currentFile && playing == m_playing) return false;
        m_currentFile = file;
        m_playing = playing;
        return true;
    }

    const QString &currentFile() const { return m_currentFile; }
    void setPulseTime(qint64 elapsed) { m_elapsed = elapsed; }
    void setFavouritesReady(bool ready) { m_favouritesReady = ready; }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {0, rowHeight}; }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const QRect row = option.rect;
        const bool current = !m_currentFile.isEmpty() && index.data(FileRole).toString() == m_currentFile;
        painter->save();
        if (current) {
            QLinearGradient glow(row.topLeft(), row.topRight());
            glow.setColorAt(0, QColor(127, 218, 255, 50));
            glow.setColorAt(.48, QColor(95, 121, 255, 26));
            glow.setColorAt(1, QColor(95, 121, 255, 8));
            painter->fillRect(row, glow);
        }
        if (option.state & QStyle::State_Selected) painter->fillRect(row, QColor(145, 161, 206, 31));
        else if (option.state & QStyle::State_MouseOver) painter->fillRect(row, QColor(255, 255, 255, 12));
        if (current) painter->fillRect(QRect(row.topLeft(), QSize(3, row.height())), QColor("#87e0ff"));
        painter->setPen(QColor(255, 255, 255, 14));
        painter->drawLine(row.bottomLeft(), row.bottomRight());

        QFont font = option.font;
        font.setPixelSize(13);
        painter->setFont(font);
        painter->setPen(current ? QColor("#b8c5ff") : QColor("#a7abba"));
        const QRect marker(row.left(), row.top(), numberWidth, row.height());
        if (current) paintMarker(painter, marker.center());
        else painter->drawText(marker, Qt::AlignCenter, index.data(NumberRole).toString());
        const int trailing = row.right() + 1 - actionsWidth;
        painter->drawText(QRect(trailing - durationWidth, row.top(), durationWidth - 5, row.height()),
                          Qt::AlignRight | Qt::AlignVCenter, index.data(DurationRole).toString());
        playerIcon(PlayerIcon::More).paint(painter, QRect(trailing + 10, row.center().y() - 8, 16, 16));
        const auto heart = favouriteRect(row).center();
        const bool hasFile = !index.data(FileRole).toString().isEmpty();
        const auto icon = index.data(FavouriteRole).toBool() ? PlayerIcon::FavouriteFilled : PlayerIcon::Favourite;
        playerIcon(icon).paint(painter, QRect(heart - QPoint(8, 8), QSize(16, 16)), Qt::AlignCenter,
                               m_favouritesReady && hasFile ? QIcon::Normal : QIcon::Disabled);

        const int width = qMax(0, favouriteRect(row).left() - row.left() - numberWidth - textInset);
        QRect text(row.left() + numberWidth, row.top(), width, row.height());
        const auto subtitle = index.data(SubtitleRole).toString();
        if (!subtitle.isEmpty()) {
            font.setPixelSize(12);
            painter->setFont(font);
            painter->setPen(QColor("#a7abba"));
            painter->drawText(text.adjusted(0, 23, 0, -4), Qt::AlignLeft | Qt::AlignVCenter,
                              painter->fontMetrics().elidedText(subtitle, Qt::ElideRight, width));
            text.adjust(0, 3, 0, -21);
        }
        font.setPixelSize(14);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->setPen(QColor("#f4f5fa"));
        painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(index.data().toString(), Qt::ElideRight, width));
        painter->restore();
    }
private:
    void paintMarker(QPainter *painter, const QPoint &center) const {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        QColor cyan("#88e5ff");
        if (m_playing) {
            const int restingHeights[] = {7, 13, 10};
            for (int bar = 0; bar < 3; ++bar) {
                double height = restingHeights[bar];
                if (m_elapsed >= 0) {
                    const double phase = (qMax<qint64>(0, m_elapsed - 140 * bar) % 900) / 900.;
                    const double segment = phase < .45 ? phase / .45 : (1. - phase) / .55;
                    const double pulse = (1. - std::cos(segment * 3.141592653589793)) / 2.;
                    height = 5. + 10. * pulse;
                    cyan.setAlphaF(.68 + .32 * pulse);
                }
                painter->setBrush(cyan);
                painter->drawRoundedRect(QRectF(center.x() - 6.5 + 5 * bar, center.y() + 7.5 - height, 3, height), 1.5, 1.5);
            }
        } else {
            painter->setBrush(cyan);
            QPolygonF triangle;
            triangle << center + QPoint(-4, -5) << center + QPoint(4, 0) << center + QPoint(-4, 5);
            painter->drawPolygon(triangle);
        }
        painter->restore();
    }

    QString m_currentFile;
    qint64 m_elapsed = -1;
    bool m_playing = false;
    bool m_favouritesReady = false;
};

TrackDrawer::TrackDrawer(CoverFlowWidget *stage, const QString &title, const QString &year,
                         const QVector<Track> &tracks, bool albumContext)
    : FrostedPanel(stage, QColor(10, 10, 20, 200), 29), m_stage(stage), m_heading(albumWithYear(title, year)) {
    setObjectName("trackDrawer");
    setAccessibleName(m_heading);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_StyledBackground);
    setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    setStyleSheet(
        "QFrame#trackDrawer { background: transparent; border: 0; }"
        "QLabel { background: transparent; border: 0; color: #aab0c2; font-size: 12px; }"
        "QLabel#drawerTitle { color: #f4f5fa; font-size: 15px; font-weight: 600; }"
        "QLabel#drawerCount { font-size: 13px; }"
        "QPushButton { background: #222430; border: 1px solid #353748; border-radius: 15px; padding: 0; }"
        "QPushButton:hover, QPushButton:focus { background: #34384d; border-color: #828aa7; }"
        "QListView { background: transparent; border: 0; outline: 0; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,58); min-height: 20px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(10, 7, 7, 7);
    header->setSpacing(5);
    auto *heading = new QVBoxLayout;
    heading->setSpacing(1);
    m_title = plainLabel(m_heading, this);
    m_title->setObjectName("drawerTitle");
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_title->setToolTip(m_heading);
    heading->addWidget(m_title);
    auto *count = plainLabel(tracks.size() == 1 ? "1 song" : QString("%1 songs").arg(tracks.size()), this);
    count->setObjectName("drawerCount");
    heading->addWidget(count);
    header->addLayout(heading, 1);
    auto iconButton = [this, header](PlayerIcon glyph, const QString &name) {
        auto *button = new QPushButton(playerIcon(glyph), QString(), this);
        button->setFixedSize(32, 30);
        button->setIconSize(QSize(17, 17));
        button->setAccessibleName(name);
        button->setToolTip(name);
        header->addWidget(button);
        return button;
    };
    if (albumContext) {
        m_heart = iconButton(PlayerIcon::Favourite, "Favourite album");
        m_heart->setCheckable(true);
        connect(m_heart, &QPushButton::clicked, this, &TrackDrawer::albumFavouriteRequested);
    }
    connect(iconButton(PlayerIcon::Close, "Close songs"), &QPushButton::clicked, this, &TrackDrawer::dismissRequested);
    layout->addLayout(header);

    auto *columns = new QHBoxLayout;
    columns->setContentsMargins(0, 3, actionsWidth + 5, 6);
    auto column = [this](const QString &text) {
        auto *label = plainLabel(text, this);
        QFont font = label->font();
        font.setPixelSize(11);
        font.setWeight(QFont::DemiBold);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        label->setFont(font);
        label->setStyleSheet("font-size: 11px;");
        return label;
    };
    auto *number = column("#");
    number->setFixedWidth(numberWidth);
    number->setAlignment(Qt::AlignCenter);
    columns->addWidget(number);
    columns->addWidget(column("TITLE"), 1);
    columns->addWidget(column("TIME"));
    layout->addLayout(columns);

    auto *list = new TrackList(this);
    m_list = list;
    m_list->setObjectName("trackDrawerList");
    m_list->setAccessibleName("Songs");
    m_list->setAccessibleDescription("Press Space on a song to toggle its favourite heart.");
    m_list->setUniformItemSizes(true);
    m_list->viewport()->setAutoFillBackground(false);
    m_list->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(m_list->viewport(), QScroller::TouchGesture);
    auto *model = new QStandardItemModel(m_list);
    for (int i = 0; i < tracks.size(); ++i) {
        const auto &track = tracks[i];
        auto *item = new QStandardItem(track.title.isEmpty() ? QString("Unknown song") : track.title);
        item->setEditable(false);
        QString subtitle = track.artist;
        if (!albumContext && !track.album.isEmpty()) {
            if (!subtitle.isEmpty()) subtitle += " · ";
            subtitle += albumWithYear(track.album, track.year);
        }
        item->setData(subtitle, SubtitleRole);
        const auto number = track.trackNumber.section('/', 0, 0).trimmed();
        item->setData(albumContext && number.toInt() > 0 ? number : QString::number(i + 1), NumberRole);
        const auto duration = track.duration > 0 ? timeLabel(track.duration) : QString("–:–");
        item->setData(duration, DurationRole);
        item->setData(track.file, FileRole);
        item->setData(item->text() + ", " + subtitle + ", " + duration, Qt::AccessibleTextRole);
        item->setToolTip(subtitle.isEmpty() ? item->text() : item->text() + "\n" + subtitle);
        model->appendRow(item);
    }
    m_list->setModel(model);
    m_rowDelegate = new TrackRowDelegate(m_list);
    m_list->setItemDelegate(m_rowDelegate);
    m_pulse.setInterval(33);
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, &TrackDrawer::syncPulse);
    connect(&m_pulse, &QTimer::timeout, this, [this] {
        m_rowDelegate->setPulseTime(m_pulseTime.elapsed());
        for (const int row : m_activeRows) {
            QRect marker = m_list->visualRect(m_list->model()->index(row, 0));
            marker.setWidth(numberWidth);
            marker &= m_list->viewport()->rect();
            if (!marker.isEmpty()) m_list->viewport()->update(marker);
        }
    });
    layout->addWidget(m_list, 1);
    if (tracks.isEmpty()) {
        auto *empty = plainLabel("No songs", this);
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty, 1);
        m_list->hide();
    }
    list->playRow = [this](int row) { emit playRequested(row); };
    list->favouriteRow = [this](int row) { emit trackFavouriteRequested(row); };
    list->showRowActions = [this](const QModelIndex &index) { showTrackActions(index); };
    list->dismiss = [this] { emit dismissRequested(); };
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &TrackDrawer::dismissRequested);
    connect(m_stage, &CoverFlowWidget::captionLayoutChanged, this, [this] { if (isVisible()) place(); });
}

void TrackDrawer::open() {
    show();
    place();
    raise();
    if (m_list->model()->rowCount() > 0) m_list->setCurrentIndex(m_list->model()->index(0, 0));
    m_list->setFocus();
    syncPulse();
}

void TrackDrawer::hideEvent(QHideEvent *event) {
    m_pulse.stop();
    QFrame::hideEvent(event);
}

void TrackDrawer::place() {
    const auto cover = m_stage->centreCoverRect();
    const int drawerWidth = qMin(m_stage->width(), qMax(348, cover.width()));
    const int drawerHeight = qMin(m_stage->height() - cover.top() - 58, qMax(140, cover.height()));
    setGeometry((m_stage->width() - drawerWidth) / 2, cover.top(), drawerWidth, drawerHeight);
    m_stage->setCaptionClearance(geometry().bottom() + 1);
    layout()->activate();
    m_title->setText(m_title->fontMetrics().elidedText(m_heading, Qt::ElideMiddle, m_title->width()));
    syncPulse();
}

void TrackDrawer::setPlayback(const PlayerState &state) {
    if (!m_rowDelegate->setPlayback(state)) return;
    m_playing = state.state == "play";
    m_activeRows.clear();
    const auto *model = m_list->model();
    if (!m_rowDelegate->currentFile().isEmpty()) {
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->index(row, 0).data(FileRole).toString() == m_rowDelegate->currentFile()) m_activeRows.push_back(row);
        }
    }
    syncPulse();
    m_list->viewport()->update();
}

void TrackDrawer::setReducedMotion(bool reduced) {
    m_reduced = reduced;
    syncPulse();
    m_list->viewport()->update();
}

void TrackDrawer::setAlbumFavourite(bool starred, bool ready) {
    if (!m_heart) return;
    m_heart->setIcon(playerIcon(starred ? PlayerIcon::FavouriteFilled : PlayerIcon::Favourite));
    const QString label = starred ? "Remove favourite album" : "Favourite album";
    m_heart->setAccessibleName(label);
    m_heart->setToolTip(label);
    m_heart->setChecked(starred);
    m_heart->setEnabled(ready);
}

void TrackDrawer::setTrackFavourites(const QSet<QString> &files, bool ready) {
    auto *model = m_list->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        const bool starred = files.contains(index.data(FileRole).toString());
        const bool previous = index.data(FavouriteRole).toBool();
        if (previous == starred) continue;
        auto summary = index.data(Qt::AccessibleTextRole).toString();
        const QString suffix = ", Favourite";
        if (previous) summary.chop(suffix.size());
        model->setData(index, starred, FavouriteRole);
        model->setData(index, starred ? summary + suffix : summary, Qt::AccessibleTextRole);
    }
    m_rowDelegate->setFavouritesReady(ready);
    m_list->viewport()->update();
}
void TrackDrawer::syncPulse() {
    if (isVisible() && m_playing && !m_reduced) {
        for (const int row : m_activeRows) {
            const QRect marker = m_list->visualRect(m_list->model()->index(row, 0));
            if (!marker.intersects(m_list->viewport()->rect())) continue;
            if (!m_pulse.isActive()) {
                m_pulseTime.start();
                m_rowDelegate->setPulseTime(0);
                m_pulse.start();
            }
            return;
        }
    }
    m_pulse.stop();
    m_rowDelegate->setPulseTime(-1);
}

void TrackDrawer::showTrackActions(const QModelIndex &index) {
    const auto rect = m_list->visualRect(index);
    emit trackActionsRequested(index.row(), m_list->viewport()->mapToGlobal(rect.bottomRight() - QPoint(actionsWidth, 0)));
}
} // namespace PiTunes
