#include <EastConstEnforcer.h>

#include <clang/Lex/Lexer.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

#include <cctype>
#include <cstddef>
#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Command line options
cl::OptionCategory EastConstCategory("east-const-enforcer options");
cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
cl::opt<bool> FixErrors("fix", cl::desc("Apply fixes to diagnosed warnings"),
                        cl::cat(EastConstCategory));
cl::opt<bool> QuietMode("quiet", cl::desc("Suppress informational output"),
                        cl::cat(EastConstCategory));

EastConstChecker::EastConstChecker(RefactoringTool &Tool) : Tool(Tool) {}

void EastConstChecker::run(const MatchFinder::MatchResult &Result) {
  if (!Result.Context || !Result.SourceManager)
    return;

  SourceManager &SM = *Result.SourceManager;
  const LangOptions &LangOpts = Result.Context->getLangOpts();

  const auto *Var = Result.Nodes.getNodeAs<VarDecl>("varDecl");
  if (!Var)
    Var = Result.Nodes.getNodeAs<VarDecl>("constVar");
  if (Var)
    processDeclaratorDecl(Var, SM, LangOpts);

  if (const auto *Field = Result.Nodes.getNodeAs<FieldDecl>("fieldDecl"))
    processDeclaratorDecl(Field, SM, LangOpts);

  if (const auto *Func = Result.Nodes.getNodeAs<FunctionDecl>("functionDecl"))
    processFunctionDecl(Func, SM, LangOpts);

  if (const auto *Typedef = Result.Nodes.getNodeAs<TypedefDecl>("typedefDecl"))
    processTypedefDecl(Typedef, SM, LangOpts);

  if (const auto *Alias = Result.Nodes.getNodeAs<TypeAliasDecl>("aliasDecl"))
    processTypedefDecl(Alias, SM, LangOpts);

  if (const auto *ClassSpec =
          Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>(
              "classTemplateSpec"))
    processClassTemplateSpec(ClassSpec, SM, LangOpts);

  if (const auto *NTTP =
          Result.Nodes.getNodeAs<NonTypeTemplateParmDecl>(
              "nonTypeTemplateParm"))
    processDeclaratorDecl(NTTP, SM, LangOpts);
}

void EastConstChecker::processDeclaratorDecl(const DeclaratorDecl *DD,
                                             SourceManager &SM,
                                             const LangOptions &LangOpts) {
  if (!DD)
    return;

  SourceLocation Loc = DD->getLocation();
  if (Loc.isInvalid() || Loc.isMacroID())
    return;

  if (SM.isInSystemHeader(Loc) || !SM.isWrittenInMainFile(Loc))
    return;

  if (TypeSourceInfo *TSI = DD->getTypeSourceInfo())
    processTypeLoc(TSI->getTypeLoc(), SM, LangOpts);
}

void EastConstChecker::processTypedefDecl(const TypedefNameDecl *TD,
                                          SourceManager &SM,
                                          const LangOptions &LangOpts) {
  if (!TD)
    return;

  SourceLocation Loc = TD->getLocation();
  if (Loc.isInvalid() || Loc.isMacroID())
    return;

  if (SM.isInSystemHeader(Loc) || !SM.isWrittenInMainFile(Loc))
    return;

  if (TypeSourceInfo *TSI = TD->getTypeSourceInfo())
    processTypeLoc(TSI->getTypeLoc(), SM, LangOpts);
}

void EastConstChecker::processFunctionDecl(const FunctionDecl *FD,
                                           SourceManager &SM,
                                           const LangOptions &LangOpts) {
  if (!FD)
    return;

  SourceLocation Loc = FD->getLocation();
  if (Loc.isInvalid() || Loc.isMacroID())
    return;

  if (SM.isInSystemHeader(Loc) || !SM.isWrittenInMainFile(Loc))
    return;

  if (TypeSourceInfo *TSI = FD->getTypeSourceInfo()) {
    TypeLoc TL = TSI->getTypeLoc();
    if (auto FnTL = TL.getAs<FunctionTypeLoc>()) {
      processTypeLoc(FnTL.getReturnLoc(), SM, LangOpts);
      bool SkipParams = FD->isThisDeclarationADefinition();
      if (!SkipParams) {
        for (unsigned I = 0; I < FnTL.getNumParams(); ++I)
          processDeclaratorDecl(FnTL.getParam(I), SM, LangOpts);
      }
      return;
    }
  }

  // Fallback: process parameter declarations directly.
  if (!FD->isThisDeclarationADefinition()) {
    for (const ParmVarDecl *Param : FD->parameters())
      processDeclaratorDecl(Param, SM, LangOpts);
  }
}

void EastConstChecker::processClassTemplateSpec(
    const ClassTemplateSpecializationDecl *Spec, SourceManager &SM,
    const LangOptions &LangOpts) {
  if (!Spec)
    return;

  SourceLocation Loc = Spec->getLocation();
  if (Loc.isInvalid() || Loc.isMacroID())
    return;
  if (SM.isInSystemHeader(Loc) || !SM.isWrittenInMainFile(Loc))
    return;

  if (const auto *ArgsInfo = Spec->getTemplateArgsAsWritten()) {
    for (const TemplateArgumentLoc &ArgLoc : ArgsInfo->arguments()) {
      if (ArgLoc.getArgument().getKind() != TemplateArgument::Type)
        continue;
      if (TypeSourceInfo *TSI = ArgLoc.getTypeSourceInfo())
        processTypeLoc(TSI->getTypeLoc(), SM, LangOpts);
    }
  }
}

void EastConstChecker::processTypeLoc(TypeLoc TL, SourceManager &SM,
                                      const LangOptions &LangOpts) {
  for (TypeLoc Current = TL; !Current.isNull();
       Current = Current.getNextTypeLoc()) {
    SourceLocation Begin = Current.getBeginLoc();
    if (Begin.isInvalid())
      continue;

    SourceLocation FileBegin = SM.getFileLoc(Begin);
    if (FileBegin.isInvalid() || FileBegin.isMacroID())
      continue;

    if (!SM.isWrittenInMainFile(FileBegin))
      continue;

    if (auto QualTL = Current.getAs<QualifiedTypeLoc>())
      processQualifiedTypeLoc(QualTL, SM, LangOpts);

    if (auto TemplateTL = Current.getAs<TemplateSpecializationTypeLoc>()) {
      for (unsigned I = 0; I < TemplateTL.getNumArgs(); ++I) {
        TemplateArgumentLoc ArgLoc = TemplateTL.getArgLoc(I);
        if (!ArgLoc.getTypeSourceInfo()) {
          if (!QuietMode) {
            llvm::errs() << "Missing type source info for template argument at "
                         << ArgLoc.getSourceRange().printToString(SM) << "\n";
          }
          continue;
        }
        processTypeLoc(ArgLoc.getTypeSourceInfo()->getTypeLoc(), SM,
                       LangOpts);
      }
    }

    if (auto FnProtoTL = Current.getAs<FunctionProtoTypeLoc>()) {
      processTypeLoc(FnProtoTL.getReturnLoc(), SM, LangOpts);
      for (unsigned I = 0; I < FnProtoTL.getNumParams(); ++I) {
        if (ParmVarDecl *P = FnProtoTL.getParam(I)) {
          if (TypeSourceInfo *ParamTSI = P->getTypeSourceInfo())
            processTypeLoc(ParamTSI->getTypeLoc(), SM, LangOpts);
          processDeclaratorDecl(P, SM, LangOpts);
        }
      }
    }

    if (shouldFixDanglingQualifier(Current))
      fixDanglingQualifierTokens(Current, SM, LangOpts);
  }
}

void EastConstChecker::processQualifiedTypeLoc(QualifiedTypeLoc QTL,
                                               SourceManager &SM,
                                               const LangOptions &LangOpts) {
  if (QTL.isNull())
    return;

  Qualifiers Quals = QTL.getType().getLocalQualifiers();
  if (!Quals.hasConst() && !Quals.hasVolatile() && !Quals.hasRestrict())
    return;
  if (!QuietMode) {
    std::string TypeDesc = QTL.getType().getAsString();
    if (TypeDesc.find("Foo::") != std::string::npos) {
      llvm::errs() << "Processing qualified type '" << TypeDesc << "' at "
                   << QTL.getSourceRange().printToString(SM) << "\n";
    }
  }

  TypeLoc Unqualified = QTL.getUnqualifiedLoc();
  if (Unqualified.isNull())
    return;
  if (!QuietMode) {
    if (auto TypedefTL = Unqualified.getAs<TypedefTypeLoc>()) {
      SourceLocation NameLoc = TypedefTL.getNameLoc();
      llvm::errs() << "Typedef loc "
                   << NameLoc.printToString(SM) << " for type '"
                   << TypedefTL.getType().getAsString() << "'\n";
    }
  }

  QualType BaseQT = Unqualified.getType();

    bool UseSpellingFallback =
      shouldUseSpellingFallback(QTL, Unqualified, SM, LangOpts);
  if (!UseSpellingFallback && isDeclaratorTypeLoc(Unqualified)) {
    // Declarator-based spellings already place qualifiers east of the base
    // entity (e.g., pointer/reference syntax), so skip to avoid duplicates.
    return;
  }

  SourceLocation BaseBegin = SM.getFileLoc(Unqualified.getBeginLoc());
  SourceLocation BaseEnd = SM.getFileLoc(Unqualified.getEndLoc());

  if (!UseSpellingFallback) {
    if (BaseBegin.isInvalid() || BaseEnd.isInvalid())
      return;
    if (BaseBegin.isMacroID() || BaseEnd.isMacroID())
      return;
    if (!SM.isWrittenInMainFile(BaseBegin) ||
        !SM.isWrittenInMainFile(BaseEnd))
      return;
  }

  SourceLocation QualBegin;
  SourceLocation RemovalEnd;
  std::vector<std::string> MovedQualifiers;

  if (UseSpellingFallback) {
    if (!findQualifierRangeFromSpelling(QTL, SM, LangOpts, QualBegin,
                                        RemovalEnd, MovedQualifiers)) {
      if (!QuietMode) {
        llvm::errs() << "Failed to find qualifier range for type '"
                     << QTL.getType().getAsString() << "' at "
                     << QTL.getSourceRange().printToString(SM) << "\n";
      }
      return;
    }
  } else {
    if (!findQualifierRange(QTL, SM, QualBegin, MovedQualifiers)) {
      if (!QuietMode) {
        llvm::errs() << "Failed to find qualifier range for type '"
                     << QTL.getType().getAsString() << "' at "
                     << Unqualified.getSourceRange().printToString(SM)
                     << "\n";
      }
      return;
    }
    RemovalEnd = BaseBegin;
  }

  unsigned QualKey = QualBegin.getRawEncoding();
  if (!ProcessedQualifierStarts.insert(QualKey).second)
    return;

  CharSourceRange RemoveRange =
      CharSourceRange::getCharRange(QualBegin, RemovalEnd);
  addReplacement(SM, RemoveRange, "");

  SourceLocation InsertLoc;
  if (UseSpellingFallback) {
    SourceLocation TypeEnd = SM.getFileLoc(QTL.getEndLoc());
    InsertLoc = Lexer::getLocForEndOfToken(TypeEnd, 0, SM, LangOpts);
  } else {
    InsertLoc = computeInsertLocation(QTL.getUnqualifiedLoc(), SM, LangOpts);
  }
  if (InsertLoc.isInvalid())
    return;

  std::string Suffix = buildQualifierSuffix(MovedQualifiers);
  if (Suffix.empty())
    return;

  CharSourceRange InsertRange =
      CharSourceRange::getCharRange(InsertLoc, InsertLoc);
  addReplacement(SM, InsertRange, Suffix);
}

bool EastConstChecker::findQualifierRange(
    QualifiedTypeLoc QTL, SourceManager &SM, SourceLocation &QualBegin,
    std::vector<std::string> &MovedQualifiers) const {
  TypeLoc Unqualified = QTL.getUnqualifiedLoc();
  if (Unqualified.isNull())
    return false;

  SourceLocation BaseBegin = SM.getFileLoc(Unqualified.getBeginLoc());
  if (BaseBegin.isInvalid() || BaseBegin.isMacroID())
    return false;

  FileID FID = SM.getFileID(BaseBegin);
  if (FID.isInvalid())
    return false;

  bool Invalid = false;
  StringRef Buffer = SM.getBufferData(FID, &Invalid);
  if (Invalid)
    return false;

  const char *BufferStart = Buffer.data();
  const char *BasePtr = SM.getCharacterData(BaseBegin);
  const char *Cursor = BasePtr;

  auto isIdentifierChar = [](char C) {
    unsigned char UC = static_cast<unsigned char>(C);
    return std::isalnum(UC) || C == '_';
  };

  auto skipWhitespace = [&](const char *&Ptr) {
    while (Ptr > BufferStart &&
           std::isspace(static_cast<unsigned char>(Ptr[-1])))
      --Ptr;
  };

  Qualifiers Remaining = QTL.getType().getLocalQualifiers();
  bool FoundAny = false;

  struct QualToken {
    SourceLocation Loc;
    std::string Keyword;
  };

  std::vector<QualToken> Tokens;

  auto matchKeyword = [&](StringRef Keyword) -> bool {
    if (Keyword.empty())
      return false;

    const char *Ptr = Cursor;
    skipWhitespace(Ptr);
    if (Ptr - Keyword.size() < BufferStart)
      return false;

    const char *WordStart = Ptr - Keyword.size();
    if (StringRef(WordStart, Keyword.size()) != Keyword)
      return false;

    if (WordStart > BufferStart && isIdentifierChar(WordStart[-1]))
      return false;

    if (Ptr < BasePtr && isIdentifierChar(*Ptr))
      return false;

    ptrdiff_t Offset = BasePtr - WordStart;
    SourceLocation TokenLoc =
        BaseBegin.getLocWithOffset(-static_cast<int>(Offset));
    Tokens.push_back({TokenLoc, Keyword.str()});
    Cursor = WordStart;
    FoundAny = true;
    return true;
  };

  bool Progress = true;
  while (Progress) {
    Progress = false;
    if (Remaining.hasConst() && matchKeyword("const")) {
      Remaining.removeConst();
      Progress = true;
      continue;
    }
    if (Remaining.hasVolatile() && matchKeyword("volatile")) {
      Remaining.removeVolatile();
      Progress = true;
      continue;
    }
    if (Remaining.hasRestrict() && matchKeyword("restrict")) {
      Remaining.removeRestrict();
      Progress = true;
      continue;
    }
  }

  if (!FoundAny)
    return false;

  if (Tokens.empty())
    return false;

  std::reverse(Tokens.begin(), Tokens.end());

  auto FirstConst =
      std::find_if(Tokens.begin(), Tokens.end(), [](const QualToken &Tok) {
        return Tok.Keyword == "const";
      });

  if (FirstConst == Tokens.end())
    return false;

  QualBegin = FirstConst->Loc;
  for (auto It = FirstConst; It != Tokens.end(); ++It)
    MovedQualifiers.push_back(It->Keyword);

  return true;
}

std::string EastConstChecker::getSourceText(const SourceManager &SM,
																						const LangOptions &LangOpts,
																						CharSourceRange Range) const {
	if (Range.isInvalid())
		return {};
	return Lexer::getSourceText(Range, SM, LangOpts).str();
}

std::string EastConstChecker::buildQualifierSuffix(
    const std::vector<std::string> &Qualifiers) const {
  if (Qualifiers.empty())
    return {};

  std::string Result;
  for (const std::string &Keyword : Qualifiers) {
    Result += " ";
    Result += Keyword;
  }
  return Result;
}

void EastConstChecker::addReplacement(const SourceManager &SM,
																			CharSourceRange Range,
																			llvm::StringRef NewText) {
	if (Range.isInvalid())
		return;

	SourceLocation Loc = Range.getBegin();
	if (Loc.isInvalid())
		return;

	std::string FilePath = SM.getFilename(Loc).str();
	if (FilePath.empty())
		return;

	Replacement Rep(SM, Range, NewText);
	auto &FileReplacements = Tool.getReplacements()[FilePath];
	llvm::Error Err = FileReplacements.add(Rep);
  if (Err) {
    if (!QuietMode) {
      llvm::errs() << "Error adding replacement to " << FilePath << ": "
           << llvm::toString(std::move(Err)) << "\n";
    }
  } else if (!QuietMode && !NewText.empty()) {
    llvm::errs() << "Inserted qualifier suffix '" << NewText << "' in "
         << FilePath << "\n";
  }
}

SourceLocation EastConstChecker::computeInsertLocation(TypeLoc Unqualified,
                                                       SourceManager &SM,
                                                       const LangOptions &LangOpts) const {
  if (Unqualified.isNull())
    return SourceLocation();

  SourceLocation BaseBegin = SM.getFileLoc(Unqualified.getBeginLoc());
  SourceLocation BaseEnd = SM.getFileLoc(Unqualified.getEndLoc());
  if (BaseBegin.isInvalid() || BaseEnd.isInvalid())
    return SourceLocation();

  CharSourceRange BaseRange =
      CharSourceRange::getTokenRange(BaseBegin, BaseEnd);
  std::string BaseText = getSourceText(SM, LangOpts, BaseRange);
  if (BaseText.empty())
    return Lexer::getLocForEndOfToken(BaseEnd, 0, SM, LangOpts);

  int Depth = 0;
  for (size_t I = 0; I < BaseText.size(); ++I) {
    char C = BaseText[I];
    if (C == '<') {
      ++Depth;
      continue;
    }
    if (C == '>') {
      if (Depth == 0)
        return BaseBegin.getLocWithOffset(static_cast<int>(I));
      --Depth;
    }
  }

  return Lexer::getLocForEndOfToken(BaseEnd, 0, SM, LangOpts);
}

bool EastConstChecker::shouldUseSpellingFallback(QualifiedTypeLoc QTL,
                                                 TypeLoc Unqualified,
                                                 SourceManager &SM,
                                                 const LangOptions &LangOpts) const {
  if (Unqualified.isNull())
    return true;

  if (typeLocNeedsSpellingFallback(Unqualified))
    return true;

  SourceLocation BaseBegin = SM.getFileLoc(Unqualified.getBeginLoc());
  SourceLocation BaseEnd = SM.getFileLoc(Unqualified.getEndLoc());
  if (BaseBegin.isInvalid() || BaseEnd.isInvalid())
    return true;
  if (BaseBegin.isMacroID() || BaseEnd.isMacroID())
    return false;
  if (!SM.isWrittenInMainFile(BaseBegin) || !SM.isWrittenInMainFile(BaseEnd))
    return true;

  const clang::Type *Ty = Unqualified.getTypePtr();
  if (!Ty)
    return true;

  return isa<AutoType>(Ty) || isa<DecltypeType>(Ty) ||
         isa<UnresolvedUsingType>(Ty) || isa<TemplateTypeParmType>(Ty) ||
         isa<InjectedClassNameType>(Ty) ||
         [&]() {
           SourceRange FullRange = QTL.getSourceRange();
           SourceLocation TypeBegin = SM.getFileLoc(FullRange.getBegin());
           SourceLocation TypeEnd = SM.getFileLoc(FullRange.getEnd());
           if (TypeBegin.isInvalid() || TypeEnd.isInvalid()) {
             if (!QuietMode)
               llvm::errs() << "Spelling fallback: invalid locs\n";
             return false;
           }
           if (TypeBegin.isMacroID() || TypeEnd.isMacroID()) {
             if (!QuietMode)
               llvm::errs() << "Spelling fallback: macro locs\n";
             return false;
           }
           SourceLocation AfterEnd =
               Lexer::getLocForEndOfToken(TypeEnd, 0, SM, LangOpts);
           if (AfterEnd.isInvalid()) {
             if (!QuietMode)
               llvm::errs() << "Spelling fallback: invalid AfterEnd\n";
             return false;
           }
           CharSourceRange Range =
               CharSourceRange::getCharRange(TypeBegin, AfterEnd);
           StringRef Text = Lexer::getSourceText(Range, SM, LangOpts);
           if (Text.contains("decltype") || Text.contains("auto"))
             return true;

           SourceLocation RangeBegin = SM.getFileLoc(QTL.getBeginLoc());
           if (RangeBegin.isInvalid() || RangeBegin.isMacroID())
             return false;

           bool InvalidBuf = false;
           StringRef Buffer = SM.getBufferData(SM.getFileID(RangeBegin), &InvalidBuf);
           if (InvalidBuf)
             return false;

           const char *BeginPtr = SM.getCharacterData(RangeBegin);
           if (!BeginPtr)
             return false;

           const char *BufferEnd = Buffer.end();
           const size_t MaxLookahead = 96;
           const char *ScanEnd = BeginPtr + MaxLookahead;
           if (ScanEnd > BufferEnd)
             ScanEnd = BufferEnd;
           StringRef Window(BeginPtr, ScanEnd - BeginPtr);
           return Window.contains("decltype") || Window.contains("auto");
         }();
}

bool EastConstChecker::typeLocNeedsSpellingFallback(TypeLoc TL) const {
  if (TL.isNull())
    return false;

  const unsigned MaxSteps = 32;
  unsigned Steps = 0;

  auto isProblematic = [](TypeLoc::TypeLocClass C) {
    switch (C) {
    case TypeLoc::Auto:
    case TypeLoc::DeducedTemplateSpecialization:
    case TypeLoc::Decltype:
    case TypeLoc::UnresolvedUsing:
    case TypeLoc::TemplateTypeParm:
    case TypeLoc::InjectedClassName:
      return true;
    default:
      return false;
    }
  };

  while (!TL.isNull() && Steps++ < MaxSteps) {
    if (isProblematic(TL.getTypeLocClass()))
      return true;

    TypeLoc Next;
    if (auto RefTL = TL.getAs<ReferenceTypeLoc>())
      Next = RefTL.getPointeeLoc();
    else if (auto PtrTL = TL.getAs<PointerTypeLoc>())
      Next = PtrTL.getPointeeLoc();
    else if (auto MemPtrTL = TL.getAs<MemberPointerTypeLoc>())
      Next = MemPtrTL.getPointeeLoc();
    else if (auto ParenTL = TL.getAs<ParenTypeLoc>())
      Next = ParenTL.getInnerLoc();
    else if (auto AttrTL = TL.getAs<AttributedTypeLoc>())
      Next = AttrTL.getModifiedLoc();
    else if (auto ElaboratedTL = TL.getAs<ElaboratedTypeLoc>())
      Next = ElaboratedTL.getNamedTypeLoc();
    else if (auto TypedefTL = TL.getAs<TypedefTypeLoc>())
      Next = TypedefTL.getUnqualifiedLoc();
    else if (auto AdjustedTL = TL.getAs<AdjustedTypeLoc>())
      Next = AdjustedTL.getOriginalLoc();
    else if (auto MacroTL = TL.getAs<MacroQualifiedTypeLoc>())
      Next = MacroTL.getInnerLoc();
    else
      break;

    if (Next.isNull() || (Next.getBeginLoc() == TL.getBeginLoc() &&
                          Next.getEndLoc() == TL.getEndLoc()))
      break;

    TL = Next;
  }

  return false;
}

bool EastConstChecker::isDeclaratorTypeLoc(TypeLoc TL) const {
  if (TL.isNull())
    return false;

  switch (TL.getTypeLocClass()) {
  case TypeLoc::Pointer:
  case TypeLoc::BlockPointer:
  case TypeLoc::MemberPointer:
  case TypeLoc::LValueReference:
  case TypeLoc::RValueReference:
  case TypeLoc::FunctionProto:
  case TypeLoc::FunctionNoProto:
  case TypeLoc::Paren:
  case TypeLoc::ConstantArray:
  case TypeLoc::IncompleteArray:
  case TypeLoc::VariableArray:
  case TypeLoc::DependentSizedArray:
  case TypeLoc::DependentSizedExtVector:
  case TypeLoc::DependentAddressSpace:
  case TypeLoc::Pipe:
    return true;
  default:
    break;
  }

  return false;
}

bool EastConstChecker::shouldFixDanglingQualifier(TypeLoc TL) const {
  if (TL.isNull())
    return false;

  const clang::Type *Ty = TL.getTypePtr();
  if (!Ty || !Ty->isReferenceType())
    return false;

  switch (TL.getTypeLocClass()) {
  case TypeLoc::Decltype:
  case TypeLoc::Auto:
  case TypeLoc::DeducedTemplateSpecialization:
    return true;
  default:
    break;
  }

  return false;
}

bool EastConstChecker::fixDanglingQualifierTokens(
    TypeLoc TL, SourceManager &SM, const LangOptions &LangOpts) {
  SourceLocation TypeBegin = SM.getFileLoc(TL.getBeginLoc());
  SourceLocation TypeEnd = SM.getFileLoc(TL.getEndLoc());
  if (TypeBegin.isInvalid() || TypeEnd.isInvalid())
    return false;
  if (TypeBegin.isMacroID() || TypeEnd.isMacroID())
    return false;
  if (!SM.isWrittenInMainFile(TypeBegin) ||
      !SM.isWrittenInMainFile(TypeEnd))
    return false;

  FileID FID = SM.getFileID(TypeBegin);
  if (FID.isInvalid())
    return false;

  bool Invalid = false;
  StringRef Buffer = SM.getBufferData(FID, &Invalid);
  if (Invalid)
    return false;

  const char *BufferStart = Buffer.data();
  const char *Cursor = SM.getCharacterData(TypeBegin);
  if (!Cursor)
    return false;

  auto isIdentifierChar = [](char C) {
    unsigned char UC = static_cast<unsigned char>(C);
    return std::isalnum(UC) || C == '_';
  };

  auto skipWhitespace = [&](const char *&Ptr) {
    while (Ptr > BufferStart &&
           std::isspace(static_cast<unsigned char>(Ptr[-1])))
      --Ptr;
  };

  struct QualToken {
    SourceLocation Loc;
    std::string Keyword;
  };

  std::vector<QualToken> Tokens;
  const char *ScanCursor = Cursor;

  auto matchKeyword = [&](StringRef Keyword) -> bool {
    if (Keyword.empty())
      return false;

    const char *Ptr = ScanCursor;
    skipWhitespace(Ptr);
    if (Ptr - Keyword.size() < BufferStart)
      return false;

    const char *WordStart = Ptr - Keyword.size();
    if (StringRef(WordStart, Keyword.size()) != Keyword)
      return false;

    if (WordStart > BufferStart && isIdentifierChar(WordStart[-1]))
      return false;

    if (Ptr < Cursor && isIdentifierChar(*Ptr))
      return false;

    ptrdiff_t Offset = Cursor - WordStart;
    SourceLocation TokenLoc =
        TypeBegin.getLocWithOffset(-static_cast<int>(Offset));
    Tokens.push_back({TokenLoc, Keyword.str()});
    ScanCursor = WordStart;
    return true;
  };

  bool Progress = true;
  while (Progress) {
    Progress = false;
    if (matchKeyword("const")) {
      Progress = true;
      continue;
    }
    if (matchKeyword("volatile")) {
      Progress = true;
      continue;
    }
    if (matchKeyword("restrict")) {
      Progress = true;
      continue;
    }
  }

  if (Tokens.empty())
    return false;

  std::reverse(Tokens.begin(), Tokens.end());
  SourceLocation QualBegin = Tokens.front().Loc;
  unsigned QualKey = QualBegin.getRawEncoding();
  if (!ProcessedQualifierStarts.insert(QualKey).second)
    return false;

  SourceLocation RemovalEnd = TypeBegin;
  CharSourceRange RemoveRange =
      CharSourceRange::getCharRange(QualBegin, RemovalEnd);
  addReplacement(SM, RemoveRange, "");

  std::vector<std::string> Keywords;
  for (const auto &Tok : Tokens)
    Keywords.push_back(Tok.Keyword);

  SourceLocation InsertLoc =
      Lexer::getLocForEndOfToken(TypeEnd, 0, SM, LangOpts);
  if (InsertLoc.isInvalid())
    return true;

  std::string Suffix = buildQualifierSuffix(Keywords);
  if (Suffix.empty())
    return true;

  CharSourceRange InsertRange =
      CharSourceRange::getCharRange(InsertLoc, InsertLoc);
  addReplacement(SM, InsertRange, Suffix);

  return true;
}

bool EastConstChecker::findQualifierRangeFromSpelling(
    QualifiedTypeLoc QTL, SourceManager &SM, const LangOptions &LangOpts,
    SourceLocation &QualBegin, SourceLocation &RemovalEnd,
    std::vector<std::string> &MovedQualifiers) const {
  (void)LangOpts;
  Qualifiers Remaining = QTL.getType().getLocalQualifiers();
  if (!Remaining.hasConst() && !Remaining.hasVolatile() &&
      !Remaining.hasRestrict())
    return false;

  SourceLocation BaseBegin = SM.getFileLoc(QTL.getBeginLoc());
  if (BaseBegin.isInvalid() || BaseBegin.isMacroID())
    return false;
  if (!SM.isWrittenInMainFile(BaseBegin))
    return false;

  FileID FID = SM.getFileID(BaseBegin);
  if (FID.isInvalid())
    return false;

  bool Invalid = false;
  StringRef Buffer = SM.getBufferData(FID, &Invalid);
  if (Invalid)
    return false;

  const char *BufferStart = Buffer.data();
  const char *BasePtr = SM.getCharacterData(BaseBegin);
  if (!BasePtr)
    return false;

  auto isIdentifierChar = [](char C) {
    unsigned char UC = static_cast<unsigned char>(C);
    return std::isalnum(UC) || C == '_';
  };

  auto skipWhitespace = [&](const char *&Ptr) {
    while (Ptr > BufferStart &&
           std::isspace(static_cast<unsigned char>(Ptr[-1])))
      --Ptr;
  };

  bool FoundAny = false;

  struct QualToken {
    SourceLocation Loc;
    std::string Keyword;
  };

  std::vector<QualToken> Tokens;

  const char *Cursor = BasePtr;

  auto matchKeyword = [&](StringRef Keyword) -> bool {
    if (Keyword.empty())
      return false;

    const char *Ptr = Cursor;
    skipWhitespace(Ptr);
    if (Ptr - Keyword.size() < BufferStart)
      return false;

    const char *WordStart = Ptr - Keyword.size();
    if (StringRef(WordStart, Keyword.size()) != Keyword)
      return false;

    if (WordStart > BufferStart && isIdentifierChar(WordStart[-1]))
      return false;

    if (Ptr < BasePtr && isIdentifierChar(*Ptr))
      return false;

    ptrdiff_t Offset = BasePtr - WordStart;
    SourceLocation TokenLoc =
        BaseBegin.getLocWithOffset(-static_cast<int>(Offset));
    Tokens.push_back({TokenLoc, Keyword.str()});
    Cursor = WordStart;
    FoundAny = true;
    return true;
  };

  bool Progress = true;
  while (Progress) {
    Progress = false;
    if (Remaining.hasConst() && matchKeyword("const")) {
      Remaining.removeConst();
      Progress = true;
      continue;
    }
    if (Remaining.hasVolatile() && matchKeyword("volatile")) {
      Remaining.removeVolatile();
      Progress = true;
      continue;
    }
    if (Remaining.hasRestrict() && matchKeyword("restrict")) {
      Remaining.removeRestrict();
      Progress = true;
      continue;
    }
  }

  if (!FoundAny)
    return false;

  if (Tokens.empty())
    return false;

  std::reverse(Tokens.begin(), Tokens.end());

  auto FirstConst =
      std::find_if(Tokens.begin(), Tokens.end(), [](const QualToken &Tok) {
        return Tok.Keyword == "const";
      });

  if (FirstConst == Tokens.end())
    return false;

  QualBegin = FirstConst->Loc;
  RemovalEnd = BaseBegin;
  for (auto It = FirstConst; It != Tokens.end(); ++It)
    MovedQualifiers.push_back(It->Keyword);

  return true;
}
