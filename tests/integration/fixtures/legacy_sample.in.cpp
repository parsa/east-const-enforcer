struct SampleString {
    SampleString(const char* v) : value(v) {}
    const char* value;
};

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
        const SampleString str = "Hello";
        const SampleString* ptr1 = &str;
        const SampleString &ref4 = str;
        const SampleString & ref5 = str;
        const SampleString& ref6 = str;
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
