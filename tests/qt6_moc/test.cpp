#include "custom_qobject/custom_qobject.hpp"

#include <QApplication>
#include <QDebug>

int main() {
    Custom test_qobject{};
    test_qobject.test_signal();

    return test_qobject.was_signal_emited()?0:1;
}
