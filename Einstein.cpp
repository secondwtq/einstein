
/*

USAGE: einstein --extra-args="-I..." -p . XXX.cpp

This program will predefine a CPP macro "EINSTEIN_GEN=1".

For every class annotated with `annotate("SL-generate")`:

* Generate Save routine:

  Signature: `void XXXExt::ExtData::SaveData(AresByteStream& Stm)`

  Call `Stm.Save()` on every:

    * Non-static data member, which is a predefined container type, including:

      `std::vector<T>`

    * Non-static data member, which has a type with `annotate("SL-non-pod")`

* Generate Load routine:

  Signature: `void XXXExt::ExtData::LoadData(AresByteStream& Stm)`

  * Call `Stm.Swizzle()` on every:

    * Non-static data member with a pointer type.

  * Call `Stm.Load()` on every:

    * Non-static data member, which is a predefined container type, including:

      `std::vector<T>`

    * Non-static data member, which has a type with `annotate("SL-non-pod")`

* The order in which the non-static data members of the class is handled need
  to be aligned in each pair of Save/Load routines.

  (I assume `Swizzle` calls don't matter?)

*/

#include "clang/AST/DeclVisitor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"
#include "fmt/format.h"

#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <unordered_set>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

template <>
struct fmt::formatter<llvm::StringRef> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) const { return ctx.begin(); }

  template <typename FormatContext>
  auto format(llvm::StringRef str, FormatContext& ctx) const {
    return fmt::formatter<fmt::string_view>().format(
        fmt::string_view(str.data(), str.size()), ctx);
  }
};

class EinsteinContext {
public:
  std::vector<const CXXRecordDecl *> generateWorkList;
  // This is to prevent duplicated entries in generateWorkList
  std::unordered_set<std::string> generateWorkListSet;
};

class EinsteinASTConsumer : public clang::ASTConsumer {
public:
  EinsteinASTConsumer(EinsteinContext &context)
      : context(context), toGenerateMatchCallback(context) {}

private:
  bool HandleTopLevelDecl(clang::DeclGroupRef d) override { return true; }

  void HandleTranslationUnit(clang::ASTContext& c) override;

  class ToGenerateMatchCallback
      : public ast_matchers::MatchFinder::MatchCallback {
  public:
    ToGenerateMatchCallback(EinsteinContext& context) : context(context) { }
    ~ToGenerateMatchCallback() override { }

    void run(const ast_matchers::MatchFinder::MatchResult& result) override {
      // if we have a class decl. w/ `[[annotate("SL-generate")]]`
      // push it into workList (if it's not in there yet)
      if (const CXXRecordDecl *d =
              result.Nodes.getNodeAs<CXXRecordDecl>("toGenerate"))
        for (const auto attr : d->getAttrs())
          if (const auto annot = dyn_cast<AnnotateAttr>(attr))
            if (annot->getAnnotation() == "SL-generate") {
              std::string name = d->getQualifiedNameAsString();
              if (this->context.generateWorkListSet.find(name) ==
                  this->context.generateWorkListSet.end())
                this->context.generateWorkList.push_back(d);
            }
    }

  private:
    EinsteinContext& context;
  };

  ast_matchers::MatchFinder matcher;
  ToGenerateMatchCallback toGenerateMatchCallback;
  EinsteinContext& context;
};

class EinsteinAction : public clang::ASTFrontendAction {
private:
  EinsteinContext context;

protected:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &ci, StringRef inFile) override {
    return llvm::make_unique<EinsteinASTConsumer>(context);
  }

  void ExecuteAction() override;

public:
  // EinsteinAction(EinsteinContext& context) : context(context) { }
  ~EinsteinAction() override { }
};

void EinsteinASTConsumer::HandleTranslationUnit(clang::ASTContext& c) {
  using namespace clang::ast_matchers;
  DeclarationMatcher attrMatcher =
      cxxRecordDecl(hasAttr(attr::Annotate)).bind("toGenerate");
  this->matcher.addMatcher(attrMatcher, &this->toGenerateMatchCallback);
  this->matcher.matchAST(c);
}

void EinsteinAction::ExecuteAction() {
  ASTFrontendAction::ExecuteAction();

  fmt::memory_buffer loadBuf, saveBuf;
  for (auto d : this->context.generateWorkList) {
    std::string name = d->getQualifiedNameAsString();
    fmt::format_to(loadBuf, "void {}::LoadData(AresByteStream& Stm) {{\n", name);
    fmt::format_to(saveBuf, "void {}::SaveData(AresByteStream& Stm) {{\n", name);

    for (auto f : d->fields()) {
      std::string comment = fmt::format("{}", f->getType().getAsString());
      if (f->getType()->isPointerType()) {
        fmt::format_to(loadBuf, "// {}\nStm.Swizzle(this->{});\n",
                       comment, f->getName());

      } else {
        const auto d = f->getType().getTypePtr()->getAsRecordDecl();
        if (!d)
          continue;

        const auto qualifiedName = d->getQualifiedNameAsString();
        // We 欽点 several standard containers, whose sources obviously cannot
        // be contaminated by us peasants, as The Honorable Non-PODs that
        // require speical tunneling.
        bool isAppointedNonPOD =
          llvm::StringSwitch<bool>(qualifiedName)
            .Case("std::vector", true)
            .Default(false);

        bool hasNonPODAttr = false;
        if (!isAppointedNonPOD && d->hasAttrs())
          for (const auto attr : d->getAttrs())
            if (const auto annot = dyn_cast<AnnotateAttr>(attr))
              if (annot->getAnnotation() == "SL-non-pod") {
                hasNonPODAttr = true;
                break;
              }

        if (hasNonPODAttr || isAppointedNonPOD) {
          fmt::format_to(loadBuf, "// {}\nStm.Load(this->{});\n",
                         comment, f->getName());
          fmt::format_to(saveBuf, "// {}\nStm.Save(this->{});\n",
                         comment, f->getName());
        }
      }
    }

    fmt::format_to(loadBuf, "}}\n\n");
    fmt::format_to(saveBuf, "}}\n\n");
  }
  std::cout << fmt::to_string(loadBuf) << std::endl;
  std::cout << fmt::to_string(saveBuf) << std::endl;
}

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static cl::OptionCategory EinsteinCategory("einstein options");

int main(int argc, const char **argv) {
  // printf("Einstein Lives!\n");
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  CommonOptionsParser optionsParser(argc, argv, EinsteinCategory);
  ClangTool Tool(optionsParser.getCompilations(),
                 optionsParser.getSourcePathList());
  Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
    { { "-DEINSTEIN_GEN=1" }, },ArgumentInsertPosition::BEGIN));
  auto ret = Tool.run(newFrontendActionFactory<EinsteinAction>().get());
  return ret;
}
