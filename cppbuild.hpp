#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <set>

namespace cppbuild {
enum class LogType {
  Info,
  Warning,
  Error,
};

static void log(LogType lt, std::string_view text) {
  std::string_view prefix;
  switch (lt) {
  case LogType::Info:
    prefix = "[cppbuild INFO] ";
    break;
  case LogType::Warning:
    prefix = "[cppbuild WARNING] ";
    break;
  case LogType::Error:
    prefix = "[cppbuild ERROR] ";
    break;
  }

  std::cout << prefix << text << std::endl;
}

// TODO(clovis): implement proper command execution
// TODO(clovis): add CommandResult enum
// To execute command specific directory use 'cd <dir> && <cmd>'.
// Returns true on success.
inline bool execute_command(std::string_view cmd) {
  log(LogType::Info, std::string{"Executing: "}.append(cmd));
  bool result {std::system(cmd.data()) == 0};
  return result;
}

// TODO(clovis): add build_dir_
// TODO(clovis): add autoremove?
class CompileCommand {
private:
  std::string_view c_path_{};
  std::string_view c_output_{};
  std::set<std::string_view> c_args_{};
  std::set<std::string_view> c_sources_{};

public:
  std::string_view compiler_path() const { return c_path_; }
  std::string_view compiler_output() const { return c_output_; }
  std::set<std::string_view> compiler_args() const { return c_args_; }
  std::set<std::string_view> compiler_sources() const { return c_sources_; }

  bool compile() const {
    std::string cmd{c_path_};
    cmd.append(" ");
    for (auto source : compiler_sources()) {
      cmd.append(source);
      cmd.append(" ");
    }
    // TODO(clovis): overload this container to easely convert it to string
    for (auto arg : compiler_args()) {
      cmd.append(arg);
      cmd.append(" ");
    }
    cmd.append("-o ");
    cmd.append(c_output_);

    return execute_command(cmd);
  }

  bool run() const {
    std::string cmd{"./"};
    cmd.append(c_output_);
    return execute_command(cmd);
  }

  bool compile_and_run() const {
    if (!compile()) return false;
    return run();
  }

  void set_compiler_path(std::string_view c_path) {
    c_path_ = c_path;
  }

  void set_compiler_output(std::string_view c_output) {
    c_output_ = c_output;
  }

  void set_compiler_args(std::set<std::string_view> c_args) {
    c_args_ = c_args;
  }
  // Returns false if arg already defined
  bool add_compiler_arg(std::string_view c_arg) {
    if (c_args_.find(c_arg) != c_args_.end()){
      return false;
    }

    c_args_.insert(c_arg);
    return true;
  }
  // Returns false if arg was not defined
  bool remove_compiler_arg(std::string_view c_arg) {
    return c_args_.erase(c_arg);
  }

  void set_compiler_sources(std::set<std::string_view> c_sources) {
    c_sources_ = c_sources;
  }
  // Returns false if source already defined
  bool add_compiler_source(std::string_view c_source) {
    if (c_sources_.find(c_source) != c_sources_.end()){
      return false;
    }

    c_sources_.insert(c_source);
    return true;
  }
  // Returns false if source is not defined
  bool remove_compiler_source(std::string_view c_source) {
    return c_sources_.erase(c_source);
  }
};

inline void hello() { std::cout << "Hello from cppbuild!" << std::endl; }
} // namespace cppbuild
