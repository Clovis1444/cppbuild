#include "../../cppbuild.hpp"

int main() {
    Cppbuild::Settings::set_display_info(false);

    const Cppbuild::Fs::path input_file{"text.txt.in"};
    const Cppbuild::Fs::path output_file{"text.txt"};

    std::map<std::string_view, std::string_view> match_list {
        {"@VER@\n", "1.44.4\n"},
        {"Hello, World!\n", "Hello from configured file!\n"},
        {"This is the first string.\nThis is the second string.", "This is the first string. This is the second string."},
    };

    Cppbuild::Result r = Cppbuild::do_configure_file(input_file, match_list, output_file);
    if (!r) {
        return 2;
    }

    const std::string test_output{Cppbuild::get_file_content(output_file)};
    const std::string correct_output{
R"(This is a test text file.

Test 1: the next string should change from "version: @VER@" to "version: 1.44.4".
version: 1.44.4

Test 2: the next string should change from "Hello, World!" to "Hello from configured file!".
Hello from configured file!

Test 3: the next two strings should change from "This is the first string.\nThis is the second string." to "This is the first string. This is the second string.".
This is the first string. This is the second string.
)"
    };

    return (test_output == correct_output)?0:3;
}
