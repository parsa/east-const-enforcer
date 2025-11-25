#ifndef EAST_CONST_ENFORCER_H
#define EAST_CONST_ENFORCER_H

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

#include <llvm/ADT/DenseSet.h>

#include <cstddef>
#include <string>
#include <vector>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

// Command line options
extern cl::OptionCategory EastConstCategory;
extern cl::extrahelp CommonHelp;
extern cl::opt<bool> FixErrors;
extern cl::opt<bool> QuietMode;

// Checker class
class EastConstChecker : public MatchFinder::MatchCallback {
public:
  EastConstChecker(RefactoringTool& Tool);
  void run(const MatchFinder::MatchResult &Result) override;

private:
  void processDeclaratorDecl(const DeclaratorDecl *DD, SourceManager &SM,
                             const LangOptions &LangOpts);
  void processTypedefDecl(const TypedefNameDecl *TD, SourceManager &SM,
                          const LangOptions &LangOpts);
  void processFunctionDecl(const FunctionDecl *FD, SourceManager &SM,
                           const LangOptions &LangOpts);
  void processTypeLoc(TypeLoc TL, SourceManager &SM,
                      const LangOptions &LangOpts);
  void processQualifiedTypeLoc(QualifiedTypeLoc QTL, SourceManager &SM,
                               const LangOptions &LangOpts);
  bool findQualifierRange(QualifiedTypeLoc QTL, SourceManager &SM,
                          SourceLocation &QualBegin,
                          std::vector<std::string> &MovedQualifiers) const;
  std::string getSourceText(const SourceManager &SM, const LangOptions &LangOpts,
                            CharSourceRange Range) const;
  std::string buildQualifierSuffix(
      const std::vector<std::string> &Qualifiers) const;
  void addReplacement(const SourceManager &SM, CharSourceRange Range,
                      llvm::StringRef NewText);
  SourceLocation computeInsertLocation(TypeLoc Unqualified, SourceManager &SM,
                                       const LangOptions &LangOpts) const;

  RefactoringTool &Tool;
  mutable llvm::DenseSet<unsigned> ProcessedQualifierStarts;
};

#endif // EAST_CONST_ENFORCER_H
