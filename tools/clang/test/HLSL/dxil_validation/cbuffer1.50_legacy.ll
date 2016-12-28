; RUN: %dxv %s | FileCheck %s

; CHECK:Cbuffer access out of bound


;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no %s
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Target                0   xyzw        0   TARGET   float   xyzw
;
;
; Pipeline Runtime Information:
;
; Pixel Shader
; DepthOutput=0
; SampleFrequency=0
;
;
; Output signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; SV_Target                0
;
; Buffer Definitions:
;
; cbuffer Foo2
; {
;
;   struct Foo2
;   {
;
;       float4 g2;                                    ; Offset:    0
;
;   } Foo2                                            ; Offset:    0 Size:    16
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; Foo2                              cbuffer      NA          NA     CB0            cb5     1
;
target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%Foo2 = type { <4 x float> }
%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }

@Foo2 = external constant %Foo2
@dx.typevar.0 = external addrspace(1) constant %Foo2
@llvm.used = appending global [3 x i8*] [i8* bitcast (%Foo2* @Foo2 to i8*), i8* bitcast (%Foo2* @Foo2 to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%Foo2 addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<4 x float>* nocapture readnone) #0 {
entry:
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 2, i32 0, i32 5, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %1, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %3 = extractvalue %dx.types.CBufRet.f32 %2, 0
  %4 = extractvalue %dx.types.CBufRet.f32 %2, 1
  %5 = extractvalue %dx.types.CBufRet.f32 %2, 2
  %6 = extractvalue %dx.types.CBufRet.f32 %2, 3
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %3)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %4)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %5)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %6)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!6, !9}
!dx.entryPoints = !{!16}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 5, i32 0}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %Foo2* @Foo2, !"Foo2", i32 0, i32 5, i32 1, i32 16, null}
!6 = !{i32 0, %Foo2 addrspace(1)* @dx.typevar.0, !7}
!7 = !{i32 0, !8}
!8 = !{i32 3, i32 0, i32 6, !"g2", i32 7, i32 9}
!9 = !{i32 1, void (<4 x float>*)* @main.flat, !10}
!10 = !{!11, !13}
!11 = !{i32 0, !12, !12}
!12 = !{}
!13 = !{i32 1, !14, !15}
!14 = !{i32 4, !"SV_TARGET", i32 7, i32 9}
!15 = !{i32 0}
!16 = !{void (<4 x float>*)* @main.flat, !"", !17, !3, null}
!17 = !{null, !18, null}
!18 = !{!19}
!19 = !{i32 0, !"SV_Target", i8 9, i8 16, !15, i8 0, i32 1, i8 4, i32 0, i8 0, null}

