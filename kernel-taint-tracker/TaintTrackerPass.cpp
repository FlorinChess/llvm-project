#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/MemorySSA.h"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <vector>

#define INDENT_H for (unsigned i = 0; i < depth * 4; ++i) { errs() << " "; } 
#define INDENT INDENT_H errs() << "|--> ";
// #define USERSPACE

using namespace llvm;

namespace {

enum TaintState {
  TS_Untainted = 0,
  TS_Tainted,
  TS_Unknown
};

static llvm::StringRef stateName(TaintState s) {
  switch (s) {
  case TS_Untainted: return "Untainted";
  case TS_Tainted:   return "Tainted";
  case TS_Unknown:   return "Unknown";
  }
  return "??";
}

class TaintTrackerPass : public PassInfoMixin<TaintTrackerPass> {

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    // FunctionAnalysisManager
  //  auto& FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    runPass(M);
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
private:

  // memState: MemoryAccess* -> TaintState
  DenseMap<const MemoryAccess*, TaintState> memState;

  // valState: Value* (loads / instructions) -> TaintState
  DenseMap<const Value*, TaintState> valState;

  // small helper to max/meet states (Tainted wins)
  static TaintState meet(TaintState a, TaintState b) {
    if (a == TS_Tainted || b == TS_Tainted) return TS_Tainted;
    if (a == TS_Unknown || b == TS_Unknown) return TS_Unknown;
    return TS_Untainted;
  }

  std::set<Value *> tainted;
  std::unordered_map<const Function *, bool> TaintSummary;

#ifdef USERSPACE
  const char* knownSourceFunctions[3] = { "fgets", "gets", "scanf" };
  const char* knownSinkFunctions[6] = { "system", "fputs", "fprintf", "printf", "puts", "fwrite" };
#else
  const char* knownSourceFunctions[10] = 
  { 
    // accessing memory from userspace
    // macros: "copy_from_user", "__copy_from_user", "get_user"
    "strncpy_from_user",
    
    // network input
    "recvmsg", "__sys_recvfrom", "tcp_recvmsg", "udp_recvmsg",

    // filesystem input
    "vfs_read", "kernel_read"
  };
  const char* knownSinkFunctions[2] = 
  { 
    "kmalloc", "printk" 
  };
#endif

  void runPass(Module &M) {
    errs() << "Performing taint analysis on module " << M.getName() << "\n";

    // step 1: identify known taint sources
    std::vector<Value*> taintSources;
    for (Function &F : M) {
      findTaintSourceInFunction(F, taintSources);
    }

    // step 2: taint sources and associated operands/buffers
    for (auto& source : taintSources) {
      errs() << "Element in list of sources: " << *source << "\n"; 
      if (Instruction* I = dyn_cast<Instruction>(source)) {
        // Taint previous instructions associated with the source
        taintSourceOperands(source);
        
        // Start traversing from the function containing the taint
        Function* function = I->getFunction();
        std::set<unsigned> indices;
        unsigned depth = 0;
        propagateToFunction(*function, false, indices, depth);
      }
    }
  
    printTaintedValues();
  }

  /// @brief Prints the set of tainted instructions
  void printTaintedValues() {
    if (tainted.empty()) {
      errs() << "\nNo tainted data!\n\n";
      return;
    } 

    errs() << "----------------------------------------------------------------------------\nTainted values: \n";
    for (Value* value : tainted) {
      errs() << *value << "\n";
    }
    errs() << "----------------------------------------------------------------------------\n\n";
  }

  /// @brief Removes given value from the set of tainted instructions 
  /// @param value Value to remove
  void removeTaint(Value* value) {
    errs() << "Removing taint from: " << *value << "\n";
    tainted.erase(value);
  }

  bool taint(Value* value) {
    if (!isTainted(value)) {
      errs() << "Tainted value: " << *value << "\n";
      tainted.insert(value);
      return true;
    }
    errs() << "Value already tainted: " << *value << "\n";
    return false;
  }

  bool isTainted(Value* value) {
    return tainted.find(value) != tainted.end();
  }
  
  bool isTaintSource(const CallInst* CI) {
    if (!CI) return false;

    if (const Function *calledFunction = CI->getCalledFunction()) {
      for (const char* knownSourceFunction : knownSourceFunctions) {
        if (calledFunction->getName().contains(knownSourceFunction)) {
          return true;
        }
      }
    }

    return false;
  }

  bool isTaintSink(const CallInst* CI) {
    if (!CI) return false;

    if (Function *Callee = CI->getCalledFunction()) {
      for (const char* knownSinkFunction : knownSinkFunctions) {
        if (Callee->getName().contains(knownSinkFunction)) {
          for (unsigned i = 0; i < CI->arg_size(); ++i) {
            Value *arg = CI->getArgOperand(i);
            // errs() << "Checking arg: " << *arg << "\n";
            if (isTainted(arg)) {
              // errs() << "[SINK WARNING] Tainted data passed to sink: " << knownSinkFunction << "\n";
              return true;
            }
          }
        }
      }
    }

    return false;
  }

  bool storeIsClean(const StoreInst *SI) {
    // errs() << "Is store clean: " << *SI <<"\n";
    Value* value = SI->getOperand(0);
    // errs() << "Operand 0: " << *value <<"\n";

    if (isa<Constant>(value)) {
      // errs() << "Is a constant!\n";
      // store of a constant (e.g., zero) -> treat as untainted
      return true;
    }

    if (dyn_cast<CallInst>(value)) {
      // storing result of some function call: treat as unknown (conservative approach)
      return false;
    }
    
    return false;
  }

  /// @brief Looks for taint sources in the provided function and stores them in the provided buffer.
  /// @param F Function in which to look for taint sources 
  /// @param taintSources Buffer for storing taint sources
  void findTaintSourceInFunction(Function& F, std::vector<Value*> &taintSources) {
    if (F.isDeclaration()) return;

    errs() << "Checking function " << F.getName() << " for taint sources...\n";
    for (Instruction &I : instructions(F)) {
      if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        if (isTaintSource(CI))
          taintSources.push_back(CI);
      }
    }
  }

  /// @brief Taints non-constant operands in the source instruction and
  /// tries to track back to the initial allocation
  /// @param source Call instruction to a know source function 
  void taintSourceOperands(Value* source) {
    if (CallInst *CI = dyn_cast<CallInst>(source)) {
      for (unsigned i = 0; i < CI->arg_size(); ++i) {
        Value *arg = CI->getArgOperand(i);
        // errs() << "Argument: " << *arg << "\n";
        
        if (!isa<Constant>(arg)) taint(arg);
        
        Value* tmp = arg;
        // most source function usually take a buffer 
        // that is passed through using an intermediate operand
        // we need to taint the origin of that memory object
track_origin:
        Value* origin = getUnderlyingObject(tmp); // MaxLookup: default = 10
        errs() << "Origin of " << *arg << " --> " << *origin << "\n";
        if (dyn_cast<AllocaInst>(origin)) {
          if (!isTainted(origin)) {
            taint(origin);
            errs() << "[Source] Tainting origin of buffer: " << *origin << "\n";
          }
        } else if (GlobalVariable* GV = dyn_cast<GlobalVariable>(origin)) {
          errs() << "Global variable: " << *GV << "\n";
          if (GV->isConstant()) {
            // immutable; no need to taint
            errs() << "[Source] Constant immutable global variable: " << *GV << "\n";
          } else if (!GV->isDeclaration()) {
            // this may also be used in some cases !GV->hasExternalLinkage()
            // mutable and is not an externally defined global variable
            taint(GV);
            errs() << "[Source] Tainting global symbol: " << *GV << "\n";
          }
        } else if (LoadInst* LI = dyn_cast<LoadInst>(origin)) {
          // errs() << "Operand: " << *operand << "\n";
          Value* operand = LI->getOperand(0);
          tmp = operand;
          goto track_origin;
        }
      }
    }
  }

  /// @brief Propagates taint taints recursively through functions starting from the function F   
  /// @param F Starting function for recursive taint propagation
  /// @param worklist Queue of operands to be processed within the given function
  /// @param paramsTainted Flag that specifies if parameters should be marked as tainted
  /// @param taintedParametersIndices Indices of tainted parameters of the function
  /// @param depth Current depth of recursion; mainly used for presentation
  void propagateToFunction(Function &F, bool paramsTainted, std::set<unsigned>& taintedParametersIndices, unsigned& depth) {
    // Indent to simulate stack trace
    INDENT_H errs() << "Processing function " << F.getName() << "():\n";
  
    unsigned i = 0;
    if (paramsTainted) {
      for (llvm::Argument& arg : F.args()) {
        // INDENT errs() << "Arg: " << arg.getName() << "\n";
    
        if (taintedParametersIndices.find(i) != taintedParametersIndices.end()) {
          INDENT taint(&arg);
        }
        i++;
      }
    }

    for (Instruction& I : instructions(F)) {
      for (Value* operand : I.operands()) {
        // If one of the operands is tainted, figure out if the rest should be tainted too
        if (!isTainted(operand)) continue;
        
        INDENT errs() << "[Propagation] Found user of tainted data: " << I << "\n";
        if (StoreInst* SI = dyn_cast<StoreInst>(&I)) {
          Value* storeDest = SI->getOperand(1);

          if (storeIsClean(SI)) {
            INDENT errs() << "Clean store dest: " << *storeDest << "\n";
            INDENT removeTaint(storeDest); // This operand was overwritten with clean data
            continue;
          } else {
            INDENT taint(storeDest);
            INDENT errs() << "Unclean store buffer: " << *storeDest << "\n";
          }
        } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          Function* calledFunction = CI->getCalledFunction();
          INDENT errs() << "[Propagation] Found potentially tainted function call to " << calledFunction->getName() << "()\n";

          if(isTaintSink(CI)) {
            INDENT errs() << "[SINK WARNING] Tainted data passed to sink!\n";
          } else if (!isTaintSource(CI) && !calledFunction->isDeclaration()) {
            
          // taint parameters
            std::set<unsigned> parameterIndices; 
            for (unsigned i = 0; i < CI->arg_size(); ++i) {
              Value* arg = CI->getArgOperand(i);
              
              if (isTainted(arg)) {
                INDENT errs() << "[Propagation] Adding index " << i << " to tainted param indexes\n";
                parameterIndices.insert(i);
              }
            }

            // INDENT errs() << "Number of tainted params: " << parameterIndices.size() << "\n";
            
            bool parametersAreTainted = parameterIndices.size() > 0;
            if (parametersAreTainted) {
              INDENT taint(&I);
              propagateToFunction(*CI->getCalledFunction(), parametersAreTainted, parameterIndices, depth += 1);
              continue;
              // TODO: edge case: we pass a tainted buffer to the function, the function clears it
              // when control comes back to the caller, operand holding reference to the buffer
              // should also be cleared
              // IDEA: use either return value or the parameterIndices to determine if the state
              // of any of the buffers has changed
            }
          }
        }
      
        INDENT taint(&I);
      }
    }

    depth--;
  }
  
}; // end TaintTrackerPass class

class Experiment {
// // Initialize memState for all MemoryAccesses with Unknown and seed taint sources/clean stores
// void initializeMemoryStates(MemorySSA &MSSA) {
//   memState.clear();
//   valState.clear();

//   // default all memory accesses to Unknown
//   for (auto &MA : MSSA.getMemoryAccessList()) {
//     memState[MA] = TS_Unknown;
//   }

//   // Seed taint sources and simple clean stores
//   for (auto &BB : *MSSA.getFunction()) {
//     for (Instruction &I : BB) {
//       if (auto *CI = dyn_cast<CallInst>(&I)) {
//         if (isTaintSource(CI)) {
//           // The MemoryAccess corresponding to this call should be tainted (it writes tainted input)
//           const MemoryAccess *MA = MSSA.getMemoryAccess(CI);
//           if (MA) memState[MA] = TS_Tainted;
//         }
//       } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
//         // The MemoryAccess for this store: if it's storing a constant, mark untainted
//         const MemoryAccess *MA = MSSA.getMemoryAccess(SI);
//         if (MA && storeIsClean(SI)) {
//           memState[MA] = TS_Untainted;
//         }
//       }
//     }
//   }
// }

// // Given a MemoryAccess that is a MemoryDef, compute its transfer function
// TaintState transferForDef(const MemoryDef *DefMA, MemorySSA &MSSA) {
//   if (!DefMA) return TS_Unknown;

//   const Instruction *I = DefMA->getMemoryInst();
//   if (!I) return TS_Unknown;

//   // Get the incoming state (state of the defining access)
//   const MemoryAccess *defining = DefMA->getDefiningAccess();
//   TaintState incoming = TS_Unknown;
//   if (defining) incoming = memState.lookup(defining);

//   // If the def is a call to a taint source, it's tainted
//   if (const CallInst *CI = dyn_cast<CallInst>(I)) {
//     if (isTaintSource(CI)) return TS_Tainted;

//     // if the call is a known "clean" writer (like memset with zero) you could mark Untainted; otherwise Unknown
//     // For simplicity treat other calls conservative: Unknown (unless previously initialized).
//     return meet(memState.lookup(DefMA), incoming); // preserve previously seeded state (if any)
//   }

//   // If it's a store, we inspect stored value
//   if (const StoreInst *SI = dyn_cast<StoreInst>(I)) {
//     const Value *stored = SI->getValueOperand();

//     // If the stored value is a constant -> clean
//     if (isa<Constant>(stored)) {
//       return TS_Untainted;
//     }

//     // If the stored value is a load, we can ask what the load read from:
//     if (const LoadInst *LI = dyn_cast<LoadInst>(stored)) {
//       const MemoryAccess *loadMA = MSSA.getMemoryAccess(LI);
//       if (loadMA) {
//         // value's taint equals the mem state of memory the load depended on
//         TaintState loaded = TS_Unknown;
//         const MemoryAccess *def = loadMA->getDefiningAccess();
//         if (def) loaded = memState.lookup(def);
//         return meet(loaded, incoming);
//       }
//     }

//     // If stored value is an instruction with valState, use it
//     if (valState.count(stored)) {
//       return meet(valState.lookup(stored), incoming);
//     }

//     // Otherwise conservative: unknown (or tainted if incoming was tainted)
//     return incoming;
//   }

//   // Other kinds of MemoryDef (e.g., call that clobbers memory): preserve seeded state or incoming
//   return incoming;
// }

// // For a MemoryUse, state is state of the defining access
// TaintState transferForUse(MemoryAccess *UseMA) {
//   if (!UseMA) return TS_Unknown;
//   MemoryAccess *def = UseMA->getDefiningAccess();
//   if (!def) return TS_Unknown;
//   return memState.lookup(def);
// }

// // Run the fixed-point over all MemoryAccesses
// void computeFixedPoint(MemorySSA &MSSA) {
//   // simple worklist: iterate until stable
//   bool changed = true;
//   unsigned iter = 0;
//   while (changed) {
//     changed = false;
//     ++iter;
//     // iterate all memory accesses
//     for (auto &MA : MSSA.getMemoryAccessList()) {
//       const MemoryAccess *access = MA;
//       TaintState old = memState.lookup(access);
//       TaintState neu = old;

//       if (isa<MemoryDef>(access)) {
//         neu = transferForDef(access, MSSA);
//       } else if (isa<MemoryUse>(access)) {
//         neu = transferForUse(access);
//       } else if (isa<MemoryPhi>(access)) {
//         // merge incoming values for MemoryPhi
//         TaintState acc = TS_Untainted;
//         const MemoryPhi *Phi = cast<MemoryPhi>(access);
//         unsigned N = Phi->getNumIncomingValues();
//         if (N == 0) acc = TS_Unknown;
//         for (unsigned i = 0; i < N; ++i) {
//           const MemoryAccess *in = Phi->getIncomingValue(i);
//           if (in) acc = meet(acc, memState.lookup(in));
//           else acc = meet(acc, TS_Unknown);
//         }
//         neu = acc;
//       }

//       if (neu != old) {
//         memState[access] = neu;
//         changed = true;
//       }
//     } // end for all MA
//     // safety: avoid infinite loops (shouldn't happen here)
//     if (iter > 1000) break;
//   } // end while changed

//   // After memState is stable, set valState for loads
//   for (auto &BB : *MSSA.getFunction()) {
//     for (Instruction &I : BB) {
//       if (const LoadInst *LI = dyn_cast<LoadInst>(&I)) {
//         const MemoryAccess *MA = MSSA.getMemoryAccess(LI);
//         if (MA) {
//           const MemoryAccess *def = MA->getDefiningAccess();
//           TaintState s = TS_Unknown;
//           if (def) s = memState.lookup(def);
//           valState[LI] = s;
//         } else {
//           valState[LI] = TS_Unknown;
//         }
//       } else {
//         // For non-load instructions we could propagate value taint from operands
//         // (not necessary for this minimal demo)
//       }
//     }
//   }
// }

// // Check calls that are sinks. For a call instruction CI, find the MemoryAccess associated with the call
// // and check whether reads that the call will do are tainted.
// void checkSinks(Function &F, MemorySSA &MSSA) {
//   for (auto &BB : F) {
//     for (Instruction &I : BB) {
//       if (auto *CI = dyn_cast<CallInst>(&I)) {
//         if (!isTaintSink(CI)) continue;

//         const MemoryAccess *callMA = MSSA.getMemoryAccess(CI);
//         if (!callMA) continue;

//         TaintState s = TS_Unknown;
//         // For a call that reads memory, the MemoryAccess is often a MemoryUse (or combined)
//         // We use memState[ callMA ] to determine whether underlying memory that call may read is tainted.
//         s = memState.lookup(callMA);

//         if (s == TS_Tainted) {
//           errs() << "Tainted memory may reach sink '" << CI->getCalledFunction()->getName() << "' in function "
//                  << F.getName() << " at: ";
//           CI->print(errs());
//           errs() << "\n";
//         } else {
//           // Additionally, check arguments that are pointers: if pointer value was loaded from tainted memory earlier,
//           // valState might capture that. We'll check pointer args that are instructions (e.g., GEPs derived from alloca).
//           for (unsigned i = 0, e = CI->getNumArgOperands(); i != e; ++i) {
//             Value *arg = CI->getArgOperand(i);
//             if (valState.count(arg) && valState.lookup(arg) == TS_Tainted) {
//               errs() << "Tainted value passed as argument " << i << " to sink at: ";
//               CI->print(errs()); errs() << "\n";
//             }
//           }
//         }
//       }
//     }
//   }
// }

// bool runOnFunction(Function &F) override {
//   if (F.isDeclaration()) return false;

//   // Build MemorySSA
//   MemorySSAWrapperPass *MSSAWP = &getAnalysis<MemorySSAWrapperPass>();
//   MemorySSA &MSSA = MSSAWP->getMSSA();

//   errs() << "Running MemorySSA taint analysis on function: " << F.getName() << "\n";

//   // Initialize mem state (seed taint sources and trivial clean stores)
//   initializeMemoryStates(MSSA);

//   // Compute fixed-point
//   computeFixedPoint(MSSA);

//   // Check sinks
//   checkSinks(F, MSSA);

//   return false; // we don't modify IR
// }
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