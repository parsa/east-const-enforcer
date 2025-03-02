#include <string>

int main() {
    {
        const int x = 5;
        const int* ptr = nullptr;
        const int& ref = x;
        const int& ref2 = x;
        const int& ref3 = x;
        const auto& y = x;
        const auto& y2 = x;
        const auto& y3 = x;
    }

    {
        const std::string str = "Hello";
        const std::string* ptr1 = &str;
        const std::string &ref4 = str;
        const std::string & ref5 = str;
        const std::string& ref6 = str;
    }

    {
        const char* const ptr2 = nullptr;
        const char * const ptr3 = nullptr;
        const char *const ptr4 = nullptr;
        const char **const ptr5 = nullptr;
        const char* const *ptr6 = nullptr;
        const char* const* ptr7 = nullptr;
        const char* const *ptr8 = nullptr;
    }
}