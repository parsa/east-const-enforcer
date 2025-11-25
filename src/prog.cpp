#include <EastConstEnforcer.h>

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
    
    // Match function declarations for return types and parameters
    auto FuncMatcher = functionDecl().bind("functionDecl");
    Finder.addMatcher(FuncMatcher, &Checker);
    
    // Match class field declarations
    auto FieldMatcher = fieldDecl().bind("fieldDecl");
    Finder.addMatcher(FieldMatcher, &Checker);
    
    // Match typedef declarations
    auto TypedefMatcher = typedefDecl().bind("typedefDecl");
    Finder.addMatcher(TypedefMatcher, &Checker);
    
    // Match using declarations (C++11 style typedefs)
    auto AliasMatcher = typeAliasDecl().bind("aliasDecl");
    Finder.addMatcher(AliasMatcher, &Checker);

    auto NonTypeParamMatcher =
      nonTypeTemplateParmDecl().bind("nonTypeTemplateParm");
    Finder.addMatcher(NonTypeParamMatcher, &Checker);
  
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