#include <EastConstEnforcer.h>
#include <gtest/gtest.h>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include <llvm/Support/Error.h>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

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

} // namespace std

)cpp";
  return Header;
}

std::string addStandardIncludes(const std::string &code) {
  return "#include \"fake_std.h\"\n\n" + code;
}

class WesterlyTest : public ::testing::Test {
protected:
  std::string runToolOnCode(const std::string &code) {
    FixedCompilationDatabase compilations(".", {"-std=c++20"});
    std::vector<std::string> sources = {"westerly.cpp"};

    RefactoringTool tool(compilations, sources);
    tool.mapVirtualFile("westerly.cpp", code);
    tool.mapVirtualFile("fake_std.h", getFakeStdHeader());

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

    auto factory = newFrontendActionFactory(&finder);
    int runResult = tool.run(factory.get());
    EXPECT_EQ(runResult, 0);

    std::string rewrittenSource = code;
    auto &fileReplacements = tool.getReplacements();
    for (const auto &Entry : fileReplacements) {
      const Replacements &repls = Entry.second;
      if (repls.empty())
        continue;

      auto result = applyAllReplacements(rewrittenSource, repls);
      if (result)
        rewrittenSource = *result;
    }

    return rewrittenSource;
  }

  void testTransformation(const std::string &input,
                          const std::string &expected) {
    const std::string wrappedInput = addStandardIncludes(input);
    const std::string wrappedExpected = addStandardIncludes(expected);
    std::string result = runToolOnCode(wrappedInput);
    EXPECT_EQ(result, wrappedExpected);
  }
};

TEST_F(WesterlyTest, HandlesGeneralCode001) {
  const std::string input = R"W001(
const int kAConstant = 55;

#define SOME_MACRO(arg)

/*
 Using declarations are no issue for us at all
 */
using IntTriplet = const int[3];
using IntPointer = const int*;

static std::string const kPrivateVar1 = "";
static const std::string kPrivateVar2 = "";

/*
 Comments containing const keywords are ignored as they should be.
 */
using ConstStringRef = const std::string&;
using ConstStringRef2 = const ::std::string&;

int main(int argc, char const *const *argv) {
  auto const kUnusedStringLiteral = R"cpp(
       const int kFoo = 33;
  )cpp";
  const std::vector<int> vector_of_ints{
      1, 2, 3, 4, 5, 6,
  };
  const std::vector<const char*> vector_of_strings{
      kUnusedStringLiteral, 
      kUnusedStringLiteral,
  };
  SOME_MACRO(const);
  return argc < kAConstant ? 0 : -1;
}

)W001";

  const std::string expected = R"W001(
int const kAConstant = 55;

#define SOME_MACRO(arg)

/*
 Using declarations are no issue for us at all
 */
using IntTriplet = int const[3];
using IntPointer = int const*;

static std::string const kPrivateVar1 = "";
static std::string const kPrivateVar2 = "";

/*
 Comments containing const keywords are ignored as they should be.
 */
using ConstStringRef = std::string const&;
using ConstStringRef2 = ::std::string const&;

int main(int argc, char const *const *argv) {
  auto const kUnusedStringLiteral = R"cpp(
       const int kFoo = 33;
  )cpp";
  std::vector<int> const vector_of_ints{
      1, 2, 3, 4, 5, 6,
  };
  std::vector<char const*> const vector_of_strings{
      kUnusedStringLiteral, 
      kUnusedStringLiteral,
  };
  SOME_MACRO(const);
  return argc < kAConstant ? 0 : -1;
}

)W001";

  testTransformation(input, expected);
}

TEST_F(WesterlyTest, HandlesInterleavedComments002) {
  const std::string input = R"W002(
/*
 This file contains a bunch of test cases where comments are interleaved
 with type definitions. Some of those are very unlikely to happen in 
 real code-bases, but then again...
 */

// Foo
const std::string kFoo1 = "foo";

const
// Foo
std::string kFoo2 = "foo";

const std::string 
// Foo
kFoo3 = "foo";

/* Foo */ const std::string kFoo4 = "foo";
const /* Foo */ std::string kFoo5 = "foo";
const std::string /* Foo */ kFoo6 = "foo";

)W002";

  const std::string expected = R"W002(
/*
 This file contains a bunch of test cases where comments are interleaved
 with type definitions. Some of those are very unlikely to happen in 
 real code-bases, but then again...
 */

// Foo
std::string const kFoo1 = "foo";

// Foo
std::string const kFoo2 = "foo";

std::string const 
// Foo
kFoo3 = "foo";

/* Foo */ std::string const kFoo4 = "foo";
/* Foo */ std::string const kFoo5 = "foo";
std::string const /* Foo */ kFoo6 = "foo";

)W002";

  testTransformation(input, expected);
}

TEST_F(WesterlyTest, HandlesConstexprAndStorageSpecs003) {
  const std::string input = R"W003(
constexpr const int kFoo1 = 1;
const constexpr int kFoo2 = 2;
const int constexpr kFoo3 = 3;
int const constexpr kFoo4 = 4;
int constexpr const kFoo5 = 5;

int static const kFoo6 = 6;
int const static kFoo7 = 7;
const int static kFoo9 = 8;
const static int kFoo10 = 9;
static const int kFoo11 = 10;

int inline const kFoo12 = 11;
int const inline kFoo13 = 12;
const int inline kFoo14 = 13;
const inline int kFoo15 = 14;
inline const int kFoo16 = 15;

)W003";

  const std::string expected = R"W003(
constexpr int const kFoo1 = 1;
constexpr int const kFoo2 = 2;
int const constexpr kFoo3 = 3;
int const constexpr kFoo4 = 4;
int constexpr const kFoo5 = 5;

int static const kFoo6 = 6;
int const static kFoo7 = 7;
int const static kFoo9 = 8;
static int const kFoo10 = 9;
static int const kFoo11 = 10;

int inline const kFoo12 = 11;
int const inline kFoo13 = 12;
int const inline kFoo14 = 13;
inline int const kFoo15 = 14;
inline int const kFoo16 = 15;

)W003";

  testTransformation(input, expected);
}

TEST_F(WesterlyTest, HandlesAccessSpecifiers) {
  const std::string input = R"W004(
class Foo {
  public:
    const std::string bar1;
    const std::string bar2;

  private:
    const static std::string bar3;
    static const std::string bar4;
};

)W004";

  const std::string expected = R"W004(
class Foo {
  public:
    std::string const bar1;
    std::string const bar2;

  private:
    static std::string const bar3;
    static std::string const bar4;
};

)W004";

  testTransformation(input, expected);
}

TEST_F(WesterlyTest, HandlesAutoDeclarations) {
  const std::string input = R"W005(
const auto foobar = 1;

)W005";

  const std::string expected = R"W005(
auto const foobar = 1;

)W005";

  testTransformation(input, expected);
}

TEST_F(WesterlyTest, HandlesVirtualMethods) {
  const std::string input = R"W006(
class Foo {
    virtual const Foo & bar();
};

)W006";

  const std::string expected = R"W006(
class Foo {
    virtual Foo const & bar();
};

)W006";

  testTransformation(input, expected);
}

} // namespace
