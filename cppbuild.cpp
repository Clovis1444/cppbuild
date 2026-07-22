#include "cppbuild.hpp"

int main() {
    Cppbuild::DO_SELF_REBUILD("clang++");

    std::string msg{"You are using cppbuild v"};
    msg.append(Cppbuild::Settings::version());
    Cppbuild::log_i(msg);

    return 0;
}
