; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -passes=loop-vectorize -force-target-supports-scalable-vectors=true -scalable-vectorization=on -S | FileCheck %s

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"

define i8 @reduction_add_trunc(ptr noalias nocapture %A) {
; CHECK-LABEL: @reduction_add_trunc(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP30:%.*]] = call i32 @llvm.vscale.i32()
; CHECK-NEXT:    [[TMP31:%.*]] = mul nuw i32 [[TMP30]], 16
; CHECK-NEXT:    [[MIN_ITERS_CHECK:%.*]] = icmp ult i32 256, [[TMP31]]
; CHECK-NEXT:    br i1 [[MIN_ITERS_CHECK]], label [[SCALAR_PH:%.*]], label [[VECTOR_PH:%.*]]
; CHECK:       vector.ph:
; CHECK-NEXT:    [[TMP2:%.*]] = call i32 @llvm.vscale.i32()
; CHECK-NEXT:    [[TMP3:%.*]] = mul nuw i32 [[TMP2]], 16
; CHECK-NEXT:    [[N_MOD_VF:%.*]] = urem i32 256, [[TMP3]]
; CHECK-NEXT:    [[N_VEC:%.*]] = sub i32 256, [[N_MOD_VF]]
; CHECK-NEXT:    [[TMP4:%.*]] = call i32 @llvm.vscale.i32()
; CHECK-NEXT:    [[TMP5:%.*]] = mul nuw i32 [[TMP4]], 16
; CHECK-NEXT:    br label [[VECTOR_BODY:%.*]]
; CHECK:       vector.body:
; CHECK-NEXT:    [[INDEX:%.*]] = phi i32 [ 0, [[VECTOR_PH]] ], [ [[INDEX_NEXT:%.*]], [[VECTOR_BODY]] ]
; CHECK-NEXT:    [[VEC_PHI:%.*]] = phi <vscale x 8 x i32> [ insertelement (<vscale x 8 x i32> zeroinitializer, i32 255, i32 0), [[VECTOR_PH]] ], [ [[TMP34:%.*]], [[VECTOR_BODY]] ]
; CHECK-NEXT:    [[VEC_PHI1:%.*]] = phi <vscale x 8 x i32> [ zeroinitializer, [[VECTOR_PH]] ], [ [[TMP36:%.*]], [[VECTOR_BODY]] ]
; CHECK-NEXT:    [[TMP14:%.*]] = and <vscale x 8 x i32> [[VEC_PHI]], splat (i32 255)
; CHECK-NEXT:    [[TMP15:%.*]] = and <vscale x 8 x i32> [[VEC_PHI1]], splat (i32 255)
; CHECK-NEXT:    [[TMP8:%.*]] = getelementptr inbounds i8, ptr [[A:%.*]], i32 [[INDEX]]
; CHECK-NEXT:    [[TMP10:%.*]] = call i64 @llvm.vscale.i64()
; CHECK-NEXT:    [[TMP11:%.*]] = mul nuw i64 [[TMP10]], 8
; CHECK-NEXT:    [[TMP12:%.*]] = getelementptr inbounds i8, ptr [[TMP8]], i64 [[TMP11]]
; CHECK-NEXT:    [[WIDE_LOAD:%.*]] = load <vscale x 8 x i8>, ptr [[TMP8]], align 4
; CHECK-NEXT:    [[WIDE_LOAD2:%.*]] = load <vscale x 8 x i8>, ptr [[TMP12]], align 4
; CHECK-NEXT:    [[TMP26:%.*]] = zext <vscale x 8 x i8> [[WIDE_LOAD]] to <vscale x 8 x i32>
; CHECK-NEXT:    [[TMP27:%.*]] = zext <vscale x 8 x i8> [[WIDE_LOAD2]] to <vscale x 8 x i32>
; CHECK-NEXT:    [[TMP28:%.*]] = add <vscale x 8 x i32> [[TMP14]], [[TMP26]]
; CHECK-NEXT:    [[TMP29:%.*]] = add <vscale x 8 x i32> [[TMP15]], [[TMP27]]
; CHECK-NEXT:    [[TMP33:%.*]] = trunc <vscale x 8 x i32> [[TMP28]] to <vscale x 8 x i8>
; CHECK-NEXT:    [[TMP35:%.*]] = trunc <vscale x 8 x i32> [[TMP29]] to <vscale x 8 x i8>
; CHECK-NEXT:    [[TMP34]] = zext <vscale x 8 x i8> [[TMP33]] to <vscale x 8 x i32>
; CHECK-NEXT:    [[TMP36]] = zext <vscale x 8 x i8> [[TMP35]] to <vscale x 8 x i32>
; CHECK-NEXT:    [[INDEX_NEXT]] = add nuw i32 [[INDEX]], [[TMP5]]
; CHECK-NEXT:    [[TMP21:%.*]] = icmp eq i32 [[INDEX_NEXT]], [[N_VEC]]
; CHECK-NEXT:    br i1 [[TMP21]], label [[MIDDLE_BLOCK:%.*]], label [[VECTOR_BODY]], !llvm.loop [[LOOP0:![0-9]+]]
; CHECK:       middle.block:
; CHECK-NEXT:    [[BIN_RDX:%.*]] = add <vscale x 8 x i8> [[TMP35]], [[TMP33]]
; CHECK-NEXT:    [[TMP39:%.*]] = call i8 @llvm.vector.reduce.add.nxv8i8(<vscale x 8 x i8> [[BIN_RDX]])
; CHECK-NEXT:    [[TMP40:%.*]] = zext i8 [[TMP39]] to i32
; CHECK-NEXT:    [[CMP_N:%.*]] = icmp eq i32 256, [[N_VEC]]
; CHECK-NEXT:    br i1 [[CMP_N]], label [[EXIT:%.*]], label [[SCALAR_PH]]
; CHECK:       scalar.ph:
; CHECK-NEXT:    [[BC_RESUME_VAL:%.*]] = phi i32 [ [[N_VEC]], [[MIDDLE_BLOCK]] ], [ 0, [[ENTRY:%.*]] ]
; CHECK-NEXT:    [[BC_MERGE_RDX:%.*]] = phi i32 [ [[TMP40]], [[MIDDLE_BLOCK]] ], [ 255, [[ENTRY]] ]
; CHECK-NEXT:    br label [[LOOP:%.*]]
; CHECK:       loop:
; CHECK-NEXT:    [[INDVARS_IV:%.*]] = phi i32 [ [[INDVARS_IV_NEXT:%.*]], [[LOOP]] ], [ [[BC_RESUME_VAL]], [[SCALAR_PH]] ]
; CHECK-NEXT:    [[SUM_02P:%.*]] = phi i32 [ [[L9:%.*]], [[LOOP]] ], [ [[BC_MERGE_RDX]], [[SCALAR_PH]] ]
; CHECK-NEXT:    [[SUM_02:%.*]] = and i32 [[SUM_02P]], 255
; CHECK-NEXT:    [[L2:%.*]] = getelementptr inbounds i8, ptr [[A]], i32 [[INDVARS_IV]]
; CHECK-NEXT:    [[L3:%.*]] = load i8, ptr [[L2]], align 4
; CHECK-NEXT:    [[L3E:%.*]] = zext i8 [[L3]] to i32
; CHECK-NEXT:    [[L9]] = add i32 [[SUM_02]], [[L3E]]
; CHECK-NEXT:    [[INDVARS_IV_NEXT]] = add i32 [[INDVARS_IV]], 1
; CHECK-NEXT:    [[EXITCOND:%.*]] = icmp eq i32 [[INDVARS_IV_NEXT]], 256
; CHECK-NEXT:    br i1 [[EXITCOND]], label [[EXIT]], label [[LOOP]], !llvm.loop [[LOOP3:![0-9]+]]
; CHECK:       exit:
; CHECK-NEXT:    [[SUM_0_LCSSA:%.*]] = phi i32 [ [[L9]], [[LOOP]] ], [ [[TMP40]], [[MIDDLE_BLOCK]] ]
; CHECK-NEXT:    [[RET:%.*]] = trunc i32 [[SUM_0_LCSSA]] to i8
; CHECK-NEXT:    ret i8 [[RET]]
;
entry:
  br label %loop

loop:                                           ; preds = %entry, %loop
  %indvars.iv = phi i32 [ %indvars.iv.next, %loop ], [ 0, %entry ]
  %sum.02p = phi i32 [ %l9, %loop ], [ 255, %entry ]
  %sum.02 = and i32 %sum.02p, 255
  %l2 = getelementptr inbounds i8, ptr %A, i32 %indvars.iv
  %l3 = load i8, ptr %l2, align 4
  %l3e = zext i8 %l3 to i32
  %l9 = add i32 %sum.02, %l3e
  %indvars.iv.next = add i32 %indvars.iv, 1
  %exitcond = icmp eq i32 %indvars.iv.next, 256
  br i1 %exitcond, label %exit, label %loop, !llvm.loop !0

exit:                                      ; preds = %loop
  %sum.0.lcssa = phi i32 [ %l9, %loop ]
  %ret = trunc i32 %sum.0.lcssa to i8
  ret i8 %ret
}

!0 = distinct !{!0, !1, !2, !3, !4}
!1 = !{!"llvm.loop.vectorize.width", i32 8}
!2 = !{!"llvm.loop.vectorize.scalable.enable", i1 true}
!3 = !{!"llvm.loop.interleave.count", i32 2}
!4 = !{!"llvm.loop.vectorize.enable", i1 true}
