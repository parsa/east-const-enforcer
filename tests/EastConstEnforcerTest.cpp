#include <EastConstEnforcer.h>
#include <gtest/gtest.h>

#include <cctype>
#include <optional>
#include <string_view>
#include <vector>

#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"  // Add this include for Rewriter
#include <llvm/Support/Error.h>
#include <clang/Tooling/CompilationDatabase.h>

namespace {

const std::string &getFakeStdHeader() {
  static const std::string Header = R"cpp(#pragma once

namespace std {

using size_t = decltype(sizeof(0));

template <typename T>
class initializer_list {
public:
  initializer_list(const T *data = nullptr, size_t size = 0)
      : Data(data), Size(size) {}
  const T *begin() const { return Data; }
  const T *end() const { return Data + Size; }
  size_t size() const { return Size; }

private:
  const T *Data;
  size_t Size;
};

class string {
public:
  string() = default;
  string(const char *) {}
  string(const string &) = default;
  string &operator=(const string &) = default;
};

template <typename T>
class vector {
public:
  vector() = default;
  vector(initializer_list<T>) {}
};

template <typename T>
class set {
public:
  set() = default;
};

template <typename Key, typename T>
class map {
public:
  map() = default;
};

template <typename T>
class shared_ptr {
public:
  shared_ptr() = default;
};

template <typename T, typename U>
struct pair {
  pair() = default;
};

template <typename T, size_t N>
struct array {
  array() = default;
};

template <typename T, T V>
struct integral_constant {
  static constexpr T value = V;
  using value_type = T;
  using type = integral_constant;
  constexpr operator value_type() const noexcept { return value; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

template <typename T>
struct is_integral : false_type {};

template <>
struct is_integral<bool> : true_type {};

template <>
struct is_integral<char> : true_type {};

template <>
struct is_integral<signed char> : true_type {};

template <>
struct is_integral<unsigned char> : true_type {};

template <>
struct is_integral<short> : true_type {};

template <>
struct is_integral<unsigned short> : true_type {};

template <>
struct is_integral<int> : true_type {};

template <>
struct is_integral<unsigned int> : true_type {};

template <>
struct is_integral<long> : true_type {};

template <>
struct is_integral<unsigned long> : true_type {};

template <>
struct is_integral<long long> : true_type {};

template <>
struct is_integral<unsigned long long> : true_type {};

template <typename T>
struct is_integral<const T> : is_integral<T> {};

template <typename T>
struct is_integral<volatile T> : is_integral<T> {};

template <typename T>
struct is_integral<const volatile T> : is_integral<T> {};

template <typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

template <typename T>
struct is_const : false_type {};

template <typename T>
struct is_const<T const> : true_type {};

template <typename T>
inline constexpr bool is_const_v = is_const<T>::value;

template <typename T, typename U>
struct is_same : false_type {};

template <typename T>
struct is_same<T, T> : true_type {};

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

} // namespace std

)cpp";
  return Header;
}

std::string addStandardIncludes(const std::string &code) {
  return "#include \"fake_std.h\"\n\n" + code;
}

} // namespace

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

class EastConstEnforcerTest : public ::testing::Test {
protected:
  std::string runToolOnCode(const std::string &code) {
    // Provide an in-memory compilation database for the test file.
    FixedCompilationDatabase compilations(".", {"-std=c++20"});
    std::vector<std::string> sources = {"test.cpp"};

    // Use a RefactoringTool for replacement tracking
    RefactoringTool tool(compilations, sources);
    tool.mapVirtualFile("test.cpp", code);
    tool.mapVirtualFile("fake_std.h", getFakeStdHeader());
    
    // Create our checker and set up matchers
    EastConstChecker checker([&tool](const SourceManager &SM,
                                     CharSourceRange Range,
                                     llvm::StringRef NewText) {
      Replacement Rep(SM, Range, NewText);
      std::string FilePath = Rep.getFilePath().str();
      if (FilePath.empty())
        return;
      auto &FileReplacements = tool.getReplacements()[FilePath];
      llvm::Error Err = FileReplacements.add(Rep);
      if (Err) {
        llvm::errs() << "Test replacement error for " << FilePath << ": "
                     << llvm::toString(std::move(Err)) << "\n";
      }
    });
    MatchFinder finder;
    registerEastConstMatchers(finder, &checker);
    
    // Run the tool
    std::unique_ptr<FrontendActionFactory> factory = newFrontendActionFactory(&finder);
    int runResult = tool.run(factory.get());  // Renamed to avoid redefinition
    EXPECT_EQ(runResult, 0);
    
    // Get the rewritten source with tool's replacements
    std::string rewrittenSource = code;
    auto &fileReplacements = tool.getReplacements();

    for (const auto &Entry : fileReplacements) {
      const Replacements &ReplacementsForFile = Entry.second;
      if (ReplacementsForFile.empty())
        continue;

      llvm::Expected<std::string> result =
          applyAllReplacements(rewrittenSource, ReplacementsForFile);
      if (result)
        rewrittenSource = std::move(*result);
    }
    
    return rewrittenSource;
  }
  
  void testTransformation(const std::string &input, const std::string &expected) {
    std::string wrappedInput = addStandardIncludes(input);
    std::string wrappedExpected = addStandardIncludes(expected);
    std::string result = runToolOnCode(wrappedInput);
    EXPECT_EQ(result, wrappedExpected);
  }
};

TEST_F(EastConstEnforcerTest, HandlesSimpleTypes) {
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

TEST_F(EastConstEnforcerTest, HandlesStdStringTypes) {
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

TEST_F(EastConstEnforcerTest, HandlesComplexPointerTypes) {
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

TEST_F(EastConstEnforcerTest, HandlesFunctionSignatures) {
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

TEST_F(EastConstEnforcerTest, HandlesContainerTypes) {
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

TEST_F(EastConstEnforcerTest, HandlesClassMembers) {
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

TEST_F(EastConstEnforcerTest, HandlesTemplateTypes) {
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

TEST_F(EastConstEnforcerTest, HandlesArrayTypes) {
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

TEST_F(EastConstEnforcerTest, HandlesTypedefAndUsing) {
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

TEST_F(EastConstEnforcerTest, NegativeTestCases) {
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

TEST_F(EastConstEnforcerTest, HandlesEdgeCases) {
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

TEST_F(EastConstEnforcerTest, HandlesFunctionPointersAndMembers) {
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

TEST_F(EastConstEnforcerTest, HandlesTrailingReturnTypes) {
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

TEST_F(EastConstEnforcerTest, HandlesMacros) {
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

TEST_F(EastConstEnforcerTest, HandlesAdvancedFunctionPointers) {
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

TEST_F(EastConstEnforcerTest, HandlesAutoAndDecltypeReturns) {
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

TEST_F(EastConstEnforcerTest, HandlesAttributesAndQualifiers) {
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

TEST_F(EastConstEnforcerTest, HandlesLambdasAndModernConstructs) {
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

TEST_F(EastConstEnforcerTest, HandlesExoticTemplates) {
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

TEST_F(EastConstEnforcerTest, HandlesNamespaceQualifiedReferences) {
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

TEST_F(EastConstEnforcerTest, HandlesFunctionDefinitionParameters) {
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

namespace {

enum ContextBit : unsigned {
  kVariableContext = 1u << 0,
  kParameterContext = 1u << 1,
  kMemberContext = 1u << 2,
  kAliasContext = 1u << 3,
  kTemplateAliasContext = 1u << 4,
  kReturnContext = 1u << 5,
};

constexpr unsigned kAllContexts = kVariableContext | kParameterContext |
                                  kMemberContext | kAliasContext |
                                  kTemplateAliasContext | kReturnContext;

struct TypeSpec {
  std::string Prefix;
  std::string Suffix;
  unsigned Contexts;
  bool CanBePointee;
  bool IsReference;
  bool IsMemberPointer;
};

enum class TypeOp { LeadConst, TrailConst, Pointer };

std::string trimCopy(const std::string &text) {
  size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

void replaceAll(std::string &text, std::string_view from,
                std::string_view to) {
  if (from.empty())
    return;
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

void normalizeConstReferenceSpacing(std::string &text) {
  replaceAll(text, "const &&", "const&&");
  replaceAll(text, "const &", "const&");
}

TypeSpec prependConst(TypeSpec spec) {
  spec.Prefix = "const " + spec.Prefix;
  return spec;
}

TypeSpec appendConst(TypeSpec spec) {
  auto insertConstBeforeToken = [](const std::string &token,
                                   TypeSpec spec,
                                   bool attachToToken) -> TypeSpec {
    const size_t pos = spec.Prefix.find(token);
    if (pos == std::string::npos)
      return spec;
    std::string before = spec.Prefix.substr(0, pos);
    std::string after = spec.Prefix.substr(pos);
    while (!before.empty() &&
           std::isspace(static_cast<unsigned char>(before.back())))
      before.pop_back();
    if (!before.empty())
      before.push_back(' ');
    spec.Prefix = before + "const";
    if (!attachToToken)
      spec.Prefix.push_back(' ');
    spec.Prefix += after;
    return spec;
  };

  if (spec.IsReference) {
    const bool isRValueRef = spec.Prefix.find("&&") != std::string::npos;
    const std::string token = isRValueRef ? "&&" : "&";
    return insertConstBeforeToken(token, spec, /*attachToToken=*/true);
  }

  if (spec.IsMemberPointer)
    return insertConstBeforeToken("(A::*", spec, /*attachToToken=*/false);

  if (!spec.Prefix.empty() && spec.Prefix.back() != ' ')
    spec.Prefix.push_back(' ');
  spec.Prefix += "const ";
  return spec;
}

std::optional<TypeSpec> wrapPointer(const TypeSpec &spec) {
  if (!spec.CanBePointee)
    return std::nullopt;

  TypeSpec result;
  if (spec.Suffix.empty()) {
    result.Prefix = spec.Prefix;
    if (result.Prefix.empty() || result.Prefix.back() != ' ')
      result.Prefix.push_back(' ');
    result.Prefix += "* ";
    result.Suffix.clear();
  } else {
    result.Prefix = spec.Prefix + "(*";
    result.Suffix = ")" + spec.Suffix;
  }
  result.Contexts = kAllContexts;
  result.CanBePointee = false;
  result.IsReference = spec.IsReference;
  result.IsMemberPointer = false;
  return result;
}

std::optional<TypeSpec> applyOps(const TypeSpec &base,
                                 const std::vector<TypeOp> &ops) {
  std::optional<TypeSpec> current = base;
  for (TypeOp op : ops) {
    if (!current)
      return std::nullopt;
    switch (op) {
    case TypeOp::LeadConst:
      current = prependConst(*current);
      break;
    case TypeOp::TrailConst:
      current = appendConst(*current);
      break;
    case TypeOp::Pointer:
      current = wrapPointer(*current);
      break;
    }
  }
  return current;
}

struct BaseEntry {
  std::string Name;
  TypeSpec Spec;
};

struct PatternEntry {
  std::string Name;
  std::vector<TypeOp> WestOps;
  std::vector<TypeOp> EastOps;
};

struct TypeCombination {
  std::string BaseName;
  std::string PatternName;
  TypeSpec West;
  TypeSpec East;
  unsigned SharedContexts;
};

const std::vector<BaseEntry> &getBaseEntries() {
  static const std::vector<BaseEntry> entries = {
      {"int",
       {"int ", "", kAllContexts, true, false, false}},
      {"int_ref",
       {"int& ", "",
        kVariableContext | kParameterContext | kMemberContext |
            kAliasContext | kTemplateAliasContext,
        false, true, false}},
      {"int_rref",
       {"int&& ", "",
        kVariableContext | kParameterContext | kMemberContext |
            kAliasContext | kTemplateAliasContext,
        false, true, false}},
      {"int_array",
       {"int ", "[3]",
        kVariableContext | kParameterContext | kMemberContext |
            kAliasContext | kTemplateAliasContext,
        true, false, false}},
      {"int_function",
       {"int ", "()", kAliasContext | kTemplateAliasContext, true, false,
        false}},
      {"int_function_with_param",
       {"int ", "(double)", kAliasContext | kTemplateAliasContext, true,
        false, false}},
      {"int_member_ptr",
       {"int (A::*", ")", kAllContexts, false, false, true}},
      {"int_member_fn_ptr",
       {"int (A::*", ")(double)", kAllContexts, false, false, true}},
  };
  return entries;
}

const std::vector<PatternEntry> &getPatternEntries() {
  static const std::vector<PatternEntry> patterns = {
      {"ConstPrefix",
       {TypeOp::LeadConst},
       {TypeOp::TrailConst}},
      {"AlreadyEastConst",
       {TypeOp::TrailConst},
       {TypeOp::TrailConst}},
      {"Pointer",
       {TypeOp::Pointer},
       {TypeOp::Pointer}},
      {"PointerToConst",
       {TypeOp::LeadConst, TypeOp::Pointer},
       {TypeOp::TrailConst, TypeOp::Pointer}},
      {"ConstPointer",
       {TypeOp::Pointer, TypeOp::TrailConst},
       {TypeOp::Pointer, TypeOp::TrailConst}},
      {"ConstPointerWithConstPointee",
       {TypeOp::LeadConst, TypeOp::Pointer, TypeOp::TrailConst},
       {TypeOp::TrailConst, TypeOp::Pointer, TypeOp::TrailConst}},
  };
  return patterns;
}

std::string sanitizeIdentifier(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (char ch : text) {
    if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
      result.push_back(ch);
    } else {
      result.push_back('_');
    }
  }
  if (!result.empty() &&
      std::isdigit(static_cast<unsigned char>(result.front()))) {
    result.insert(result.begin(), '_');
  }
  return result;
}

std::string instantiateDecl(const TypeSpec &spec,
                            const std::string &identifier) {
  return spec.Prefix + identifier + spec.Suffix;
}

std::string typeExpression(const TypeSpec &spec) {
  return trimCopy(spec.Prefix + spec.Suffix);
}

std::string functionReturnDecl(const TypeSpec &spec,
                               const std::string &identifier) {
  return instantiateDecl(spec, identifier + "()") + ";";
}

struct SnippetPair {
  std::string Input;
  std::string Expected;
};

void appendVariableContext(const TypeCombination &combo,
                           const std::string &caseId,
                           std::string &input,
                           std::string &expected) {
  const std::string westName = "g_" + caseId + "_west";
  const std::string eastName = "g_" + caseId + "_east";
  input += "extern " + instantiateDecl(combo.West, westName) + ";\n";
  input += "extern " + instantiateDecl(combo.East, eastName) + ";\n";
  expected += "extern " + instantiateDecl(combo.East, westName) + ";\n";
  expected += "extern " + instantiateDecl(combo.East, eastName) + ";\n";
  const std::string assertion =
      "static_assert(std::is_same_v<decltype(" + westName + "), decltype(" +
      eastName + ")>);\n";
  input += assertion;
  expected += assertion;
}

void appendParameterContext(const TypeCombination &combo,
                            const std::string &caseId,
                            std::string &input,
                            std::string &expected) {
  const std::string westFunc = "ParamWest_" + caseId;
  const std::string eastFunc = "ParamEast_" + caseId;
  input += "void " + westFunc + "(" +
           instantiateDecl(combo.West, "value") + ");\n";
  input += "void " + eastFunc + "(" +
           instantiateDecl(combo.East, "value") + ");\n";
  expected += "void " + westFunc + "(" +
              instantiateDecl(combo.East, "value") + ");\n";
  expected += "void " + eastFunc + "(" +
              instantiateDecl(combo.East, "value") + ");\n";
  const std::string assertion =
      "static_assert(std::is_same_v<decltype(&" + westFunc + "), decltype(&" +
      eastFunc + ")>);\n";
  input += assertion;
  expected += assertion;
}

void appendMemberContext(const TypeCombination &combo,
                         const std::string &caseId,
                         std::string &input,
                         std::string &expected) {
  const std::string structName = "MemberHolder_" + caseId;
  input += "struct " + structName + " {\n  " +
           instantiateDecl(combo.West, "west_member") + ";\n  " +
           instantiateDecl(combo.East, "east_member") + ";\n};\n";
  expected += "struct " + structName + " {\n  " +
              instantiateDecl(combo.East, "west_member") + ";\n  " +
              instantiateDecl(combo.East, "east_member") + ";\n};\n";
  const std::string westAccess = "((" + structName + "*)nullptr)->west_member";
  const std::string eastAccess = "((" + structName + "*)nullptr)->east_member";
  const std::string assertion =
      "static_assert(std::is_same_v<decltype(" + westAccess + "), decltype(" +
      eastAccess + ")>);\n";
  input += assertion;
  expected += assertion;
}

void appendAliasContext(const TypeCombination &combo,
                        const std::string &caseId,
                        std::string &input,
                        std::string &expected) {
  const std::string aliasWest = "AliasWest_" + caseId;
  const std::string aliasEast = "AliasEast_" + caseId;
  const std::string tmplWest = "AliasTemplateWest_" + caseId;
  const std::string tmplEast = "AliasTemplateEast_" + caseId;

  input += "using " + aliasWest + " = " + typeExpression(combo.West) + ";\n";
  input += "using " + aliasEast + " = " + typeExpression(combo.East) + ";\n";
  input += "template <typename Dummy>\nusing " + tmplWest +
           " = " + typeExpression(combo.West) + ";\n";
  input += "template <typename Dummy>\nusing " + tmplEast +
           " = " + typeExpression(combo.East) + ";\n";

  expected += "using " + aliasWest + " = " + typeExpression(combo.East) +
              ";\n";
  expected += "using " + aliasEast + " = " + typeExpression(combo.East) +
              ";\n";
  expected += "template <typename Dummy>\nusing " + tmplWest +
              " = " + typeExpression(combo.East) + ";\n";
  expected += "template <typename Dummy>\nusing " + tmplEast +
              " = " + typeExpression(combo.East) + ";\n";

  const std::string aliasAssert =
      "static_assert(std::is_same_v<" + aliasWest + ", " + aliasEast + ">);\n";
  const std::string tmplAssert =
      "static_assert(std::is_same_v<" + tmplWest + "<int>, " +
      tmplEast + "<int>>);\n";
  input += aliasAssert;
  input += tmplAssert;
  expected += aliasAssert;
  expected += tmplAssert;
}

void appendReturnContext(const TypeCombination &combo,
                         const std::string &caseId,
                         std::string &input,
                         std::string &expected) {
  const std::string retWest = "ReturnWest_" + caseId;
  const std::string retEast = "ReturnEast_" + caseId;
  const std::string trailingWest = "TrailingReturnWest_" + caseId;
  const std::string trailingEast = "TrailingReturnEast_" + caseId;

  input += functionReturnDecl(combo.West, retWest) + "\n";
  input += functionReturnDecl(combo.East, retEast) + "\n";
  input += "auto " + trailingWest + "() -> " + typeExpression(combo.West) +
           ";\n";
  input += "auto " + trailingEast + "() -> " + typeExpression(combo.East) +
           ";\n";

  expected += functionReturnDecl(combo.East, retWest) + "\n";
  expected += functionReturnDecl(combo.East, retEast) + "\n";
  expected += "auto " + trailingWest + "() -> " + typeExpression(combo.East) +
              ";\n";
  expected += "auto " + trailingEast + "() -> " + typeExpression(combo.East) +
              ";\n";

  const std::string retAssert =
      "static_assert(std::is_same_v<decltype(" + retWest + "()), decltype(" +
      retEast + "())>);\n";
  const std::string trailingAssert =
      "static_assert(std::is_same_v<decltype(" + trailingWest + "()), decltype(" +
      trailingEast + "())>);\n";
  input += retAssert;
  input += trailingAssert;
  expected += retAssert;
  expected += trailingAssert;
}

void appendCase(const TypeCombination &combo, std::string &input,
                std::string &expected) {
  const std::string caseId =
      sanitizeIdentifier(combo.BaseName + "_" + combo.PatternName);
  const std::string comment = "// Case: " + combo.BaseName + " with " +
                              combo.PatternName + "\n";
  input += comment;
  expected += comment;

  if (combo.SharedContexts & kVariableContext)
    appendVariableContext(combo, caseId, input, expected);
  if (combo.SharedContexts & kParameterContext)
    appendParameterContext(combo, caseId, input, expected);
  if (combo.SharedContexts & kMemberContext)
    appendMemberContext(combo, caseId, input, expected);
  if ((combo.SharedContexts & (kAliasContext | kTemplateAliasContext)) != 0)
    appendAliasContext(combo, caseId, input, expected);
  if (combo.SharedContexts & kReturnContext)
    appendReturnContext(combo, caseId, input, expected);

  input += "\n";
  expected += "\n";
}

SnippetPair buildConstGridSnippets() {
  SnippetPair snippets;
  snippets.Input = "namespace generated_const_grid {\n";
  snippets.Expected = snippets.Input;
  const char *preamble =
      "struct A {\n  int value;\n  int method();\n  int method_with_arg(double);\n};\n\n";
  snippets.Input += preamble;
  snippets.Expected += preamble;

  for (const auto &base : getBaseEntries()) {
    for (const auto &pattern : getPatternEntries()) {
      auto westOpt = applyOps(base.Spec, pattern.WestOps);
      auto eastOpt = applyOps(base.Spec, pattern.EastOps);
      if (!westOpt || !eastOpt)
        continue;
      unsigned shared = westOpt->Contexts & eastOpt->Contexts;
      if (shared == 0)
        continue;
      TypeCombination combo{base.Name, pattern.Name, *westOpt, *eastOpt, shared};
      appendCase(combo, snippets.Input, snippets.Expected);
    }
  }

  snippets.Input += "} // namespace generated_const_grid\n";
  snippets.Expected += "} // namespace generated_const_grid\n";
  normalizeConstReferenceSpacing(snippets.Expected);
  return snippets;
}

} // namespace

TEST_F(EastConstEnforcerTest, ProceduralConstGrid) {
  auto snippets = buildConstGridSnippets();
  testTransformation(snippets.Input, snippets.Expected);
}
