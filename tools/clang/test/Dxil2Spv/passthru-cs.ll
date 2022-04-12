; RUN: %dxil2spv
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no parameters
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no parameters
; shader hash: aba83cb71e5a9eee4db93a4e5df0d6cd
;
; Pipeline Runtime Information: 
;
;
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; Buffer0                           texture    byte         r/o      T0             t0     1
; BufferOut                             UAV    byte         r/w      U0             u1     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%struct.ByteAddressBuffer = type { i32 }
%struct.RWByteAddressBuffer = type { i32 }

define void @main() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %4 = shl i32 %3, 2
  %5 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %2, i32 %4, i32 undef)  ; BufferLoad(srv,index,wot)
  %6 = extractvalue %dx.types.ResRet.i32 %5, 0
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %1, i32 %4, i32 undef, i32 %6, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind
declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.entryPoints = !{!9}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 7}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{!5, !7, null, null}
!5 = !{!6}
!6 = !{i32 0, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 0, i32 1, i32 11, i32 0, null}
!7 = !{!8}
!8 = !{i32 0, %struct.RWByteAddressBuffer* undef, !"", i32 0, i32 1, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!9 = !{void ()* @main, !"main", null, !4, !10}
!10 = !{i32 0, i64 16, i32 4, !11}
!11 = !{i32 1, i32 1, i32 1}

; CHECK-WHOLE-SPIR-V:
; ; SPIR-V
; ; Version: 1.0
; ; Generator: Google spiregg; 0
; ; Bound: 25
; ; Schema: 0
;                OpCapability Shader
;                OpMemoryModel Logical GLSL450
;                OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID
;                OpExecutionMode %main LocalSize 1 1 1
;                OpName %type_ByteAddressBuffer "type.ByteAddressBuffer"
;                OpName %type_RWByteAddressBuffer "type.RWByteAddressBuffer"
;                OpName %main "main"
;                OpDecorate %3 DescriptorSet 0
;                OpDecorate %3 Binding 0
;                OpDecorate %4 DescriptorSet 0
;                OpDecorate %4 Binding 1
;                OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId
;                OpDecorate %_runtimearr_uint ArrayStride 4
;                OpMemberDecorate %type_ByteAddressBuffer 0 Offset 0
;                OpMemberDecorate %type_ByteAddressBuffer 0 NonWritable
;                OpDecorate %type_ByteAddressBuffer BufferBlock
;                OpMemberDecorate %type_RWByteAddressBuffer 0 Offset 0
;                OpDecorate %type_RWByteAddressBuffer BufferBlock
;        %uint = OpTypeInt 32 0
;      %uint_0 = OpConstant %uint 0
;      %uint_2 = OpConstant %uint 2
; %_runtimearr_uint = OpTypeRuntimeArray %uint
; %type_ByteAddressBuffer = OpTypeStruct %_runtimearr_uint
; %_ptr_Uniform_type_ByteAddressBuffer = OpTypePointer Uniform %type_ByteAddressBuffer
; %type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
; %_ptr_Uniform_type_RWByteAddressBuffer = OpTypePointer Uniform %type_RWByteAddressBuffer
;      %v3uint = OpTypeVector %uint 3
; %_ptr_Input_v3uint = OpTypePointer Input %v3uint
;        %void = OpTypeVoid
;          %16 = OpTypeFunction %void
; %_ptr_Input_uint = OpTypePointer Input %uint
; %_ptr_Uniform_uint = OpTypePointer Uniform %uint
;           %3 = OpVariable %_ptr_Uniform_type_ByteAddressBuffer Uniform
;           %4 = OpVariable %_ptr_Uniform_type_RWByteAddressBuffer Uniform
; %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input
;        %main = OpFunction %void None %16
;          %17 = OpLabel
;          %19 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0
;          %20 = OpLoad %uint %19
;          %21 = OpShiftLeftLogical %uint %20 %uint_2
;          %23 = OpAccessChain %_ptr_Uniform_uint %3 %uint_0 %21
;          %24 = OpLoad %uint %23
;                OpReturn
;                OpFunctionEnd
; CHECK-ERRORS:
; error: Unhandled LLVM instruction:   %6 = extractvalue %dx.types.ResRet.i32 %5, 0
; error: Unhandled DXIL opcode: BufferStore
