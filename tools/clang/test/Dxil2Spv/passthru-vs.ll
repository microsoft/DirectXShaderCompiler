; RUN: %dxil2spv
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; POSITION                 0   xyzw        0     NONE   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float   xyzw
; COLOR                    0   xyzw        1     NONE   float   xyzw
;
; shader hash: 1e0d030966adc0863e25cb3cfa84166b
;
; Pipeline Runtime Information: 
;
; Vertex Shader
; OutputPositionPresent=1
;
;
; Input signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; POSITION                 0                              
; COLOR                    0                              
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective       
; COLOR                    0                 linear       
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
; Number of inputs: 8, outputs: 8
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0 }
;   output 1 depends on inputs: { 1 }
;   output 2 depends on inputs: { 2 }
;   output 3 depends on inputs: { 3 }
;   output 4 depends on inputs: { 4 }
;   output 5 depends on inputs: { 5 }
;   output 6 depends on inputs: { 6 }
;   output 7 depends on inputs: { 7 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @VSMain() {
  %1 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %5)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %6)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %7)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %8)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %1)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %2)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %3)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %4)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
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
!3 = !{!"vs", i32 6, i32 0}
!4 = !{[10 x i32] [i32 8, i32 8, i32 1, i32 2, i32 4, i32 8, i32 16, i32 32, i32 64, i32 128]}
!5 = !{void ()* @VSMain, !"VSMain", !6, null, null}
!6 = !{!7, !12, null}
!7 = !{!8, !11}
!8 = !{i32 0, !"POSITION", i8 9, i8 0, !9, i8 0, i32 1, i8 4, i32 0, i8 0, !10}
!9 = !{i32 0}
!10 = !{i32 3, i32 15}
!11 = !{i32 1, !"COLOR", i8 9, i8 0, !9, i8 0, i32 1, i8 4, i32 1, i8 0, !10}
!12 = !{!13, !14}
!13 = !{i32 0, !"SV_Position", i8 9, i8 3, !9, i8 4, i32 1, i8 4, i32 0, i8 0, !10}
!14 = !{i32 1, !"COLOR", i8 9, i8 0, !9, i8 2, i32 1, i8 4, i32 1, i8 0, !10}

; CHECK-WHOLE-SPIR-V:
; ; SPIR-V
; ; Version: 1.0
; ; Generator: Google spiregg; 0
; ; Bound: 44
; ; Schema: 0
;                OpCapability Shader
;                OpMemoryModel Logical GLSL450
;                OpEntryPoint Vertex %VSMain "VSMain" %POSITION %COLOR %SV_Position %COLOR_0
;                OpName %POSITION "POSITION"
;                OpName %COLOR "COLOR"
;                OpName %SV_Position "SV_Position"
;                OpName %COLOR_0 "COLOR"
;                OpName %VSMain "VSMain"
;                OpDecorate %POSITION Location 0
;                OpDecorate %COLOR Location 1
;                OpDecorate %SV_Position Location 0
;                OpDecorate %COLOR_0 Location 1
;        %uint = OpTypeInt 32 0
;      %uint_0 = OpConstant %uint 0
;      %uint_1 = OpConstant %uint 1
;      %uint_2 = OpConstant %uint 2
;      %uint_3 = OpConstant %uint 3
;       %float = OpTypeFloat 32
;     %v4float = OpTypeVector %float 4
; %_ptr_Input_v4float = OpTypePointer Input %v4float
; %_ptr_Output_v4float = OpTypePointer Output %v4float
;        %void = OpTypeVoid
;          %16 = OpTypeFunction %void
; %_ptr_Input_float = OpTypePointer Input %float
; %_ptr_Output_float = OpTypePointer Output %float
;    %POSITION = OpVariable %_ptr_Input_v4float Input
;       %COLOR = OpVariable %_ptr_Input_v4float Input
; %SV_Position = OpVariable %_ptr_Output_v4float Output
;     %COLOR_0 = OpVariable %_ptr_Output_v4float Output
;      %VSMain = OpFunction %void None %16
;          %17 = OpLabel
;          %19 = OpAccessChain %_ptr_Input_float %COLOR %uint_0
;          %20 = OpLoad %float %19
;          %21 = OpAccessChain %_ptr_Input_float %COLOR %uint_1
;          %22 = OpLoad %float %21
;          %23 = OpAccessChain %_ptr_Input_float %COLOR %uint_2
;          %24 = OpLoad %float %23
;          %25 = OpAccessChain %_ptr_Input_float %COLOR %uint_3
;          %26 = OpLoad %float %25
;          %27 = OpAccessChain %_ptr_Input_float %POSITION %uint_0
;          %28 = OpLoad %float %27
;          %29 = OpAccessChain %_ptr_Input_float %POSITION %uint_1
;          %30 = OpLoad %float %29
;          %31 = OpAccessChain %_ptr_Input_float %POSITION %uint_2
;          %32 = OpLoad %float %31
;          %33 = OpAccessChain %_ptr_Input_float %POSITION %uint_3
;          %34 = OpLoad %float %33
;          %36 = OpAccessChain %_ptr_Output_float %SV_Position %uint_0
;                OpStore %36 %28
;          %37 = OpAccessChain %_ptr_Output_float %SV_Position %uint_1
;                OpStore %37 %30
;          %38 = OpAccessChain %_ptr_Output_float %SV_Position %uint_2
;                OpStore %38 %32
;          %39 = OpAccessChain %_ptr_Output_float %SV_Position %uint_3
;                OpStore %39 %34
;          %40 = OpAccessChain %_ptr_Output_float %COLOR_0 %uint_0
;                OpStore %40 %20
;          %41 = OpAccessChain %_ptr_Output_float %COLOR_0 %uint_1
;                OpStore %41 %22
;          %42 = OpAccessChain %_ptr_Output_float %COLOR_0 %uint_2
;                OpStore %42 %24
;          %43 = OpAccessChain %_ptr_Output_float %COLOR_0 %uint_3
;                OpStore %43 %26
;                OpReturn
;                OpFunctionEnd
