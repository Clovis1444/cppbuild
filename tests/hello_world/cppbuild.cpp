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
    cmd.set_compiler_sources({"test.cpp"});
    cmd.set_target_name("hello_world_test");

    cmd.do_compile_and_run();

    return 0;
}
