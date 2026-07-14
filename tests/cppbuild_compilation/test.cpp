#include "../../cppbuild.hpp"

int main() {
    Cppbuild::Settings::set_display_info(false);
    Cppbuild::log_i(Cppbuild::Settings::version());
    return 0;
}
