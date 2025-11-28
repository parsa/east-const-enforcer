#pragma once

#include <EastConstEnforcer.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include <llvm/Support/Error.h>

inline bool &eastConstHarnessVerboseFlag() {
  static bool flag = false;
  return flag;
}

inline bool eastConstHarnessVerbose() { return eastConstHarnessVerboseFlag(); }
inline void setEastConstHarnessVerbose(bool enabled) {
  eastConstHarnessVerboseFlag() = enabled;
}

class EastConstTestHarness : public ::testing::Test {
protected:
  static const std::string &getFakeStdHeader();
  static std::string addStandardIncludes(const std::string &code);

  std::string runToolOnCode(const std::string &code);
  void testTransformation(const std::string &input,
                          const std::string &expected);
};

inline const std::string &EastConstTestHarness::getFakeStdHeader() {
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

inline std::string EastConstTestHarness::addStandardIncludes(
    const std::string &code) {
  return "#include \"fake_std.h\"\n\n" + code;
}

inline std::string EastConstTestHarness::runToolOnCode(const std::string &code) {
  clang::tooling::FixedCompilationDatabase compilations(
      ".", {"-std=c++20"});
  std::vector<std::string> sources = {"test.cpp"};

  setQuietMode(!eastConstHarnessVerbose());

  clang::tooling::RefactoringTool tool(compilations, sources);
  tool.mapVirtualFile("test.cpp", code);
  tool.mapVirtualFile("fake_std.h", getFakeStdHeader());

  EastConstChecker checker(
      [&tool](const clang::SourceManager &SM, clang::CharSourceRange Range,
              llvm::StringRef NewText) {
        clang::tooling::Replacement Rep(SM, Range, NewText);
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

  clang::ast_matchers::MatchFinder finder;
  registerEastConstMatchers(finder, &checker);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&finder);
  int runResult = tool.run(factory.get());
  EXPECT_EQ(runResult, 0);

  std::string rewrittenSource = code;
  auto &fileReplacements = tool.getReplacements();

  for (const auto &Entry : fileReplacements) {
    const clang::tooling::Replacements &ReplacementsForFile = Entry.second;
    if (ReplacementsForFile.empty())
      continue;

    llvm::Expected<std::string> result =
        clang::tooling::applyAllReplacements(rewrittenSource,
                                             ReplacementsForFile);
    if (result)
      rewrittenSource = std::move(*result);
  }

  return rewrittenSource;
}

inline void EastConstTestHarness::testTransformation(const std::string &input,
                                                 const std::string &expected) {
  std::string wrappedInput = addStandardIncludes(input);
  std::string wrappedExpected = addStandardIncludes(expected);
  std::string result = runToolOnCode(wrappedInput);
  EXPECT_EQ(result, wrappedExpected);
}
