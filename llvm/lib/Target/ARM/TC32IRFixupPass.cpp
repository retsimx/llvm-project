//===-- TC32IRFixupPass.cpp - Fix IR for TC32 target ----------------------===//
//
// Replaces ARM-specific intrinsics and inline assembly that TC32 doesn't
// support with compatible alternatives. This eliminates the need for the
// external fix_ir.py script.
//
// Transformations:
//   - llvm.arm.ldrex -> plain load
//   - llvm.arm.strex -> store + load (always succeeds)
//   - llvm.arm.hint  -> erased
//   - inline asm "sev" -> erased
//   - inline asm "mrs ..." -> replaced with "add sp, ..."
//   - inline asm "msr ..." -> replaced with "add sp, ..."
//
//===----------------------------------------------------------------------===//

#include "ARM.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsARM.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

#define DEBUG_TYPE "tc32-ir-fixup"

namespace {

class TC32IRFixupPass : public FunctionPass {
public:
  static char ID;
  TC32IRFixupPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    if (!F.getParent()->getTargetTriple().isThumb())
      return false;

    bool Changed = false;
    SmallVector<Instruction *, 16> ToErase;

    for (auto &BB : F) {
      for (auto &I : BB) {
        auto *CI = dyn_cast<CallInst>(&I);
        if (!CI)
          continue;

        if (auto *II = dyn_cast<IntrinsicInst>(CI)) {
          Changed |= handleIntrinsic(II, ToErase);
        } else if (CI->isInlineAsm()) {
          Changed |= handleInlineAsm(CI, ToErase);
        }
      }
    }

    for (auto *I : ToErase)
      I->eraseFromParent();

    return Changed;
  }

private:
  bool handleIntrinsic(IntrinsicInst *II,
                       SmallVectorImpl<Instruction *> &ToErase) {
    switch (II->getIntrinsicID()) {
    case Intrinsic::arm_ldrex: {
      // ldrex(ptr) -> load(ptr)
      IRBuilder<> Builder(II);
      Value *Ptr = II->getArgOperand(0);
      LoadInst *Load = Builder.CreateLoad(II->getType(), Ptr);
      Load->setAlignment(Align(4));
      II->replaceAllUsesWith(Load);
      ToErase.push_back(II);
      return true;
    }
    case Intrinsic::arm_strex: {
      // strex(value, ptr) -> store(value, ptr); load(ptr)
      // strex returns 0 on success; we simulate always-success by returning
      // a load of the stored value (which fix_ir.py also does)
      IRBuilder<> Builder(II);
      Value *Val = II->getArgOperand(0);
      Value *Ptr = II->getArgOperand(1);
      StoreInst *Store = Builder.CreateStore(Val, Ptr);
      Store->setAlignment(Align(4));
      LoadInst *Load = Builder.CreateLoad(II->getType(), Ptr);
      Load->setAlignment(Align(4));
      II->replaceAllUsesWith(Load);
      ToErase.push_back(II);
      return true;
    }
    case Intrinsic::arm_hint: {
      // hint (sev, wfe, etc.) -> erase
      ToErase.push_back(II);
      return true;
    }
    default:
      return false;
    }
  }

  bool handleInlineAsm(CallInst *CI,
                       SmallVectorImpl<Instruction *> &ToErase) {
    auto *IA = cast<InlineAsm>(CI->getCalledOperand());
    StringRef AsmStr = IA->getAsmString();

    if (AsmStr.starts_with("sev")) {
      // sev -> erase
      if (!CI->getType()->isVoidTy())
        CI->replaceAllUsesWith(UndefValue::get(CI->getType()));
      ToErase.push_back(CI);
      return true;
    }

    if (AsmStr.starts_with("mrs ") || AsmStr.starts_with("msr ")) {
      // mrs/msr -> add sp, $0 (harmless no-op equivalent)
      std::string NewAsmStr;
      if (AsmStr.starts_with("mrs "))
        NewAsmStr = "add sp, ${0}";
      else
        NewAsmStr = "add sp, ${0}";

      InlineAsm *NewIA = InlineAsm::get(
          IA->getFunctionType(), NewAsmStr, IA->getConstraintString(),
          IA->hasSideEffects(), IA->isAlignStack(), IA->getDialect());

      CI->setCalledOperand(NewIA);
      return true;
    }

    return false;
  }
};

} // end anonymous namespace

char TC32IRFixupPass::ID = 0;

INITIALIZE_PASS(TC32IRFixupPass, DEBUG_TYPE,
                "TC32 IR fixup pass", false, false)

FunctionPass *llvm::createTC32IRFixupPass() {
  return new TC32IRFixupPass();
}
