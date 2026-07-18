#include "add_two.h"

int main() {
    int x{3};
    add_two(x);
    constexpr int kAnwser{5};
    return (x == kAnwser)?0:1;
}
