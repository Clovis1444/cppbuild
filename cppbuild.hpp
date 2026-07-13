// NOTE: all functions prefixed with "do_" have side effects(may modify
// filesystem/execute shell commands).
//
// TODO(clovis): add timer functionality
// TODO(clovis): implement caching for CompileCommand, QtMoc, do_configure_file()
// TODO(clovis): add download feature

#pragma once

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace Cppbuild {

#if defined(_WIN32) || defined(_WIN64)
inline int WEXITSTATUS(int exit_status) { return exit_status; }
inline FILE* popen(const char* cmd, const char* modes) { return ::_popen(cmd, modes); }
inline int   pclose(FILE* f) { return ::_pclose(f); }
#else
inline FILE* popen(const char* cmd, const char* modes) { return ::popen(cmd, modes); }
inline int   pclose(FILE* f) { return ::pclose(f); }
#endif

namespace Fs = std::filesystem;

using CompilerArgs    = std::set<std::string>;
using CompilerSources = std::set<std::string>;

// Represents shell command execution result.
class Result {
public:
    explicit Result(int exit_code) :exit_code_{exit_code} {}
    Result(int exit_code, std::string_view output) :Result{exit_code} {
        output_ = output;
    }
    explicit Result(bool success) :exit_code_{success?kSuccess:kFailure} {}
    Result(bool success, std::string_view output) :Result{success} {
        output_ = output;
    }

    explicit operator int() const { return exit_code_; }
    explicit operator bool() const { return is_ok(); }

    // Returns Result with kSuccess exit_code.
    static Result SUCCESS() { return Result{kSuccess}; }
    // Returns Result with kFailure exit_code.
    static Result FAILURE() { return Result{kFailure}; }

    int exit_code() const { return exit_code_; }
    // Returns if result is success. Returns false otherwise.
    bool is_ok() const { return exit_code_ == kSuccess; }
    // Returns true if exit code is equal to kSuccess
    bool is_success() const { return is_ok(); }
    // Returns true if exit code is equal to kFailure
    bool is_failure() const { return !is_ok(); }

    // Returns command output if there is any.
    std::string output() const { return output_; }

    static constexpr int kSuccess{EXIT_SUCCESS};
    static constexpr int kFailure{EXIT_FAILURE};
private:
    int exit_code_;
    std::string output_;
};

// Represents how command will be executed.
struct ShellCommand {
    std::string cmd;
    // Whether a command throws an error on failure.
    bool weak{false};
    // Whether a command output will be printed.
    bool silent{false};
    // In which shell a command will be executed.
    // std::optional<std::string> shell{std::nullopt};

    static ShellCommand STRONG(const std::string& cmd) {
        ShellCommand command{};
        command.cmd = cmd;
        command.weak = false;
        return command;
    }
    static ShellCommand WEAK(const std::string& cmd) {
        ShellCommand command{};
        command.cmd = cmd;
        command.weak = true;
        return command;
    }
};

// Represents compilation command for each object file
struct CompilationObj {
    std::string source;
    std::string cmd;
    std::string output;
};

// Collection of all setting entries.
struct SettingsCollection {
    std::string version{"0.0.1"};
    bool display_info{true};
    bool display_warn{true};
    int thread_limit{static_cast<int>(std::thread::hardware_concurrency())};
    bool parallel_compilation{true};
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
    // Returns if info logs will be displayed.
    static bool display_info() {
        std::lock_guard lock{mtx_};
        return sc_.display_info;
    }
    static void set_display_info(bool val) {
        std::lock_guard lock{mtx_};
        sc_.display_info = val;
    }
    // Returns if warning logs will be displayed.
    static bool display_warn() {
        std::lock_guard lock{mtx_};
        return sc_.display_warn;
    }
    static void set_display_warn(bool val) {
        std::lock_guard lock{mtx_};
        sc_.display_warn = val;
    }

    // Returns max amount of threads that will be used.
    static int thread_limit() {
        std::lock_guard lock{mtx_};
        return sc_.thread_limit;
    }
    static void set_thread_limit(int val) {
        std::lock_guard lock{mtx_};
        sc_.thread_limit = val;
    }

    // Returns if CompileCommand will compile in multiple threads.
    static bool parallel_compilation() {
        std::lock_guard lock{mtx_};
        return sc_.parallel_compilation;
    }
    static void set_parallel_compilation(bool val) {
        std::lock_guard lock{mtx_};
        sc_.parallel_compilation = val;
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
    // For general info.
    Info,
    // For not critical failures.
    Warning,
    // For critical failures. When build proccess should be terminated.
    Error,
};

// If "force_display" is true - log will be displayed regardless of the setting.
// Error log will always be displayed.
static void log(LogType lt, std::string_view text, bool force_display = false) {
    bool do_not_display_info {!force_display && lt == LogType::Info && !Settings::display_info()};
    bool do_not_display_warn {!force_display && lt == LogType::Info && !Settings::display_warn()};
    if (do_not_display_info || do_not_display_warn ) {
        return;
    }

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

    if (lt == LogType::Error) {
        std::exit(1);
    }
}
// Same as log(LogType::Info, ...)
static void log_i(std::string_view text, bool force_display = false) {
    log(LogType::Info, text, force_display);
}
// Same as log(LogType::Warning, ...)
static void log_w(std::string_view text, bool force_display = false) {
    log(LogType::Warning, text, force_display);
}
// Same as log(LogType::Error, ...)
static void log_e(std::string_view text, bool force_display = false) {
    log(LogType::Error, text, force_display);
}

// Executes command in system shell.
// If cmd.cmd is an empty string - returns Result::SUCCESS().
// Throws an error on failure if cmd.weak = false.
inline Result do_execute_command(const ShellCommand& cmd) {
    if (cmd.cmd.empty()){
        return Result::SUCCESS();
    }

    if (!cmd.silent) {
        log_i("Executing: " + cmd.cmd);
    }

    // TODO(clovis): popen() always poops cmd error into the output
    FILE* f{Cppbuild::popen(cmd.cmd.data(), "r")};
    if (f == nullptr) {
        Cppbuild::log_e("popen() failed");
        return Result::FAILURE();
    }

    const size_t line_buff_size{512};
    std::array<char, line_buff_size> line_buff{};
    // Get cmd output
    std::string cmd_output{};

    while (std::fgets(line_buff.data(), line_buff.max_size(), f) != nullptr) {
        cmd_output.append(line_buff.data());
    }

    // Print cmd output
    if (!cmd.silent) {
        std::cout << cmd_output;
    }

    // Get cmd exit code
    int exit_status{Cppbuild::pclose(f)};
    int exit_code{WEXITSTATUS(exit_status)};

    Result r{exit_code, cmd_output};
    // Throw error
    if (!cmd.weak && r.is_failure()) {
        Cppbuild::log_e(
            cmd.cmd + ": command failed with exit code "
            + std::to_string(r.exit_code())
        );
    }

    return r;
}
// Same as do_execute_command(ShellCommand::STRONG(cmd))
inline Result do_execute_command_strong(const std::string& cmd) {
    return do_execute_command(ShellCommand::STRONG(cmd));
}
// Same as do_execute_command(ShellCommand::WEAK(cmd))
inline Result do_execute_command_weak(const std::string& cmd) {
    return do_execute_command(ShellCommand::WEAK(cmd));
}

// inline Result do_execute_commands_parallel_weak(
//     const std::vector<std::string>& cmds,
//     const ExecuteCommandOptions& opt = ExecuteCommandOptions::WEAK(),
//     int thread_limit = Settings::thread_limit()
// ) {}
// inline Result do_execute_commands_parallel(
//     // Pair of <Cmd, Cmd options>
//     const std::map<std::string, ExecuteCommandOptions>& cmds,
//     const int thread_limit = Settings::thread_limit()
// ) {
//     return Result::SUCCESS();
// }

// Returns Result::SUCCESS() if directory was created or already exist.
inline Result do_mkdir(const Fs::path& dir_path) {
    if (Fs::exists(dir_path) && Fs::is_directory(dir_path)) {
        return Result::SUCCESS();
    }

    log_i(std::string{"Creating directory: "}.append(dir_path.string()));

    if (Fs::exists(dir_path) && !Fs::is_directory(dir_path)) {
        log_e(std::string{dir_path.string()}.append(": is not a directory"));
        return Result::FAILURE();
    }

    try {
        Fs::create_directories(dir_path);
    } catch (const Fs::filesystem_error& e) {
        log_e(std::string{dir_path.string()}.append(": ").append(e.what()));
        return Result::FAILURE();
    }

    return Result::SUCCESS();
}
// Removes entry recursively if exists. Returns true if entry was removed or did
// not exist.
inline Result do_rm(const Fs::path& path) {
    if (!Fs::exists(path)) {
        return Result::SUCCESS();
    }

    log_i(std::string{"Removing: "}.append(path.string()));

    try {
        Fs::remove_all(path);
    } catch (const Fs::filesystem_error& e) {
        log_e(std::string{path.string()}.append(": ").append(e.what()));
        return Result::FAILURE();
    }

    if (Fs::exists(path)) {
        log_e(std::string{"Failed to remove "}.append(path.string()));
    }

    return Result::SUCCESS();
}

// Returns current working directory.
inline Fs::path working_dir() { return Fs::current_path(); }

// Returns path represented in full(absolute aka canonical) form.
// If input path is relative and does not yet exists return working_dir() + path.
inline Fs::path full_path(const Fs::path& path) {
    Fs::path p{Fs::weakly_canonical(path)};
    if (p.is_relative()) {
        p = working_dir() / p;
    }
    return p;
}

// Changes current working directory. Same as "cd" command.
// Returns true on success.
inline Result do_cd(const Fs::path& path) {
    log_i(std::string{"Changing working directory to: "}.append(path.string()));
    try {
        Fs::current_path(path);
    } catch(const Fs::filesystem_error& e) {
        log_e(std::string{"Failed to change working dir to "}
        .append(path.string()).append(": ").append(e.what()));
        return Result::FAILURE();
    }

    return Result::SUCCESS();
}

// Returns CFLAGS and linker flags for specified package.
// Throws error on failure.
// pkgconf must be installed.
inline std::string do_get_package_args(std::string_view package, bool msvc_syntax = false) {
    std::string cmd{"pkgconf --cflags --libs "};
    if (msvc_syntax) {
        cmd.append("--msvc-syntax ");
    }
    cmd.append(package);

    ShellCommand shell_cmd{ShellCommand::WEAK(cmd)};
    shell_cmd.silent = true;
    Result r{do_execute_command(shell_cmd)};
    if (r.is_failure()) {
        log_e(std::string{"Failed to find package: "}.append(package));
        return {};
    }

    std::string p_args{r.output()};
    // Remove newlines from command output
    for (auto it{p_args.begin()}; it != p_args.end();) {
        if (*it == '\n' || *it == '\r') {
            it = p_args.erase(it);
        } else {
            ++it;
        }
    }

    return p_args;
}

// Reads file content into string.
// On failure throws an error and returns an empty string.
// Note: this function is not optimised for large files.
inline std::string get_file_content(
    const Fs::path& path,
    std::ios_base::openmode mode = std::ios_base::in
) {
    if((mode & std::ios_base::out) == std::ios_base::out) {
        log_e(path.string() + ": wrong openmode std::ios_base::out");
        return {};
    }

    std::ifstream file{path, mode};
    if (!file) {
        log_e(full_path(path).string() + ": failed to open file");
        return {};
    }

    std::stringstream content{};
    content << file.rdbuf();

    if (content.fail()) {
        log_e(full_path(path).string() + ": error while reading file");
        return {};
    }

    return content.str();
}

// Returns string where each occurence of
// match.key is replaced by match.value.
inline std::string configure_string(
    const std::string& s,
    const std::map<std::string_view, std::string_view>& match
) {
    if(match.empty() || s.empty()) {
        return s;
    }

    std::string result{};
    result.reserve(s.size());

    size_t i{};
    while(i < s.size()) {
        bool matched{};
        for (const auto& [key, value] : match) {
            if(s.compare(i, key.size(), key) == 0) {
                result.append(value);
                i += key.size();
                break;
            }
        }

        if(!matched){
            result += s[i];
            ++i;
        }
    }

    return result;
}

// Returns Result::SUCCESS() if file was created/overwritten.
// If parent dir is not exists - creates it.
// Throws an error on failure.
inline Result do_create_file(
    const Fs::path& f_path,
    const std::string& f_content,
    // If overwrite is false - return Result::FAILURE() if file already exists
    bool overwrite = false,
    std::ios_base::openmode mode = std::ios_base::out
) {
    const Fs::path path{full_path(f_path)};

    if((mode & std::ios_base::in) == std::ios_base::in) {
        log_e(path.string() + ": wrong openmode std::ios_base::in");
        return Result::FAILURE();
    }

    if (!overwrite && Fs::exists(path)) {
        return Result::FAILURE();
    }
    if (Fs::exists(path) && !Fs::is_regular_file(path)) {
        log_e(path.string() + ": is not a regular file");
        return Result::FAILURE();
    }

    // Create parent dir.
    if(!do_mkdir(path.parent_path())) {
        log_e(path.string() + ": failed to create parent dir " + path.parent_path().string());
        return Result::FAILURE();
    }

    // Create file
    std::ofstream file{path, mode};
    if(!file) {
        log_e(path.string() + ": failed to create/open file");
        return Result::FAILURE();
    }

    if (!f_content.empty()) {
        file << f_content;
    }
    if(!file) {
        log_e(path.string() + ": failed to write file");
        return Result::FAILURE();
    }

    return Result::SUCCESS();
}

// Returns Result::SUCCESS() if something was written to output_file.
inline Result do_configure_file(
    const Fs::path& input_file,
    const std::map<std::string_view, std::string_view>& match,
    const Fs::path& output_file,
    // If overwrite is false - return Result::FAILURE() if output_file already exists
    bool overwrite = true
) {
    if(!overwrite && Fs::exists(output_file)) {
        return Result::FAILURE();
    }
    if (Fs::exists(output_file) && !Fs::is_regular_file(output_file)) {
        log_e(output_file.string() + ": is not a regular file");
        return Result::FAILURE();
    }

    // This may be an empty string
    std::string content{get_file_content(input_file)};

    // Configure string
    content = configure_string(content, match);

    // Write file
    return do_create_file(output_file, content, true, std::ios_base::out);
}

class CompileCommand {
public:
    CompileCommand() = default;
    explicit CompileCommand(std::string_view compiler) :c_path_{compiler} {}


    /////////////////////////////////////GETTERS////////////////////////////////////

    const std::string& compiler() const { return c_path_; }
    const std::string& target_name() const { return target_name_; }
    const CompilerArgs& compiler_args() const { return c_args_; }
    std::string compiler_args_str(bool strip_linker_flags = false) const {
        std::string args{};
        for (auto it{compiler_args().begin()}; it != compiler_args().end(); ++it) {
            if (strip_linker_flags && is_linker_flag(*it)) {
                continue;
            }

            args.append(*it);
            if (it != std::prev(compiler_args().end())) {
            args.push_back(' ');
            }
        }
        return args;
    }
    const CompilerSources& compiler_sources() const { return c_sources_; }
    // Default value is "build".
    Fs::path build_dir() const {
        if (build_dir_.empty()) {
            return working_dir();
        }

        return full_path(build_dir_);
    }
    // On Windows return target_name() + ".exe"(if target_name() does not provide it).
    // On other platforms just returns target_name().
    std::string target_full_name() const {
        std::string t_name{target_name()};
#if defined(_WIN32) || defined(_WIN64)
        if (!Fs::path{t_name}.has_extension()) {
            t_name.append(".exe");
        }
#endif
        return t_name;
    }
    // Returns build_dir() + target_full_name().
    Fs::path target_path() const { return build_dir().append(target_full_name()); }

    // Returns vector of CompilationCmd, containing compile command for each source file.
    // Returns empty vector if no sources were added or compiler was not defined.
    std::vector<CompilationObj> compilation_cmds() const {
        const CompilerSources& sources{compiler_sources()};
        if (sources.empty() || compiler().empty()) {
            return {};
        }

        const std::string args{compiler_args_str(true)};
        std::vector<CompilationObj> cmds{};
        cmds.reserve(sources.size());

        for (const auto& source : compiler_sources()) {
            std::string cmd{compiler()};
            cmd.append(" ");
            cmd.append(args);
#if defined(_WIN32) || defined(_WIN64)
            const std::string src_out_file{Fs::path{source}.filename().string() + ".obj"};
#else
            const std::string src_out_file{Fs::path{source}.filename().string() + ".o"};
#endif
            const std::string src_out_path{build_dir().append(src_out_file).string()};
            const std::string src_path{full_path(source).string()};
            if (is_using_msvc()) {
                cmd.append(" /c ");
                cmd.append(src_path);
                cmd.append(" /Fo ");
                cmd.append(src_out_path);
            } else {
                cmd.append(" -c ");
                cmd.append(src_path);
                cmd.append(" -o ");
                cmd.append(src_out_path);
            }

            std::pair<std::string, std::string> cmd_pair{full_path(source).string(), cmd};
            CompilationObj comp_cmd{};
            comp_cmd.source = src_path;
            comp_cmd.cmd = cmd;
            comp_cmd.output = src_out_path;

            cmds.emplace_back(comp_cmd);
        }

        return cmds;
    }
    // Returns string containing linking command for the target.
    // Returns empty string if no sources were added or compiler was not defined.
    std::string linking_cmd() const {
        const CompilerSources& sources{compiler_sources()};
        if (sources.empty() || compiler().empty()) {
            return {};
        }

        std::string cmd{compiler()};
        cmd.append(" ");
        cmd.append(compiler_args_str());
        for (const auto& source : compiler_sources()) {
#if defined(_WIN32) || defined(_WIN64)
            const std::string src_out_file{Fs::path{source}.filename().string() + ".obj"};
#else
            const std::string src_out_file{Fs::path{source}.filename().string() + ".o"};
#endif
            const std::string src_out_path{build_dir().append(src_out_file).string()};

            cmd.append(" ");
            cmd.append(src_out_path);
        }
        if (is_using_msvc()) {
            cmd.append(" /Fe \"");
            cmd.append(target_path().string() + "\"");
        } else {
            cmd.append(" -o ");
            cmd.append(target_path().string());
        }

        return cmd;
    }

    ////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////SETTERS////////////////////////////////////

    void set_compiler(std::string_view c_path) { c_path_ = c_path; }

    void set_target_name(std::string_view target_name) {
        target_name_ = target_name;
    }

    // Overrides compiler args.
    void set_compiler_args(const CompilerArgs& c_args) {
        c_args_.clear();
        for (const auto& arg : c_args) {
            add_compiler_arg(arg);
        }
    }
    // Inserts arg into compiler args list.
    // Returns false if arg was already present.
    bool add_compiler_arg(const std::string& c_arg) {
        // Trim prefixed whitespaces
        int str_start_index{0};
        for (int i{0}; i < c_arg.size(); ++i) {
            if(!std::isspace(c_arg[i])) {
                str_start_index = i;
                break;
            }
        }
        return c_args_.insert(c_arg.substr(str_start_index)).second;
    }
    // Inserts args into compiler args list.
    // Returns false if at least one source was already present.
    bool add_compiler_args(const CompilerArgs& c_args) {
        bool result{true};
        for (const auto& arg : c_args) {
            if (!add_compiler_arg(arg)) {
                result = false;
            }
        }
        return result;
    }
    // Erases arg from compiler args list.
    // Returns false if arg was present.
    bool remove_compiler_arg(const std::string& c_arg) {
        return c_args_.erase(c_arg) > 0;
    }
    // Erases args from compiler args list.
    // Returns false if at least one arg was not present.
    bool remove_compiler_args(const CompilerArgs& c_args) {
        bool result{true};
        for (const auto& arg : c_args) {
            if (!remove_compiler_arg(arg)) {
                result = false;
            }
        }
        return result;
    }

    // Overrides compiler sources.
    void set_compiler_sources(const CompilerSources& c_sources) {
        c_sources_ = c_sources;
    }
    // Inserts source into compiler sources list.
    // Returns false if source was already present.
    bool add_compiler_source(const std::string& c_source) {
        return c_sources_.insert(c_source).second;
    }
    // Inserts sources into compiler sources list.
    // Returns false if at least one source was already present.
    bool add_compiler_sources(const CompilerSources& c_sources) {
        bool result{true};
        for (const auto& source : c_sources) {
            if (!add_compiler_source(source)) {
                result = false;
            }
        }
        return result;
    }
    // Erases source from compiler sources list.
    // Returns false if source was not present.
    bool remove_compiler_source(const std::string& c_source) {
        return c_sources_.erase(c_source) > 0;
    }
    // Erases sources from compiler sources list.
    // Returns false if at least one source was not present.
    bool remove_compiler_sources(const CompilerSources& c_sources) {
        bool result{true};
        for (const auto& source : c_sources) {
            if (!remove_compiler_source(source)) {
                result = false;
            }
        }
        return result;
    }

    void set_build_dir(std::string_view build_dir) { build_dir_ = build_dir; }

    ////////////////////////////////////////////////////////////////////////////////

    // Creates build directory and executes compile command.
    // If weak = true - does not throw an error on failed compilation.
    Result do_compile(bool weak = false) const {
        if (compiler().empty()) {
            if (!weak) {
                log_e("Compiler is not set");
            }
            return Result::FAILURE();
        }
        if (target_name().empty()) {
            if (!weak) {
                log_e("Target is not set");
            }
            return Result::FAILURE();
        }

        // Build dir step
        if (!Fs::exists(build_dir())) {
            Result r{do_make_build_dir()};
            if (r.is_failure()) {
                return r;
            }
        }

        log_i(std::string{"Compiling target: "}.append(target_name()));

        // TODO(clovis): check if everything works on Windows
        // TODO(clovis): add parallel compilation
        // Compilation step
        for (const auto& cmd : compilation_cmds()) {
            Result r{weak?do_execute_command_weak(cmd.cmd):do_execute_command_strong(cmd.cmd)};

            if (r.is_failure()) {
                return r;
            }
        }
        // Linking step
        const std::string cmd{linking_cmd()};
        Result r{weak?do_execute_command_weak(cmd):do_execute_command_strong(cmd)};

        return r;
    }

    Result do_run(bool weak = false) const {
        log_i(std::string{"Running target: "}.append(target_path().string()));

        if (!Fs::exists(target_path())) {
            if (!weak) {
                log_e(std::string{target_path().string()}.append(" target does not exist"));
            }
            return Result::FAILURE();
        }

        std::string cmd{target_path().string()};
        return weak?do_execute_command_weak(cmd):do_execute_command_strong(cmd);
    }

    // do_compile() + do_run()
    Result do_compile_and_run(bool weak = false) const {
        if (!do_compile(weak)) {
            return Result::FAILURE();
        }

        return do_run(weak);
    }

    // Returns Result::SUCCESS() if build directory was created or already exist.
    Result do_make_build_dir() const { return do_mkdir(build_dir()); }
    // Returns Result::SUCCESS() if build directory was removed or did not exist.
    Result do_clear_build_dir() const { return do_rm(build_dir()); }

    // Adds CFLAGS and linker flags for specified package to compiler_args().
    // You can pass multiple packages space separated.
    // Returns Result::FAILURE() if package was not found, otherwise returns Result::SUCCESS().
    // Throws error on failure.
    // pkgconf must be installed.
    Result do_add_package(std::string_view package) {
        bool msvc_syntax{is_using_msvc()};

        std::stringstream args_sstream{do_get_package_args(package, msvc_syntax)};

        std::string arg{};
        CompilerArgs p_args{};
        while(args_sstream >> arg) {
            p_args.insert(arg);
        }

        if (p_args.empty()) {
            return Result::FAILURE();
        }

        add_compiler_args(p_args);

        return Result::SUCCESS();
    }

    // Returns false if compiler is not defined or not using: "cl" or "msvc"
    bool is_using_msvc() const {
        if (compiler().empty()) {
            return false;
        }

        Fs::path c_path{compiler()};
        if (!c_path.has_filename()) {
            return false;
        }

        std::string c_name{c_path.stem().string()};

        static std::unordered_set<std::string_view> msvc_names {
            "cl",
            "msvc"
        };

        // Check if compiler name is one of msvc_names
        bool is_msvc{msvc_names.find(c_name) != msvc_names.end()};

        return is_msvc;
    }

    // Returns compile_commands string for the current CompileCommand configuration.
    std::string compile_commands_string(bool enclosed = true) const {
        std::vector<CompilationObj> cmds {compilation_cmds()};
        if (cmds.empty()) {
            return {};
        }

        std::string str{};

        if (enclosed) {
            str.append("[\n");
        }

        for (auto it{cmds.begin()}; it != cmds.end(); ++it ) {
            const std::string& file{it->source};

            str.append("{\n");

            // directory
            str += "\"directory\": ";
            str += "\"";
            str += working_dir().string();
            str += "\",\n";
            // file
            str += "\"file\": ";
            str += "\"";
            str += file;
            str += "\",\n";
            // command
            str += "\"command\": ";
            str += "\"";
            str += it->cmd;
            str += "\",\n";
            // output
            str += "\"output\": ";
            str += "\"";
            str += it->output;
            str += "\"\n";

            str.append("}");
            // add comma if not last element
            if (it != std::prev(cmds.end())) {
                str.append(",");
            }
            str.append("\n");
        }

        if (enclosed) {
            str.append("]");
        }

        return str;
    }

    // Generates compile_commands.json in build directory.
    // If file already exists - overwrites it.
    // Returns Result::SUCCESS() if file was written without errors.
    Result generate_compile_commands_json() const {
        if (!do_make_build_dir()) {
            return Result::FAILURE();
        }

        const std::string file_name{build_dir().append("compile_commands.json").string()};
        std::ofstream file{file_name};
        if (!file) {
            log_w(std::string{file_name}.append(": failed to create file stream"));
            return Result::FAILURE();
        }

        file << compile_commands_string();
        if (!file) {
            log_w(std::string{file_name}.append(": failed to write file"));
            return Result::FAILURE();
        }

        return Result::SUCCESS();
    }

    // Prints usefull info.
    void log_info() const {
        log_i(std::string{"cppbuild v"}.append(Settings::version()));
        log_i(std::string{"Working directory: "}.append(working_dir().string()));
        log_i(std::string{"Build directory:   "}.append(build_dir().string()));
        log_i(std::string{"Target path:       "}.append(target_path().string()));
        log_i(std::string{"Compiler path:     "}.append(compiler()));
        std::cout << '\n' << std::flush;
    }

private:
    static bool is_linker_flag(const std::string& flag) {
        // TODO(clovis): ensure this list is exhaustive
        // Check if starts with
        bool r{
            flag.find("-l")       == 0 ||
            flag.find("-L")       == 0 ||
            flag.find("-Wl")      == 0 ||
            flag.find("/LIBPATH") == 0 ||
            flag.find("/link")    == 0
        };
        return r;
    }

private:
    std::string c_path_;
    std::string target_name_;
    CompilerArgs c_args_;
    CompilerSources c_sources_;
    std::string build_dir_{"build"};
};

// TODO(clovis): check if this works on windows and msvc
// TODO(clovis): for now generated source file just compiles by CompileCommand
// like any other source file with the same compiler args.
// This may cause false compiler warnings/errors. Find a way to solve this.
class QtMoc: private CompileCommand {
public:
    explicit QtMoc(std::string_view moc_path, CompileCommand* parent_command = nullptr)
    : parent_command_{parent_command}
    {
        set_compiler(moc_path);
        set_build_dir("moc/");
    }
    explicit QtMoc(CompileCommand* parent_command = nullptr)
    : QtMoc{"moc", parent_command} {}

    inline static const std::string kMocOutputExtPrefix{".moc"};
    inline static const std::string kMocOutputExt{".cpp"};

    /////////////////////////////////////GETTERS////////////////////////////////////
    Fs::path build_dir() const {
        if (parent_command()) {
            return parent_command()-> build_dir().append("moc/");
        }

        return CompileCommand::build_dir();
    }
    using CompileCommand::compiler;
    using CompileCommand::compiler_sources;

    CompilerArgs compiler_args() const {
        CompilerArgs moc_args{CompileCommand::compiler_args()};

        if (!parent_command()) {
            return moc_args;
        }

        const CompilerArgs parent_args{parent_command()->compiler_args()};

        for (const auto& arg : parent_args) {
            // Check if starts with
            bool moc_relevant_arg{
                arg.find("-I") == 0 ||
                arg.find("-D") == 0 ||
                arg.find("-U") == 0 ||
                arg.find("/I") == 0 ||
                arg.find("/D") == 0 ||
                arg.find("/U") == 0
            };

            if (moc_relevant_arg) {
                // Convert from msvc style arg to unix style arg
                if (arg[0] == '/') {
                    std::string unix_arg{arg};
                    unix_arg[0] = '-';
                    moc_args.insert(unix_arg);
                } else {
                    moc_args.insert(arg);
                }
            }
        }

        if (parent_command()->is_using_msvc()) {
            moc_args.insert("--compiler-flavor msvc");
        }

        return moc_args;
    }

    CompileCommand* parent_command() const { return parent_command_; }
    // Returns list of sources that will be generated after calling do_compile().
    CompilerSources moc_output() const {
        CompilerSources moc_out{};

        for (const auto& input : compiler_sources()) {
            moc_out.insert(to_moc_output_name(input));
        }

        return moc_out;
    }
    ////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////SETTERS////////////////////////////////////
    using CompileCommand::set_compiler;

    using CompileCommand::set_compiler_args;
    using CompileCommand::add_compiler_arg;
    using CompileCommand::add_compiler_args;
    using CompileCommand::remove_compiler_arg;

    using CompileCommand::set_compiler_sources;
    using CompileCommand::add_compiler_source;
    using CompileCommand::add_compiler_sources;
    using CompileCommand::remove_compiler_source;
    using CompileCommand::remove_compiler_sources;

    void set_parent_command(CompileCommand* parent_comand) { parent_command_ = parent_comand; }
    ////////////////////////////////////////////////////////////////////////////////

    Result do_make_build_dir() const { return do_mkdir(build_dir()); }

    // Runs moc that will generate source files.
    // If moc exits without errors - add generated source files into parent_command sources.
    // Returns Result::SUCCESS() on success.
    Result do_compile(bool weak = false) {
        if (compiler().empty()) {
            if (!weak) {
                log_e("Moc path is not set");
            }
            return Result::FAILURE();
        }
        if (compiler_sources().empty()) {
            if (!weak) {
                log_e("No compiler sources were defined");
            }
            return Result::FAILURE();
        }

        // Build dir step
        if (!Fs::exists(build_dir())) {
            Result r{do_make_build_dir()};
            if (r.is_failure()) {
                return r;
            }
        }

        log_i("Running moc...");

        // Compilation step
        std::string args{};
        for (const auto& arg : compiler_args()) {
            args.append(arg + ' ');
        }

        // TODO(clovis): add parallel compilation
        for (const auto& source : compiler_sources()) {
            std::string cmd{compiler() + " "};

            cmd.append(source);
            cmd.append(" -o ");
            cmd.append(to_moc_output_name(source) + " ");

            cmd.append(args);

            Result r{weak?do_execute_command_weak(cmd):do_execute_command_strong(cmd)};

            if (r.is_failure()) {
                return r;
            }
        }

        // Add parent sources step
        if (parent_command()) {
            parent_command()->add_compiler_sources(moc_output());
        }

        return Result::SUCCESS();
    }

    std::string to_moc_output_name(const Fs::path& file_path) const {
        const std::string filename{
            file_path.stem().string() +
            kMocOutputExtPrefix +
            kMocOutputExt
        };

        Fs::path moc_output{build_dir()/filename};

        return moc_output.string();
    }
    static bool has_moc_output_name(const Fs::path& file_path) {
        return file_path.extension().string() == kMocOutputExt && file_path.stem().extension().string() == kMocOutputExtPrefix;
    }

private:
    // Clears all moc output from parent sources.
    // Returns false if at least one source was not present or parent is nullptr.
    bool clear_parent() const {
        if (!parent_command_) {
            return false;
        }

        return parent_command_->remove_compiler_sources(moc_output());
    }
private:
    CompileCommand* parent_command_{nullptr};
};
}  // namespace Cppbuild
