; RUN: %dxv %s | FileCheck %s

; CHECK: Instructions should not read uninitialized value

;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; B                        0   x           0     NONE unknown
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Depth                 0   x           0    DEPTH   float   xyzw
;
;
; Pipeline Runtime Information:
;
; Pixel Shader
; DepthOutput=1
; SampleFrequency=0
;
;
; Input signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; B                        0        nointerpolation
;
; Output signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; SV_Depth                 0
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;
target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
define void @"\01?main@@YAMM@Z.flat"(float, float* nocapture readnone) #0 {
entry:
  %2 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %add = fadd fast float %2, undef
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %add)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!12}

!0 = !{!"clang version 3.7.0 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 6, i32 0}
!3 = !{i32 1, void (float, float*)* @"\01?main@@YAMM@Z.flat", !4}
!4 = !{!5, !7, !10}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{i32 0, !8, !9}
!8 = !{i32 4, !"B", i32 7, i32 13}
!9 = !{i32 0}
!10 = !{i32 1, !11, !9}
!11 = !{i32 4, !"SV_DEPTH", i32 7, i32 9}
!12 = !{void (float, float*)* @"\01?main@@YAMM@Z.flat", !"", !13, null, null}
!13 = !{!14, !16, null}
!14 = !{!15}
!15 = !{i32 0, !"B", i8 13, i8 0, !9, i8 1, i32 1, i8 1, i32 0, i8 0, null}
!16 = !{!17}
!17 = !{i32 0, !"SV_Depth", i8 9, i8 17, !9, i8 0, i32 1, i8 1, i32 0, i8 0, null}

