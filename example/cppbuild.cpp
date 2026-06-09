#include "../cppbuild.hpp"

int main() {
    Cppbuild::CompileCommand cmd{};
    cmd.set_compiler("clang++");
    cmd.set_compiler_args({
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
    });
    cmd.set_compiler_sources({"example.cpp"});
    cmd.set_target_name("example");
    cmd.set_build_dir("build");

    cmd.do_compile_and_run();

    return 0;
}
