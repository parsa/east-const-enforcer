#include <EastConstEnforcer.h>

#include <clang/Basic/Diagnostic.h>
#include <clang-tidy/ClangTidy.h>
#include <clang-tidy/ClangTidyCheck.h>
#include <clang-tidy/ClangTidyModule.h>
#include <clang-tidy/ClangTidyModuleRegistry.h>

#include <optional>

using namespace clang;
using namespace clang::tidy;

namespace {

class EastConstTidyCheck : public ClangTidyCheck {
public:
  EastConstTidyCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context),
        Checker([this](const SourceManager &, CharSourceRange Range,
                        llvm::StringRef NewText) {
          handleReplacement(Range, NewText);
        }) {}

  void registerMatchers(MatchFinder *Finder) override {
    registerEastConstMatchers(*Finder, this);
  }

  void check(const MatchFinder::MatchResult &Result) override {
    Checker.run(Result);
  }

  void onEndOfTranslationUnit() override {
    flushPendingRemoval();
  }

private:
  void handleReplacement(CharSourceRange Range, llvm::StringRef NewText) {
    if (Range.isInvalid())
      return;

    if (NewText.empty()) {
      flushPendingRemoval();
      PendingRemoval = Range;
      return;
    }

    auto Builder = diag(Range.getBegin(),
                        "move qualifier east of the declarator");
    if (PendingRemoval) {
      Builder << FixItHint::CreateReplacement(PendingRemoval->getAsRange(),
                                              "");
      PendingRemoval.reset();
    }
    Builder << FixItHint::CreateReplacement(Range.getAsRange(), NewText);
  }

  void flushPendingRemoval() {
    if (!PendingRemoval)
      return;
    auto Builder =
        diag(PendingRemoval->getBegin(), "remove stray qualifier token");
    Builder << FixItHint::CreateReplacement(PendingRemoval->getAsRange(), "");
    PendingRemoval.reset();
  }

  std::optional<CharSourceRange> PendingRemoval;
  EastConstChecker Checker;
};

class EastConstTidyModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &Factories) override {
    Factories.registerCheck<EastConstTidyCheck>("east-const-enforcer");
  }
};

} // namespace

static ClangTidyModuleRegistry::Add<EastConstTidyModule>
    X("east-const-enforcer-module",
      "Moves west const qualifiers to east const style.");

volatile int EastConstTidyModuleAnchorSource = 0;
