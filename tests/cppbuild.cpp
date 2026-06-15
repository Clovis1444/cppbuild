#include "../cppbuild.hpp"

int main() {
    Cppbuild::CompileCommand cmd{"clang++"};

    std::string root_dir{Cppbuild::working_dir()};

    // NOTE: relative path
    cmd.add_compiler_arg("-std=c++17");
    cmd.add_compiler_arg("-I" + root_dir);
    cmd.set_target_name("cppbuild");
    cmd.set_build_dir(root_dir + "/build");

    Cppbuild::log_i("RUNNING TESTS...");
    // TODO(clovis): remove this when CompileCommand::compile_weak() will be implemented
    Cppbuild::Settings::override_collection({
        .exit_on_error=false,
        .not_idiot=true,
        .display_info=false,
    });

    int tests_count{};
    int success_tests{};
    for (const auto& i : Cppbuild::Fs::directory_iterator{"."}) {
        if (!i.is_directory()) {
            continue;
        }

        const Cppbuild::Fs::path path {Cppbuild::Fs::canonical(i)};
        // Ignore build dir
        if (path == cmd.build_dir()) {
            continue;
        }

        Cppbuild::Fs::path cppbuild_path{path / "cppbuild.cpp"};

        Cppbuild::do_cd(path);

        if (Cppbuild::Fs::exists(cppbuild_path)) {
            cmd.set_compiler_sources({cppbuild_path});

            // TODO(clovis): do compile_weak() here
            Cppbuild::Result r{cmd.do_compile_and_run()};
            std::string msg_prefix {"TEST("};
            msg_prefix.append(path.stem()).append("): ");
            if (r.is_success()) {
                Cppbuild::log_i(msg_prefix.append("SUCCESS"), true);
                ++success_tests;
            } else {
                Cppbuild::log_w(msg_prefix.append("FAILURE"));
            }

            ++tests_count;
        } else {
            // TODO(clovis): handle case where no build file (use default args) here
            Cppbuild::log_w(std::string{path.stem()}
                            + ": Failed to find cppbuild.cpp. Default args tests currently not supported!"
                            );
        }

        Cppbuild::do_cd("..");
    }

    Cppbuild::Settings::set_display_info(true);
    std::string msg{std::to_string(success_tests)};
    msg.append("/").append(std::to_string(tests_count));
    msg.append(" tests finished successfully.");
    Cppbuild::log_i(msg);

    if (success_tests != tests_count) {
        return 1;
    }

    return 0;
}
