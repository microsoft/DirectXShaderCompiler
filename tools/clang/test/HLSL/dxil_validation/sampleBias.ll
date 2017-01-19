; RUN: %dxv %s | FileCheck %s

; CHECK: bias amount for sample_b must be in the range [-16.000000,15.990000], but 18.000000 was specified as an immediate

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.Texture2D = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"$Globals" = type { float }
%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%struct.SamplerState = type { i32 }

@"\01?bias@@3MA" = global float 0.000000e+00, align 4
@dx.typevar.0 = external addrspace(1) constant %class.Texture2D
@dx.typevar.1 = external addrspace(1) constant %"class.Texture2D<vector<float, 4> >::mips_type"
@dx.typevar.2 = external addrspace(1) constant %"$Globals"
@llvm.used = appending global [3 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%class.Texture2D addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"$Globals" addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<2 x float>, <4 x float>* nocapture readnone) #0 {
  %text1_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 0, i32 0, i32 3, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %samp1_sampler = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 3, i32 0, i32 5, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 59, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %6 = call %dx.types.ResRet.f32 @dx.op.sampleBias.f32(i32 63, %dx.types.Handle %text1_texture_2d, %dx.types.Handle %samp1_sampler, float %3, float %4, float undef, float undef, i32 undef, i32 undef, i32 undef, float 1.8000000e01, float undef)  ; SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
  %7 = extractvalue %dx.types.ResRet.f32 %6, 0
  %8 = extractvalue %dx.types.ResRet.f32 %6, 1
  %9 = extractvalue %dx.types.ResRet.f32 %6, 2
  %10 = extractvalue %dx.types.ResRet.f32 %6, 3
  %11 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 61, %dx.types.Handle %5, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %12 = extractvalue %dx.types.CBufRet.f32 %11, 0
  %13 = call %dx.types.ResRet.f32 @dx.op.sampleBias.f32(i32 63, %dx.types.Handle %text1_texture_2d, %dx.types.Handle %samp1_sampler, float %3, float %4, float undef, float undef, i32 -5, i32 7, i32 undef, float %12, float undef)  ; SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
  %14 = extractvalue %dx.types.ResRet.f32 %13, 0
  %15 = extractvalue %dx.types.ResRet.f32 %13, 1
  %16 = extractvalue %dx.types.ResRet.f32 %13, 2
  %17 = extractvalue %dx.types.ResRet.f32 %13, 3
  %.i0 = fadd fast float %14, %7
  %.i1 = fadd fast float %15, %8
  %.i2 = fadd fast float %16, %9
  %.i3 = fadd fast float %17, %10
  %18 = call %dx.types.ResRet.f32 @dx.op.sampleBias.f32(i32 63, %dx.types.Handle %text1_texture_2d, %dx.types.Handle %samp1_sampler, float %3, float %4, float undef, float undef, i32 -4, i32 1, i32 undef, float %12, float 1.8000000e01)  ; SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
  %19 = extractvalue %dx.types.ResRet.f32 %18, 0
  %20 = extractvalue %dx.types.ResRet.f32 %18, 1
  %21 = extractvalue %dx.types.ResRet.f32 %18, 2
  %22 = extractvalue %dx.types.ResRet.f32 %18, 3
  %.i01 = fadd fast float %.i0, %19
  %.i12 = fadd fast float %.i1, %20
  %.i23 = fadd fast float %.i2, %21
  %.i34 = fadd fast float %.i3, %22
  %23 = call %dx.types.ResRet.f32 @dx.op.sampleBias.f32(i32 63, %dx.types.Handle %text1_texture_2d, %dx.types.Handle %samp1_sampler, float %3, float %4, float undef, float undef, i32 -3, i32 2, i32 undef, float %12, float 0.000000e+00)  ; SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
  %24 = extractvalue %dx.types.ResRet.f32 %23, 0
  %25 = extractvalue %dx.types.ResRet.f32 %23, 1
  %26 = extractvalue %dx.types.ResRet.f32 %23, 2
  %27 = extractvalue %dx.types.ResRet.f32 %23, 3
  %28 = extractvalue %dx.types.ResRet.f32 %23, 4
  %.i05 = fadd fast float %.i01, %24
  %.i16 = fadd fast float %.i12, %25
  %.i27 = fadd fast float %.i23, %26
  %.i38 = fadd fast float %.i34, %27
  %29 = uitofp i32 %28 to float
  %.i09 = fadd fast float %.i05, %29
  %.i110 = fadd fast float %.i16, %29
  %.i211 = fadd fast float %.i27, %29
  %.i312 = fadd fast float %.i38, %29
  %30 = call %dx.types.ResRet.f32 @dx.op.sampleBias.f32(i32 63, %dx.types.Handle %text1_texture_2d, %dx.types.Handle %samp1_sampler, float %3, float %4, float undef, float undef, i32 -3, i32 2, i32 undef, float %12, float %3)  ; SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
  %31 = extractvalue %dx.types.ResRet.f32 %30, 0
  %32 = extractvalue %dx.types.ResRet.f32 %30, 1
  %33 = extractvalue %dx.types.ResRet.f32 %30, 2
  %34 = extractvalue %dx.types.ResRet.f32 %30, 3
  %35 = extractvalue %dx.types.ResRet.f32 %30, 4
  %.i013 = fadd fast float %.i09, %31
  %.i114 = fadd fast float %.i110, %32
  %.i215 = fadd fast float %.i211, %33
  %.i316 = fadd fast float %.i312, %34
  %36 = uitofp i32 %35 to float
  %.i017 = fadd fast float %.i013, %36
  %.i118 = fadd fast float %.i114, %36
  %.i219 = fadd fast float %.i215, %36
  %.i320 = fadd fast float %.i316, %36
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %.i017)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %.i118)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %.i219)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %.i320)  ; StoreOutput(outputtSigId,rowIndex,colIndex,value)
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
declare %dx.types.ResRet.f32 @dx.op.sampleBias.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float, float) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.valver = !{!1}
!dx.version = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.typeAnnotations = !{!12, !20}
!dx.entryPoints = !{!29}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 0}
!3 = !{!"ps", i32 6, i32 0}
!4 = !{!5, null, !8, !10}
!5 = !{!6}
!6 = !{i32 0, %class.Texture2D* undef, !"text1", i32 0, i32 3, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{!9}
!9 = !{i32 0, %"$Globals"* undef, !"$Globals", i32 0, i32 0, i32 1, i32 4, null}
!10 = !{!11}
!11 = !{i32 0, %struct.SamplerState* undef, !"samp1", i32 0, i32 5, i32 1, i32 0, null}
!12 = !{i32 0, %class.Texture2D addrspace(1)* @dx.typevar.0, !13, %"class.Texture2D<vector<float, 4> >::mips_type" addrspace(1)* @dx.typevar.1, !16, %"$Globals" addrspace(1)* @dx.typevar.2, !18}
!13 = !{i32 20, !14, !15}
!14 = !{i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!15 = !{i32 3, i32 16, i32 6, !"mips"}
!16 = !{i32 4, !17}
!17 = !{i32 3, i32 0, i32 6, !"handle", i32 7, i32 5}
!18 = !{i32 0, !19}
!19 = !{i32 3, i32 0, i32 6, !"bias", i32 7, i32 9}
!20 = !{i32 1, void (<2 x float>, <4 x float>*)* @main.flat, !21}
!21 = !{!22, !24, !27}
!22 = !{i32 0, !23, !23}
!23 = !{}
!24 = !{i32 0, !25, !26}
!25 = !{i32 4, !"A", i32 7, i32 9}
!26 = !{i32 0}
!27 = !{i32 1, !28, !26}
!28 = !{i32 4, !"SV_Target", i32 7, i32 9}
!29 = !{void (<2 x float>, <4 x float>*)* @main.flat, !"main", !30, !4, null}
!30 = !{!31, !33, null}
!31 = !{!32}
!32 = !{i32 0, !"A", i8 9, i8 0, !26, i8 2, i32 1, i8 2, i32 0, i8 0, null}
!33 = !{!34}
!34 = !{i32 0, !"SV_Target", i8 9, i8 16, !26, i8 0, i32 1, i8 4, i32 0, i8 0, null}

