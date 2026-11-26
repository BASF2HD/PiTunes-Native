#include "TouchKeyboard.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
namespace PiTunes {
TouchKeyboard::TouchKeyboard(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(2);
    for (const QString &keys : {QString("1234567890"), QString("qwertyuiop"), QString("asdfghjkl"), QString("zxcvbnm./-_@")}) {
        auto *row = new QHBoxLayout; row->setSpacing(2); layout->addLayout(row);
        for (QChar key : keys) {
            auto *button = new QPushButton(QString(key), this); button->setFocusPolicy(Qt::NoFocus);
            button->setMinimumHeight(30); row->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, button] { if (m_target) m_target->insert(button->text()); });
        }
    }
    auto *row = new QHBoxLayout; layout->addLayout(row);
    for (const QString &label : {QString("Shift"), QString("Space"), QString("Backspace"), QString("Done")}) {
        auto *button = new QPushButton(label, this); button->setFocusPolicy(Qt::NoFocus); row->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, label] {
            if (label == "Done") { hide(); return; }
            if (!m_target) return;
            if (label == "Space") m_target->insert(" ");
            else if (label == "Backspace") m_target->backspace();
            else for (auto *key : findChildren<QPushButton *>()) {
                if (key->text().size() == 1) {
                    const auto current = key->text();
                    key->setText(current == current.toLower() ? current.toUpper() : current.toLower());
                    // Connections read the displayed label below via eventFilter-independent edits.
                }
            }
        });
    }
    hide();
}
void TouchKeyboard::watch(QLineEdit *field, bool automatic) {
    field->setProperty("touchKeyboardAutomatic", automatic); field->installEventFilter(this);
}
bool TouchKeyboard::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::FocusIn) {
        m_target = qobject_cast<QLineEdit *>(watched);
        if (watched->property("touchKeyboardAutomatic").toBool()) show();
    }
    return QWidget::eventFilter(watched, event);
}
}
