; RUN: %dxil2spv | %FileCheck %s
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

; CHECK:      ; SPIR-V
; CHECK-NEXT: ; Version: 1.0
; CHECK-NEXT: ; Generator: Google spiregg; 0
; CHECK-NEXT: ; Bound: 59
; CHECK-NEXT: ; Schema: 0
; CHECK-NEXT:                OpCapability Shader
; CHECK-NEXT:                OpMemoryModel Logical GLSL450
; CHECK-NEXT:                OpEntryPoint GLCompute %main "main" %gl_GlobalInvocationID
; CHECK-NEXT:                OpExecutionMode %main LocalSize 1 1 1
; CHECK-NEXT:                OpName %type_ByteAddressBuffer "type.ByteAddressBuffer"
; CHECK-NEXT:                OpName %type_RWByteAddressBuffer "type.RWByteAddressBuffer"
; CHECK-NEXT:                OpName %main "main"
; CHECK-NEXT:                OpName %dx_types_ResRet_i32 "dx.types.ResRet.i32"
; CHECK-NEXT:                OpDecorate %3 DescriptorSet 0
; CHECK-NEXT:                OpDecorate %3 Binding 0
; CHECK-NEXT:                OpDecorate %4 DescriptorSet 0
; CHECK-NEXT:                OpDecorate %4 Binding 1
; CHECK-NEXT:                OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId
; CHECK-NEXT:                OpDecorate %_runtimearr_uint ArrayStride 4
; CHECK-NEXT:                OpMemberDecorate %type_ByteAddressBuffer 0 Offset 0
; CHECK-NEXT:                OpMemberDecorate %type_ByteAddressBuffer 0 NonWritable
; CHECK-NEXT:                OpDecorate %type_ByteAddressBuffer BufferBlock
; CHECK-NEXT:                OpMemberDecorate %type_RWByteAddressBuffer 0 Offset 0
; CHECK-NEXT:                OpDecorate %type_RWByteAddressBuffer BufferBlock
; CHECK-NEXT:        %uint = OpTypeInt 32 0
; CHECK-NEXT:      %uint_0 = OpConstant %uint 0
; CHECK-NEXT:      %uint_2 = OpConstant %uint 2
; CHECK-NEXT:      %uint_1 = OpConstant %uint 1
; CHECK-NEXT:      %uint_3 = OpConstant %uint 3
; CHECK-NEXT:      %uint_4 = OpConstant %uint 4
; CHECK-NEXT: %_runtimearr_uint = OpTypeRuntimeArray %uint
; CHECK-NEXT: %type_ByteAddressBuffer = OpTypeStruct %_runtimearr_uint
; CHECK-NEXT: %_ptr_Uniform_type_ByteAddressBuffer = OpTypePointer Uniform %type_ByteAddressBuffer
; CHECK-NEXT: %type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
; CHECK-NEXT: %_ptr_Uniform_type_RWByteAddressBuffer = OpTypePointer Uniform %type_RWByteAddressBuffer
; CHECK-NEXT:      %v3uint = OpTypeVector %uint 3
; CHECK-NEXT: %_ptr_Input_v3uint = OpTypePointer Input %v3uint
; CHECK-NEXT:        %void = OpTypeVoid
; CHECK-NEXT:          %19 = OpTypeFunction %void
; CHECK-NEXT:         %int = OpTypeInt 32 1
; CHECK-NEXT: %dx_types_ResRet_i32 = OpTypeStruct %int %int %int %int %int
; CHECK-NEXT: %_ptr_Function_dx_types_ResRet_i32 = OpTypePointer Function %dx_types_ResRet_i32
; CHECK-NEXT: %_ptr_Input_uint = OpTypePointer Input %uint
; CHECK-NEXT: %_ptr_Uniform_uint = OpTypePointer Uniform %uint
; CHECK-NEXT: %_ptr_Function_int = OpTypePointer Function %int
; CHECK-NEXT:           %3 = OpVariable %_ptr_Uniform_type_ByteAddressBuffer Uniform
; CHECK-NEXT:           %4 = OpVariable %_ptr_Uniform_type_RWByteAddressBuffer Uniform
; CHECK-NEXT: %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input
; CHECK-NEXT:        %main = OpFunction %void None %19
; CHECK-NEXT:          %20 = OpLabel
; CHECK-NEXT:          %24 = OpVariable %_ptr_Function_dx_types_ResRet_i32 Function
; CHECK-NEXT:          %26 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0
; CHECK-NEXT:          %27 = OpLoad %uint %26
; CHECK-NEXT:          %28 = OpShiftLeftLogical %uint %27 %uint_2
; CHECK-NEXT:          %29 = OpIAdd %uint %28 %uint_0
; CHECK-NEXT:          %31 = OpAccessChain %_ptr_Uniform_uint %3 %uint_0 %29
; CHECK-NEXT:          %32 = OpLoad %uint %31
; CHECK-NEXT:          %34 = OpAccessChain %_ptr_Function_int %24 %uint_0
; CHECK-NEXT:          %35 = OpBitcast %int %32
; CHECK-NEXT:                OpStore %34 %35
; CHECK-NEXT:          %36 = OpIAdd %uint %28 %uint_1
; CHECK-NEXT:          %37 = OpAccessChain %_ptr_Uniform_uint %3 %uint_0 %36
; CHECK-NEXT:          %38 = OpLoad %uint %37
; CHECK-NEXT:          %39 = OpAccessChain %_ptr_Function_int %24 %uint_1
; CHECK-NEXT:          %40 = OpBitcast %int %38
; CHECK-NEXT:                OpStore %39 %40
; CHECK-NEXT:          %41 = OpIAdd %uint %28 %uint_2
; CHECK-NEXT:          %42 = OpAccessChain %_ptr_Uniform_uint %3 %uint_0 %41
; CHECK-NEXT:          %43 = OpLoad %uint %42
; CHECK-NEXT:          %44 = OpAccessChain %_ptr_Function_int %24 %uint_2
; CHECK-NEXT:          %45 = OpBitcast %int %43
; CHECK-NEXT:                OpStore %44 %45
; CHECK-NEXT:          %46 = OpIAdd %uint %28 %uint_3
; CHECK-NEXT:          %47 = OpAccessChain %_ptr_Uniform_uint %3 %uint_0 %46
; CHECK-NEXT:          %48 = OpLoad %uint %47
; CHECK-NEXT:          %49 = OpAccessChain %_ptr_Function_int %24 %uint_3
; CHECK-NEXT:          %50 = OpBitcast %int %48
; CHECK-NEXT:                OpStore %49 %50
; CHECK-NEXT:          %51 = OpIAdd %uint %28 %uint_4
; CHECK-NEXT:          %52 = OpAccessChain %_ptr_Uniform_uint %3 %uint_0 %51
; CHECK-NEXT:          %53 = OpLoad %uint %52
; CHECK-NEXT:          %54 = OpAccessChain %_ptr_Function_int %24 %uint_4
; CHECK-NEXT:          %55 = OpBitcast %int %53
; CHECK-NEXT:                OpStore %54 %55
; CHECK-NEXT:          %56 = OpAccessChain %_ptr_Function_int %24 %uint_0
; CHECK-NEXT:          %57 = OpAccessChain %_ptr_Uniform_uint %4 %uint_0 %28
; CHECK-NEXT:          %58 = OpBitcast %uint %56
; CHECK-NEXT:                OpStore %57 %58
; CHECK-NEXT:                OpReturn
; CHECK-NEXT:                OpFunctionEnd
