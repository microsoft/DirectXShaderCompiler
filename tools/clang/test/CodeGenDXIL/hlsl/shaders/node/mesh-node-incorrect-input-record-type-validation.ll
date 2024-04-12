; RUN: not %dxv %s | FileCheck %s

; This test was taken from tools\clang\test\CodeGenDXIL\hlsl\shaders\node\mesh-node-writable-input-record.hlsl
; and compiled with Tlib_6_9, after disabling the diagnostic that was emitted
; for incompatible input types.
; The test is to ensure that the validator catches incorrect input record types in metadata,
; as well as any node outputs.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @myFancyNode1() {
  ret void
}

define void @myFancyNode2() {
  ret void
}

define void @myFancyNode3() {
  ret void
}

define void @myFancyNode4() {
  ret void
}

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!7, !8, !17, !22, !27}

!0 = !{!"dxc(private) 1.8.0.4467 (mesh-nodes-out-params, 278d6cb3bac9-dirty)"}
!1 = !{i32 1, i32 9}
!2 = !{!"lib", i32 6, i32 9}
; CHECK: mesh node shader 'myFancyNode1' has incompatible input record type (should be DispatchNodeInputRecord)
!3 = !{i32 1, void ()* @myFancyNode1, !4, void ()* @myFancyNode2, !4, void ()* @myFancyNode3, !4, void ()* @myFancyNode4, !4}
!4 = !{!5}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{null, !"", null, null, null}
!8 = !{void ()* @myFancyNode1, !"myFancyNode1", null, null, !9}
!9 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !10, i32 16, i32 -1, i32 18, !11, i32 65536, i32 1, i32 20, !12, i32 4, !15, i32 5, !16}
!10 = !{!"myFancyNode1", i32 0}
!11 = !{i32 2, i32 2, i32 2}
!12 = !{!13}
!13 = !{i32 1, i32 33, i32 2, !14}
!14 = !{i32 0, i32 16, i32 2, i32 4}
!15 = !{i32 4, i32 5, i32 6}
!16 = !{i32 0}
; CHECK: mesh node shader 'myFancyNode2' has incompatible input record type (should be DispatchNodeInputRecord)
!17 = !{void ()* @myFancyNode2, !"myFancyNode2", null, null, !18}
!18 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !19, i32 16, i32 -1, i32 18, !11, i32 65536, i32 1, i32 20, !20, i32 4, !15, i32 5, !16}
!19 = !{!"myFancyNode2", i32 0}
!20 = !{!21}
!21 = !{i32 1, i32 37, i32 2, !14}
; CHECK: mesh node shader 'myFancyNode3' has incompatible input record type (should be DispatchNodeInputRecord)
!22 = !{void ()* @myFancyNode3, !"myFancyNode3", null, null, !23}
!23 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !24, i32 16, i32 -1, i32 18, !11, i32 65536, i32 1, i32 20, !25, i32 4, !15, i32 5, !16}
!24 = !{!"myFancyNode3", i32 0}
!25 = !{!26}
!26 = !{i32 1, i32 65, i32 2, !14}
; CHECK: mesh node shader 'myFancyNode4' has incompatible input record type (should be DispatchNodeInputRecord)
!27 = !{void ()* @myFancyNode4, !"myFancyNode4", null, null, !28}
!28 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !29, i32 16, i32 -1, i32 18, !11, i32 65536, i32 1, i32 20, !30, i32 4, !15, i32 5, !16}
!29 = !{!"myFancyNode4", i32 0}
!30 = !{!31}
!31 = !{i32 1, i32 69, i32 2, !14}