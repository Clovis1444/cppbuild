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

    uintmax_t file_size{};
    try {
        file_size = std::filesystem::file_size(file_path);
    } catch(const std::filesystem::filesystem_error&) {
        return 3;
    }

    std::string content(file_size, '\0');

    file.read(content.data(), static_cast<std::streamsize>(file_size));
    if (file.fail()){
        return 4;
    }

    std::regex pattern{
R"(\[
\{
\"directory\": \".*tests\/compile_commands\",
\"file\": \"some_dir\/some_source\.cpp\",
\"command\": \".* some_dir\/some_source.cpp test.cpp -Isome_dir -Wall -Werror -Wextra -Wpedantic -o .*tests\/compile_commands\/build\/compile_commands_test\"
\},
\{
\"directory\": \".*tests\/compile_commands\",
\"file\": \"test\.cpp\",
\"command\": \".* some_dir\/some_source.cpp test.cpp -Isome_dir -Wall -Werror -Wextra -Wpedantic -o .*tests\/compile_commands\/build\/compile_commands_test\"
\}
\])"
    };

    bool is_json_valid{std::regex_match(content, pattern)};

    if (!is_json_valid) {
        return 5;
    }

    return 0;
}
