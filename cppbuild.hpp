// NOTE: all functions prefixed with "do_" have side effects(may modify
// filesystem/execute shell commands).
//
// TODO(clovis): add timer functionality
// TODO(clovis): add ability to pass env vars into object file
// TODO(clovis): add configuration file feature
// TODO(clovis): add header file support: needed for qt's moc and compile_commands
// TODO(clovis): implement feature system?: enable/disable feature

#pragma once

#include <array>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>

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

using CompilerArgs = std::set<std::string>;
using CompilerSources = std::set<std::string>;
using CompilerHeaders = std::set<std::string>;

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
struct ExecuteCommandOptions {
    // Whether a command output will be printed.
    bool silent{false};
    // In which shell a command will be executed.
    // std::optional<std::string> shell{std::nullopt};

    static ExecuteCommandOptions STRONG() {
        ExecuteCommandOptions opt{};
        return opt;
    }
    static ExecuteCommandOptions WEAK() {
        ExecuteCommandOptions opt{};
        return opt;
    }
};

// Represents compilation command for each object file
struct CompilationCmd {
    std::string source;
    std::string cmd;
    std::string output;
};

// Collection of all setting entries.
struct SettingsCollection {
    std::string version{"0.0.1"};
    bool display_info{true};
    bool display_warn{true};
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

// Executes command in system shell, throws an error if command failed to execute.
// Does not throw an error if command result is failure.
inline Result do_execute_command_weak(
    const std::string& cmd,
    const ExecuteCommandOptions& opt = ExecuteCommandOptions::WEAK()
) {
    if (!opt.silent) {
        log_i("Executing: " + cmd);
    }

    // TODO(clovis): popen() always poops cmd error into the output
    FILE* f{Cppbuild::popen(cmd.data(), "r")};
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
    if (!opt.silent) {
        std::cout << cmd_output;
    }

    // Get cmd exit code
    int exit_status{Cppbuild::pclose(f)};
    int exit_code{WEXITSTATUS(exit_status)};

    return Result{exit_code, cmd_output};
}
// Executes command in system shell. Does throw an error on command failure.
inline Result do_execute_command(
    const std::string& cmd,
    const ExecuteCommandOptions& opt = ExecuteCommandOptions::STRONG()
) {
    Result result{do_execute_command_weak(cmd, opt)};

    if (result.is_failure()) {
        Cppbuild::log_e(cmd + ": command failed with exit code "
                      + std::to_string(result.exit_code()));
    }

    return result;
}

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
        Fs::create_directory(dir_path);
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

    ExecuteCommandOptions cmd_opt{};
    cmd_opt.silent = true;
    Result r{do_execute_command_weak(cmd, cmd_opt)};
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
    const CompilerHeaders& compiler_headers() const { return c_headers_; }
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
    std::vector<CompilationCmd> compilation_cmds() const {
        const CompilerSources& sources{compiler_sources()};
        if (sources.empty() || compiler().empty()) {
            return {};
        }

        const std::string args{compiler_args_str(true)};
        std::vector<CompilationCmd> cmds{};
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
            CompilationCmd comp_cmd{};
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
        c_args_ = c_args;
    }
    // Inserts arg into compiler args list.
    // Returns false if arg was already present.
    bool add_compiler_arg(const std::string& c_arg) {
        return c_args_.insert(c_arg).second;
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

    // Overrides compiler headers.
    void set_compiler_headers(const CompilerHeaders& c_headers) {
        c_headers_ = c_headers;
    }
    // Inserts header into compiler headers list.
    // Returns false if header was already present.
    bool add_compiler_header(const std::string& c_header) {
        return c_headers_.insert(c_header).second;
    }
    // Inserts headers into compiler headers list.
    // Returns false if at least one header was already present.
    bool add_compiler_headers(const CompilerHeaders& c_headers) {
        bool result{true};
        for (const auto& header : c_headers) {
            if (!add_compiler_header(header)) {
                result = false;
            }
        }
        return result;
    }
    // Erases header from compiler headers list.
    // Returns false if header was not present.
    bool remove_compiler_header(const std::string& c_header) {
        return c_headers_.erase(c_header) > 0;
    }
    // Erases headers from compiler headers list.
    // Returns false if at least one header was not present.
    bool remove_compiler_headers(const CompilerHeaders& c_headers) {
        bool result{true};
        for (const auto& header : c_headers) {
            if (!remove_compiler_header(header)) {
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
            Result r{weak?do_execute_command_weak(cmd.cmd):do_execute_command(cmd.cmd)};

            if (r.is_failure()) {
                return r;
            }
        }
        // Linking step
        const std::string cmd{linking_cmd()};
        Result r{weak?do_execute_command_weak(cmd):do_execute_command(cmd)};

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
        return weak?do_execute_command_weak(cmd):do_execute_command(cmd);
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
        std::vector<CompilationCmd> cmds {compilation_cmds()};
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
        bool r{
            flag.find("-l")       != std::string::npos ||
            flag.find("-L")       != std::string::npos ||
            flag.find("-Wl")      != std::string::npos ||
            flag.find("/LIBPATH") != std::string::npos ||
            flag.find("/link")    != std::string::npos
        };
        return r;
    }

   private:
    std::string c_path_;
    std::string target_name_;
    CompilerArgs c_args_;
    CompilerSources c_sources_;
    CompilerHeaders c_headers_;
    std::string build_dir_{"build"};
};
}  // namespace Cppbuild
