//===-- TC32IRFixupPass.cpp - Fix IR for TC32 target ----------------------===//
//
// Replaces ARM-specific intrinsics and inline assembly that TC32 doesn't
// support with compatible alternatives. This eliminates the need for the
// external fix_ir.py script.
//
// Transformations:
//   - "frame-pointer"="non-leaf" -> "none" (no unwinder in the firmware)
//   - llvm.arm.ldrex -> plain load
//   - llvm.arm.strex -> store (always succeeds, returns 0)
//   - llvm.arm.hint  -> erased
//   - inline asm "sev" -> erased
//   - inline asm "mrs ..." -> "mov <out>, #0" (or erased if no output reg)
//   - inline asm "msr ..." -> erased
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

    // The firmware is built with panic=abort and .cantunwind, with no
    // unwinder, so a frame pointer is pure overhead. rustc asks for
    // "non-leaf" per function; drop it so r7 stays allocatable and the
    // prologue does not need "add r7, sp, #0".
    Attribute FP = F.getFnAttribute("frame-pointer");
    if (FP.isStringAttribute() && FP.getValueAsString() == "non-leaf") {
      F.addFnAttr("frame-pointer", "none");
      Changed = true;
    }

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
      // strex(value, ptr) -> store(value, ptr); return 0.
      // The single-core TC32 store always succeeds; ARM strex returns 0 on
      // success, not the stored value.
      IRBuilder<> Builder(II);
      Value *Val = II->getArgOperand(0);
      Value *Ptr = II->getArgOperand(1);
      StoreInst *Store = Builder.CreateStore(Val, Ptr);
      Store->setAlignment(Align(4));
      II->replaceAllUsesWith(ConstantInt::get(II->getType(), 0));
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
      // TC32 has no status-register access instructions.
      // A "mrs" read becomes "mov ${0}, #0" when the first constraint is an
      // output register; otherwise the call is dropped. An "msr" write has no
      // TC32 equivalent and is a no-op on this core, so it is dropped.
      // Nothing here may modify sp.
      if (AsmStr.starts_with("msr ")) {
        if (!CI->getType()->isVoidTy())
          CI->replaceAllUsesWith(UndefValue::get(CI->getType()));
        ToErase.push_back(CI);
        return true;
      }

      StringRef Constraints = IA->getConstraintString();
      bool FirstIsOutput =
          !Constraints.empty() &&
          (Constraints.front() == '=' || Constraints.front() == '+');
      if (!FirstIsOutput) {
        if (!CI->getType()->isVoidTy())
          CI->replaceAllUsesWith(UndefValue::get(CI->getType()));
        ToErase.push_back(CI);
        return true;
      }

      InlineAsm *NewIA = InlineAsm::get(IA->getFunctionType(), "mov ${0}, #0",
                                        Constraints, IA->hasSideEffects(),
                                        IA->isAlignStack(), IA->getDialect());

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
