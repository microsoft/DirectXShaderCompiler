; RUN: %dxv %s | FileCheck %s

; CHECK: uav load don't support offset
; CHECK: uav load don't support mipLevel/sampleIndex
; CHECK: store on typed uav must write to all four components of the UAV
; CHECK: sync in a non-Compute Shader must only sync UAV (sync_uglobal)


target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%class.RWTexture2D = type { <4 x float> }
%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }

@"\01?uav1@@3V?$RWTexture2D@V?$vector@M$03@@@@A" = available_externally global %class.RWTexture2D zeroinitializer, align 4
@dx.typevar.0 = external addrspace(1) constant %class.RWTexture2D
@llvm.used = appending global [2 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%class.RWTexture2D addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* bitcast (%class.RWTexture2D* @"\01?uav1@@3V?$RWTexture2D@V?$vector@M$03@@@@A" to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<2 x i32>, <2 x i32>, <4 x float>* nocapture readnone) #0 {
entry:
  %uav1_UAV_2d = tail call %dx.types.Handle @dx.op.createHandle(i32 58, i8 1, i32 0, i32 0, i1 false)
  %3 = tail call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %4 = tail call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 1, i32 undef)
  %5 = tail call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %6 = tail call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %TextureLoad = tail call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 67, %dx.types.Handle %uav1_UAV_2d, i32 %3, i32 %3, i32 %4, i32 %3, i32 undef, i32 %3, i32 undef)
  %7 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 0
  %8 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 1
  %9 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 2
  %10 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 3
  tail call void @dx.op.barrier(i32 83, i32 9)
  %TextureLoad1 = tail call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 67, %dx.types.Handle %uav1_UAV_2d, i32 undef, i32 %5, i32 %6, i32 undef, i32 undef, i32 undef, i32 undef)
  %11 = extractvalue %dx.types.ResRet.f32 %TextureLoad1, 0
  %12 = extractvalue %dx.types.ResRet.f32 %TextureLoad1, 1
  %13 = extractvalue %dx.types.ResRet.f32 %TextureLoad1, 2
  %14 = extractvalue %dx.types.ResRet.f32 %TextureLoad1, 3
  %15 = extractvalue %dx.types.ResRet.f32 %TextureLoad1, 4
  %conv = uitofp i32 %15 to float
  %factor = fmul fast float %conv, 2.000000e+00
  %add4.i0 = fadd fast float %11, %7
  %add9.i0 = fadd fast float %add4.i0, %factor
  %factor4 = fmul fast float %conv, 2.000000e+00
  %add4.i1 = fadd fast float %12, %8
  %add9.i1 = fadd fast float %add4.i1, %factor4
  %factor5 = fmul fast float %conv, 2.000000e+00
  %add4.i2 = fadd fast float %13, %9
  %add9.i2 = fadd fast float %add4.i2, %factor5
  %factor6 = fmul fast float %conv, 2.000000e+00
  %add4.i3 = fadd fast float %14, %10
  %add9.i3 = fadd fast float %add4.i3, %factor6
  tail call void @dx.op.barrier(i32 83, i32 2)
  tail call void @dx.op.textureStore.f32(i32 68, %dx.types.Handle %uav1_UAV_2d, i32 %3, i32 %4, i32 undef, float %add9.i0, float %add9.i1, float %add9.i2, float undef, i8 7)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %add9.i0)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %add9.i1)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %add9.i2)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %add9.i3)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #2

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #0

; Function Attrs: nounwind
declare void @dx.op.barrier(i32, i32) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!7, !10}
!dx.entryPoints = !{!21}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 6, i32 0}
!3 = !{null, !4, null, null}
!4 = !{!5}
!5 = !{i32 0, %class.RWTexture2D* @"\01?uav1@@3V?$RWTexture2D@V?$vector@M$03@@@@A", !"uav1", i32 0, i32 3, i32 1, i32 2, i1 false, i1 false, i1 false, !6}
!6 = !{i32 0, i32 9}
!7 = !{i32 0, %class.RWTexture2D addrspace(1)* @dx.typevar.0, !8}
!8 = !{i32 16, !9}
!9 = !{i32 3, i32 0, i32 6, !"h", i32 7, i32 9}
!10 = !{i32 1, void (<2 x i32>, <2 x i32>, <4 x float>*)* @main.flat, !11}
!11 = !{!12, !14, !17, !19}
!12 = !{i32 0, !13, !13}
!13 = !{}
!14 = !{i32 0, !15, !16}
!15 = !{i32 4, !"A", i32 7, i32 5}
!16 = !{i32 0}
!17 = !{i32 0, !18, !16}
!18 = !{i32 4, !"B", i32 7, i32 5}
!19 = !{i32 1, !20, !16}
!20 = !{i32 4, !"SV_Target", i32 7, i32 9}
!21 = !{void (<2 x i32>, <2 x i32>, <4 x float>*)* @main.flat, !"", !22, !3, !28}
!22 = !{!23, !26, null}
!23 = !{!24, !25}
!24 = !{i32 0, !"A", i8 5, i8 0, !16, i8 1, i32 1, i8 2, i32 0, i8 0, null}
!25 = !{i32 1, !"B", i8 5, i8 0, !16, i8 1, i32 1, i8 2, i32 1, i8 0, null}
!26 = !{!27}
!27 = !{i32 0, !"SV_Target", i8 9, i8 16, !16, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!28 = !{i32 0, i64 8192}
