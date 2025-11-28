#include "EastConstTestHarness.h"

class EastConstExampleCasesTest : public EastConstTestHarness {};

TEST_F(EastConstExampleCasesTest, HandlesSimpleTypes) {
  std::string input = R"cpp(
    const int x = 5;
    const int* ptr = nullptr;
    const int& ref = x;
    const int& ref2 = x;
    const int& ref3 = x;
    const auto& y = x;
    const auto& y2 = x;
    const auto& y3 = x;
  )cpp";
  
  std::string expected = R"cpp(
    int const x = 5;
    int const* ptr = nullptr;
    int const& ref = x;
    int const& ref2 = x;
    int const& ref3 = x;
    auto const& y = x;
    auto const& y2 = x;
    auto const& y3 = x;
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesStdStringTypes) {
  std::string input = R"cpp(
    const std::string str = "Hello";
    const std::string* ptr1 = &str;
    const std::string &ref4 = str;
    const std::string & ref5 = str;
    const std::string& ref6 = str;
  )cpp";
  
  std::string expected = R"cpp(
    std::string const str = "Hello";
    std::string const* ptr1 = &str;
    std::string const &ref4 = str;
    std::string const & ref5 = str;
    std::string const& ref6 = str;
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesComplexPointerTypes) {
  std::string input = R"cpp(
    const char* const ptr2 = nullptr;
    const char * const ptr3 = nullptr;
    const char *const ptr4 = nullptr;
    const char **const ptr5 = nullptr;
    const char* const *ptr6 = nullptr;
    const char* const* ptr7 = nullptr;
    const char* const *ptr8 = nullptr;
  )cpp";
  
  std::string expected = R"cpp(
    char const* const ptr2 = nullptr;
    char const * const ptr3 = nullptr;
    char const *const ptr4 = nullptr;
    char const **const ptr5 = nullptr;
    char const* const *ptr6 = nullptr;
    char const* const* ptr7 = nullptr;
    char const* const *ptr8 = nullptr;
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesFunctionSignatures) {
  std::string input = R"cpp(
    const int foo();
    void bar(const int x, const std::string& s);
    const char* getString();
    const std::vector<const int*>& getVectorRef();
  )cpp";
  
  std::string expected = R"cpp(
    int const foo();
    void bar(int const x, std::string const& s);
    char const* getString();
    std::vector<int const*> const& getVectorRef();
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesContainerTypes) {
  std::string input = R"cpp(
    const std::vector<int> vec;
    const std::map<std::string, int> map;
    const std::set<const int*> set;
    const std::pair<const int, const std::string> pair;
    const std::vector<const std::vector<int>> nested;
  )cpp";
  
  std::string expected = R"cpp(
    std::vector<int> const vec;
    std::map<std::string, int> const map;
    std::set<int const*> const set;
    std::pair<int const, std::string const> const pair;
    std::vector<std::vector<int> const> const nested;
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesClassMembers) {
  std::string input = R"cpp(
    struct Foo {
      const int x;
      const std::string name;
      const int* ptr;
      const std::vector<int> values;
      
      void method(const int param) const;
    };
  )cpp";
  
  std::string expected = R"cpp(
    struct Foo {
      int const x;
      std::string const name;
      int const* ptr;
      std::vector<int> const values;
      
      void method(int const param) const;
    };
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesTemplateTypes) {
  std::string input = R"cpp(
    template<typename T>
    const T getValue();
    
    template<const int SIZE>
    class Array {};
    
    const std::shared_ptr<const int> ptr;
    
    template<typename T>
    const std::vector<const T> getVector();
  )cpp";
  
  std::string expected = R"cpp(
    template<typename T>
    T const getValue();
    
    template<int const SIZE>
    class Array {};
    
    std::shared_ptr<int const> const ptr;
    
    template<typename T>
    std::vector<T const> const getVector();
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesArrayTypes) {
  std::string input = R"cpp(
    const int arr1[10] = {};
    const char* arr2[5];
    const std::array<int, 5> stdArr1;
    const std::array<const std::string, 3> stdArr2;
  )cpp";
  
  std::string expected = R"cpp(
    int const arr1[10] = {};
    char const* arr2[5];
    std::array<int, 5> const stdArr1;
    std::array<std::string const, 3> const stdArr2;
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesTypedefAndUsing) {
  std::string input = R"cpp(
    typedef const int ConstInt;
    typedef const std::string* ConstStringPtr;
    using ConstDouble = const double;
    using ConstCharRef = const char&;
    using ConstVectorInt = const std::vector<const int>;
  )cpp";
  
  std::string expected = R"cpp(
    typedef int const ConstInt;
    typedef std::string const* ConstStringPtr;
    using ConstDouble = double const;
    using ConstCharRef = char const&;
    using ConstVectorInt = std::vector<int const> const;
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, NegativeTestCases) {
  std::string input = R"cpp(
    // Already east-const style
    int const alreadyEast = 5;
    std::string const& alreadyEastRef = "hello";
    
    // Function definitions should also be rewritten
    void foo(const int x, int const y) {}
    
    // Non-const types should be unchanged
    int nonConst = 10;
    std::string normalString = "test";
  )cpp";
  
  std::string expected = R"cpp(
    // Already east-const style
    int const alreadyEast = 5;
    std::string const& alreadyEastRef = "hello";
    
    // Function definitions should also be rewritten
    void foo(int const x, int const y) {}
    
    // Non-const types should be unchanged
    int nonConst = 10;
    std::string normalString = "test";
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesEdgeCases) {
  std::string input = R"cpp(
    // const_cast
    const int* const_ptr = nullptr;
    int* ptr = const_cast<int*>(const_ptr);
    
    struct S {
      mutable const int* ptrMember;  // mutable with const pointee
      volatile const int y; // volatile with const
      const volatile int z; // const and volatile
    };
    
    // Multiple declarations
    const int a = 1, *b = &a, &c = a;
  )cpp";
  
  std::string expected = R"cpp(
    // const_cast
    int const* const_ptr = nullptr;
    int* ptr = const_cast<int*>(const_ptr);
    
    struct S {
      mutable int const* ptrMember;  // mutable with const pointee
      volatile int const y; // volatile with const
      int const volatile z; // const and volatile
    };
    
    // Multiple declarations
    int const a = 1, *b = &a, &c = a;
  )cpp";
  
  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesFunctionPointersAndMembers) {
  std::string input = R"cpp(
    struct Foo;

    using FuncPtr = const int (*)(const int*, const std::string&);
    using MemberFuncPtr = const std::string (Foo::*)(const int&) const;

    const int (*fp)(const int*, const std::string&) = nullptr;
    const int* (*fp2)(const int* const, const std::string&) = nullptr;
    const FuncPtr fpAlias = nullptr;

    struct Bar {
      const int (*callback)(const int*, const std::string&);
      const MemberFuncPtr methodPtr;
    };
  )cpp";

  std::string expected = R"cpp(
    struct Foo;

    using FuncPtr = int const (*)(int const*, std::string const&);
    using MemberFuncPtr = std::string const (Foo::*)(int const&) const;

    int const (*fp)(int const*, std::string const&) = nullptr;
    int const* (*fp2)(int const* const, std::string const&) = nullptr;
    FuncPtr const fpAlias = nullptr;

    struct Bar {
      int const (*callback)(int const*, std::string const&);
      MemberFuncPtr const methodPtr;
    };
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesTrailingReturnTypes) {
  std::string input = R"cpp(
    auto func1() -> const int;
    auto func2() -> const std::string&;
    auto func3() -> const std::vector<const int*>;
    auto func4() -> const std::pair<const int, const std::string>;
  )cpp";

  std::string expected = R"cpp(
    auto func1() -> int const;
    auto func2() -> std::string const&;
    auto func3() -> std::vector<int const*> const;
    auto func4() -> std::pair<int const, std::string const> const;
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesMacros) {
  std::string input = R"cpp(
    #define DECL_CONST_INT(name) const int name = 42;
    #define DECL_MIXED(name) const std::vector<const int*> name;

    DECL_CONST_INT(macroVar)
    DECL_MIXED(macroVec)

    const int outsideMacro = 5;
    const std::vector<const int*> outsideVec;
  )cpp";

  std::string expected = R"cpp(
    #define DECL_CONST_INT(name) const int name = 42;
    #define DECL_MIXED(name) const std::vector<const int*> name;

    DECL_CONST_INT(macroVar)
    DECL_MIXED(macroVec)

    int const outsideMacro = 5;
    std::vector<int const*> const outsideVec;
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesCommentInterleavingAndLiterals) {
  std::string input = R"cpp(
    #define SOME_MACRO(x) static_assert(true, #x)

    /* Leading block */
    const std::string name1 = "const should stay";

    const
    // inline comment
    std::string name2 = "const";

    const std::string
    /* block between */
    name3 = "still const";

    const std::string name4 /* trailing comment */ = R"str(
      const auto inside_string = true;
    )str";

    SOME_MACRO(const);
  )cpp";

  std::string expected = R"cpp(
    #define SOME_MACRO(x) static_assert(true, #x)

    /* Leading block */
    std::string const name1 = "const should stay";

    // inline comment
    std::string const name2 = "const";

    std::string const
    /* block between */
    name3 = "still const";

    std::string const name4 /* trailing comment */ = R"str(
      const auto inside_string = true;
    )str";

    SOME_MACRO(const);
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesStructuredBindingsAndConcepts) {
  std::string input = R"cpp(
    struct Point {
      int first;
      int second;
    };

    const Point source{1, 2};
    const auto [a, b] = source;
    const auto& [ra, rb] = source;
    const auto&& forwarded = Point{3, 4};

    consteval const int compute() { return 42; }
    constinit const int GlobalValue = compute();

    template <typename T>
    requires std::is_integral_v<T>
    const T twice(const T& value) {
      return value + value;
    }
  )cpp";

  std::string expected = R"cpp(
    struct Point {
      int first;
      int second;
    };

    Point const source{1, 2};
    auto const [a, b] = source;
    auto const& [ra, rb] = source;
    auto const&& forwarded = Point{3, 4};

    consteval int const compute() { return 42; }
    constinit int const GlobalValue = compute();

    template <typename T>
    requires std::is_integral_v<T>
    T const twice(T const& value) {
      return value + value;
    }
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesContinuationsAndDirectives) {
  std::string input = R"cpp(
    #define WRAP_CONST(name) const int name = 7;

    const int continued \
    = 1;

    const int spaced =\
    2;

    #if 1
    const std::string active = "enabled";
    #else
    const std::string inactive = "disabled";
    #endif
  )cpp";

  std::string expected = R"cpp(
    #define WRAP_CONST(name) const int name = 7;

    int const continued \
    = 1;

    int const spaced =\
    2;

    #if 1
    std::string const active = "enabled";
    #else
    const std::string inactive = "disabled";
    #endif
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesAdvancedFunctionPointers) {
  std::string input = R"cpp(
    struct Foo {
      const int memFn(const int& x) const noexcept;
      const int& refFn(const int* const ptr) const &;
    };

    using MemFnPtr = const int (Foo::*)(const int&) const noexcept;
    using RefMemFnPtr = const int& (Foo::*)(const int* const) const &;

    const MemFnPtr p1 = &Foo::memFn;
    const RefMemFnPtr p2 = &Foo::refFn;

    void takesFuncPtr(const int (*fp)(const int&));
    void takesMemPtr(const int (Foo::*mp)(const int&) const);
  )cpp";

  std::string expected = R"cpp(
    struct Foo {
      int const memFn(int const& x) const noexcept;
      int const& refFn(int const* const ptr) const &;
    };

    using MemFnPtr = int const (Foo::*)(int const&) const noexcept;
    using RefMemFnPtr = int const& (Foo::*)(int const* const) const &;

    MemFnPtr const p1 = &Foo::memFn;
    RefMemFnPtr const p2 = &Foo::refFn;

    void takesFuncPtr(int const (*fp)(int const&));
    void takesMemPtr(int const (Foo::*mp)(int const&) const);
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesAutoAndDecltypeReturns) {
  std::string input = R"cpp(
    const int global = 0;

    const auto func1() { return global; }
    const auto& func2();
    const decltype(global) func3();
    const decltype((global)) func4();
  )cpp";

  std::string expected = R"cpp(
    int const global = 0;

    auto const func1() { return global; }
    auto const& func2();
    decltype(global) const func3();
    decltype((global)) const func4();
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesAttributesAndQualifiers) {
  std::string input = R"cpp(
    [[nodiscard]] const int attrFn1();
    __attribute__((nodiscard)) const int attrFn2();

    [[nodiscard]] const std::vector<const int*> attrVec();
  )cpp";

  std::string expected = R"cpp(
    [[nodiscard]] int const attrFn1();
    __attribute__((nodiscard)) int const attrFn2();

    [[nodiscard]] std::vector<int const*> const attrVec();
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesLambdasAndModernConstructs) {
  std::string input = R"cpp(
    auto lambda1 = [](const int x) -> const int { return x; };
    auto lambda2 = [](const std::string& s) { const int y = 42; return s; };

    constexpr const int cx = 5;
    constexpr const std::vector<const int> cv = {};
  )cpp";

  std::string expected = R"cpp(
    auto lambda1 = [](int const x) -> int const { return x; };
    auto lambda2 = [](std::string const& s) { int const y = 42; return s; };

    constexpr int const cx = 5;
    constexpr std::vector<int const> const cv = {};
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesExoticTemplates) {
  std::string input = R"cpp(
    template <typename T, const int N = 4>
    struct ArrayWrapper {
      const T data[N];
    };

    template <typename T>
    struct Wrapper;

    template <typename T>
    struct Wrapper<const T> {
      const T value;
    };

    template <typename T>
    concept ConstIntegral = std::is_integral_v<T> && std::is_const_v<T>;

    template <ConstIntegral T>
    const T getConstIntegral();
  )cpp";

  std::string expected = R"cpp(
    template <typename T, int const N = 4>
    struct ArrayWrapper {
      T const data[N];
    };

    template <typename T>
    struct Wrapper;

    template <typename T>
    struct Wrapper<T const> {
      T const value;
    };

    template <typename T>
    concept ConstIntegral = std::is_integral_v<T> && std::is_const_v<T>;

    template <ConstIntegral T>
    T const getConstIntegral();
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesNamespaceQualifiedReferences) {
  std::string input = R"cpp(
    namespace api {
      struct Finder {
        struct Result {};
      };
    }

    struct UsesMatcher {
      void setCallback(const api::Finder::Result &Result);
      const api::Finder::Result &getResult() const;
    };

    const api::Finder::Result &GlobalResult();
  )cpp";

  std::string expected = R"cpp(
    namespace api {
      struct Finder {
        struct Result {};
      };
    }

    struct UsesMatcher {
      void setCallback(api::Finder::Result const &Result);
      api::Finder::Result const &getResult() const;
    };

    api::Finder::Result const &GlobalResult();
  )cpp";

  testTransformation(input, expected);
}

TEST_F(EastConstExampleCasesTest, HandlesFunctionDefinitionParameters) {
  std::string input = R"cpp(
    namespace api {
      struct Finder {
        struct Result {};
      };
    }

    class Handler {
    public:
      void consume(const api::Finder::Result &Result) {
        (void)Result;
      }
    };

    const api::Finder::Result &Make(const api::Finder::Result &Result) {
      return Result;
    }
  )cpp";

  std::string expected = R"cpp(
    namespace api {
      struct Finder {
        struct Result {};
      };
    }

    class Handler {
    public:
      void consume(api::Finder::Result const &Result) {
        (void)Result;
      }
    };

    api::Finder::Result const &Make(api::Finder::Result const &Result) {
      return Result;
    }
  )cpp";

  testTransformation(input, expected);
}
