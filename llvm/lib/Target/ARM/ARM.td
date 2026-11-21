//===-- ARM.td - Describe the ARM Target Machine -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "ARMFeatures.td"
include "ARMArchitectures.td"

//===----------------------------------------------------------------------===//
// Telink TC32
//===----------------------------------------------------------------------===//

// The TC32 core is an ARMv4T Thumb-1 core that permutes bits[15:12] of every
// 16-bit instruction halfword. Enabling this feature makes the MC layer
// scramble emitted halfwords and unscramble disassembled ones.
def FeatureTC32 : SubtargetFeature<"tc32", "IsTC32", "true",
  "Telink TC32 Thumb-1 variant (top-nibble-permuted instruction encoding)">;

//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "ARMRegisterInfo.td"
include "ARMRegisterBanks.td"
include "ARMCallingConv.td"

//===----------------------------------------------------------------------===//
// ARM schedules.
//===----------------------------------------------------------------------===//
//
include "ARMPredicates.td"
include "ARMSchedule.td"

//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "ARMInstrInfo.td"

def Thumb1OnlyMode : HwMode<[IsThumb1Only]>;
def arm_ptr_rc : RegClassByHwMode<
  [DefaultMode, Thumb1OnlyMode],
  [GPR, tGPR]>;

defm : RemapAllTargetPseudoPointerOperands<arm_ptr_rc>;

def ARMInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// ARM schedules
//
include "ARMScheduleV6.td"
include "ARMScheduleA8.td"
include "ARMScheduleA9.td"
include "ARMScheduleSwift.td"
include "ARMScheduleR52.td"
include "ARMScheduleA57.td"
include "ARMScheduleM4.td"
include "ARMScheduleM55.td"
include "ARMScheduleM7.td"
include "ARMScheduleM85.td"

include "ARMProcessors.td"

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def ARMAsmWriter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  int PassSubtarget = 1;
  int Variant = 0;
  bit isMCAsmWriter = 1;
}

def ARMAsmParser : AsmParser {
  bit ReportMultipleNearMisses = 1;
  let PreferSmallerInstructions = true;
}

def ARMAsmParserVariant : AsmParserVariant {
  int Variant = 0;
  string Name = "ARM";
  string BreakCharacters = ".";
}

def ARM : Target {
  // Pull in Instruction Info.
  let InstructionSet = ARMInstrInfo;
  let AssemblyWriters = [ARMAsmWriter];
  let AssemblyParsers = [ARMAsmParser];
  let AssemblyParserVariants = [ARMAsmParserVariant];
  let AllowRegisterRenaming = 1;
}
