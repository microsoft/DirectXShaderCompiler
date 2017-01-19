; RUN: %dxv %s | FileCheck %s

; CHECK: Invalid sampler mode on sampler 'g_samLinear'
; CHECK: Invalid sampler mode on sampler 'g_samLinearC'
; CHECK: Type 'st' is a struct type but is used as a parameter in function 'main.flat'
; CHECK: sample_c_*/gather_c instructions require sampler declared in comparison mode
; CHECK: sample, lod and gather should on srv resource
; CHECK: lod instruction requires sampler declared in default mode
; CHECK: sample, lod and gather should on srv resource
; CHECK: sample/_l/_d/_cl_s/gather instruction requires sampler declared in default mode
; CHECK: sample, lod and gather should on srv resource
; CHECK: sample/_l/_d/_cl_s/gather instruction requires sampler declared in default mode
; CHECK: sample, lod and gather should on srv resource
; CHECK: sample_c_*/gather_c instructions require sampler declared in comparison mode
; CHECK: sample, lod and gather should on srv resource

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.Texture2D = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%class.RWTexture2D = type { <4 x float> }
%struct.PS_INPUT = type { <3 x float>, <2 x float> }
%"$Globals" = type { float }
%cbPerFrame = type { <3 x float>, float }
%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%struct.SamplerState = type { i32 }
%struct.SamplerComparisonState = type { i32 }

@"\01?cmpVal@@3MA" = global float 0.000000e+00, align 4
@dx.typevar.0 = external addrspace(1) constant %class.Texture2D
@dx.typevar.1 = external addrspace(1) constant %"class.Texture2D<vector<float, 4> >::mips_type"
@dx.typevar.2 = external addrspace(1) constant %class.RWTexture2D
@dx.typevar.3 = external addrspace(1) constant %struct.PS_INPUT
@dx.typevar.4 = external addrspace(1) constant %"$Globals"
@dx.typevar.5 = external addrspace(1) constant %cbPerFrame
@llvm.used = appending global [6 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%class.Texture2D addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.RWTexture2D addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.PS_INPUT addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"$Globals" addrspace(1)* @dx.typevar.4 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%cbPerFrame addrspace(1)* @dx.typevar.5 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<3 x float>* nocapture readnone, <2 x float>* nocapture readnone, <4 x float>* nocapture readnone, %struct.PS_INPUT * %st) #0 {
entry:
  %uav1_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 1, i32 0, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %g_txDiffuse_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 1, i32 0, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %g_samLinearC_sampler = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 3, i32 1, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %g_samLinear_sampler = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 3, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 2, i32 1, i32 1, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 62, %dx.types.Handle %g_txDiffuse_texture_2d, %dx.types.Handle %g_samLinear_sampler, float %5, float %6, float undef, float undef, i32 undef, i32 undef, i32 undef, float undef)  ; Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
  %8 = extractvalue %dx.types.ResRet.f32 %7, 0
  %9 = extractvalue %dx.types.ResRet.f32 %7, 1
  %10 = extractvalue %dx.types.ResRet.f32 %7, 2
  %11 = extractvalue %dx.types.ResRet.f32 %7, 3
  %12 = call float @dx.op.calculateLOD.f32(i32 83, %dx.types.Handle %g_txDiffuse_texture_2d, %dx.types.Handle %g_samLinear_sampler, float %5, float %6, float undef, i1 true)  ; CalculateLOD(handle,sampler,coord0,coord1,coord2,clamped)
  %add.i0 = fadd fast float %8, %12
  %add.i1 = fadd fast float %9, %12
  %add.i2 = fadd fast float %10, %12
  %add.i3 = fadd fast float %11, %12
  %13 = call %dx.types.ResRet.f32 @dx.op.textureGather.f32(i32 75, %dx.types.Handle %g_txDiffuse_texture_2d, %dx.types.Handle %g_samLinear_sampler, float %5, float %6, float undef, float undef, i32 undef, i32 undef, i32 0)  ; TextureGather(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel)
  %14 = extractvalue %dx.types.ResRet.f32 %13, 0
  %15 = extractvalue %dx.types.ResRet.f32 %13, 1
  %16 = extractvalue %dx.types.ResRet.f32 %13, 2
  %17 = extractvalue %dx.types.ResRet.f32 %13, 3
  %add5.i0 = fadd fast float %add.i0, %14
  %add5.i1 = fadd fast float %add.i1, %15
  %add5.i2 = fadd fast float %add.i2, %16
  %add5.i3 = fadd fast float %add.i3, %17
  %18 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 61, %dx.types.Handle %4, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %19 = extractvalue %dx.types.CBufRet.f32 %18, 0
  %20 = call %dx.types.ResRet.f32 @dx.op.sampleCmp.f32(i32 66, %dx.types.Handle %g_txDiffuse_texture_2d, %dx.types.Handle %g_samLinearC_sampler, float %5, float %6, float undef, float undef, i32 undef, i32 undef, i32 undef, float %19, float undef)  ; SampleCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,clamp)
  %21 = extractvalue %dx.types.ResRet.f32 %20, 0
  %add10.i0 = fadd fast float %add5.i0, %21
  %add10.i1 = fadd fast float %add5.i1, %21
  %add10.i2 = fadd fast float %add5.i2, %21
  %add10.i3 = fadd fast float %add5.i3, %21
  %22 = call %dx.types.ResRet.f32 @dx.op.textureGatherCmp.f32(i32 76, %dx.types.Handle %g_txDiffuse_texture_2d, %dx.types.Handle %g_samLinearC_sampler, float %5, float %6, float undef, float undef, i32 undef, i32 undef, i32 0, float %19)  ; TextureGatherCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel,compareVale)
  %23 = extractvalue %dx.types.ResRet.f32 %22, 0
  %24 = extractvalue %dx.types.ResRet.f32 %22, 1
  %25 = extractvalue %dx.types.ResRet.f32 %22, 2
  %26 = extractvalue %dx.types.ResRet.f32 %22, 3
  %add13.i0 = fadd fast float %add10.i0, %23
  %add13.i1 = fadd fast float %add10.i1, %24
  %add13.i2 = fadd fast float %add10.i2, %25
  %add13.i3 = fadd fast float %add10.i3, %26
  %27 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %28 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %29 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %30 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 61, %dx.types.Handle %3, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %31 = extractvalue %dx.types.CBufRet.f32 %30, 0
  %32 = extractvalue %dx.types.CBufRet.f32 %30, 1
  %33 = extractvalue %dx.types.CBufRet.f32 %30, 2
  %34 = call float @dx.op.dot3.f32(i32 57, float %31, float %32, float %33, float %27, float %28, float %29)  ; Dot3(ax,ay,az,bx,by,bz)
  %Saturate = call float @dx.op.unary.f32(i32 7, float %34)  ; Saturate(value)
  %35 = extractvalue %dx.types.CBufRet.f32 %30, 3
  %FMax = call float @dx.op.binary.f32(i32 35, float %Saturate, float %35)  ; FMax(a,b)
  %mul.i0 = fmul fast float %FMax, %add13.i0
  %mul.i1 = fmul fast float %FMax, %add13.i1
  %mul.i2 = fmul fast float %FMax, %add13.i2
  %mul.i3 = fmul fast float %FMax, %add13.i3
  %TextureLoad = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 68, %dx.types.Handle %uav1_UAV_2d, i32 undef, i32 0, i32 0, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %36 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 0
  %37 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 1
  %38 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 2
  %39 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 3
  %mul20.i0 = fmul fast float %mul.i0, %36
  %mul20.i1 = fmul fast float %mul.i1, %37
  %mul20.i2 = fmul fast float %mul.i2, %38
  %mul20.i3 = fmul fast float %mul.i3, %39
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %mul20.i0)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %mul20.i1)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %mul20.i2)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %mul20.i3)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #2

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureGatherCmp.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #2

; Function Attrs: nounwind readonly
declare float @dx.op.calculateLOD.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, i1) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureGather.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sample.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleCmp.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float, float) #2

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.valver = !{!1}
!dx.version = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.typeAnnotations = !{!16, !31}
!dx.entryPoints = !{!42}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 0}
!3 = !{!"ps", i32 6, i32 0}
!4 = !{!5, !8, !10, !13}
!5 = !{!6}
!6 = !{i32 0, %class.Texture2D* undef, !"g_txDiffuse", i32 0, i32 0, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{!9}
!9 = !{i32 0, %class.RWTexture2D* undef, !"uav1", i32 0, i32 3, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!10 = !{!11, !12}
!11 = !{i32 0, %"$Globals"* undef, !"$Globals", i32 0, i32 0, i32 1, i32 4, null}
!12 = !{i32 1, %cbPerFrame* undef, !"cbPerFrame", i32 0, i32 1, i32 1, i32 16, null}
!13 = !{!14, !15}
!14 = !{i32 0, %struct.SamplerState* undef, !"g_samLinear", i32 0, i32 0, i32 1, i32 3, null}
!15 = !{i32 1, %struct.SamplerComparisonState* undef, !"g_samLinearC", i32 0, i32 1, i32 1, i32 3, null}
!16 = !{i32 0, %class.Texture2D addrspace(1)* @dx.typevar.0, !17, %"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1, !20, %class.RWTexture2D addrspace(1)* @dx.typevar.2, !22, %struct.PS_INPUT addrspace(1)* @dx.typevar.3, !23, %"$Globals" addrspace(1)* @dx.typevar.4, !26, %cbPerFrame addrspace(1)* @dx.typevar.5, !28}
!17 = !{i32 20, !18, !19}
!18 = !{i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!19 = !{i32 3, i32 16, i32 6, !"mips"}
!20 = !{i32 4, !21}
!21 = !{i32 3, i32 0, i32 6, !"handle", i32 7, i32 5}
!22 = !{i32 16, !18}
!23 = !{i32 24, !24, !25}
!24 = !{i32 3, i32 0, i32 4, !"NORMAL", i32 5, i32 6, i32 6, !"vNormal", i32 7, i32 9}
!25 = !{i32 3, i32 16, i32 4, !"TEXCOORD0", i32 5, i32 4, i32 6, !"vTexcoord", i32 7, i32 9}
!26 = !{i32 0, !27}
!27 = !{i32 3, i32 0, i32 6, !"cmpVal", i32 7, i32 9}
!28 = !{i32 0, !29, !30}
!29 = !{i32 3, i32 0, i32 6, !"g_vLightDir", i32 7, i32 9}
!30 = !{i32 3, i32 12, i32 6, !"g_fAmbient", i32 7, i32 9}
!31 = !{i32 1, void (<3 x float>*, <2 x float>*, <4 x float>*, %struct.PS_INPUT * )* @main.flat, !32}
!32 = !{!33, !35, !38, !40, !40}
!33 = !{i32 0, !34, !34}
!34 = !{}
!35 = !{i32 0, !36, !37}
!36 = !{i32 4, !"NORMAL", i32 5, i32 6, i32 7, i32 9}
!37 = !{i32 0}
!38 = !{i32 0, !39, !37}
!39 = !{i32 4, !"TEXCOORD0", i32 5, i32 4, i32 7, i32 9}
!40 = !{i32 1, !41, !37}
!41 = !{i32 4, !"SV_TARGET", i32 7, i32 9}
!42 = !{void (<3 x float>*, <2 x float>*, <4 x float>*, %struct.PS_INPUT * )* @main.flat, !"main", !43, !4, !49}
!43 = !{!44, !47, null}
!44 = !{!45, !46}
!45 = !{i32 0, !"NORMAL", i8 9, i8 0, !37, i8 6, i32 1, i8 3, i32 0, i8 0, null}
!46 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !37, i8 4, i32 1, i8 2, i32 1, i8 0, null}
!47 = !{!48}
!48 = !{i32 0, !"SV_Target", i8 9, i8 16, !37, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!49 = !{i32 0, i64 8192}
