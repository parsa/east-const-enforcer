#ifndef EAST_CONST_ENFORCER_H
#define EAST_CONST_ENFORCER_H

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/DenseSet.h>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

using ReplacementHandler =
    std::function<void(const clang::SourceManager &, CharSourceRange,
                       llvm::StringRef)>;

void setQuietMode(bool Enabled);
bool isQuietMode();

// Checker class
class EastConstChecker : public MatchFinder::MatchCallback {
public:
  explicit EastConstChecker(ReplacementHandler Handler);
  void run(const MatchFinder::MatchResult &Result) override;

private:
  void processDeclaratorDecl(const DeclaratorDecl *DD, SourceManager &SM,
                             const LangOptions &LangOpts);
  void processTypedefDecl(const TypedefNameDecl *TD, SourceManager &SM,
                          const LangOptions &LangOpts);
  void processFunctionDecl(const FunctionDecl *FD, SourceManager &SM,
                           const LangOptions &LangOpts);
  void processClassTemplateSpec(
      const ClassTemplateSpecializationDecl *Spec, SourceManager &SM,
      const LangOptions &LangOpts);
  void processTypeLoc(TypeLoc TL, SourceManager &SM,
                      const LangOptions &LangOpts);
  void processQualifiedTypeLoc(QualifiedTypeLoc QTL, SourceManager &SM,
                               const LangOptions &LangOpts);
  bool findQualifierRange(QualifiedTypeLoc QTL, SourceManager &SM,
                          const LangOptions &LangOpts,
                          SourceLocation &QualBegin,
                          SourceLocation &RemovalEnd,
                          std::vector<std::string> &MovedQualifiers) const;
  bool shouldUseSpellingFallback(QualifiedTypeLoc QTL, TypeLoc Unqualified,
                                 SourceManager &SM,
                                 const LangOptions &LangOpts) const;
  bool findQualifierRangeFromSpelling(
      QualifiedTypeLoc QTL, SourceManager &SM, const LangOptions &LangOpts,
      SourceLocation &QualBegin, SourceLocation &BaseBegin,
      std::vector<std::string> &MovedQualifiers) const;
  bool collectQualifierTokens(SourceLocation BaseBegin, SourceManager &SM,
                              const LangOptions &LangOpts, Qualifiers Quals,
                              SourceLocation &QualBegin,
                              SourceLocation &RemovalEnd,
                              std::vector<std::string> &MovedQualifiers) const;
  bool typeLocNeedsSpellingFallback(TypeLoc TL) const;
  bool isDeclaratorTypeLoc(TypeLoc TL) const;
  bool shouldFixDanglingQualifier(TypeLoc TL) const;
  bool fixDanglingQualifierTokens(TypeLoc TL, SourceManager &SM,
                                  const LangOptions &LangOpts);
  std::string getSourceText(const SourceManager &SM, const LangOptions &LangOpts,
                            CharSourceRange Range) const;
  std::string buildQualifierSuffix(
      const std::vector<std::string> &Qualifiers) const;
  void addReplacement(const SourceManager &SM, CharSourceRange Range,
                      llvm::StringRef NewText);
  SourceLocation computeInsertLocation(TypeLoc Unqualified, SourceManager &SM,
                                       const LangOptions &LangOpts) const;

  ReplacementHandler ReplacementCallback;
  mutable llvm::DenseSet<unsigned> ProcessedQualifierStarts;
};

void registerEastConstMatchers(MatchFinder &Finder,
                               MatchFinder::MatchCallback *Callback);

#endif // EAST_CONST_ENFORCER_H
