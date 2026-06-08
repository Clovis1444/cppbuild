// NOTE: all functions prefixed with "do_" have side effects(may modify
// filesystem/execute shell commands).

#pragma once

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <mutex>
#include <string_view>

namespace Cppbuild {

namespace Fs = std::filesystem;

// Collection of all setting entries.
struct SettingsCollection {
    std::string version{"0.0.1"};
    bool exit_on_error{true};
};
// Global Settings class.
class Settings {
    public:
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;

    static std::string version() {
        std::lock_guard lock{mtx_};
        return sc_.version;
    }
    static bool exit_on_error() {
        std::lock_guard lock{mtx_};
        return sc_.exit_on_error;
    }
    static void set_exit_on_error(bool val) {
        std::lock_guard lock{mtx_};
        sc_.exit_on_error = val;
    }

    // Returns copy of a current SettingsCollection.
    static SettingsCollection get_collection_copy() {
        std::lock_guard lock{mtx_};
        return sc_;
    }
    // Sets provided collection as current.
    static void override(const SettingsCollection& sc) {
        std::lock_guard lock{mtx_};
        sc_ = sc;
    }

    private:
    Settings() {}
    inline static SettingsCollection sc_{};
    inline static std::mutex mtx_{};
};

enum class LogType {
    Info,
    Warning,
    Error,
};

static void log(LogType lt, std::string_view text, bool exit_on_error = Settings::exit_on_error()) {
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

    std::cout << prefix << text << '\n' << std::flush;

    if (exit_on_error && lt == LogType::Error) {
        std::exit(1);
    }
}

// TODO(clovis): implement proper command execution
// TODO(clovis): add CommandResult enum
// To execute command specific directory use 'cd <dir> && <cmd>'.
// Returns true on success.
inline bool do_execute_command(const std::string& cmd) {
    log(LogType::Info, std::string{"Executing: "}.append(cmd));
    bool result{std::system(cmd.data()) == 0};

    if (!result && Settings::exit_on_error()) {
        std::exit(1);
    }

    return result;
}

// Returns true if directory was created or already existed.
inline bool do_make_dir(const Fs::path& dir_path) {
    if (Fs::exists(dir_path) && Fs::is_directory(dir_path)) {
        return true;
    }

    log(LogType::Info, std::string{"Creating directory: "}.append(dir_path));

    if (Fs::exists(dir_path) && !Fs::is_directory(dir_path)) {
        log(LogType::Error,
            std::string{dir_path}.append(": is not a directory"));
        return false;
    }

    try {
        Fs::create_directory(dir_path);
    } catch (const Fs::filesystem_error& e) {
        log(LogType::Error,
            std::string{dir_path}.append(": ").append(e.what()));
        return false;
    }

    return true;
}
// Removes entry recursively if exists. Returns true if entry was removed or did
// not exist.
inline bool do_rm_rf_if_exists(const Fs::path& path) {
    if (!Fs::exists(path)) {
        return true;
    }

    log(LogType::Info, std::string{"Removing: "}.append(path));

    try {
        Fs::remove_all(path);
    } catch (const Fs::filesystem_error& e) {
        log(LogType::Error, std::string{path}.append(": ").append(e.what()));
        return false;
    }

    if (Fs::exists(path)) {
        log(LogType::Error, std::string{"Failed to remove "}.append(path));
    }

    return true;
}

// TODO(clovis): implement generating compile_commands.json using clang -MJ
class CompileCommand {
   private:
    std::string c_path_;
    std::string target_name_;
    std::set<std::string> c_args_;
    std::set<std::string> c_sources_;
    std::string build_dir_{"build"};

   public:
    /////////////////////////////////////GETTERS////////////////////////////////////

    const std::string& compiler_path() const { return c_path_; }
    const std::string& target_name() const { return target_name_; }
    const std::set<std::string>& compiler_args() const { return c_args_; }
    const std::set<std::string>& compiler_sources() const { return c_sources_; }
    // Default value is "build".
    Fs::path build_dir() const {
        if (build_dir_.empty()) {
            return working_dir();
        }

        Fs::path b_dir{Fs::weakly_canonical(build_dir_)};
        if (b_dir.is_relative()) {
            b_dir = working_dir() / b_dir;
        }
        return b_dir;
    }
    static Fs::path working_dir() { return Fs::current_path(); }
    // Returns build_dir() + target_name().
    Fs::path target_path() const { return build_dir().append(target_name()); }

    // Returns string containing current compile command.
    // Do not performs any checks.
    std::string cmd_string() const {
        std::string cmd{compiler_path()};
        cmd.append(" ");
        for (const auto& source : compiler_sources()) {
            cmd.append(source);
            cmd.append(" ");
        }
        for (const auto& arg : compiler_args()) {
            cmd.append(arg);
            cmd.append(" ");
        }
        cmd.append("-o ");
        cmd.append(target_path());

        return cmd;
    }

    ////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////SETTERS////////////////////////////////////

    void set_compiler_path(std::string_view c_path) { c_path_ = c_path; }

    void set_target_name(std::string_view target_name) {
        target_name_ = target_name;
    }

    void set_compiler_args(const std::set<std::string>& c_args) {
        c_args_ = c_args;
    }
    // Returns false if arg already defined
    bool add_compiler_arg(const std::string& c_arg) {
        if (c_args_.find(c_arg) != c_args_.end()) {
            return false;
        }

        c_args_.insert(c_arg);
        return true;
    }
    // Returns false if arg was not defined
    bool remove_compiler_arg(const std::string& c_arg) {
        return static_cast<bool>(c_args_.erase(c_arg));
    }

    void set_compiler_sources(const std::set<std::string>& c_sources) {
        c_sources_ = c_sources;
    }
    // Returns false if source already defined
    bool add_compiler_source(const std::string& c_source) {
        if (c_sources_.find(c_source) != c_sources_.end()) {
            return false;
        }

        c_sources_.insert(c_source);
        return true;
    }
    // Returns false if source is not defined
    bool remove_compiler_source(const std::string& c_source) {
        return static_cast<bool>(c_sources_.erase(c_source));
    }

    void set_build_dir(std::string_view build_dir) { build_dir_ = build_dir; }

    ////////////////////////////////////////////////////////////////////////////////

    // Creates build directory and executes compile command
    bool do_compile() const {
        if (compiler_path().empty()) {
            log(LogType::Error, "Compiler is not set");
            return false;
        }
        if (target_name().empty()) {
            log(LogType::Error, "Target is not set");
            return false;
        }

        // Build dir step
        if (!Fs::exists(build_dir())) {
            if (!do_make_build_dir()) {
                return false;
            }
        }

        // Compilation step
        std::string cmd{cmd_string()};
        return do_execute_command(cmd);
    }

    bool do_run() const {
        if (!Fs::exists(target_path())) {
            log(LogType::Error,
                std::string{target_path()}.append(" target does not exist"));
            return false;
        }

        std::string cmd{target_path()};
        return do_execute_command(cmd);
    }

    // do_compile() + do_run()
    bool do_compile_and_run() const {
        if (!do_compile()) {
            return false;
        }

        return do_run();
    }

    // Returns true if build directory was created or already existed.
    bool do_make_build_dir() const {
        if (!do_make_dir(build_dir())) {
            log(LogType::Error,
                std::string{build_dir()}.append(" failed to create build dir"));
            return false;
        }
        return true;
    }
    bool do_clear_build_dir() const { return do_rm_rf_if_exists(build_dir()); }

    // Prints usefull info.
    void log_info() const {
        log(LogType::Info, std::string{"cppbuild v"}.append(Settings::version()));
        log(LogType::Info,
            std::string{"Working directory: "}.append(working_dir()));
        log(LogType::Info,
            std::string{"Build directory:   "}.append(build_dir()));
        log(LogType::Info,
            std::string{"Target path:       "}.append(target_path()));
        log(LogType::Info,
            std::string{"Compiler path:     "}.append(compiler_path()));
        std::cout << '\n' << std::flush;
    }
};
}  // namespace Cppbuild
