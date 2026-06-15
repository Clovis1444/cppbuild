#include "../../cppbuild.hpp"

int main() {
    Cppbuild::Settings::set_display_info(false);

    Cppbuild::CompileCommand cmd{"clang++"};
    cmd.add_compiler_args({
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        // Include dir here
        "-Isome_dir",
    });
    cmd.set_compiler_sources({
        "test.cpp",
        "some_dir/some_source.cpp",
    });
    cmd.set_target_name("compile_commands_test");

    cmd.generate_compile_commands_json();

    cmd.do_compile_and_run();

    return 0;
}
