#include <raylib.h>

int main() {
    constexpr int kMajor{6};
    constexpr int kMinor{0};
    constexpr int kPatch{0};
    bool is_correct_vesrion{
        RAYLIB_VERSION_MAJOR == kMajor &&
        RAYLIB_VERSION_MINOR == kMinor &&
        RAYLIB_VERSION_PATCH == kPatch
    };

    bool is_correct_value{GetRandomValue(1,1) == 1};

    return (is_correct_vesrion && is_correct_value)?0:1;
}
