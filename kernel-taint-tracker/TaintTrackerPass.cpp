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
  std::unordered_map<const Function *, bool> TaintSummary;
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    errs() << "Running Module-level Taint Analysis...\n";

    // Step 1: Identify known taint sources
    for (Function &F : M) {
      for (Instruction &I : instructions(F)) {
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          if (Function *Callee = CI->getCalledFunction()) {
            if (Callee->getName().contains("fgets")) {
              Value *Buf = CI->getArgOperand(0);
              TaintedValues.insert(Buf);
              errs() << "[Source] Tainting buffer from fgets: " << *Buf << "\n";
            }
          }
        }
      }
    }

    // Step 2: Propagate taint within each function and record return taint
    for (Function &F : M) {
        bool returnsTainted = processFunction(F);
        TaintSummary[&F] = returnsTainted;
    }

    // Step 3: Revisit functions to resolve interprocedural taint flows
    for (Function &F : M) {
      for (Instruction &I : instructions(F)) {
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          if (Function *Callee = CI->getCalledFunction()) {
            if (TaintSummary[Callee]) {
              TaintedValues.insert(CI);
              errs() << "[Interprocedural] Tainting call result from tainted function: "
                      << Callee->getName() << "\n";
            }
          }
        }
      }
    }

    // Step 4: Report if tainted data reaches a sink like system()
    for (Function &F : M) {
      for (Instruction &I : instructions(F)) {
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          if (Function *Callee = CI->getCalledFunction()) {
            if (Callee->getName().contains("system")) {
              for (unsigned i = 0; i < CI->arg_size(); ++i) {
                Value *Arg = CI->getArgOperand(i);
                if (TaintedValues.count(Arg)) {
                  errs() << "[SINK WARNING] Tainted argument passed to system(): "
                          << *Arg << "\n";
                }
              }
            }
          }
        }
      }
    }

    errs() << "Taint analysis complete!\n";

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