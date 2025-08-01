; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py UTC_ARGS: --version 5
; RUN: llc < %s -mtriple=riscv64 -mattr=+v -verify-machineinstrs | FileCheck %s

; Use cttz to test if we properly prove never-zero. There is a very
; simple transform from cttz -> cttz_zero_undef if its operand is
; known never zero.

; Even without vscale_range, vscale is always guaranteed to be non-zero.
define i32 @vscale_known_nonzero() {
; CHECK-LABEL: vscale_known_nonzero:
; CHECK:       # %bb.0:
; CHECK-NEXT:    csrr a0, vlenb
; CHECK-NEXT:    srli a0, a0, 3
; CHECK-NEXT:    neg a1, a0
; CHECK-NEXT:    and a0, a0, a1
; CHECK-NEXT:    slli a1, a0, 6
; CHECK-NEXT:    slli a2, a0, 8
; CHECK-NEXT:    slli a3, a0, 10
; CHECK-NEXT:    slli a4, a0, 12
; CHECK-NEXT:    add a1, a1, a2
; CHECK-NEXT:    slli a2, a0, 16
; CHECK-NEXT:    sub a3, a3, a4
; CHECK-NEXT:    slli a4, a0, 18
; CHECK-NEXT:    sub a2, a2, a4
; CHECK-NEXT:    slli a4, a0, 4
; CHECK-NEXT:    sub a4, a0, a4
; CHECK-NEXT:    add a1, a4, a1
; CHECK-NEXT:    slli a4, a0, 14
; CHECK-NEXT:    sub a3, a3, a4
; CHECK-NEXT:    slli a4, a0, 23
; CHECK-NEXT:    sub a2, a2, a4
; CHECK-NEXT:    slli a0, a0, 27
; CHECK-NEXT:    add a1, a1, a3
; CHECK-NEXT:    add a0, a2, a0
; CHECK-NEXT:    add a0, a1, a0
; CHECK-NEXT:    srliw a0, a0, 27
; CHECK-NEXT:    lui a1, %hi(.LCPI0_0)
; CHECK-NEXT:    addi a1, a1, %lo(.LCPI0_0)
; CHECK-NEXT:    add a0, a1, a0
; CHECK-NEXT:    lbu a0, 0(a0)
; CHECK-NEXT:    ret
  %x = call i32 @llvm.vscale()
  %r = call i32 @llvm.cttz.i32(i32 %x, i1 false)
  ret i32 %r
}
