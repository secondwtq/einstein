
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

#include <stdio.h>

using namespace llvm;
using namespace clang::tooling;

class EinsteinContext {

};

class EinsteinASTConsumer : public clang::ASTConsumer {

};

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

static cl::OptionCategory EinsteinCategory("einstein options");

int main(int argc, const char **argv) {
  printf("Einstein\n");
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  CommonOptionsParser optionsParser(argc, argv, EinsteinCategory);
  ClangTool Tool(optionsParser.getCompilations(),
                 optionsParser.getSourcePathList());
  auto ret = Tool.run(newFrontendActionFactory<clang::SyntaxOnlyAction>().get());
  return 0;
}
