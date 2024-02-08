; RUN: %dxilver 1.7 | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%struct.RWByteAddressBuffer = type { i32 }

@"\01?BAB@@3URWByteAddressBuffer@@A" = external constant %dx.types.Handle, align 4

; Ensure min shader target incorporates optional features used

; RDAT: FunctionTable[{{.*}}] = {

; SM 6.6+

;/////////////////////////////////////////////////////////////////////////////
; ShaderFeatureInfo_DerivativesInMeshAndAmpShaders (0x1000000) = 16777216
; OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256

; OptFeatureInfo_UsesDerivatives Flag used to indicate derivative use in
; functions, then fixed up for entry functions.
; Val. ver. 1.8 required to recursively check called functions.

; RDAT-LABEL: UnmangledName: "deriv_in_func"
; RDAT: FeatureInfo1: 0
; OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
; RDAT18:   FeatureInfo2: 256
; Old: deriv use not tracked
; RDAT16:   FeatureInfo2: 0
; RDAT17:   FeatureInfo2: 0
; Pixel(0), Compute(5), Library(6), Mesh(13), Amplification(14), Node(15) = 0xE061 = 57441
; RDAT18: ShaderStageFlag: 57441
; Old would not report Compute, Mesh, Amplification, or Node compatibility.
; Pixel(0), Library(6) = 0x41 = 65
; RDAT16: ShaderStageFlag: 65
; RDAT17: ShaderStageFlag: 65
; MinShaderTarget: (Library(6) << 16) + (SM 6.0 ((6 << 4) + 0)) = 0x60060 = 393312
; RDAT17: MinShaderTarget: 393312
; RDAT18: MinShaderTarget: 393312
; Old: Didn't set min target properly for lib function
; RDAT16: MinShaderTarget: 393318

; Function Attrs: noinline nounwind
define void @"\01?deriv_in_func@@YAXV?$vector@M$01@@@Z"(<2 x float> %uv) #0 {
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

; RDAT-LABEL: UnmangledName: "deriv_in_mesh"
; ShaderFeatureInfo_DerivativesInMeshAndAmpShaders (0x1000000) = 16777216
; RDAT18: FeatureInfo1: 16777216
; Old: missed called function
; RDAT16: FeatureInfo1: 0
; RDAT17: FeatureInfo1: 0
; OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
; RDAT18:   FeatureInfo2: 256
; Old: deriv use not tracked
; RDAT16:   FeatureInfo2: 0
; RDAT17:   FeatureInfo2: 0
; Mesh(13) = 0x2000 = 8192
; RDAT: ShaderStageFlag: 8192
; MinShaderTarget: (Mesh(13) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0xD0066 = 852070
; RDAT18: MinShaderTarget: 852070
; Old: 6.0
; RDAT16: MinShaderTarget: 852064
; RDAT17: MinShaderTarget: 852064

define void @deriv_in_mesh() {
  %1 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %2 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %3 = uitofp i32 %1 to float
  %4 = uitofp i32 %2 to float
  %5 = fmul fast float %3, 1.250000e-01
  %6 = fmul fast float %4, 1.250000e-01
  %7 = insertelement <2 x float> undef, float %5, i32 0
  %8 = insertelement <2 x float> %7, float %6, i32 1
  call void @"\01?deriv_in_func@@YAXV?$vector@M$01@@@Z"(<2 x float> %8)
  ret void
}

; RDAT-LABEL: UnmangledName: "deriv_in_compute"
; RDAT:   FeatureInfo1: 0
; OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
; RDAT18:   FeatureInfo2: 256
; Old: deriv use not tracked
; RDAT16:   FeatureInfo2: 0
; RDAT17:   FeatureInfo2: 0
; MinShaderTarget: (Compute(5) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x50066 = 327782
; Compute(5) = 0x20 = 32
; RDAT: ShaderStageFlag: 32
; RDAT18: MinShaderTarget: 327782
; Old: 6.0
; RDAT16: MinShaderTarget: 327776
; RDAT17: MinShaderTarget: 327776

define void @deriv_in_compute() {
  %1 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %2 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %3 = uitofp i32 %1 to float
  %4 = uitofp i32 %2 to float
  %5 = fmul fast float %3, 1.250000e-01
  %6 = fmul fast float %4, 1.250000e-01
  %7 = insertelement <2 x float> undef, float %5, i32 0
  %8 = insertelement <2 x float> %7, float %6, i32 1
  call void @"\01?deriv_in_func@@YAXV?$vector@M$01@@@Z"(<2 x float> %8)
  ret void
}

; RDAT-LABEL: UnmangledName: "deriv_in_pixel"
; RDAT:   FeatureInfo1: 0
; OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
; RDAT18:   FeatureInfo2: 256
; Old: deriv use not tracked
; RDAT16:   FeatureInfo2: 0
; RDAT17:   FeatureInfo2: 0
; Pixel(0) = 0x1 = 1
; RDAT: ShaderStageFlag: 1
; MinShaderTarget: (Pixel(0) << 16) + (SM 6.0 ((6 << 4) + 0)) = 0x60 = 96
; RDAT: MinShaderTarget: 96

define void @deriv_in_pixel() {
  %1 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %2 = insertelement <2 x float> undef, float %1, i64 0
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = insertelement <2 x float> %2, float %3, i64 1
  call void @"\01?deriv_in_func@@YAXV?$vector@M$01@@@Z"(<2 x float> %4)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.rawBufferStore.f32(i32, %dx.types.Handle, i32, i32, float, float, float, float, i8, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #1

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
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.typeAnnotations = !{!7}
!dx.entryPoints = !{!15, !17, !21, !24}

!0 = !{!"dxc(private) 1.7.0.4429 (rdat-minsm-check-flags, 9d3b6ba57)"}
!1 = !{i32 1, i32 7}
!2 = !{i32 1, i32 7}
!3 = !{!"lib", i32 6, i32 7}
!4 = !{null, !5, null, null}
!5 = !{!6}
!6 = !{i32 0, %struct.RWByteAddressBuffer* bitcast (%dx.types.Handle* @"\01?BAB@@3URWByteAddressBuffer@@A" to %struct.RWByteAddressBuffer*), !"BAB", i32 0, i32 1, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!7 = !{i32 1, void (<2 x float>)* @"\01?deriv_in_func@@YAXV?$vector@M$01@@@Z", !8, void ()* @deriv_in_mesh, !13, void ()* @deriv_in_compute, !13, void ()* @deriv_in_pixel, !13}
!8 = !{!9, !11}
!9 = !{i32 1, !10, !10}
!10 = !{}
!11 = !{i32 0, !12, !10}
!12 = !{i32 7, i32 9}
!13 = !{!14}
!14 = !{i32 0, !10, !10}
!15 = !{null, !"", null, !4, !16}
!16 = !{i32 0, i64 558882684944}
!17 = !{void ()* @deriv_in_compute, !"deriv_in_compute", null, null, !18}
!18 = !{i32 8, i32 5, i32 4, !19, i32 5, !20}
!19 = !{i32 8, i32 8, i32 1}
!20 = !{i32 0}
!21 = !{void ()* @deriv_in_mesh, !"deriv_in_mesh", null, null, !22}
!22 = !{i32 8, i32 13, i32 9, !23, i32 5, !20}
!23 = !{!19, i32 0, i32 0, i32 2, i32 0}
!24 = !{void ()* @deriv_in_pixel, !"deriv_in_pixel", !25, null, !28}
!25 = !{!26, null, null}
!26 = !{!27}
!27 = !{i32 0, !"TEXCOORD", i8 9, i8 0, !20, i8 2, i32 1, i8 2, i32 0, i8 0, null}
!28 = !{i32 8, i32 0, i32 5, !20}

; Make sure function-level derivative flag isn't in RequiredFeatureFlags,
; and make sure mesh shader sets required flag.

; RDAT-LABEL: ID3D12LibraryReflection:

; RDAT-LABEL: D3D12_FUNCTION_DESC: Name:
; RDAT-SAME: deriv_in_func
; RDAT: RequiredFeatureFlags: 0

; RDAT-LABEL: D3D12_FUNCTION_DESC: Name: deriv_in_compute
; RDAT: RequiredFeatureFlags: 0

; RDAT-LABEL: D3D12_FUNCTION_DESC: Name: deriv_in_mesh
; ShaderFeatureInfo_DerivativesInMeshAndAmpShaders (0x1000000) = 16777216
; RDAT18: RequiredFeatureFlags: 0x1000000
; Old: missed called function
; RDAT16: RequiredFeatureFlags: 0
; RDAT17: RequiredFeatureFlags: 0

; RDAT-LABEL: D3D12_FUNCTION_DESC: Name: deriv_in_pixel
; RDAT: RequiredFeatureFlags: 0
