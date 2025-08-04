#include "llvm/Transforms/TaintTracker/TaintTracker.h"

using namespace llvm;

PreserveAnalyses TaintTracker::run(Function &F,  FunctionAnalysisManager &AM) {
  // if no change has been made, return all
  return PreserverdAnalyses::all();
}
