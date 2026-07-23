#include "lib.h"

int main() {
    int two{return_two()};
    int three{return_three()};

    bool is_correct{((two == 2) && (three == 3))};
    return is_correct?0:1;
}
