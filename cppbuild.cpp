#include "cppbuild.h"

int main() {
  cppbuild::CompileCommand cmd{};

  cmd.set_compiler_path("clang++");
  cmd.set_compiler_sources({"test.cpp"});

  std::set<std::string_view> c_args {
    "-Wall",
    "-Wextra",
  };
  cmd.set_compiler_args(c_args);
  cmd.add_compiler_arg("-Werror");
  cmd.remove_compiler_arg("-Werror");

  cmd.set_compiler_output("build/kefteme");

  if (!cppbuild::execute_command("rm -rf build && mkdir build")) {
    return 1;
  }

  bool result{};
  // result = cmd.compile();
  // result = cmd.run();
  result = cmd.compile_and_run();

  return result?0:1;
}
