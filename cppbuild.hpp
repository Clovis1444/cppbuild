// NOTE: all functions prefixed with "do_" have side effects(may modify
// filesystem/execute shell commands).
//
// TODO(clovis): add download feature

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
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
namespace Chr = std::chrono;

using CompilerArgs    = std::set<std::string>;
using CompilerSources = std::set<std::string>;
using TimePoint = Chr::steady_clock::time_point;
template<typename Period = std::ratio<1, 1>>
using Duration = Chr::duration<double, Period>;

template<typename Period = std::ratio<1, 1>>
inline std::string duration_str(const Duration<Period>& dur) {
    int64_t total_ms{Chr::duration_cast<Chr::milliseconds>(dur).count()};

    // Delimeters
    constexpr int64_t kS{1000};
    constexpr int64_t kM{60 * kS};
    constexpr int64_t kH{60 * kM};
    constexpr int64_t kD{24 * kH};

    const int64_t d{total_ms/kD};
    total_ms %= kD;
    const int64_t h{total_ms/kH};
    total_ms %= kH;
    const int64_t m{total_ms/kM};
    total_ms %= kM;
    const int64_t s{total_ms/1000};
    const int64_t ms{total_ms%1000};

    std::ostringstream ss;
    // days
    if (d) {
        ss << d << ":";
    }
    // hours
    if (h || !ss.str().empty()) {
        ss << std::setfill('0') << std::setw(2) << h << ":";
    }
    // minutes
    if (m || !ss.str().empty()) {
        ss << std::setfill('0') << std::setw(2) << m << ":";
    }
    // seconds
    ss << std::setfill('0') << std::setw(2) << s << ".";
    // milliseconds
    ss << std::setfill('0') << std::setw(3) << ms;

    return ss.str();
}

class Timer {
public:
    explicit Timer(const TimePoint& start_point = now())
    : start_point_{start_point} {}

    // Returns time point from which time is measured.
    TimePoint start_point() const { return start_point_; }
    // Sets new start point. Returns elapsed(time_start) from old start point.
    template<typename Period = std::ratio<1, 1>>
    Duration<Period> restart(const TimePoint& time_point = now()) {
        const auto dur{elapsed<Period>(time_point)};

        start_point_ = time_point;

        return dur;
    }

    static TimePoint now() { return Chr::steady_clock::now(); }

    // Returns elapsed time from start_point() to end_point.
    template<typename Period = std::ratio<1, 1>>
    Duration<Period> elapsed(const TimePoint& end_point = now()) const {
        return end_point - start_point();
    }
    template<typename Period = std::ratio<1, 1>>
    double elapsed_num() const {
        return elapsed<Period>().count();
    }
    template<typename Period = std::ratio<1, 1>>
    std::string elapsed_str() const {
        return duration_str(elapsed<Period>());
    }

private:
    TimePoint start_point_;
};

// Represents shell command execution result.
class Result {
public:
    explicit Result(int exit_code, const Duration<>& dur = {})
    :exit_code_{exit_code}, dur_{dur} {}

    Result(int exit_code, std::string_view output, const Duration<>& dur = {})
    :Result{exit_code, dur} {
        output_ = output;
    }

    explicit Result(bool success, const Duration<>& dur = {})
    :exit_code_{success?kSuccess:kFailure}, dur_{dur} {}

    Result(bool success, std::string_view output, const Duration<>& dur = {})
    :Result{success, dur} {
        output_ = output;
    }

    explicit operator int() const { return exit_code_; }
    explicit operator bool() const { return is_ok(); }

    // Returns Result with kSuccess exit_code.
    static Result SUCCESS(const Duration<>& dur = {}) { return Result {kSuccess, dur}; }
    // Returns Result with kFailure exit_code.
    static Result FAILURE(const Duration<>& dur = {}) { return Result{kFailure, dur}; }

    Result plus_dur(const Duration<>& dur) {
        Result r{*this};
        r.dur_ += dur;
        return r;
    }
    Result with_dur(const Duration<>& dur) {
        Result r{*this};
        r.dur_ = dur;
        return r;
    }

    int exit_code() const { return exit_code_; }
    // Returns if result is success. Returns false otherwise.
    bool is_ok() const { return exit_code_ == kSuccess; }
    // Returns true if exit code is equal to kSuccess
    bool is_success() const { return is_ok(); }
    // Returns true if exit code is equal to kFailure
    bool is_failure() const { return !is_ok(); }

    // Returns command output if there is any.
    std::string output() const { return output_; }

    // Returns command duration.
    Duration<> dur() const { return dur_; }
    // Returns command duration as std::string.
    std::string dur_str() const { return duration_str(dur_); }

    static constexpr int kSuccess{EXIT_SUCCESS};
    static constexpr int kFailure{EXIT_FAILURE};
private:
    int exit_code_;
    std::string output_;
    Duration<> dur_{};
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
    std::string dep_cmd;
    std::string output;
};

// Collection of all setting entries.
struct SettingsCollection {
    std::string version{"0.0.1"};
    bool display_info{true};
    bool display_warn{true};
    unsigned int thread_limit{std::thread::hardware_concurrency()};
    bool parallel_compilation{true};
    bool comp_caching{true};
    bool log_prefix_enabled{true};
    bool log_prefix_colored{true};
    bool log_text_colored{true};
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
    static unsigned int thread_limit() {
        std::lock_guard lock{mtx_};
        return sc_.thread_limit;
    }
    static void set_thread_limit(unsigned int val) {
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

    // Returns max amount of threads that will be used with respect of
    // parallel_compilation.
    static unsigned int thread_limit_comp() {
        std::lock_guard lock{mtx_};
        unsigned int t_l{
            sc_.parallel_compilation?
            sc_.thread_limit:
            1
        };
        return t_l;
    }

    static bool comp_caching() {
        std::lock_guard lock{mtx_};
        return sc_.comp_caching;
    }
    static void set_comp_caching(bool val) {
        std::lock_guard lock{mtx_};
        sc_.comp_caching = val;
    }
    
    static bool log_prefix_enabled() {
        std::lock_guard lock{mtx_};
        return sc_.log_prefix_enabled;
    }
    static void set_log_prefix_enabled(bool val) {
        std::lock_guard lock{mtx_};
        sc_.log_prefix_enabled = val;
    }
    static bool log_prefix_colored() {
        std::lock_guard lock{mtx_};
        return sc_.log_prefix_colored;
    }
    static void set_log_prefix_colored(bool val) {
        std::lock_guard lock{mtx_};
        sc_.log_prefix_colored = val;
    }
    static bool log_text_clored() {
        std::lock_guard lock{mtx_};
        return sc_.log_text_colored;
    }
    static void set_log_text_colored(bool val) {
        std::lock_guard lock{mtx_};
        sc_.log_text_colored = val;
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
    
    static const std::map<LogType, std::string_view> kPrefixTexts{
        {LogType::Info,    "[INFO]"},
        {LogType::Warning, "[WARNING]"},
        {LogType::Error,   "[Error]"},
    };
    // Note(clovis): Color format is \033[38;2<r>;<g>;<b>m
    static const std::map<LogType, std::string_view> kPrefixColors{
        {LogType::Info,    "\033[38;2;100;180;255m"},
        {LogType::Warning, "\033[38;2;255;220;50m"},
        {LogType::Error,   "\033[38;2;255;100;100m"},
    };
    static const std::map<LogType, std::string_view> kTextColors{
        {LogType::Info,    "\033[38;2;180;220;255m"},
        {LogType::Warning, "\033[38;2;255;240;150m"},
        {LogType::Error,   "\033[38;2;255;150;150m"},
    };
    static constexpr std::string_view kResetColor{"\033[0m"};
    
    std::string log_msg;

    if (Settings::log_prefix_enabled()) {
        if (Settings::log_prefix_colored()) {
            log_msg += kPrefixColors.at(lt);
            log_msg += kPrefixTexts.at(lt);
            log_msg += kResetColor;
        } else {
            log_msg += kPrefixTexts.at(lt);
        }

        log_msg += ' ';
    }

    if (Settings::log_text_clored()) {
        log_msg += kTextColors.at(lt);
        log_msg += text;
        log_msg += kResetColor;
    } else {
        log_msg += text;
    }

    std::cout << log_msg << '\n' << std::flush;

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
    Timer t{};

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
    Duration<> cmd_dur{t.elapsed()};
    int exit_code{WEXITSTATUS(exit_status)};

    Result r{exit_code, cmd_output, cmd_dur};
    // Throw error
    if (!cmd.weak && r.is_failure()) {
        Cppbuild::log_e(
            cmd.cmd + ": command failed with exit code "
            + std::to_string(r.exit_code())
            + " in " + r.dur_str() + "."
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

// Executes functions in parallel threads. Order of execution is not defined.
// If funcs.size() > thread_limit - executes the rest of funcs in the main thread.
// Finishes when all funcs finish execution.
inline void execute_funcs_parallel(
    const std::vector<std::function<void()>>& funcs,
    const unsigned int thread_limit = Settings::thread_limit()
) {
    const size_t threads_size(std::clamp(
        funcs.size(),
        size_t{0},
        static_cast<size_t>(thread_limit == 0 ? 0 : thread_limit - 1)
    ));

    std::vector<std::thread> threads{};
    threads.resize(threads_size);

    // Execute funcs
    try {
        for (size_t i{0}; i < funcs.size(); ++i) {
            // Execute in new thread
            if (i < threads_size) {
                threads[i] = std::thread{funcs[i]};
            }
            // If threads hit thread limit - execute in the main thread
            else {
                funcs[i]();
            }
        }
    }
    // If some of threads throws an exception - let already create threads finish
    catch (...) {
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        // Re-throw the exception after all threads finish
        throw;
    }

    // Waiting for all threads to finish
    for (std::thread& thread : threads) {
        thread.join();
    }
}

// Executes commands in parallel threads. Order of execution is not defined.
// If cmds.size() > thread_limit - executes the rest of commands in the main thread.
// Returns vector of pairs of Cmd - Result.
// Order of elements of returned vector is the same as order of cmds.
inline std::vector<std::pair<std::string, Result>> do_execute_commands_parallel(
    const std::vector<ShellCommand>& cmds,
    const unsigned int thread_limit = Settings::thread_limit()
) {
    std::vector<std::pair<std::string, Result>> r{};
    r.resize(cmds.size(), {"", Result::FAILURE()});

    if (cmds.empty()) {
        return r;
    }

    if (cmds.size() == 1) {
        r[0] = {cmds[0].cmd, do_execute_command(cmds[0])};
        return r;
    }

    // Step 1: populate funcs vector
    std::vector<std::function<void()>> funcs{};
    funcs.reserve(cmds.size());
    for (size_t i{0}; i < cmds.size(); ++i) {
        std::function<void()> func{[&cmds, i, &r](){
            ShellCommand cmd{cmds[i]};
            // Important: execute all commands weakly because
            // we do not want to terminate program before all threads are finished.
            cmd.weak = true;
            r[i] = {cmd.cmd, do_execute_command(cmd)};
        }};

        funcs.push_back(func);
    }

    // Step 2: executing commands
    execute_funcs_parallel(funcs, thread_limit);

    // Step 3: throw an error if needed
    int failed_cmds_count{};
    std::string failed_cmds{};
    for (size_t i{}; i < cmds.size(); ++i) {
        const ShellCommand& shell_cmd{cmds[i]};
        const Result& cmd_result{r[i].second};
        if (!shell_cmd.weak && cmd_result.is_failure()) {
            failed_cmds.append(
                "\n" +
                shell_cmd.cmd +
                ": failed with exit code " +
                std::to_string(cmd_result.exit_code())
            );

            ++failed_cmds_count;
        }
    }
    if (!failed_cmds.empty()) {
        std::string err_msg{
            "The following commands(" +
            std::to_string(failed_cmds_count) +
            ") failed:"
        };

        err_msg.append(failed_cmds);

        log_e(err_msg);
    }

    return r;
}
// Constructs vector of ShellCommands based on cmd_pattern and passes it to
// do_execute_commands_parallel(const std::vector<ShellCommand>&,const unsigned int).
inline std::vector<std::pair<std::string, Result>> do_execute_commands_parallel(
    const std::vector<std::string>& cmds,
    const ShellCommand& cmd_pattern,
    const unsigned int thread_limit = Settings::thread_limit()
) {
    if (cmds.empty()) {
        return {};
    }

    std::vector<ShellCommand> shell_cmds{};
    shell_cmds.reserve(cmds.size());

    ShellCommand shell_cmd{cmd_pattern};
    for (const auto& cmd : cmds) {
        shell_cmd.cmd = cmd;
        shell_cmds.push_back(shell_cmd);
    }

    return do_execute_commands_parallel(shell_cmds, thread_limit);
}
// Same as do_execute_commands_parallel(cmds, cmd_pattern, thread_limit), where
// cmd_pattern is ShellCommand::WEAK("") with silent = true.
inline std::vector<std::pair<std::string, Result>> do_execute_commands_parallel_weak(
    const std::vector<std::string>& cmds,
    const unsigned int thread_limit = Settings::thread_limit()
) {
    ShellCommand cmd_pattern{ShellCommand::WEAK("")};
    cmd_pattern.silent = true;
    return do_execute_commands_parallel(cmds, cmd_pattern, thread_limit);
}
// Same as do_execute_commands_parallel(cmds, cmd_pattern, thread_limit), where
// cmd_pattern is ShellCommand::STRONG("") with silent = true.
inline std::vector<std::pair<std::string, Result>> do_execute_commands_parallel_strong(
    const std::vector<std::string>& cmds,
    const unsigned int thread_limit = Settings::thread_limit()
) {
    ShellCommand cmd_pattern{ShellCommand::STRONG("")};
    cmd_pattern.silent = true;
    return do_execute_commands_parallel(cmds, cmd_pattern, thread_limit);
}

// Returns Result::SUCCESS() if directory was created or already exist.
inline Result do_mkdir(const Fs::path& dir_path) {
    Timer t{};

    if (Fs::exists(dir_path) && Fs::is_directory(dir_path)) {
        return Result::SUCCESS();
    }

    log_i(std::string{"Creating directory: "}.append(dir_path.string()));

    if (Fs::exists(dir_path) && !Fs::is_directory(dir_path)) {
        log_e(std::string{dir_path.string()}.append(": is not a directory"));
        return Result::FAILURE(t.elapsed());
    }

    try {
        Fs::create_directories(dir_path);
    } catch (const Fs::filesystem_error& e) {
        log_e(std::string{dir_path.string()}.append(": ").append(e.what()));
        return Result::FAILURE(t.elapsed());
    }

    return Result::SUCCESS(t.elapsed());
}
// Removes entry recursively if exists. Returns true if entry was removed or did
// not exist.
inline Result do_rm(const Fs::path& path) {
    Timer t{};

    if (!Fs::exists(path)) {
        return Result::SUCCESS();
    }

    log_i(std::string{"Removing: "}.append(path.string()));

    try {
        Fs::remove_all(path);
    } catch (const Fs::filesystem_error& e) {
        log_e(std::string{path.string()}.append(": ").append(e.what()));
        return Result::FAILURE(t.elapsed());
    }

    if (Fs::exists(path)) {
        log_e(std::string{"Failed to remove "}.append(path.string()));
        return Result::FAILURE(t.elapsed());
    }

    return Result::SUCCESS(t.elapsed());
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
    Timer t{};

    log_i(std::string{"Changing working directory to: "}.append(path.string()));

    try {
        Fs::current_path(path);
    } catch(const Fs::filesystem_error& e) {
        log_e(std::string{"Failed to change working dir to "}
        .append(path.string()).append(": ").append(e.what()));
        return Result::FAILURE(t.elapsed());
    }

    return Result::SUCCESS(t.elapsed());
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
    if ((mode & std::ios_base::out) == std::ios_base::out) {
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
    if (match.empty() || s.empty()) {
        return s;
    }

    std::string result{};
    result.reserve(s.size());

    size_t i{};
    while(i < s.size()) {
        bool matched{};
        for (const auto& [key, value] : match) {
            if (s.compare(i, key.size(), key) == 0) {
                result.append(value);
                i += key.size();
                break;
            }
        }

        if (!matched){
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
    Timer t{};

    const Fs::path path{full_path(f_path)};

    if ((mode & std::ios_base::in) == std::ios_base::in) {
        log_e(path.string() + ": wrong openmode std::ios_base::in");
        return Result::FAILURE(t.elapsed());
    }

    if (!overwrite && Fs::exists(path)) {
        return Result::FAILURE(t.elapsed());
    }
    if (Fs::exists(path) && !Fs::is_regular_file(path)) {
        log_e(path.string() + ": is not a regular file");
        return Result::FAILURE(t.elapsed());
    }

    // Create parent dir.
    if (!do_mkdir(path.parent_path())) {
        log_e(path.string() + ": failed to create parent dir " + path.parent_path().string());
        return Result::FAILURE(t.elapsed());
    }

    // Create file
    std::ofstream file{path, mode};
    if (!file) {
        log_e(path.string() + ": failed to create/open file");
        return Result::FAILURE(t.elapsed());
    }

    if (!f_content.empty()) {
        file << f_content;
    }
    if (!file) {
        log_e(path.string() + ": failed to write file");
        return Result::FAILURE(t.elapsed());
    }

    return Result::SUCCESS(t.elapsed());
}

// This function is usefull when you have an output that is determined by the source.
// Returns true if source last write was AFTER output last wrtite.
// Returns true if either source or output does not exist.
// Returns true if any filesystem error occured.
inline bool is_source_modified(const Fs::path& source, const Fs::path& output) {
    if (!Fs::exists(source) || !Fs::exists(output)) {
        return true;
    }
    try {
        return Fs::last_write_time(source) > Fs::last_write_time(output);
    } catch(...) {
        return true;
    }
}

// Returns Result::SUCCESS() if something was written to output_file.
inline Result do_configure_file(
    const Fs::path& input_file,
    const std::map<std::string_view, std::string_view>& match,
    const Fs::path& output_file,
    // If overwrite is false - return Result::FAILURE() if output_file already exists
    bool overwrite = true
) {
    Timer t{};

    if (!overwrite && Fs::exists(output_file)) {
        return Result::FAILURE();
    }
    if (Fs::exists(output_file) && !Fs::is_regular_file(output_file)) {
        log_e(output_file.string() + ": is not a regular file");
        return Result::FAILURE();
    }

    // If source does not change - do nothing
    if (!is_source_modified(input_file, output_file)) {
        return Result::SUCCESS(t.elapsed());
    }

    // This may be an empty string
    std::string content{get_file_content(input_file)};

    // Configure string
    content = configure_string(content, match);

    // Write file
    Result r{do_create_file(output_file, content, true, std::ios_base::out)};
    // Update result duration
    r = r.with_dur(t.elapsed());

    return r;
}

enum class CompileTargetType {
    Executable,
    // You probably want to add "-fPIC" arg if you are using unix compiler
    // when building shared library
    SharedLib,
};

class CompileCommand {
public:
    CompileCommand() = default;
    explicit CompileCommand(std::string_view compiler) :c_path_{compiler} {}


    /////////////////////////////////////GETTERS////////////////////////////////////

    const std::string& compiler() const { return c_path_; }
    const std::string& target_name() const { return target_name_; }
    CompileTargetType target_type() const { return target_type_; }
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
    // If target_type() == Executable:
    //   On Windows return target_name() + ".exe"(if target_name() does not provide it).
    //   On other platforms just returns target_name().
    // If target_type() == SharedLib:
    //   On Windows return target_name() + ".dll".
    //   On other platforms returns target_name() + ".so".
    std::string target_full_name() const {
        std::string t_name{target_name()};
#if defined(_WIN32) || defined(_WIN64)
        if (target_type() == CompileTargetType::SharedLib) {
            t_name.append(".dll");
        }
        else if (!Fs::path{t_name}.has_extension()) {
            t_name.append(".exe");
        }
#else
        if (target_type() == CompileTargetType::SharedLib) {
            t_name.append(".so");
        }
#endif
        return t_name;
    }
    // If target_full_name() is absolute - returns its full path.
    // Otherwise, returns full path of build_dir() + target_full_name().
    Fs::path target_path() const { 
        Fs::path target_name{target_full_name()};
        if (target_name.is_absolute()) {
            return full_path(target_name);
       }
        
        return full_path(build_dir().append(target_name.string()));
    }
    
    // Returns full dir path of target_path().
    Fs::path target_dir_path() const { return target_path().parent_path(); }

    // Returns vector of CompilationObj, containing compile command for each source file.
    // Returns empty vector if no sources were added or compiler was not defined.
    std::vector<CompilationObj> compilation_objs() const {
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

            std::string dep_cmd{cmd};

#if defined(_WIN32) || defined(_WIN64)
            const std::string src_out_file{Fs::path{source}.filename().string() + ".obj"};
#else
            const std::string src_out_file{Fs::path{source}.filename().string() + ".o"};
#endif
            const std::string src_out_path{full_path(build_dir().append(src_out_file)).string()};
            const std::string src_path{full_path(source).string()};
            if (is_using_msvc()) {
                cmd.append(" /c ");
                cmd += src_path; // enclosed path
                cmd.append(" /Fo ");
                cmd += src_out_path; // enclosed path

                // TODO(clovis): this does not work(it needs the full command with args), it generates a lot of bloat
                dep_cmd.append(" /showIncludes " + src_path);
            } else {
                cmd.append(" -c ");
                cmd += src_path;
                cmd.append(" -o ");
                cmd += src_out_path;

                dep_cmd.append(" -MM " + src_path);
            }

            std::pair<std::string, std::string> cmd_pair{full_path(source).string(), cmd};
            CompilationObj comp_cmd{};
            comp_cmd.source = src_path;
            comp_cmd.cmd = cmd;
            comp_cmd.dep_cmd = dep_cmd;
            comp_cmd.output = src_out_path;

            cmds.emplace_back(comp_cmd);
        }

        return cmds;
    }
    // Returns compilation commands for all sources.
    std::vector<std::string> get_compilation_cmds() const {
        std::vector<std::string> cmds{};
        for (const auto& obj : compilation_objs()) {
            cmds.push_back(obj.cmd);
        }

        return cmds;
    }
    // Returns compilation commands only for sources that needs to be recompiled.
    std::vector<std::string> do_get_recompilation_cmds() const {
        std::vector<std::string> cmds{};

        std::vector<CompilationObj> objs_to_check{};
        std::vector<std::string> cmds_to_exec{};
        for (const auto& obj : compilation_objs()) {
            if (!Fs::exists(obj.output)) {
                cmds.push_back(obj.cmd);
            } else {
                objs_to_check.emplace_back(obj);
                cmds_to_exec.emplace_back(obj.dep_cmd);
            }
        }

        // Get all dependency headers
        std::vector<std::pair<std::string, Result>> cmds_r {
            do_execute_commands_parallel_strong(cmds_to_exec, Settings::thread_limit_comp())
        };

        // IMPORTANT: this whole loop relies on deterministic of cmds_r
        for (size_t i{}; i < cmds_r.size(); ++i) {
            const auto& r {cmds_r[i]};

            // Vector of all dependencies that should be checked
            std::vector<Fs::path> deps_paths{};

            // Step 1: populate deps_paths
            // msvc
            if (is_using_msvc()) {
                // TODO(clovis): handle msvc output here
                log_e("Not yet implemented for msvc");
            }
            // Unix compilers
            else {
                std::string data{r.second.output()};
                // Replace all "\\\n" symbols with space
                size_t pos{};
                while ((pos = data.find("\\\n", pos)) != std::string::npos) {
                    data.replace(pos, 2, " ");
                    pos += 1;
                }

                std::istringstream iss{data};
                std::string token{};
                bool is_first_token{true};
                while (iss >> token) {
                    // Skip source name token
                    if (is_first_token) {
                        is_first_token = false;
                        continue;
                    }
                    // Populate path vector
                    deps_paths.emplace_back(token);
                }
            }

            // Step 2: check each file time
            const CompilationObj& obj{objs_to_check[i]};
            const auto& obj_time {Fs::last_write_time(obj.output)};
            for (const auto& dep : deps_paths) {
                const auto& dep_time {Fs::last_write_time(dep)};

                // Check if dep was modified after obj creation
                if (dep_time > obj_time) {
                    cmds.emplace_back(obj.cmd);
                    break;
                }
            }
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
        if (target_type() == CompileTargetType::SharedLib) {
            if (is_using_msvc()) {
                cmd.append(" /DLL");
            } else {
                cmd.append(" -shared");
            }
        }

        for (const auto& source : compiler_sources()) {
#if defined(_WIN32) || defined(_WIN64)
            const std::string src_out_file{Fs::path{source}.filename().string() + ".obj"};
#else
            const std::string src_out_file{Fs::path{source}.filename().string() + ".o"};
#endif
            const std::string src_out_path{full_path(build_dir().append(src_out_file)).string()};

            cmd += " " + src_out_path;
        }
        if (is_using_msvc()) {
            cmd.append(" /Fe ");
            cmd += target_path().string();
        } else {
            cmd.append(" -o ");
            cmd += target_path().string();
        }

        return cmd;
    }

    ////////////////////////////////////////////////////////////////////////////////

    /////////////////////////////////////SETTERS////////////////////////////////////

    void set_compiler(std::string_view c_path) { c_path_ = c_path; }

    void set_target_name(std::string_view target_name) {
        target_name_ = target_name;
    }

    void set_target_type(const CompileTargetType& type) {
        target_type_ = type;
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
        for (int i{0}; i < static_cast<int>(c_arg.size()); ++i) {
            if (!std::isspace(c_arg[i])) {
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
        Timer t{};

        if (compiler().empty()) {
            if (!weak) {
                log_e("Compiler is not set");
            }
            return Result::FAILURE();
        }
        if (compiler_sources().empty()) {
            if (!weak) {
                log_e("Compiler sources is not set");
            }
            return Result::FAILURE();
        }
        if (target_name().empty()) {
            if (!weak) {
                log_e("Target is not set");
            }
            return Result::FAILURE();
        }

        // Step 1: Build dir and target dir step
        Result build_dir_r{do_make_build_dir()};
        if (build_dir_r.is_failure()) {
            return build_dir_r;
        }
        Result target_dir_r{do_mkdir(target_dir_path())};
        if (target_dir_r.is_failure()) {
            return target_dir_r;
        }

        log_i(std::string{"Compiling target: "}.append(target_path().string()));

        // Step 2: Compilation step
        std::vector<std::string> cmds{};
        // TODO(clovis): comp_caching is not support for msvc for now
        if (Settings::comp_caching() && !is_using_msvc()) {
            cmds = do_get_recompilation_cmds();
        } else {
            cmds = get_compilation_cmds();
        }

        // If there is nothing to compile and target already exists - skip linking step
        if (cmds.empty() && Fs::exists(target_path())) {
            return Result::SUCCESS(t.elapsed());
        }

        // Do actual compilation
        const std::vector<std::pair<std::string, Result>>& comp_results{
            weak?
            do_execute_commands_parallel_weak(cmds, Settings::thread_limit_comp()):
            do_execute_commands_parallel_strong(cmds, Settings::thread_limit_comp())
        };
        for ( const auto& result : comp_results) {
            if (result.second.is_failure()) {
                if (!weak) {
                    log_e(
                        result.first +
                        ": failed with exit code " +
                        std::to_string(result.second.exit_code())
                    );
                }
                return result.second;
            }
        }

        Duration<> comp_dur{t.elapsed()};

        // Step 3: Linking step
        const std::string cmd{linking_cmd()};
        Result link_r{weak?do_execute_command_weak(cmd):do_execute_command_strong(cmd)};

        return link_r.plus_dur(comp_dur);
    }

    Result do_run(bool weak = false) const {
        if (target_type() != CompileTargetType::Executable) {
            return Result::SUCCESS();
        }

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
        Result comp_r{do_compile(weak)};
        if (!comp_r || target_type() != CompileTargetType::Executable) {
            return comp_r;
        }

        Result run_r{do_run(weak)};

        return run_r.plus_dur(comp_r.dur());
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
        Timer t{};

        bool msvc_syntax{is_using_msvc()};

        std::stringstream args_sstream{do_get_package_args(package, msvc_syntax)};

        std::string arg{};
        CompilerArgs p_args{};
        while(args_sstream >> arg) {
            p_args.insert(arg);
        }

        if (p_args.empty()) {
            return Result::FAILURE(t.elapsed());
        }

        add_compiler_args(p_args);

        return Result::SUCCESS(t.elapsed());
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
        std::vector<CompilationObj> cmds {compilation_objs()};
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
            // str += working_dir().string();
            str += working_dir().generic_string();
            str += "\",\n";
            // file
            str += "\"file\": ";
            str += "\"";
            // str += file;
            str += Fs::path{file}.generic_string();
            str += "\",\n";
            // command
            str += "\"command\": ";
            str += "\"";
            // str += it->cmd;
            str += Fs::path{it->cmd}.generic_string();
            str += "\",\n";
            // output
            str += "\"output\": ";
            str += "\"";
            // str += it->output;
            str += Fs::path{it->output}.generic_string();
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
        Timer t{};

        Result mkdir_r{do_make_build_dir()};
        if (!mkdir_r) {
            return mkdir_r;
        }

        const std::string file_name{build_dir().append("compile_commands.json").string()};
        std::ofstream file{file_name};
        if (!file) {
            log_w(std::string{file_name}.append(": failed to create file stream"));
            return Result::FAILURE(t.elapsed());
        }

        file << compile_commands_string();
        if (!file) {
            log_w(std::string{file_name}.append(": failed to write file"));
            return Result::FAILURE(t.elapsed());
        }

        return Result::SUCCESS(t.elapsed());
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
    CompileTargetType target_type_{CompileTargetType::Executable};
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

    // Returns vector of compilation commands for each source.
    std::vector<std::string> get_compilation_cmds(bool comp_caching = Settings::comp_caching()) const {
        std::vector<std::string> cmds{};

        std::string args{};
        for (const auto& arg : compiler_args()) {
            args.append(arg + ' ');
        }
        for (const auto& source : compiler_sources()) {
            std::string output{to_moc_output_name(source)};

            // If source was not modified - no need to call moc.
            if (comp_caching && !is_source_modified(source, output)) {
                continue;
            }

            std::string cmd{compiler() + " "};

            cmd += source;
            cmd.append(" -o ");
            cmd += output + " ";

            cmd.append(args);

            cmds.emplace_back(cmd);
        }

        return cmds;
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
        Timer t{};

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

        // Step 1: Build dir step
        Result build_dir_r{do_make_build_dir()};
        if (build_dir_r.is_failure()) {
            return build_dir_r;
        }

        // Step 2: Compilation step
        std::vector<std::string> cmds{get_compilation_cmds()};
        if (cmds.empty()) {
            return Result::SUCCESS(t.elapsed());
        }

        log_i("Running moc...");

        // Do actual compilation
        const std::vector<std::pair<std::string, Result>>& comp_results{
            weak?
            do_execute_commands_parallel_weak(cmds, Settings::thread_limit_comp()):
            do_execute_commands_parallel_strong(cmds, Settings::thread_limit_comp())
        };
        for ( const auto& result : comp_results) {
            if (result.second.is_failure()) {
                if (!weak) {
                    log_e(
                        result.first +
                        ": failed with exit code " +
                        std::to_string(result.second.exit_code())
                    );
                }
                return result.second;
            }
        }

        // Step 3: Add parent sources step
        if (parent_command()) {
            parent_command()->add_compiler_sources(moc_output());
        }

        return Result::SUCCESS(t.elapsed());
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

// The purpose of this function is to automatically rebuild cppbuild program if needed.
// Without this function you manualy do something like this on each project build:
// clang++ cppbuild.cpp -o cppbuild && ./cppbuild
//
// With this function you build your project like this:
// 1. Do initial build ONCE: clang++ cppbuild.cpp -o cppbuild
// 2. When you want to rebuild your project justs execute: ./cppbuild
//
// The benefit of this is that you do not rebuild your build program
// every time you need to build your project; also you dont need to
// to write long compile command every time, you just run cppbuild executable.
//
// How to use:
// Just call DO_SELF_REBUILD() at the beginning of your main function in cppbuild.cpp.
// IMPORTANT: You should manually rebuild your cppbuild.cpp in this cases:
// 1. You change something BEFORE DO_SELF_REBUILD() call
// 2. You pass different parameters of DO_SELF_REBUILD()
// TODO(clovis): for now this feature is not supported on windows because
// windows does not allow to overwrite currently running exe file
inline void DO_SELF_REBUILD(CompileCommand& cc) {
#if defined(_WIN32) || defined(_WIN64)
    return;
#endif
    // If rebuild is needed
    if (!cc.do_get_recompilation_cmds().empty()) {
        log_i("Rebuilding " + full_path(cc.target_path()).string() + "...");

        Settings::set_display_info(false);
        const int exit_code{cc.do_compile_and_run(true).exit_code()};
        Settings::set_display_info(true);

        std::exit(exit_code);
    }
}
// Create CompileCommand and passes it to DO_SELF_REBUILD(CompileCommand&).
// - sets compiler to compiler;
// - sets build dir to "build/cppbuild/"
// - sets compiler sources to sources
// - sets target name to "../../cppbuild"
// For more info see DO_SELF_REBUILD(CompileCommand&).
inline void DO_SELF_REBUILD(
    const std::string& compiler,
    const CompilerSources& sources = {"cppbuild.cpp"}
) {
    CompileCommand cc{compiler};
    cc.set_build_dir("build/cppbuild/");
    cc.set_compiler_sources(sources);
    cc.set_target_name("../../cppbuild");
    
    DO_SELF_REBUILD(cc);
}
}  // namespace Cppbuild
