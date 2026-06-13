#include "cppbuild.hpp"

int main() {
    std::string msg{"You are using cppbuild v"};
    msg.append(Cppbuild::Settings::version());
    Cppbuild::log_i(msg);

    return 0;
}
