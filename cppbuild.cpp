#include "cppbuild.hpp"

int main() {
    Cppbuild::CompileCommand cmd{};
    cmd.set_compiler("clang++");
    cmd.set_compiler_args({
        "-std=c++17",
    });
    // Note: all these paths are relative
    cmd.set_compiler_sources({"cppbuild.cpp"});
    cmd.set_target_name("cppbuild");
    cmd.set_build_dir("build");

    // Prints usefull info
    bool info_task{true};
    // Compiles simple hello world cpp program
    bool example_task{true};

    const Cppbuild::Fs::path root{Cppbuild::working_dir()};

    if (info_task) {
        cmd.log_info();
    }

    if (example_task) {
        Cppbuild::log(Cppbuild::LogType::Info, std::string{"RUNNING BUILD TASK AT: "}.append(Cppbuild::working_dir()));
        Cppbuild::do_cd("example");
        cmd.do_compile_and_run();
        Cppbuild::do_cd(root);
    }

    return 0;
}
