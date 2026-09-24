; RUN: %dxopt %s -jump-threading -S | FileCheck %s

; Verify that the JumpThreading pass respects the noduplicate attribute on
; the dx.op.setMeshOutputCounts DXIL operation. Prior to GitHub issue #8104,
; SetMeshOutputCounts did not have noduplicate, and JumpThreading would
; clone the call into multiple predecessor blocks when its arguments were
; PHIs whose incoming values came from a scalar branch, causing
; "SetMeshOutputCounts cannot be called multiple times" validation
; failures.

; The noduplicate attribute on the call site (and the declaration) must
; prevent JumpThreading from duplicating the call.

; CHECK-LABEL: define void @test
; CHECK: call void @dx.op.setMeshOutputCounts(i32 168,
; CHECK-NOT: call void @dx.op.setMeshOutputCounts

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @test(i1 %c, i32 %a, i32 %b) {
entry:
  br i1 %c, label %then, label %merge

then:
  br label %merge

merge:
  %nv = phi i32 [ %a, %then ], [ 0, %entry ]
  %np = phi i32 [ %b, %then ], [ 0, %entry ]
  call void @dx.op.setMeshOutputCounts(i32 168, i32 %nv, i32 %np)
  br i1 %c, label %t2, label %t3

t2:
  ret void

t3:
  ret void
}

; Function Attrs: noduplicate nounwind
declare void @dx.op.setMeshOutputCounts(i32, i32, i32) #0

attributes #0 = { noduplicate nounwind }
