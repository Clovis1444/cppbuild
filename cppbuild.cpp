#include "cppbuild.hpp"

int main() {
  cppbuild::CompileCommand cmd{};
  cmd.set_compiler_path("clang++");
  cmd.set_compiler_args({
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Werror",
  });
  cmd.set_compiler_sources({"test.cpp"});
  cmd.set_target_name("test");
  cmd.set_build_dir("build");

  cmd.log_info();

  cmd.compile_and_run();

  return 0;
}
