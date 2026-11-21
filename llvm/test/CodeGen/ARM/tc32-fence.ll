; RUN: llc -mtriple=thumbv6m-none-eabi -mcpu=arm9 -mattr=+tc32 < %s | FileCheck %s
; A single-thread fence and a system fence on a core with no data barrier must
; lower to a zero-instruction compiler barrier, never to __sync_synchronize, and
; acquire/release atomics must lower to plain loads/stores.

; CHECK-LABEL: cf:
; CHECK-NOT: __sync_synchronize
; CHECK-NOT: dmb
; CHECK: bx lr

; CHECK-LABEL: hf:
; CHECK-NOT: __sync_synchronize
; CHECK: bx lr

; CHECK-LABEL: ld_acq:
; CHECK-NOT: __sync
; CHECK: ldr r0, [r0]
; CHECK: bx lr

; CHECK-LABEL: st_rel:
; CHECK-NOT: __sync
; CHECK: str r1, [r0]
; CHECK: bx lr

define void @cf() {
  fence syncscope("singlethread") acquire
  ret void
}

define void @hf() {
  fence acquire
  ret void
}

define i32 @ld_acq(ptr %p) {
  %v = load atomic i32, ptr %p acquire, align 4
  ret i32 %v
}

define void @st_rel(ptr %p, i32 %v) {
  store atomic i32 %v, ptr %p release, align 4
  ret void
}
