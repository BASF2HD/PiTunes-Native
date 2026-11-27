#include "TrackDrawer.h"
#include "coverflow/CoverFlowWidget.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScroller>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <functional>

namespace PiTunes {
namespace {
enum TrackRole { SubtitleRole = Qt::UserRole + 1, NumberRole, DurationRole, FileRole };
constexpr int rowHeight = 45;
constexpr int numberWidth = 34;
constexpr int durationWidth = 56;
constexpr int actionsWidth = 36;
constexpr int textInset = 8;

class TrackList : public QListView {
public:
    using QListView::QListView;
    std::function<void(int)> playRow;
    std::function<void(const QModelIndex &)> showRowActions;
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
        if (event->pos().x() >= visualRect(index).right() - actionsWidth + 1) showRowActions(index);
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

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {0, rowHeight}; }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const QRect row = option.rect;
        const bool current = !m_currentFile.isEmpty() && index.data(FileRole).toString() == m_currentFile;
        painter->save();
        if (option.state & QStyle::State_Selected) painter->fillRect(row, QColor("#30354c"));
        else if (current) painter->fillRect(row, QColor("#242a3c"));
        else if (option.state & QStyle::State_MouseOver) painter->fillRect(row, QColor("#212330"));
        painter->setPen(QColor("#303241"));
        painter->drawLine(row.bottomLeft(), row.bottomRight());

        QFont font = option.font;
        font.setPixelSize(12);
        painter->setFont(font);
        painter->setPen(current ? QColor("#b8c5ff") : QColor("#a7abba"));
        painter->drawText(QRect(row.left(), row.top(), numberWidth, row.height()), Qt::AlignCenter,
                          current ? (m_playing ? "▶" : "Ⅱ") : index.data(NumberRole).toString());
        const int trailing = row.right() + 1 - actionsWidth;
        painter->drawText(QRect(trailing - durationWidth, row.top(), durationWidth - 5, row.height()),
                          Qt::AlignRight | Qt::AlignVCenter, index.data(DurationRole).toString());
        font.setPixelSize(17);
        painter->setFont(font);
        painter->drawText(QRect(trailing, row.top(), actionsWidth, row.height()), Qt::AlignCenter, "⋯");

        const int width = qMax(0, trailing - durationWidth - row.left() - numberWidth - textInset);
        QRect text(row.left() + numberWidth, row.top(), width, row.height());
        const auto subtitle = index.data(SubtitleRole).toString();
        if (!subtitle.isEmpty()) {
            font.setPixelSize(11);
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
    QString m_currentFile;
    bool m_playing = false;
};

TrackDrawer::TrackDrawer(CoverFlowWidget *stage, const QString &title, const QString &year,
                         const QVector<Track> &tracks, bool albumContext)
    : QFrame(stage), m_stage(stage), m_heading(albumWithYear(title, year)) {
    setObjectName("trackDrawer");
    setAccessibleName(m_heading);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(
        "QFrame#trackDrawer { background: #151620; border: 1px solid #3b3d4d; border-radius: 6px; }"
        "QLabel { background: transparent; border: 0; color: #aab0c2; font-size: 12px; }"
        "QLabel#drawerTitle { color: #f4f5fa; font-size: 15px; font-weight: 600; }"
        "QPushButton { background: #222430; border: 1px solid #353748; padding: 0; font-size: 18px; }"
        "QPushButton:hover, QPushButton:focus { background: #34384d; border-color: #828aa7; }"
        "QListView { background: #151620; border: 0; outline: 0; }"
        "QScrollBar:vertical { background: #151620; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #4b4f64; min-height: 20px; border-radius: 3px; }"
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
    heading->addWidget(plainLabel(tracks.size() == 1 ? "1 song" : QString("%1 songs").arg(tracks.size()), this));
    header->addLayout(heading, 1);
    auto iconButton = [this, header](const QString &symbol, const QString &name) {
        auto *button = new QPushButton(symbol, this);
        button->setFixedSize(32, 30);
        button->setAccessibleName(name);
        button->setToolTip(name);
        header->addWidget(button);
        return button;
    };
    if (albumContext) {
        connect(iconButton("♡", "Favourite album"), &QPushButton::clicked, this, &TrackDrawer::albumFavouriteRequested);
    }
    connect(iconButton("×", "Close songs"), &QPushButton::clicked, this, &TrackDrawer::dismissRequested);
    layout->addLayout(header);

    auto *columns = new QHBoxLayout;
    columns->setContentsMargins(0, 3, actionsWidth + 5, 6);
    auto *number = plainLabel("#", this);
    number->setFixedWidth(numberWidth);
    number->setAlignment(Qt::AlignCenter);
    columns->addWidget(number);
    columns->addWidget(plainLabel("Title", this), 1);
    columns->addWidget(plainLabel("Time", this));
    layout->addLayout(columns);

    auto *list = new TrackList(this);
    m_list = list;
    m_list->setObjectName("trackDrawerList");
    m_list->setAccessibleName("Songs");
    m_list->setUniformItemSizes(true);
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
    layout->addWidget(m_list, 1);
    if (tracks.isEmpty()) {
        auto *empty = plainLabel("No songs", this);
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty, 1);
        m_list->hide();
    }
    list->playRow = [this](int row) { emit playRequested(row); };
    list->showRowActions = [this](const QModelIndex &index) { showTrackActions(index); };
    list->dismiss = [this] { emit dismissRequested(); };
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &TrackDrawer::dismissRequested);
    m_stage->installEventFilter(this);
}

void TrackDrawer::open() {
    show();
    place();
    raise();
    if (m_list->model()->rowCount() > 0) m_list->setCurrentIndex(m_list->model()->index(0, 0));
    m_list->setFocus();
}

bool TrackDrawer::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_stage && event->type() == QEvent::Resize && isVisible()) place();
    return QFrame::eventFilter(watched, event);
}

void TrackDrawer::place() {
    const auto cover = m_stage->centreCoverRect();
    const int drawerWidth = qMin(m_stage->width(), qMax(316, cover.width()));
    const int drawerHeight = qMin(m_stage->height() - cover.top() - 58, qMax(230, cover.height()));
    setGeometry((m_stage->width() - drawerWidth) / 2, cover.top(), drawerWidth, drawerHeight);
    m_stage->setCaptionClearance(geometry().bottom() + 1);
    layout()->activate();
    m_title->setText(m_title->fontMetrics().elidedText(m_heading, Qt::ElideMiddle, m_title->width()));
}

void TrackDrawer::setPlayback(const PlayerState &state) {
    if (m_rowDelegate->setPlayback(state)) m_list->viewport()->update();
}

void TrackDrawer::showTrackActions(const QModelIndex &index) {
    auto *menu = new QMenu(this);
    menu->setStyleSheet("QMenu { background: #202232; color: #f0f1f7; } QMenu::item:selected { background: #38415d; }");
    const int row = index.row();
    menu->addAction("Play song", this, [this, row] { emit playRequested(row); });
    menu->addAction("Favourite song", this, [this, row] { emit trackFavouriteRequested(row, true); });
    menu->addAction("Remove song from favourites", this, [this, row] { emit trackFavouriteRequested(row, false); });
    menu->addSeparator();
    menu->addAction("Save list as playlist…", this, &TrackDrawer::savePlaylistRequested);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    const auto rect = m_list->visualRect(index);
    menu->popup(m_list->viewport()->mapToGlobal(rect.bottomRight() - QPoint(actionsWidth, 0)));
}
} // namespace PiTunes
