#pragma once

#include <QObject>

class Custom: public QObject {
Q_OBJECT

public:
    Custom();

    bool was_signal_emited() const;

signals:
    void test_signal();
public slots:
    void test_slot();

private:
    bool was_signal_emited_{};
};
