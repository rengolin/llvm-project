; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py UTC_ARGS: --version 5
; RUN: llc < %s -mtriple=nvptx64 -mcpu=sm_20 | FileCheck %s
; RUN: %if ptxas %{ llc < %s -mtriple=nvptx64 -mcpu=sm_20 | %ptxas-verify %}

define void @foo1(<2 x float> %val, ptr %ptr) {
; CHECK-LABEL: foo1(
; CHECK:       {
; CHECK-NEXT:    .reg .b64 %rd<3>;
; CHECK-EMPTY:
; CHECK-NEXT:  // %bb.0:
; CHECK-NEXT:    ld.param.b64 %rd1, [foo1_param_0];
; CHECK-NEXT:    ld.param.b64 %rd2, [foo1_param_1];
; CHECK-NEXT:    st.b64 [%rd2], %rd1;
; CHECK-NEXT:    ret;
  store <2 x float> %val, ptr %ptr
  ret void
}

define void @foo2(<4 x float> %val, ptr %ptr) {
; CHECK-LABEL: foo2(
; CHECK:       {
; CHECK-NEXT:    .reg .b64 %rd<4>;
; CHECK-EMPTY:
; CHECK-NEXT:  // %bb.0:
; CHECK-NEXT:    ld.param.v2.b64 {%rd1, %rd2}, [foo2_param_0];
; CHECK-NEXT:    ld.param.b64 %rd3, [foo2_param_1];
; CHECK-NEXT:    st.v2.b64 [%rd3], {%rd1, %rd2};
; CHECK-NEXT:    ret;
  store <4 x float> %val, ptr %ptr
  ret void
}

define void @foo3(<2 x i32> %val, ptr %ptr) {
; CHECK-LABEL: foo3(
; CHECK:       {
; CHECK-NEXT:    .reg .b32 %r<3>;
; CHECK-NEXT:    .reg .b64 %rd<2>;
; CHECK-EMPTY:
; CHECK-NEXT:  // %bb.0:
; CHECK-NEXT:    ld.param.v2.b32 {%r1, %r2}, [foo3_param_0];
; CHECK-NEXT:    ld.param.b64 %rd1, [foo3_param_1];
; CHECK-NEXT:    st.v2.b32 [%rd1], {%r1, %r2};
; CHECK-NEXT:    ret;
  store <2 x i32> %val, ptr %ptr
  ret void
}

define void @foo4(<4 x i32> %val, ptr %ptr) {
; CHECK-LABEL: foo4(
; CHECK:       {
; CHECK-NEXT:    .reg .b32 %r<5>;
; CHECK-NEXT:    .reg .b64 %rd<2>;
; CHECK-EMPTY:
; CHECK-NEXT:  // %bb.0:
; CHECK-NEXT:    ld.param.v4.b32 {%r1, %r2, %r3, %r4}, [foo4_param_0];
; CHECK-NEXT:    ld.param.b64 %rd1, [foo4_param_1];
; CHECK-NEXT:    st.v4.b32 [%rd1], {%r1, %r2, %r3, %r4};
; CHECK-NEXT:    ret;
  store <4 x i32> %val, ptr %ptr
  ret void
}

define void @v16i8(ptr %a, ptr %b) {
; CHECK-LABEL: v16i8(
; CHECK:       {
; CHECK-NEXT:    .reg .b32 %r<5>;
; CHECK-NEXT:    .reg .b64 %rd<3>;
; CHECK-EMPTY:
; CHECK-NEXT:  // %bb.0:
; CHECK-NEXT:    ld.param.b64 %rd1, [v16i8_param_0];
; CHECK-NEXT:    ld.v4.b32 {%r1, %r2, %r3, %r4}, [%rd1];
; CHECK-NEXT:    ld.param.b64 %rd2, [v16i8_param_1];
; CHECK-NEXT:    st.v4.b32 [%rd2], {%r1, %r2, %r3, %r4};
; CHECK-NEXT:    ret;
  %v = load <16 x i8>, ptr %a
  store <16 x i8> %v, ptr %b
  ret void
}
