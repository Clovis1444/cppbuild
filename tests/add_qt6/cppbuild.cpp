#include "../../cppbuild.hpp"

int main() {
    Cppbuild::Settings::set_display_info(false);

    Cppbuild::CompileCommand cmd{"clang++"};
    cmd.add_compiler_args({
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
    });
    cmd.do_add_package("Qt6Core Qt6Widgets");
    cmd.set_compiler_sources({"test.cpp"});
    cmd.set_target_name("add_qt6_test");
    cmd.set_build_dir("../build");

    cmd.do_compile_and_run();

    return 0;
}
