#include "EastConstTestHarness.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class EastConstGridCodeGenTest : public EastConstTestHarness {};

namespace {

std::optional<std::string> gDumpConstGridPath;
bool gHarnessVerboseFlag = false;

bool HandleCustomFlag(std::string_view arg) {
  constexpr std::string_view kDumpPrefix = "--dump-const-grid=";
  constexpr std::string_view kHarnessVerbose = "--east-const-test-verbose";
  if (arg == kHarnessVerbose) {
    gHarnessVerboseFlag = true;
    return true;
  }

  const bool hasPrefix = arg.size() >= kDumpPrefix.size() &&
                         arg.substr(0, kDumpPrefix.size()) == kDumpPrefix;
  if (!hasPrefix)
    return false;

  std::string path(arg.substr(kDumpPrefix.size()));
  if (path.empty()) {
    std::fputs("error: --dump-const-grid requires a non-empty path\n", stderr);
    std::exit(EXIT_FAILURE);
  }
  gDumpConstGridPath = std::move(path);
  return true;
}

void StripCustomFlags(int &argc, char **argv) {
  int writeIdx = 1;
  for (int readIdx = 1; readIdx < argc; ++readIdx) {
    std::string_view arg(argv[readIdx]);
    if (HandleCustomFlag(arg))
      continue;
    argv[writeIdx++] = argv[readIdx];
  }
  argc = writeIdx;
}

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

std::string canonicalTypeExpression(const TypeSpec &spec) {
  std::string text = typeExpression(spec);
  normalizeConstReferenceSpacing(text);
  return text;
}

bool haveIdenticalTypeExpressions(const TypeSpec &lhs,
                                  const TypeSpec &rhs) {
  return canonicalTypeExpression(lhs) == canonicalTypeExpression(rhs);
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
      if (haveIdenticalTypeExpressions(*westOpt, *eastOpt))
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

TEST_F(EastConstGridCodeGenTest, GeneratedConstGrid) {
  auto snippets = buildConstGridSnippets();
  if (const auto &dumpPath = gDumpConstGridPath) {
    std::ofstream dumpStream(*dumpPath);
    if (!dumpStream) {
      ADD_FAILURE() << "Failed to open --dump-const-grid target: " << *dumpPath;
    } else {
      dumpStream << "// Input\n" << snippets.Input << "\n// Expected\n"
                 << snippets.Expected;
    }
  }
  testTransformation(snippets.Input, snippets.Expected);
}

int main(int argc, char **argv) {
  StripCustomFlags(argc, argv);
  setEastConstHarnessVerbose(gHarnessVerboseFlag);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
