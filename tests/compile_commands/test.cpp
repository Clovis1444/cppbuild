#include <fstream>
#include <filesystem>
#include <regex>

#include "some_header.h"

int main() {
    if (return_two() != 2) {
        return 1;
    }

    // Check if file exists and has valid syntax

    std::filesystem::path file_path{"build/compile_commands.json"};
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
\"directory\": \".*tests[\/\\]compile_commands\",
\"file\": \"some_dir[\/\\]some_source\.cpp\",
\"command\": \".* some_dir[\/\\]some_source.cpp test.cpp -Isome_dir -Wall -Werror -Wextra -Wpedantic -o .*tests[\/\\]compile_commands[\/\\]build[\/\\]compile_commands_test(\.exe)?\"
\},
\{
\"directory\": \".*tests[\/\\]compile_commands\",
\"file\": \"test\.cpp\",
\"command\": \".* some_dir[\/\\]some_source.cpp test.cpp -Isome_dir -Wall -Werror -Wextra -Wpedantic -o .*tests[\/\\]compile_commands[\/\\]build[\/\\]compile_commands_test(\.exe)?\"
\}
\])"
    };

    bool is_json_valid{std::regex_match(content, pattern)};

    if (!is_json_valid) {
        return 4;
    }

    return 0;
}
