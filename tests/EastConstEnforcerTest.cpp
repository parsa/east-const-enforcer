#include <EastConstEnforcer.h>
#include <gtest/gtest.h>

#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"  // Add this include for Rewriter
#include <clang/Tooling/CompilationDatabase.h>

namespace {

const std::string &getFakeStdHeader() {
  static const std::string Header = R"cpp(#pragma once

namespace std {

using size_t = decltype(sizeof(0));

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
    FixedCompilationDatabase compilations(".", {"-std=c++17"});
    std::vector<std::string> sources = {"test.cpp"};

    // Use a RefactoringTool for replacement tracking
    RefactoringTool tool(compilations, sources);
    tool.mapVirtualFile("test.cpp", code);
    tool.mapVirtualFile("fake_std.h", getFakeStdHeader());
    
    // Create our checker and set up matchers
    EastConstChecker checker(tool);
    MatchFinder finder;

    auto varMatcher = varDecl(unless(parmVarDecl())).bind("varDecl");
    finder.addMatcher(varMatcher, &checker);

    auto funcMatcher = functionDecl().bind("functionDecl");
    finder.addMatcher(funcMatcher, &checker);

    auto fieldMatcher = fieldDecl().bind("fieldDecl");
    finder.addMatcher(fieldMatcher, &checker);

    auto typedefMatcher = typedefDecl().bind("typedefDecl");
    finder.addMatcher(typedefMatcher, &checker);

    auto aliasMatcher = typeAliasDecl().bind("aliasDecl");
    finder.addMatcher(aliasMatcher, &checker);

    auto nonTypeParamMatcher =
      nonTypeTemplateParmDecl().bind("nonTypeTemplateParm");
    finder.addMatcher(nonTypeParamMatcher, &checker);
    
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
    
    // Function parameters should be unchanged
    void foo(const int x, int const y) {}
    
    // Non-const types should be unchanged
    int nonConst = 10;
    std::string normalString = "test";
  )cpp";
  
  std::string expected = input;  // Should remain unchanged
  
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
