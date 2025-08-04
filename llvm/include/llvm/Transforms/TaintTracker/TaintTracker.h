#ifndef LLVM_TRANSFORMS_TAINT_TRACKER_TAINT_TRACKER_H
#define LLVM_TRANSFORMS_TAINT_TRACKER_TAINT_TRACKER_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class Function;

class TaintTrackerPass : PassInfoMixin<TaintTrackerPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
  static bool isRequired() { return false; }  
}

} // end namespace llvm

#endif // LLVM_TRANSFORMS_TAINT_TRACKER_TAINT_TRACKER_H
