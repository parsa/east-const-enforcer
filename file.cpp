#include <string>

int main() {
    {
        int const x = 5;
        int const* ptr = nullptr;
        int const& ref = x;
        int const& ref2 = x;
        int const& ref3 = x;
        auto const&y = x;
        auto const& y2 = x;
        auto const& y3 = x;
    }

    {
        const std::string str = "Hello";
        const std::string* ptr1 = &str;
        const std::string &ref4 = str;
        const std::string & ref5 = str;
        const std::string& ref6 = str;
    }

    {
        char const* const ptr2 = nullptr;
        char const* const ptr3 = nullptr;
        char const*const ptr4 = nullptr;
        const const char ** constconst ptr5 = nullptr;
        char const* const *ptr6 = nullptr;
        char const* const* ptr7 = nullptr;
        char const* const *ptr8 = nullptr;
    }
}