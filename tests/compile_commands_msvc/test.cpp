#include <fstream>
#include <filesystem>
#include <regex>

#include "some_header.h"

int main() {
    if (return_two() != 2) {
        return 1;
    }

    // Check if file exists and has valid syntax

    std::filesystem::path file_path{"compile_commands_msvc/build/compile_commands.json"};
    std::ifstream file {file_path};
    if (!file) {
        return 2;
    }

    std::stringstream content_buffer{};
    content_buffer << file.rdbuf();
    std::string content{content_buffer.str()};

    if (content_buffer.fail()) {
        return 3;
    }

    std::regex pattern{
R"(\[
\{
\"directory\": \".*tests\",
\"file\": \".*some_dir[\/\\]some_source\.cpp\",
\"command\": \"msvc \/Icompile_commands_msvc \/Icompile_commands_msvc[\/\\]some_dir \/W4 \/WX \/Wpermissive- \/c .*some_dir[\/\\]some_source\.cpp \/Fo .*tests[\/\\]compile_commands_msvc[\/\\]build[\/\\]some_source\.cpp\.o(bj)?\",
\"output\": \".*some_source\.cpp\.o(bj)?\"
\},
\{
\"directory\": \".*tests\",
\"file\": \".*test\.cpp\",
\"command\": \"msvc \/Icompile_commands_msvc \/Icompile_commands_msvc[\/\\]some_dir \/W4 \/WX \/Wpermissive- \/c .*test\.cpp \/Fo .*tests[\/\\]compile_commands_msvc[\/\\]build[\/\\]test\.cpp\.o(bj)?\",
\"output\": \".*test\.cpp\.o(bj)?\"
\}
\])"
    };

    bool is_json_valid{std::regex_match(content, pattern)};

    if (!is_json_valid) {
        return 4;
    }

    return 0;
}
