#pragma once
#include <QLineEdit>
#include <QPointer>
#include <QWidget>
namespace PiTunes {
class TouchKeyboard : public QWidget {
    Q_OBJECT
public:
    explicit TouchKeyboard(QWidget *parent = nullptr);
    void watch(QLineEdit *field, bool automatic = true);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    QPointer<QLineEdit> m_target;
};
}
