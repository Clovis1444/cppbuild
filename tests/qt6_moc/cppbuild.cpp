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
    cmd.set_compiler_sources({
        "test.cpp",
        "custom_qobject/custom_qobject.cpp"
    });
    cmd.set_target_name("add_qt6_test");

    Cppbuild::QtMoc moc{&cmd};
    // TODO(clovis): this will not work on windows
    moc.set_compiler("/usr/lib/qt6/moc");
    moc.add_compiler_sources({
        "custom_qobject/custom_qobject.hpp",
    });

    moc.do_compile();
    cmd.generate_compile_commands_json();
    cmd.do_compile_and_run();

    return 0;
}
