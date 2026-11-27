#include "RadioDialog.h"
#include "PlayerIcons.h"
#include "models/Domain.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QScroller>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

namespace PiTunes {
namespace {
constexpr int stationRole = Qt::UserRole + 1;
constexpr int logoRole = Qt::UserRole + 2;
constexpr int stationHeight = 56;
constexpr int pageSize = 24;
constexpr int maxSearchRows = 240;

QString streamUrl(const QJsonObject &station) {
    const auto stream = textValue(station["streamUrl"]);
    return stream.isEmpty() ? textValue(station["url"]) : stream;
}

QString logoPath(const QJsonObject &station) {
    auto logo = textValue(station["artUrl"]);
    if (logo.isEmpty()) logo = textValue(station["favicon"]);
    const QUrl url(logo);
    if ((url.scheme() != "https" && url.scheme() != "http") || url.host().isEmpty() || !url.userInfo().isEmpty()) return {};
    return "/api/library/radio/icon?wait=1&url=" + QString::fromUtf8(QUrl::toPercentEncoding(logo));
}

class StationDelegate : public QStyledItemDelegate {
public:
    StationDelegate(ArtworkCache &artwork, QListView *parent) : QStyledItemDelegate(parent), m_artwork(artwork) {}

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {0, stationHeight}; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto station = index.data(stationRole).toJsonObject();
        const auto name = index.data().toString();
        QStringList details;
        const auto genre = textValue(station["genre"]);
        details << (genre.isEmpty() ? QString("Internet radio") : genre);
        for (const auto key : {"country", "codec"}) {
            const auto value = textValue(station[key]);
            if (!value.isEmpty() && value.compare("unknown", Qt::CaseInsensitive) != 0) details << value;
        }
        const QRect row = option.rect;
        const QRect logo(row.left() + 9, row.top() + 8, 40, 40);
        painter->save();
        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        if (option.state & QStyle::State_Selected) painter->fillRect(row, QColor("#30394a"));
        else if (option.state & QStyle::State_MouseOver) painter->fillRect(row, QColor("#252c39"));
        painter->setPen(QColor("#2d3442"));
        painter->drawLine(row.bottomLeft(), row.bottomRight());
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#424c60"));
        painter->drawRoundedRect(logo, 4, 4);
        const auto *art = m_artwork.find(index.data(logoRole).toString());
        painter->setPen(QColor("#f4f5fa"));
        if (art) {
            const QSize size = art->cover.size().scaled(logo.size(), Qt::KeepAspectRatio);
            QRect image(QPoint(), size);
            image.moveCenter(logo.center());
            painter->drawImage(image, art->cover);
        } else {
            const auto words = name.simplified().split(' ');
            const auto initials = words.size() > 1 ? words[0].left(1) + words[1].left(1) : name.left(2);
            painter->drawText(logo, Qt::AlignCenter, initials.toUpper());
        }
        const auto text = row.adjusted(59, 7, -9, -7);
        auto font = option.font;
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(text, Qt::AlignTop, painter->fontMetrics().elidedText(name, Qt::ElideRight, text.width()));
        font.setPixelSize(12);
        font.setBold(false);
        painter->setFont(font);
        painter->setPen(QColor("#b5bfd2"));
        painter->drawText(text, Qt::AlignBottom, painter->fontMetrics().elidedText(details.join(" · "), Qt::ElideRight, text.width()));
        painter->restore();
    }
private:
    ArtworkCache &m_artwork;
};
}

RadioDialog::RadioDialog(ApiClient *api, QWidget *parent)
    : QDialog(parent), m_api(api), m_artwork(api, 4, this, 64) {
    setWindowTitle("Internet radio");
    setObjectName("radioDialog");
    resize(560, 430);
    setStyleSheet(
        "QDialog#radioDialog { color: #f4f5fa; background: #12151d; }"
        "QLabel { background: transparent; color: #bac3d3; }"
        "QListView { background: #191e28; border: 1px solid #333d4f; outline: 0; }"
        "QLineEdit, QPushButton { background: #202631; border: 1px solid #404a5c; border-radius: 4px; padding: 6px 8px; }"
        "QPushButton:hover, QPushButton:focus { border-color: #8393b0; }"
        "QPushButton:disabled { color: #8993a5; }");
    auto *layout = new QVBoxLayout(this);
    auto *search = new QHBoxLayout;
    m_query = new QLineEdit(this);
    m_query->setPlaceholderText("Search radio stations");
    m_query->setAccessibleName("Search radio stations");
    m_query->setClearButtonEnabled(true);
    m_query->setMaxLength(160);
    auto *find = new QPushButton(playerIcon(PlayerIcon::Search), "Search", this);
    auto *saved = new QPushButton("Saved stations", this);
    search->addWidget(m_query, 1);
    search->addWidget(find);
    search->addWidget(saved);
    layout->addLayout(search);

    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_list = new QListView(this);
    m_list->setAccessibleName("Radio stations");
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->viewport()->installEventFilter(this);
    QScroller::grabGesture(m_list->viewport(), QScroller::TouchGesture);
    m_model = new QStandardItemModel(m_list);
    m_list->setModel(m_model);
    m_list->setItemDelegate(new StationDelegate(m_artwork, m_list));
    layout->addWidget(m_list, 1);

    auto *actions = new QHBoxLayout;
    m_more = new QPushButton("Load more", this);
    m_play = new QPushButton(playerIcon(PlayerIcon::Play), "Play station", this);
    m_save = new QPushButton("Save station", this);
    auto *close = new QPushButton("Close", this);
    for (auto *button : {find, saved, m_more, m_play, m_save, close}) button->setAutoDefault(false);
    actions->addWidget(m_more);
    actions->addStretch();
    actions->addWidget(m_play);
    actions->addWidget(m_save);
    actions->addWidget(close);
    layout->addLayout(actions);

    const auto submit = [this] { loadStations(m_query->text().trimmed()); };
    connect(find, &QPushButton::clicked, this, submit);
    connect(m_query, &QLineEdit::returnPressed, this, submit);
    connect(saved, &QPushButton::clicked, this, [this] { loadStations({}); });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_play, &QPushButton::clicked, this, &RadioDialog::playSelected);
    connect(m_list, &QListView::activated, this, &RadioDialog::playSelected);
    connect(m_save, &QPushButton::clicked, this, &RadioDialog::saveSelected);
    connect(m_more, &QPushButton::clicked, this, [this] { loadStations(m_search, m_nextOffset); });
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, &RadioDialog::syncActions);
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, &RadioDialog::prefetchVisible);
    connect(&m_artwork, &ArtworkCache::ready, m_list->viewport(), QOverload<>::of(&QWidget::update));
    connect(this, &QDialog::finished, this, [this] {
        ++m_generation;
        if (m_listReply) m_listReply->abort();
    });
    loadStations({});
    m_query->setFocus();
}

void RadioDialog::loadStations(const QString &query, int offset) {
    const auto generation = ++m_generation;
    if (m_listReply) m_listReply->abort();
    m_search = query;
    m_more->setVisible(false);
    if (offset == 0) {
        m_model->clear();
        m_artwork.prefetch({});
    }
    const bool saved = query.isEmpty();
    m_status->setText(saved ? "Loading saved stations…" : "Searching stations…");
    const auto path = saved ? QString("/api/library/radio")
        : QString("/api/library/radio/search?limit=%1&offset=%2&q=%3")
            .arg(pageSize).arg(offset).arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
    const QPointer<RadioDialog> guard(this);
    m_listReply = m_api->get(path, [guard, generation, saved, offset](QJsonObject response, QString error) {
        if (!guard || guard->m_generation != generation) return;
        guard->m_listReply.clear();
        if (!error.isEmpty()) {
            guard->m_status->setText(error);
            guard->m_more->setVisible(offset > 0);
            return;
        }
        const auto stations = response["stations"].toArray();
        if (saved) {
            guard->m_savedUuids.clear();
            guard->m_savedUrls.clear();
        }
        for (const auto &value : stations) {
            const auto station = value.toObject();
            if (streamUrl(station).isEmpty()) continue;
            if (saved) guard->rememberSaved(station);
            auto *item = new QStandardItem(textValue(station["name"]));
            item->setEditable(false);
            item->setData(station, stationRole);
            item->setData(logoPath(station), logoRole);
            guard->m_model->appendRow(item);
        }
        if (!guard->m_list->currentIndex().isValid()) guard->m_list->setCurrentIndex(guard->m_model->index(0, 0));
        guard->m_nextOffset = offset + pageSize;
        guard->m_more->setVisible(!saved && boolValue(response["hasMore"]) && guard->m_nextOffset < maxSearchRows);
        guard->m_status->setText(guard->m_model->rowCount() ? (saved ? "Saved stations" : "Search results")
                                                          : (saved ? "No saved stations" : "No stations found"));
        guard->syncActions();
        guard->prefetchVisible();
    }, 45000);
    syncActions();
}

void RadioDialog::rememberSaved(const QJsonObject &station) {
    const auto id = textValue(station["externalUuid"]);
    if (!id.isEmpty()) m_savedUuids.insert(id);
    const auto url = streamUrl(station);
    if (!url.isEmpty()) m_savedUrls.insert(url);
}

bool RadioDialog::isSaved(const QJsonObject &station) const {
    return m_search.isEmpty() || m_savedUuids.contains(textValue(station["externalUuid"])) || m_savedUrls.contains(streamUrl(station));
}

void RadioDialog::syncActions() {
    const auto index = m_list->currentIndex();
    const bool saved = index.isValid() && isSaved(index.data(stationRole).toJsonObject());
    m_play->setEnabled(index.isValid());
    m_save->setText(saved ? "Saved" : "Save station");
    m_save->setEnabled(index.isValid() && !saved && !m_saving);
}

void RadioDialog::playSelected() {
    const auto station = m_list->currentIndex().data(stationRole).toJsonObject();
    if (station.isEmpty()) return;
    QJsonObject body{{"url", streamUrl(station)}, {"name", station["name"]}};
    const auto id = textValue(station["id"]);
    if (m_search.isEmpty() && !id.isEmpty()) body["stationId"] = id;
    emit playRequested(body);
    accept();
}

void RadioDialog::saveSelected() {
    const auto station = m_list->currentIndex().data(stationRole).toJsonObject();
    if (station.isEmpty() || isSaved(station) || m_saving) return;
    auto payload = station;
    payload.remove("id");
    m_saving = true;
    m_status->setText("Saving " + textValue(station["name"]) + "…");
    syncActions();
    const auto generation = m_generation;
    const QPointer<RadioDialog> guard(this);
    m_api->post("/api/library/radio/stations", payload, [guard, station, generation](QJsonObject response, QString error) {
        if (!guard) return;
        guard->m_saving = false;
        if (error.isEmpty() && response["station"].isObject()) {
            guard->rememberSaved(response["station"].toObject());
            guard->rememberSaved(station);
            if (guard->m_generation == generation) guard->m_status->setText(textValue(station["name"]) + " saved");
            else if (guard->m_search.isEmpty()) guard->loadStations({});
        } else if (guard->m_generation == generation) {
            guard->m_status->setText(error.isEmpty() ? "The station could not be saved." : error);
        }
        guard->syncActions();
    }, 45000);
}

void RadioDialog::prefetchVisible() {
    const int start = qMax(0, m_list->indexAt(QPoint(4, 0)).row());
    const int end = qMin(m_model->rowCount(), start + m_list->viewport()->height() / stationHeight + 2);
    QStringList keys;
    for (int row = start; row < end; ++row) keys.append(m_model->index(row, 0).data(logoRole).toString());
    keys.removeDuplicates();
    m_artwork.prefetch(keys);
}

bool RadioDialog::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Resize && watched == m_list->viewport())
        QTimer::singleShot(0, this, &RadioDialog::prefetchVisible);
    return QDialog::eventFilter(watched, event);
}
} // namespace PiTunes
