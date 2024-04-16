; RUN: not %dxv %s | FileCheck %s

; REQUIRES: dxil-1-9

; This test was taken from tools\clang\test\CodeGenDXIL\hlsl\shaders\node\mesh-node-no-outputs.hlsl
; and compiled with Tlib_6_9, after changing the node launch type from mesh to broadcasting.
; The launch type has been manually written to be mesh below.
; The test is to ensure that the validator catches incorrect input record types in metadata,
; as well as any node outputs.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @myFancyNode() {
  ret void
}

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!7, !8}

!0 = !{!"dxc(private) 1.8.0.4467 (mesh-nodes-out-params, 278d6cb3bac9-dirty)"}
!1 = !{i32 1, i32 9}
!2 = !{!"lib", i32 6, i32 9}
!3 = !{i32 1, void ()* @myFancyNode, !4}
!4 = !{!5}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{null, !"", null, null, null}
; CHECK: error: node shader 'myFancyNode' has disallowed node output 'myFascinatingNode' for 'mesh' launch
!8 = !{void ()* @myFancyNode, !"myFancyNode", null, null, !9}
; change the 4th argument from 1->4, to set mesh node launch type
!9 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !10, i32 16, i32 -1, i32 18, !11, i32 20, !12, i32 21, !15, i32 4, !29, i32 5, !30}
!10 = !{!"myFancyNode", i32 0}
!11 = !{i32 2, i32 2, i32 2}
!12 = !{!13}
!13 = !{i32 1, i32 97, i32 2, !14}
!14 = !{i32 0, i32 8, i32 2, i32 4}
!15 = !{!16, !20, !22, !25, !27}
!16 = !{i32 1, i32 6, i32 2, !17, i32 3, i32 0, i32 0, !19}
!17 = !{i32 0, i32 20, i32 1, !18, i32 2, i32 4}
!18 = !{i32 0, i32 5, i32 3}
!19 = !{!"myFascinatingNode", i32 0}
!20 = !{i32 1, i32 6, i32 2, !17, i32 3, i32 4, i32 0, !21}
; CHECK: error: node shader 'myFancyNode' has disallowed node output 'myNiftyNode' for 'mesh' launch
!21 = !{!"myNiftyNode", i32 3}
!22 = !{i32 1, i32 22, i32 2, !23, i32 3, i32 0, i32 5, i32 63, i32 4, i32 1, i32 6, i1 true, i32 0, !24}
!23 = !{i32 0, i32 16, i32 2, i32 4}
; CHECK: error: node shader 'myFancyNode' has disallowed node output 'myMaterials' for 'mesh' launch
!24 = !{!"myMaterials", i32 0}
!25 = !{i32 1, i32 10, i32 3, i32 20, i32 0, !26}
; CHECK: error: node shader 'myFancyNode' has disallowed node output 'myProgressCounter' for 'mesh' launch
!26 = !{!"myProgressCounter", i32 0}
!27 = !{i32 1, i32 26, i32 3, i32 20, i32 0, !28}
; CHECK: error: node shader 'myFancyNode' has disallowed node output 'myProgressCounter2' for 'mesh' launch
!28 = !{!"myProgressCounter2", i32 0}
!29 = !{i32 4, i32 5, i32 6}
!30 = !{i32 0}
