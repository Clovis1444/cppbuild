#include "../cppbuild.hpp"

int main() {
    Cppbuild::CompileCommand cmd{"clang++"};

    std::string root_dir{Cppbuild::working_dir().string()};

    // NOTE: relative path
    cmd.add_compiler_arg("-std=c++17");
    cmd.add_compiler_arg("-I" + root_dir);
    cmd.set_target_name("cppbuild");
    cmd.set_build_dir(root_dir + "/build");

    Cppbuild::log_i("RUNNING TESTS...");
    Cppbuild::SettingsCollection sc{};
    sc.display_info = false;
    Cppbuild::Settings::override_collection(sc);

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
            cmd.set_compiler_sources({cppbuild_path.string()});

            Cppbuild::Result r{cmd.do_compile_and_run(true)};
            std::string msg_prefix {"TEST("};
            msg_prefix.append(path.stem().string()).append("): ");
            if (r.is_success()) {
                Cppbuild::log_i(msg_prefix.append("SUCCESS"), true);
                ++success_tests;
            } else {
                Cppbuild::log_w(msg_prefix.append("FAILURE"));
            }

            ++tests_count;
        } else {
            Cppbuild::log_w(std::string{path.stem().string()}
                            + ": Failed to find cppbuild.cpp!"
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
