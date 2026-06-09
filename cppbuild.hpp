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

using CompilerArgs = std::set<std::string>;
using CompilerSources = std::set<std::string>;

// Collection of all setting entries.
struct SettingsCollection {
    std::string version{"0.0.1"};
    bool exit_on_error{true};
    bool not_idiot{false};
};
// Global Settings class.
class Settings {
    public:
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;

    // Returns current cppbuild version.
    static std::string version() {
        std::lock_guard lock{mtx_};
        return sc_.version;
    }
    // Returns whether build process interups after cppbuild::log() error call;
    // Default value is "false".
    // It is higly recommended to do not change it.
    static bool exit_on_error() {
        std::lock_guard lock{mtx_};
        return sc_.exit_on_error;
    }
    // Do not mess around with this function.
    static void set_exit_on_error(bool val) {
        std::lock_guard lock{mtx_};

        if (!val && !Settings::sc_.not_idiot) {
            std::cout <<  "[cppbuild WARNING] "
            << "It is higly unrecommended to change value of \"exit_on_error\".\n"
            << "Any build process must be performed sequentially.\n"
            << "Each build process step affects the next step.\n"
            << "So if one step fails - the process should be interupted.\n\n"
            << "For example, you can accidentally\n"
            << "nuke your entire file system if you are not careful.\n\n"
            << "Just do not do it. You probably do not need it.\n\n"
            << "If you still want to do it - assure cppbuild you are not_idiot.\n"
            << std::flush;

            std::exit(0);
        }

        sc_.exit_on_error = val;
    }

    // Returns copy of a current SettingsCollection.
    static SettingsCollection get_collection_copy() {
        std::lock_guard lock{mtx_};
        return sc_;
    }
    // Sets provided collection as current SettingsCollection.
    static void override_collection(const SettingsCollection& sc) {
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
inline bool do_mkdir(const Fs::path& dir_path) {
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
inline bool do_rm(const Fs::path& path) {
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

// Returns current working directory.
inline Fs::path working_dir() { return Fs::current_path(); }

// Changes current working directory. Same as "cd" command.
// Returns true on success.
inline bool do_cd(const Fs::path& path) {
    log(LogType::Info, std::string{"Changing working directory to: "}.append(path));
    try {
        Fs::current_path(path);
    } catch(const Fs::filesystem_error& e) {
        log(LogType::Error, std::string{"Failed to change working dir to "}
        .append(path).append(": ").append(e.what()));
        return false;
    }

    return true;
}

// TODO(clovis): implement generating compile_commands.json using clang -MJ
class CompileCommand {
   public:
    CompileCommand() = default;


    /////////////////////////////////////GETTERS////////////////////////////////////

    const std::string& compiler() const { return c_path_; }
    const std::string& target_name() const { return target_name_; }
    const CompilerArgs& compiler_args() const { return c_args_; }
    const CompilerSources& compiler_sources() const { return c_sources_; }
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
    // Returns build_dir() + target_name().
    Fs::path target_path() const { return build_dir().append(target_name()); }

    // Returns string containing current compile command.
    // Do not performs any checks.
    std::string cmd_string() const {
        std::string cmd{compiler()};
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

    void set_compiler(std::string_view c_path) { c_path_ = c_path; }

    void set_target_name(std::string_view target_name) {
        target_name_ = target_name;
    }

    void set_compiler_args(const CompilerArgs& c_args) {
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

    void set_compiler_sources(const CompilerSources& c_sources) {
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
        if (compiler().empty()) {
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

        log(LogType::Info,
            std::string{"Compiling target: "}.append(target_name()));

        // Compilation step
        std::string cmd{cmd_string()};
        return do_execute_command(cmd);
    }

    bool do_run() const {
        log(LogType::Info,
            std::string{"Running target: "}.append(target_path()));

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
        if (!do_mkdir(build_dir())) {
            log(LogType::Error,
                std::string{build_dir()}.append(" failed to create build dir"));
            return false;
        }
        return true;
    }
    bool do_clear_build_dir() const { return do_rm(build_dir()); }

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
            std::string{"Compiler path:     "}.append(compiler()));
        std::cout << '\n' << std::flush;
    }

   private:
    std::string c_path_;
    std::string target_name_;
    CompilerArgs c_args_;
    CompilerSources c_sources_;
    std::string build_dir_{"build"};
};
}  // namespace Cppbuild
