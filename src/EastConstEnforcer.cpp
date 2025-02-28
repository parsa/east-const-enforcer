#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <cstddef>
#include <string>
#include <regex>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

// Command line options
static cl::OptionCategory EastConstCategory("east-const-enforcer options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::opt<bool> FixErrors("fix", cl::desc("Apply fixes to diagnosed warnings"), cl::cat(EastConstCategory));
static cl::opt<bool> QuietMode("quiet", cl::desc("Suppress informational output"), 
                              cl::cat(EastConstCategory));

struct EastConstChecker : public MatchFinder::MatchCallback {
  EastConstChecker(RefactoringTool& Tool) : Tool(Tool) {}

  void run(const MatchFinder::MatchResult &Result) override {
    const auto *VD = Result.Nodes.getNodeAs<VarDecl>("varDecl");
    if (!VD) return;
    
    SourceManager &SM = *Result.SourceManager;
    
    // Skip variables in system headers or non-main files
    if (SM.isInSystemHeader(VD->getLocation()) || !SM.isInMainFile(VD->getLocation())) {
      return;
    }
    
    ASTContext &Context = *Result.Context;
    const LangOptions &LangOpts = Context.getLangOpts();
    
    bool isMainFile = SM.isInMainFile(VD->getLocation());
    llvm::errs() << "Found var: " << VD->getNameAsString() 
                 << " in file: " << SM.getFilename(VD->getLocation()).str()
                 << " isMainFile: " << (isMainFile ? "true" : "false") << "\n";
    
    if (!isMainFile) return;

    QualType QT = VD->getType();
    
    // Check for const qualifier in the type or pointee/referenced type
    bool hasConst = QT.isConstQualified();
    if (QT->isPointerType()) {
      // For pointers, check if the pointee type is const-qualified
      hasConst = hasConst || QT->getPointeeType().isConstQualified();
    } else if (QT->isReferenceType()) {
      // For references, check if the referenced type is const-qualified
      hasConst = hasConst || QT->getPointeeType().isConstQualified();
    }
    if (!hasConst) return;

    if (!QuietMode) {
      llvm::errs() << "Processing variable: " << VD->getNameAsString() << "\n";
    }

    llvm::errs() << "Processing variable: " << VD->getNameAsString() 
                 << " Type: " << QT.getAsString() 
                 << " isPointer: " << (QT->isPointerType() ? "yes" : "no") << "\n";

    // Get the source text before the variable name
    SourceLocation VarLoc = VD->getLocation();
    SourceLocation TypeStart = VD->getBeginLoc();
    
    StringRef Text = Lexer::getSourceText(
        CharSourceRange::getCharRange(TypeStart, VarLoc),
        SM, LangOpts);
    
    llvm::errs() << "Variable: " << VD->getNameAsString() 
                << " Declaration prefix: '" << Text.trim() << "'\n";
    
    // Check if the variable is in west-const style by examining QT's string and comparing with prefix
    // This is more reliable than just checking for "const" at the beginning
    std::string TypeStr = QT.getAsString();
    
    // Check if it's a pointer type with west-const style
    bool isWestConst = false;
    if (QT->isPointerType()) {
        // For pointers with const pointee (const int*)
        isWestConst = Text.trim().starts_with("const") && 
                      QT->getPointeeType().isConstQualified();
    } else if (QT->isReferenceType()) {
        // For references with const referenced type (const int&)
        isWestConst = Text.trim().starts_with("const") && 
                      QT->getPointeeType().isConstQualified();
    } else {
        // For non-pointers/references (const int)
        isWestConst = Text.trim().starts_with("const") && 
                      QT.isConstQualified();
    }

    if (isWestConst) {
      SourceLocation ConstLoc = TypeStart;
      
      DiagnosticsEngine &DE = Result.Context->getDiagnostics();
      unsigned DiagID = DE.getCustomDiagID(
          DiagnosticsEngine::Warning,
          "use east const style (place 'const' after the type)");

      auto Diag = DE.Report(ConstLoc, DiagID);

      // Instead of getting just the type range, get the full declaration range
      // from the 'const' token to the end of type
      SourceRange TypeRange = VD->getTypeSourceInfo()->getTypeLoc().getSourceRange();
      // Create a new range that includes the 'const' at the beginning
      SourceRange FullRange(TypeStart, TypeRange.getEnd());
      
      std::string FullTypeText = Lexer::getSourceText(
          CharSourceRange::getTokenRange(FullRange),
          SM, LangOpts).str();
          
      llvm::errs() << "Full type text: '" << FullTypeText << "'\n";

      try {
        // Declare NewType at a higher scope so it's available for debugging
        std::string NewType;
        
        // Create a fix-it hint
        if (QT->isPointerType()) {
          // For pointer types, move "const" before the "*"
          NewType = std::regex_replace(FullTypeText,
                                    std::regex("const\\s+(\\w+)\\s*(\\*+)"), 
                                    "$1 const$2");
        } else if (QT->isReferenceType()) {
          // For reference types, move "const" before the "&"
          NewType = std::regex_replace(FullTypeText,
                                    std::regex("const\\s+(\\w+)\\s*(&)"), 
                                    "$1 const$2");
        } else {
          // For non-pointer types, move "const" to the end
          NewType = std::regex_replace(FullTypeText, 
                                    std::regex("const\\s+([^\\s]+)"), 
                                    "$1 const");
        }
        
        llvm::errs() << "After regex: Variable: " << VD->getNameAsString() << "\n";
        llvm::errs() << "  Original: '" << FullTypeText << "'\n";
        llvm::errs() << "  NewType:  '" << NewType << "'\n";
        llvm::errs() << "  Changed:  " << (NewType != FullTypeText ? "yes" : "no") << "\n";
        
        if (NewType != FullTypeText) {
          // Create a replacement
          Replacement Rep(SM, CharSourceRange::getTokenRange(FullRange), NewType);
          
          // Add to diagnostics for display purposes
          Diag << FixItHint::CreateReplacement(
              CharSourceRange::getTokenRange(FullRange),
              NewType);
              
          // Also add to the tool's replacement map for actual application
          std::string FilePath = SM.getFilename(FullRange.getBegin()).str();
          auto &FileReplaces = Tool.getReplacements()[FilePath];
          llvm::consumeError(FileReplaces.add(Rep));
          
          llvm::errs() << "Added replacement for " << FilePath << "\n"; 
        }
      } catch (const std::regex_error& e) {
        llvm::errs() << "Regex error: " << e.what() << "\n";
      }
    }

    if (QT->isReferenceType() || QT->isPointerType()) {
      // Get the qualified type locations
      TypeLoc TL = VD->getTypeSourceInfo()->getTypeLoc();
      
      // For pointers and references with const pointee
      if (const PointerTypeLoc PTL = TL.getAs<PointerTypeLoc>()) {
        TypeLoc PointeeTL = PTL.getPointeeLoc();
        // Find the const qualifier's exact location
        if (auto QualTL = PointeeTL.getAs<QualifiedTypeLoc>()) {
          SourceRange QualRange = QualTL.getSourceRange();
          // Now we have the exact location of the 'const' token
        }
      }
      // Handle references
      else if (const ReferenceTypeLoc RTL = TL.getAs<ReferenceTypeLoc>()) {
        TypeLoc PointeeTL = RTL.getPointeeLoc();
        // Find the const qualifier's exact location
        if (auto QualTL = PointeeTL.getAs<QualifiedTypeLoc>()) {
          SourceRange QualRange = QualTL.getSourceRange();
          
          // Get the base type (without qualifiers)
          QualType BaseType = QualTL.getUnqualifiedLoc().getType();
          std::string BaseTypeStr = BaseType.getAsString();
          
          // Get the reference location
          SourceLocation RefLoc = RTL.getLocalSourceRange().getBegin();
          
          // Create a diagnostic
          DiagnosticsEngine &DE = Result.Context->getDiagnostics();
          unsigned DiagID = DE.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "use east const style (place 'const' after the type)");
          
          auto Diag = DE.Report(QualRange.getBegin(), DiagID);
          
          // Create the replacement: [base_type] const&
          std::string NewTypeStr = BaseTypeStr + " const" + 
                                  Lexer::getSourceText(
                                      CharSourceRange::getCharRange(RefLoc, RefLoc.getLocWithOffset(1)),
                                      SM, LangOpts).str();
          
          // Get the full range from the beginning of const qualifier to after the &
          SourceRange FullRange(QualRange.getBegin(), RefLoc.getLocWithOffset(1));
          
          // Add the fix-it hint
          Diag << FixItHint::CreateReplacement(
              CharSourceRange::getTokenRange(FullRange),
              NewTypeStr);
              
          // Also add to the tool's replacement map
          std::string FilePath = SM.getFilename(FullRange.getBegin()).str();
          Replacement Rep(SM, CharSourceRange::getTokenRange(FullRange), NewTypeStr);
          auto &FileReplaces = Tool.getReplacements()[FilePath];
          llvm::consumeError(FileReplaces.add(Rep));
          
          llvm::errs() << "Added AST-based replacement for reference type in " << FilePath << "\n";
        }
      }
      // For simple types
      if (auto QualTL = TL.getAs<QualifiedTypeLoc>()) {
        SourceRange QualRange = QualTL.getSourceRange();
        QualType BaseType = QualTL.getUnqualifiedLoc().getType();
        std::string BaseTypeStr = BaseType.getAsString();
        
        // Create a replacement with the const after the type
        std::string NewTypeStr = BaseTypeStr + " const";
        
        // Apply the replacement
        // Create a diagnostic
        DiagnosticsEngine &DE = Result.Context->getDiagnostics();
        unsigned DiagID = DE.getCustomDiagID(
            DiagnosticsEngine::Warning,
            "use east const style (place 'const' after the type)");
        
        auto Diag = DE.Report(QualRange.getBegin(), DiagID);
        
        // Get the full range from the beginning of const qualifier to after the type
        SourceRange FullRange(QualRange.getBegin(), QualRange.getEnd());
        
        // Add the fix-it hint
        Diag << FixItHint::CreateReplacement(
            CharSourceRange::getTokenRange(FullRange),
            NewTypeStr);
            
        // Also add to the tool's replacement map
        std::string FilePath = SM.getFilename(FullRange.getBegin()).str();
        Replacement Rep(SM, CharSourceRange::getTokenRange(FullRange), NewTypeStr);
        auto &FileReplaces = Tool.getReplacements()[FilePath];
        llvm::consumeError(FileReplaces.add(Rep));
        
        llvm::errs() << "Added AST-based replacement for simple type in " << FilePath << "\n";
      }
    }
  }

private:
  RefactoringTool &Tool;
};

int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, EastConstCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser& OptionsParser = ExpectedParser.get();
  
  // Create the tool with the proper options
  RefactoringTool Tool(OptionsParser.getCompilations(),
                      OptionsParser.getSourcePathList());
  
  if (FixErrors) {
    llvm::errs() << "Fix mode enabled\n";
  }
  
  EastConstChecker Checker(Tool);
  MatchFinder Finder;

  // Match variable declarations
  auto VarMatcher = varDecl(unless(parmVarDecl())).bind("varDecl");
  Finder.addMatcher(VarMatcher, &Checker);

  // Create custom factory
  auto Factory = newFrontendActionFactory(&Finder);
  
  int Result = Tool.run(Factory.get());
  
  if (FixErrors) {
    // Get the replacements map
    auto &ReplacementsMap = Tool.getReplacements();
    
    // Remove any entries with empty file paths
    ReplacementsMap.erase("");
    
    // Apply the replacements to each file
    llvm::errs() << "Applying fixes to " << ReplacementsMap.size() << " files\n";
    
    for (const auto &FileAndReplacements : ReplacementsMap) {
      const std::string &FilePath = FileAndReplacements.first;
      const Replacements &Replaces = FileAndReplacements.second;
      
      llvm::errs() << "Processing file: " << FilePath << " with "
                   << Replaces.size() << " replacements\n";
      
      // Read the file content using LLVM's file system functions
      auto FileOrError = llvm::MemoryBuffer::getFile(FilePath);
      if (std::error_code EC = FileOrError.getError()) {
        llvm::errs() << "Error reading file " << FilePath << ": " << EC.message() << "\n";
        continue;
      }
      
      std::string FileContent = FileOrError.get()->getBuffer().str();
      
      // Apply replacements to the file content
      llvm::Expected<std::string> NewContent = applyAllReplacements(FileContent, Replaces);
      if (!NewContent) {
        llvm::errs() << "Error applying replacements to " << FilePath << ": "
                     << llvm::toString(NewContent.takeError()) << "\n";
        continue;
      }
      
      // Write the modified content back to the file
      std::error_code EC;
      llvm::raw_fd_ostream OS(FilePath, EC, llvm::sys::fs::OF_None);
      if (EC) {
        llvm::errs() << "Error opening file for writing " << FilePath << ": " << EC.message() << "\n";
        continue;
      }
      
      OS << *NewContent;
      OS.close();
      
      if (OS.has_error()) {
        llvm::errs() << "Error writing to " << FilePath << "\n";
      } else {
        llvm::errs() << "Successfully modified: " << FilePath << "\n";
      }
    }
  }
  
  return Result;
}
