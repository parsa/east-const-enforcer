struct SampleString {
    SampleString(char const* v) : value(v) {}
    char const* value;
};

int main() {
    {
        int const x = 5;
        int const* ptr = nullptr;
        int const& ref = x;
        int const& ref2 = x;
        int const& ref3 = x;
        auto const& y = x;
        auto const& y2 = x;
        auto const& y3 = x;
    }

    {
        SampleString const str = "Hello";
        SampleString const* ptr1 = &str;
        SampleString const &ref4 = str;
        SampleString const & ref5 = str;
        SampleString const& ref6 = str;
    }

    {
        char const* const ptr2 = nullptr;
        char const * const ptr3 = nullptr;
        char const *const ptr4 = nullptr;
        char const **const ptr5 = nullptr;
        char const* const *ptr6 = nullptr;
        char const* const* ptr7 = nullptr;
        char const* const *ptr8 = nullptr;
    }
}
