; RUN: %dxil2spv | %FileCheck %s
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float       
; COLOR                    0   xyzw        1     NONE   float   xyzw
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Target                0   xyzw        0   TARGET   float   xyzw
;
; shader hash: 9a0b5310118e98d0a258ef3921723379
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
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
; COLOR                    0                 linear       
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Target                0                              
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;
;
; ViewId state:
;
; Number of inputs: 8, outputs: 4
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 4 }
;   output 1 depends on inputs: { 5 }
;   output 2 depends on inputs: { 6 }
;   output 3 depends on inputs: { 7 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @PSMain() {
  %1 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %1)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %2)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %3)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %4)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.viewIdState = !{!4}
!dx.entryPoints = !{!5}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 7}
!3 = !{!"ps", i32 6, i32 0}
!4 = !{[10 x i32] [i32 8, i32 4, i32 0, i32 0, i32 0, i32 0, i32 1, i32 2, i32 4, i32 8]}
!5 = !{void ()* @PSMain, !"PSMain", !6, null, null}
!6 = !{!7, !12, null}
!7 = !{!8, !10}
!8 = !{i32 0, !"SV_Position", i8 9, i8 3, !9, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!9 = !{i32 0}
!10 = !{i32 1, !"COLOR", i8 9, i8 0, !9, i8 2, i32 1, i8 4, i32 1, i8 0, !11}
!11 = !{i32 3, i32 15}
!12 = !{!13}
!13 = !{i32 0, !"SV_Target", i8 9, i8 16, !9, i8 0, i32 1, i8 4, i32 0, i8 0, !11}

; CHECK:      ; SPIR-V
; CHECK-NEXT: ; Version: 1.0
; CHECK-NEXT: ; Generator: Google spiregg; 0
; CHECK-NEXT: ; Bound: 31
; CHECK-NEXT: ; Schema: 0
; CHECK-NEXT:                OpCapability Shader
; CHECK-NEXT:                OpMemoryModel Logical GLSL450
; CHECK-NEXT:                OpEntryPoint Fragment %PSMain "PSMain" %gl_Position %COLOR %SV_Target
; CHECK-NEXT:                OpExecutionMode %PSMain OriginUpperLeft
; CHECK-NEXT:                OpName %COLOR "COLOR"
; CHECK-NEXT:                OpName %SV_Target "SV_Target"
; CHECK-NEXT:                OpName %PSMain "PSMain"
; CHECK-NEXT:                OpDecorate %gl_Position BuiltIn Position
; CHECK-NEXT:                OpDecorate %COLOR Location 1
; CHECK-NEXT:                OpDecorate %SV_Target Location 0
; CHECK-NEXT:        %uint = OpTypeInt 32 0
; CHECK-NEXT:      %uint_0 = OpConstant %uint 0
; CHECK-NEXT:      %uint_1 = OpConstant %uint 1
; CHECK-NEXT:      %uint_2 = OpConstant %uint 2
; CHECK-NEXT:      %uint_3 = OpConstant %uint 3
; CHECK-NEXT:       %float = OpTypeFloat 32
; CHECK-NEXT:     %v4float = OpTypeVector %float 4
; CHECK-NEXT: %_ptr_Input_v4float = OpTypePointer Input %v4float
; CHECK-NEXT: %_ptr_Output_v4float = OpTypePointer Output %v4float
; CHECK-NEXT:        %void = OpTypeVoid
; CHECK-NEXT:          %15 = OpTypeFunction %void
; CHECK-NEXT: %_ptr_Input_float = OpTypePointer Input %float
; CHECK-NEXT: %_ptr_Output_float = OpTypePointer Output %float
; CHECK-NEXT: %gl_Position = OpVariable %_ptr_Input_v4float Input
; CHECK-NEXT:       %COLOR = OpVariable %_ptr_Input_v4float Input
; CHECK-NEXT:   %SV_Target = OpVariable %_ptr_Output_v4float Output
; CHECK-NEXT:      %PSMain = OpFunction %void None %15
; CHECK-NEXT:          %16 = OpLabel
; CHECK-NEXT:          %18 = OpAccessChain %_ptr_Input_float %COLOR %uint_0
; CHECK-NEXT:          %19 = OpLoad %float %18
; CHECK-NEXT:          %20 = OpAccessChain %_ptr_Input_float %COLOR %uint_1
; CHECK-NEXT:          %21 = OpLoad %float %20
; CHECK-NEXT:          %22 = OpAccessChain %_ptr_Input_float %COLOR %uint_2
; CHECK-NEXT:          %23 = OpLoad %float %22
; CHECK-NEXT:          %24 = OpAccessChain %_ptr_Input_float %COLOR %uint_3
; CHECK-NEXT:          %25 = OpLoad %float %24
; CHECK-NEXT:          %27 = OpAccessChain %_ptr_Output_float %SV_Target %uint_0
; CHECK-NEXT:                OpStore %27 %19
; CHECK-NEXT:          %28 = OpAccessChain %_ptr_Output_float %SV_Target %uint_1
; CHECK-NEXT:                OpStore %28 %21
; CHECK-NEXT:          %29 = OpAccessChain %_ptr_Output_float %SV_Target %uint_2
; CHECK-NEXT:                OpStore %29 %23
; CHECK-NEXT:          %30 = OpAccessChain %_ptr_Output_float %SV_Target %uint_3
; CHECK-NEXT:                OpStore %30 %25
; CHECK-NEXT:                OpReturn
; CHECK-NEXT:                OpFunctionEnd
