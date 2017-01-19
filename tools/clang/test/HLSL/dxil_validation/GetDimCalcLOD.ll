; RUN: %dxv %s | FileCheck %s

; CHECK: GetDimensions used undef dimension z on TextureCube
; CHECK: coord uninitialized

;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; UV                       0   xy          0     NONE   float
;
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
; Input signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; UV                       0                 linear
;
; Output signature:
;
; Name                 Index             InterpMode
; -------------------- ----- ----------------------
; SV_Target                0
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; g_sam                             sampler      NA          NA      S0             s0     1
; cube                              texture     f32        cube      T0             t0     1
;
target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.TextureCube = type { <4 x float> }
%struct.SamplerState = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.Dimensions = type { i32, i32, i32, i32 }

@"\01?cube@@3V?$TextureCube@V?$vector@M$03@@@@A" = available_externally global %class.TextureCube zeroinitializer, align 4
@"\01?g_sam@@3USamplerState@@A" = available_externally global %struct.SamplerState zeroinitializer, align 4
@dx.typevar.0 = external addrspace(1) constant %class.TextureCube
@llvm.used = appending global [5 x i8*] [i8* bitcast (%class.TextureCube* @"\01?cube@@3V?$TextureCube@V?$vector@M$03@@@@A" to i8*), i8* bitcast (%struct.SamplerState* @"\01?g_sam@@3USamplerState@@A" to i8*), i8* bitcast (%class.TextureCube* @"\01?cube@@3V?$TextureCube@V?$vector@M$03@@@@A" to i8*), i8* bitcast (%struct.SamplerState* @"\01?g_sam@@3USamplerState@@A" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.TextureCube addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<2 x float>, <4 x float>* nocapture readnone) #0 {
entry:
  %cube_texture_cube = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 0, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %g_sam_sampler = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 3, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call %dx.types.Dimensions @dx.op.getDimensions(i32 74, %dx.types.Handle %cube_texture_cube, i32 0)  ; GetDimensions(handle,mipLevel)
  %5 = extractvalue %dx.types.Dimensions %4, 0
  %6 = extractvalue %dx.types.Dimensions %4, 2
  %7 = call float @dx.op.calculateLOD.f32(i32 83, %dx.types.Handle %cube_texture_cube, %dx.types.Handle %g_sam_sampler, float %2, float %3, float undef, i1 true)  ; CalculateLOD(handle,sampler,coord0,coord1,coord2,clamped)
  %conv = uitofp i32 %5 to float
  %conv1 = uitofp i32 %6 to float
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %conv)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %conv1)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %7)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 1.000000e+00)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readonly
declare %dx.types.Dimensions @dx.op.getDimensions(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare float @dx.op.calculateLOD.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, i1) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!9, !12}
!dx.entryPoints = !{!21}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{!"ps", i32 6, i32 0}
!3 = !{!4, null, null, !7}
!4 = !{!5}
!5 = !{i32 0, %class.TextureCube* @"\01?cube@@3V?$TextureCube@V?$vector@M$03@@@@A", !"cube", i32 0, i32 0, i32 1, i32 5, i32 0, !6}
!6 = !{i32 0, i32 9}
!7 = !{!8}
!8 = !{i32 0, %struct.SamplerState* @"\01?g_sam@@3USamplerState@@A", !"g_sam", i32 0, i32 0, i32 1, i32 0, null}
!9 = !{i32 0, %class.TextureCube addrspace(1)* @dx.typevar.0, !10}
!10 = !{i32 16, !11}
!11 = !{i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!12 = !{i32 1, void (<2 x float>, <4 x float>*)* @main.flat, !13}
!13 = !{!14, !16, !19}
!14 = !{i32 0, !15, !15}
!15 = !{}
!16 = !{i32 0, !17, !18}
!17 = !{i32 4, !"UV", i32 7, i32 9}
!18 = !{i32 0}
!19 = !{i32 1, !20, !18}
!20 = !{i32 4, !"SV_TARGET", i32 7, i32 9}
!21 = !{void (<2 x float>, <4 x float>*)* @main.flat, !"", !22, !3, null}
!22 = !{!23, !25, null}
!23 = !{!24}
!24 = !{i32 0, !"UV", i8 9, i8 0, !18, i8 2, i32 1, i8 2, i32 0, i8 0, null}
!25 = !{!26}
!26 = !{i32 0, !"SV_Target", i8 9, i8 16, !18, i8 0, i32 1, i8 4, i32 0, i8 0, null}

