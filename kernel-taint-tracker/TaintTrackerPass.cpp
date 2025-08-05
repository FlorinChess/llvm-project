#include "llvm/IR/InstIterator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <unordered_set>

using namespace llvm;

namespace {

class TaintTrackerPass : public PassInfoMixin<TaintTrackerPass> {
public:
  std::unordered_set<const Value *> TaintedValues;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    errs() << "Analyzing module: " << M.getName() << "\n";
    
    for (Function &F : M) {
      errs() << " - Function: " << F.getName() << "\n";
    }

    

    // errs() << "number of parameters: " << F.arg_size() << "\n";
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }

  bool processFunction(Function &F) {
        bool returnTainted = false;
        for (Instruction &I : instructions(F)) {
            // Taint propagation via operands
            for (auto &Op : I.operands()) {
                if (TaintedValues.count(Op.get())) {
                    TaintedValues.insert(&I);
                    break;
                }
            }

            // Store taint
            if (auto *SI = dyn_cast<StoreInst>(&I)) {
                if (TaintedValues.count(SI->getValueOperand())) {
                    TaintedValues.insert(SI->getPointerOperand());
                }
            }

            // Load taint
            if (auto *LI = dyn_cast<LoadInst>(&I)) {
                if (TaintedValues.count(LI->getPointerOperand())) {
                    TaintedValues.insert(&I);
                }
            }

            // Return value taint tracking
            if (auto *RI = dyn_cast<ReturnInst>(&I)) {
                if (Value *RetVal = RI->getReturnValue()) {
                    if (TaintedValues.count(RetVal)) {
                        returnTainted = true;
                    }
                }
            }
        }
        return returnTainted;
    }
};

} // end anonymous namespace

extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "TaintTrackerPass", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "taint-tracker") {
            FPM.addPass(TaintTrackerPass());
            return true;
          }
          return false;
        });
    }
  };
}