#include "../../cppbuild.hpp"

int main() {
    Cppbuild::Settings::set_display_info(false);

    Cppbuild::CompileCommand cmd{"clang++"};
    Cppbuild::CompilerArgs clang_args{
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        // Include dir here
        "-Isome_dir",
    };
    cmd.set_compiler_sources({
        "test.cpp",
        "some_dir/some_source.cpp",
    });
    cmd.set_target_name("compile_commands_test");

    cmd.set_compiler("msvc");
    Cppbuild::CompilerArgs msvc_args{
        "/W4",
        "/Wpermissive-",
        "/WX",
        // Include dir here
        "/Isome_dir",
    };
    cmd.set_compiler_args(msvc_args);
    cmd.generate_compile_commands_json();

    cmd.set_compiler("clang++");
    cmd.set_compiler_args(clang_args);

    cmd.do_compile_and_run();

    return 0;
}
