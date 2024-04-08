; RUN: %dxilver 1.8 | %dxv %s
; This test's content is from the compilation output of 
; \tools\clang\test\HLSLFileCheck\d3dreflect\rdat_mintarget\sm69_mesh_node_deriv.hlsl
; compiling with -T lib_6_9. 
; The purpose of the test is to detect validation errors when using 
; derivative operations on node shaders with the mesh launch type.
; No diagnostics are expected, because derivatives are expected to be allowed in mesh nodes.

; Ensure that categories of deriv ops are allowed for mesh node shaders.
; Ensure that the OptFeatureInfo_UsesDerivatives flag is set as well.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%struct.RWByteAddressBuffer = type { i32 }
%struct.SamplerState = type { i32 }

@"\01?T2D@@3V?$Texture2D@V?$vector@M$03@@@@A" = external constant %dx.types.Handle, align 4
@"\01?Samp@@3USamplerState@@A" = external constant %dx.types.Handle, align 4
@"\01?BAB@@3URWByteAddressBuffer@@A" = external constant %dx.types.Handle, align 4

define void @node_deriv() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %2 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %3 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %4 = uitofp i32 %2 to float
  %5 = uitofp i32 %3 to float
  %6 = fmul fast float %4, 2.500000e-01
  %7 = fmul fast float %5, 2.500000e-01
  %8 = call float @dx.op.unary.f32(i32 83, float %6)  ; DerivCoarseX(value)
  %9 = call float @dx.op.unary.f32(i32 83, float %7)  ; DerivCoarseX(value)
  %10 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %11 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %11, i32 0, i32 undef, float %8, float %9, float undef, float undef, i8 3, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

; Function Attrs: noinline nounwind
define void @"\01?use_deriv@@YAXV?$vector@M$01@@@Z"(<2 x float> %uv) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %2 = extractelement <2 x float> %uv, i64 0
  %3 = extractelement <2 x float> %uv, i64 1
  %4 = call float @dx.op.unary.f32(i32 83, float %2)  ; DerivCoarseX(value)
  %5 = call float @dx.op.unary.f32(i32 83, float %3)  ; DerivCoarseX(value)
  %6 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %7 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %7, i32 0, i32 undef, float %4, float %5, float undef, float undef, i8 3, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

define void @node_deriv_in_call() {
  %1 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %2 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %3 = uitofp i32 %1 to float
  %4 = uitofp i32 %2 to float
  %5 = fmul fast float %3, 2.500000e-01
  %6 = fmul fast float %4, 2.500000e-01
  %7 = insertelement <2 x float> undef, float %5, i32 0
  %8 = insertelement <2 x float> %7, float %6, i32 1
  call void @"\01?use_deriv@@YAXV?$vector@M$01@@@Z"(<2 x float> %8)
  ret void
}

define void @node_calclod() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?Samp@@3USamplerState@@A", align 4
  %2 = load %dx.types.Handle, %dx.types.Handle* @"\01?T2D@@3V?$Texture2D@V?$vector@M$03@@@@A", align 4
  %3 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %4 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %5 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %6 = uitofp i32 %4 to float
  %7 = uitofp i32 %5 to float
  %8 = fmul fast float %6, 2.500000e-01
  %9 = fmul fast float %7, 2.500000e-01
  %10 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %2)  ; CreateHandleForLib(Resource)
  %11 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %12 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %13 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %14 = call float @dx.op.calculateLOD.f32(i32 81, %dx.types.Handle %11, %dx.types.Handle %13, float %8, float %9, float undef, i1 true)  ; CalculateLOD(handle,sampler,coord0,coord1,coord2,clamped)
  %15 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %3)  ; CreateHandleForLib(Resource)
  %16 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %15, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %16, i32 0, i32 undef, float %14, float undef, float undef, float undef, i8 1, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

; Function Attrs: noinline nounwind
define void @"\01?use_calclod@@YAXV?$vector@M$01@@@Z"(<2 x float> %uv) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?Samp@@3USamplerState@@A", align 4
  %2 = load %dx.types.Handle, %dx.types.Handle* @"\01?T2D@@3V?$Texture2D@V?$vector@M$03@@@@A", align 4
  %3 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %4 = extractelement <2 x float> %uv, i64 0
  %5 = extractelement <2 x float> %uv, i64 1
  %6 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %2)  ; CreateHandleForLib(Resource)
  %7 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %8 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %9 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %10 = call float @dx.op.calculateLOD.f32(i32 81, %dx.types.Handle %7, %dx.types.Handle %9, float %4, float %5, float undef, i1 true)  ; CalculateLOD(handle,sampler,coord0,coord1,coord2,clamped)
  %11 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %3)  ; CreateHandleForLib(Resource)
  %12 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %12, i32 0, i32 undef, float %10, float undef, float undef, float undef, i8 1, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

define void @node_calclod_in_call() {
  %1 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %2 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %3 = uitofp i32 %1 to float
  %4 = uitofp i32 %2 to float
  %5 = fmul fast float %3, 2.500000e-01
  %6 = fmul fast float %4, 2.500000e-01
  %7 = insertelement <2 x float> undef, float %5, i32 0
  %8 = insertelement <2 x float> %7, float %6, i32 1
  call void @"\01?use_calclod@@YAXV?$vector@M$01@@@Z"(<2 x float> %8)
  ret void
}

define void @node_sample() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?Samp@@3USamplerState@@A", align 4
  %2 = load %dx.types.Handle, %dx.types.Handle* @"\01?T2D@@3V?$Texture2D@V?$vector@M$03@@@@A", align 4
  %3 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %4 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %5 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %6 = uitofp i32 %4 to float
  %7 = uitofp i32 %5 to float
  %8 = fmul fast float %6, 2.500000e-01
  %9 = fmul fast float %7, 2.500000e-01
  %10 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %2)  ; CreateHandleForLib(Resource)
  %11 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %12 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %13 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %14 = call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 60, %dx.types.Handle %11, %dx.types.Handle %13, float %8, float %9, float undef, float undef, i32 0, i32 0, i32 undef, float undef)  ; Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
  %15 = extractvalue %dx.types.ResRet.f32 %14, 0
  %16 = extractvalue %dx.types.ResRet.f32 %14, 1
  %17 = extractvalue %dx.types.ResRet.f32 %14, 2
  %18 = extractvalue %dx.types.ResRet.f32 %14, 3
  %19 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %3)  ; CreateHandleForLib(Resource)
  %20 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %19, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %20, i32 0, i32 undef, float %15, float %16, float %17, float %18, i8 15, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

; Function Attrs: noinline nounwind
define void @"\01?use_sample@@YAXV?$vector@M$01@@@Z"(<2 x float> %uv) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?Samp@@3USamplerState@@A", align 4
  %2 = load %dx.types.Handle, %dx.types.Handle* @"\01?T2D@@3V?$Texture2D@V?$vector@M$03@@@@A", align 4
  %3 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %4 = extractelement <2 x float> %uv, i64 0
  %5 = extractelement <2 x float> %uv, i64 1
  %6 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %2)  ; CreateHandleForLib(Resource)
  %7 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %8 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %9 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %10 = call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 60, %dx.types.Handle %7, %dx.types.Handle %9, float %4, float %5, float undef, float undef, i32 0, i32 0, i32 undef, float undef)  ; Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
  %11 = extractvalue %dx.types.ResRet.f32 %10, 0
  %12 = extractvalue %dx.types.ResRet.f32 %10, 1
  %13 = extractvalue %dx.types.ResRet.f32 %10, 2
  %14 = extractvalue %dx.types.ResRet.f32 %10, 3
  %15 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %3)  ; CreateHandleForLib(Resource)
  %16 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %15, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %16, i32 0, i32 undef, float %11, float %12, float %13, float %14, i8 15, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

define void @node_sample_in_call() {
  %1 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %2 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %3 = uitofp i32 %1 to float
  %4 = uitofp i32 %2 to float
  %5 = fmul fast float %3, 2.500000e-01
  %6 = fmul fast float %4, 2.500000e-01
  %7 = insertelement <2 x float> undef, float %5, i32 0
  %8 = insertelement <2 x float> %7, float %6, i32 1
  call void @"\01?use_sample@@YAXV?$vector@M$01@@@Z"(<2 x float> %8)
  ret void
}

define void @node_quad() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %2 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %3 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %4 = uitofp i32 %2 to float
  %5 = uitofp i32 %3 to float
  %6 = fmul fast float %4, 2.500000e-01
  %7 = fmul fast float %5, 2.500000e-01
  %8 = call float @dx.op.quadOp.f32(i32 123, float %6, i8 2)  ; QuadOp(value,op)
  %9 = call float @dx.op.quadOp.f32(i32 123, float %7, i8 2)  ; QuadOp(value,op)
  %10 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %11 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %11, i32 0, i32 undef, float %8, float %9, float undef, float undef, i8 3, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

; Function Attrs: noinline nounwind
define void @"\01?use_quad@@YAXV?$vector@M$01@@@Z"(<2 x float> %uv) #0 {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A", align 4
  %2 = extractelement <2 x float> %uv, i64 0
  %3 = call float @dx.op.quadOp.f32(i32 123, float %2, i8 2)  ; QuadOp(value,op)
  %4 = extractelement <2 x float> %uv, i64 1
  %5 = call float @dx.op.quadOp.f32(i32 123, float %4, i8 2)  ; QuadOp(value,op)
  %6 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %7 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
  call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %7, i32 0, i32 undef, float %3, float %5, float undef, float undef, i8 3, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
  ret void
}

define void @node_quad_in_call() {
  %1 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
  %2 = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
  %3 = uitofp i32 %1 to float
  %4 = uitofp i32 %2 to float
  %5 = fmul fast float %3, 2.500000e-01
  %6 = fmul fast float %4, 2.500000e-01
  %7 = insertelement <2 x float> undef, float %5, i32 0
  %8 = insertelement <2 x float> %7, float %6, i32 1
  call void @"\01?use_quad@@YAXV?$vector@M$01@@@Z"(<2 x float> %8)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadIdInGroup.i32(i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #1

; Function Attrs: nounwind
declare void @dx.op.rawBufferStore.f32(i32, %dx.types.Handle, i32, i32, float, float, float, float, i8, i32) #2

; Function Attrs: nounwind readonly
declare float @dx.op.calculateLOD.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, i1) #3

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sample.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #3

; Function Attrs: nounwind
declare float @dx.op.quadOp.f32(i32, float, i8) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #1

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32, %dx.types.Handle) #3

attributes #0 = { noinline nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }
attributes #3 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!11}
!dx.entryPoints = !{!19, !21, !27, !30, !33, !36, !39, !42, !45}

!0 = !{!"dxc(private) 1.8.0.4467 (mesh-nodes-out-params, 278d6cb3bac9-dirty)"}
!1 = !{i32 1, i32 9}
!2 = !{!"lib", i32 6, i32 9}
!3 = !{!4, !7, null, !9}
!4 = !{!5}
!5 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* bitcast (%dx.types.Handle* @"\01?T2D@@3V?$Texture2D@V?$vector@M$03@@@@A" to %"class.Texture2D<vector<float, 4> >"*), !"T2D", i32 0, i32 0, i32 1, i32 2, i32 0, !6}
!6 = !{i32 0, i32 9}
!7 = !{!8}
!8 = !{i32 0, %struct.RWByteAddressBuffer* bitcast (%dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A" to %struct.RWByteAddressBuffer*), !"BAB", i32 0, i32 1, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!9 = !{!10}
!10 = !{i32 0, %struct.SamplerState* bitcast (%dx.types.Handle* @"\01?Samp@@3USamplerState@@A" to %struct.SamplerState*), !"Samp", i32 0, i32 0, i32 1, i32 0, null}
!11 = !{i32 1, void (<2 x float>)* @"\01?use_deriv@@YAXV?$vector@M$01@@@Z", !12, void (<2 x float>)* @"\01?use_calclod@@YAXV?$vector@M$01@@@Z", !12, void (<2 x float>)* @"\01?use_sample@@YAXV?$vector@M$01@@@Z", !12, void (<2 x float>)* @"\01?use_quad@@YAXV?$vector@M$01@@@Z", !12, void ()* @node_deriv, !17, void ()* @node_deriv_in_call, !17, void ()* @node_calclod, !17, void ()* @node_calclod_in_call, !17, void ()* @node_sample, !17, void ()* @node_sample_in_call, !17, void ()* @node_quad, !17, void ()* @node_quad_in_call, !17}
!12 = !{!13, !15}
!13 = !{i32 1, !14, !14}
!14 = !{}
!15 = !{i32 0, !16, !14}
!16 = !{i32 7, i32 9, i32 13, i32 2}
!17 = !{!18}
!18 = !{i32 0, !14, !14}
!19 = !{null, !"", null, !3, !20}
!20 = !{i32 0, i64 8590458896}
!21 = !{void ()* @node_calclod, !"node_calclod", null, null, !22}
!22 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !23, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!23 = !{!"node_calclod", i32 0}
!24 = !{i32 1, i32 1, i32 1}
!25 = !{i32 4, i32 4, i32 1}
!26 = !{i32 0}
!27 = !{void ()* @node_calclod_in_call, !"node_calclod_in_call", null, null, !28}
!28 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !29, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!29 = !{!"node_calclod_in_call", i32 0}
!30 = !{void ()* @node_deriv, !"node_deriv", null, null, !31}
!31 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !32, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!32 = !{!"node_deriv", i32 0}
!33 = !{void ()* @node_deriv_in_call, !"node_deriv_in_call", null, null, !34}
!34 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !35, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!35 = !{!"node_deriv_in_call", i32 0}
!36 = !{void ()* @node_quad, !"node_quad", null, null, !37}
!37 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !38, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!38 = !{!"node_quad", i32 0}
!39 = !{void ()* @node_quad_in_call, !"node_quad_in_call", null, null, !40}
!40 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !41, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!41 = !{!"node_quad_in_call", i32 0}
!42 = !{void ()* @node_sample, !"node_sample", null, null, !43}
!43 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !44, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!44 = !{!"node_sample", i32 0}
!45 = !{void ()* @node_sample_in_call, !"node_sample_in_call", null, null, !46}
!46 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !47, i32 16, i32 -1, i32 18, !24, i32 65536, i32 1, i32 4, !25, i32 5, !26}
!47 = !{!"node_sample_in_call", i32 0}