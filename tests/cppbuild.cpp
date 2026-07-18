#include "../cppbuild.hpp"

namespace {
    size_t success_count{};
    void handle_test_result(const std::string& test_name, const Cppbuild::Result& r) {
        std::string msg{"TEST (" + test_name + "): "};
        if (r) {
            ++success_count;
            msg += "SUCCESS";
            msg += " in " + r.dur_str();
            Cppbuild::log_i(msg, true);
        } else {
            msg += "FAILURE with exit code ";
            msg += std::to_string(r.exit_code());
            msg += " in " + r.dur_str();
            Cppbuild::log_w(msg);
        }
    }
}  // namespace

int main() {
    Cppbuild::Timer t{};

    // Basic CompileCommand template for all tests
    Cppbuild::CompileCommand cc{"clang++"};
    cc.set_compiler_args({
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
    });

    Cppbuild::log_i("RUNNING TESTS...");
    Cppbuild::Settings::set_display_info(false);

    // All tests source code here
    std::vector<std::function<void()>> tests_funcs{
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            const std::string test_name{"cppbuild_compilation"};
            cc.set_compiler_args({
                "-std=c++17",
                "-Wall",
                "-Wextra",
                // "-Werror",
                "-pedantic-errors",
                "-fstrict-flex-arrays=3",
            });
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.set_compiler_sources({test_dir + "test.cpp"});

            Cppbuild::Result r{cc.do_compile_and_run(true)};

            // Generate compile_commands
            cc.set_build_dir("../build/");
            cc.generate_compile_commands_json();

            handle_test_result(test_name, r);
        },
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            const std::string test_name{"add_qt6"};
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.do_add_package("Qt6Core Qt6Widgets");
            cc.set_compiler_sources({test_dir + "test.cpp"});

            Cppbuild::Result r{cc.do_compile_and_run(true)};

            handle_test_result(test_name, r);
        },
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            const std::string test_name{"compile_commands"};
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.add_compiler_arg("-I" + test_dir + "some_dir"),
            cc.set_compiler_sources({
                test_dir + "test.cpp",
                test_dir + "some_dir/some_source.cpp",
            });

            cc.generate_compile_commands_json();

            Cppbuild::Result r {cc.do_compile_and_run(true)};

            handle_test_result(test_name, r);
        },
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            const std::string test_name{"compile_commands_msvc"};
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.add_compiler_arg("-I" + test_dir + "some_dir"),
            cc.set_compiler_sources({
                test_dir + "test.cpp",
                test_dir + "some_dir/some_source.cpp",
            });

            // Compile with clang
            cc.do_compile(true);

            cc.set_compiler("msvc");
            Cppbuild::CompilerArgs msvc_args{
                "/W4",
                "/Wpermissive-",
                "/WX",
                // Include dir here
                "/I" + test_dir + "some_dir",
                "/I" + test_name
            };
            cc.set_compiler_args(msvc_args);
            // Generate compile_commands with msvc compiler
            cc.generate_compile_commands_json();

            Cppbuild::Result r {cc.do_run(true)};

            handle_test_result(test_name, r);
        },
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            cc.set_compiler_args({});
            const std::string test_name{"configure_file"};
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.set_compiler_sources({test_dir + "test.cpp"});

            Cppbuild::Result r{cc.do_compile_and_run(true)};

            handle_test_result(test_name, r);
        },
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            const std::string test_name{"hello_world"};
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.set_compiler_sources({test_dir + "test.cpp"});

            Cppbuild::Result r{cc.do_compile_and_run(true)};

            handle_test_result(test_name, r);
        },
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            const std::string test_name{"qt6_moc"};
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.do_add_package("Qt6Core Qt6Widgets");
            cc.set_compiler_sources({
                test_dir + "test.cpp",
                test_dir + "custom_qobject/custom_qobject.cpp",
            });

            Cppbuild::QtMoc moc{&cc};
            // TODO(clovis): add Windows support
#if defined(_WIN32) || defined(_WIN64)
#else
            moc.set_compiler("/usr/lib/qt6/moc");
#endif
            moc.add_compiler_sources({
                test_dir + "custom_qobject/custom_qobject.hpp",
            });

            moc.do_compile(true);

            Cppbuild::Result r{cc.do_compile_and_run(true)};

            handle_test_result(test_name, r);
        },
////////////////////////////////////////////////////////////////////////////////
        [cc] () mutable {
            Cppbuild::Timer t{};
            const std::string test_name{"comp_caching"};
            const std::string test_dir{test_name + "/"};
            cc.add_compiler_arg("-I" + test_name);
            cc.set_build_dir(test_dir + "build/");
            cc.set_target_name(test_name + "_test");

            cc.set_compiler_sources({
                test_dir + "test.cpp",
                test_dir + "add_two.cpp",
            });

            cc.do_clear_build_dir();
            if (cc.do_get_recompilation_cmds().size() != 2) {
                handle_test_result(test_name, Cppbuild::Result{1, t.elapsed()});
                return;
            }

            Cppbuild::Result r{cc.do_compile_and_run(true)};

            if (cc.do_get_recompilation_cmds().size() != 0) {
                handle_test_result(test_name, Cppbuild::Result{2, t.elapsed()});
                return;
            }

            handle_test_result(test_name, r);
        },
        // Add new tests here
    };

    // Running tests here
    Cppbuild::execute_funcs_parallel(tests_funcs);

    // Log final tests results
    std::string msg{std::to_string(success_count)};
    msg.append("/").append(std::to_string(tests_funcs.size()));
    msg.append(" tests finished successfully");
    msg.append(" in " + t.elapsed_str() + ".");
    Cppbuild::log_i(msg, true);

    if (success_count != tests_funcs.size()) {
        return 1;
    }

    return 0;
}
