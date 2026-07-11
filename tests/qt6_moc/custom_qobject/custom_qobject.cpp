#include "custom_qobject.hpp"

Custom::Custom() {
    connect(this, &Custom::test_signal, this, &Custom::test_slot);
}

void Custom::test_slot() {
    was_signal_emited_ = true;
}

bool Custom::was_signal_emited() const {
    return was_signal_emited_;
}
