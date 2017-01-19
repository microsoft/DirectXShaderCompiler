; RUN: %dxv %s | FileCheck %s

; CHECK: DS input control point count must be [0..32].  36 specified
; CHECK: TessFactor size mismatch the domain.
; CHECK: InsideTessFactor size mismatch the domain.
; CHECK: DomainLocation component index out of bounds for the domain.
; CHECK: DomainLocation component index out of bounds for the domain.
; CHECK: DomainLocation component index out of bounds for the domain.


target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%struct.PSSceneIn = type { <4 x float>, <2 x float>, <3 x float> }
%struct.HSPerVertexData = type { %struct.PSSceneIn }
%struct.HSPerPatchData = type { [3 x float], float }
%class.OutputPatch = type { [3 x %struct.HSPerVertexData] }

@dx.typevar.0 = external addrspace(1) constant %struct.PSSceneIn
@dx.typevar.1 = external addrspace(1) constant %struct.HSPerVertexData
@dx.typevar.2 = external addrspace(1) constant %struct.HSPerPatchData
@dx.typevar.3 = external addrspace(1) constant %class.OutputPatch
@llvm.used = appending global [4 x i8*] [i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerVertexData addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.HSPerPatchData addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.OutputPatch addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.PSSceneIn addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<3 x float>, [3 x <4 x float>]* nocapture readnone, [3 x <2 x float>]* nocapture readnone, [3 x <3 x float>]* nocapture readnone, [3 x float]* nocapture readnone, float* nocapture readnone, <4 x float>* nocapture readnone, <2 x float>* nocapture readnone, <3 x float>* nocapture readnone) #0 {
entry:
  %9 = tail call float @dx.op.domainLocation.f32(i32 107, i8 0)
  %10 = tail call float @dx.op.domainLocation.f32(i32 107, i8 1)
  %11 = tail call float @dx.op.domainLocation.f32(i32 107, i8 2)
  %12 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 0)
  %13 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 0)
  %14 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 0)
  %15 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 0)
  %mul.i0 = fmul fast float %12, %9
  %mul.i1 = fmul fast float %13, %9
  %mul.i2 = fmul fast float %14, %9
  %mul.i3 = fmul fast float %15, %9
  %16 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 1)
  %17 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 1)
  %18 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 1)
  %19 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 1)
  %mul5.i0 = fmul fast float %16, %10
  %mul5.i1 = fmul fast float %17, %10
  %mul5.i2 = fmul fast float %18, %10
  %mul5.i3 = fmul fast float %19, %10
  %add.i0 = fadd fast float %mul5.i0, %mul.i0
  %add.i1 = fadd fast float %mul5.i1, %mul.i1
  %add.i2 = fadd fast float %mul5.i2, %mul.i2
  %add.i3 = fadd fast float %mul5.i3, %mul.i3
  %20 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 2)
  %21 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 2)
  %22 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 2)
  %23 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 2)
  %mul10.i0 = fmul fast float %20, %11
  %mul10.i1 = fmul fast float %21, %11
  %mul10.i2 = fmul fast float %22, %11
  %mul10.i3 = fmul fast float %23, %11
  %add11.i0 = fadd fast float %add.i0, %mul10.i0
  %add11.i1 = fadd fast float %add.i1, %mul10.i1
  %add11.i2 = fadd fast float %add.i2, %mul10.i2
  %add11.i3 = fadd fast float %add.i3, %mul10.i3
  %24 = tail call float @dx.op.loadPatchConstant.f32(i32 106, i32 0, i32 1, i8 0)
  %add14.i0 = fadd fast float %add11.i0, %24
  %add14.i1 = fadd fast float %add11.i1, %24
  %add14.i2 = fadd fast float %add11.i2, %24
  %add14.i3 = fadd fast float %add11.i3, %24
  %25 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 0)
  %26 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 0)
  %mul19.i0 = fmul fast float %25, %9
  %mul19.i1 = fmul fast float %26, %9
  %27 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 1)
  %28 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 1)
  %mul24.i0 = fmul fast float %27, %10
  %mul24.i1 = fmul fast float %28, %10
  %add25.i0 = fadd fast float %mul24.i0, %mul19.i0
  %add25.i1 = fadd fast float %mul24.i1, %mul19.i1
  %29 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 2)
  %30 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 2)
  %mul30.i0 = fmul fast float %29, %11
  %mul30.i1 = fmul fast float %30, %11
  %add31.i0 = fadd fast float %add25.i0, %mul30.i0
  %add31.i1 = fadd fast float %add25.i1, %mul30.i1
  %31 = tail call float @dx.op.loadPatchConstant.f32(i32 106, i32 0, i32 0, i8 0)
  %add36.i0 = fadd fast float %add31.i0, %31
  %add36.i1 = fadd fast float %add31.i1, %31
  %32 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 0)
  %33 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 0)
  %34 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 0)
  %mul41.i0 = fmul fast float %32, %9
  %mul41.i1 = fmul fast float %33, %9
  %mul41.i2 = fmul fast float %34, %9
  %35 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 1)
  %36 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 1)
  %37 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 1)
  %mul46.i0 = fmul fast float %35, %10
  %mul46.i1 = fmul fast float %36, %10
  %mul46.i2 = fmul fast float %37, %10
  %add47.i0 = fadd fast float %mul46.i0, %mul41.i0
  %add47.i1 = fadd fast float %mul46.i1, %mul41.i1
  %add47.i2 = fadd fast float %mul46.i2, %mul41.i2
  %38 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 2)
  %39 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 2)
  %40 = tail call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 2)
  %mul52.i0 = fmul fast float %38, %11
  %mul52.i1 = fmul fast float %39, %11
  %mul52.i2 = fmul fast float %40, %11
  %add53.i0 = fadd fast float %add47.i0, %mul52.i0
  %add53.i1 = fadd fast float %add47.i1, %mul52.i1
  %add53.i2 = fadd fast float %add47.i2, %mul52.i2
  %41 = tail call float @dx.op.loadPatchConstant.f32(i32 106, i32 1, i32 0, i8 0)
  %add56.i0 = fadd fast float %add53.i0, %41
  %add56.i1 = fadd fast float %add53.i1, %41
  %add56.i2 = fadd fast float %add53.i2, %41
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %add14.i0)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %add14.i1)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %add14.i2)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %add14.i3)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %add36.i0)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %add36.i1)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %add56.i0)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %add56.i1)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float %add56.i2)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.domainLocation.f32(i32, i8) #1

; Function Attrs: nounwind readnone
declare float @dx.op.loadPatchConstant.f32(i32, i32, i32, i8) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3, !15}
!dx.entryPoints = !{!36}

!0 = !{!"my awesome compiler"}
!1 = !{i32 1, i32 0}
!2 = !{!"ds", i32 6, i32 0}
!3 = !{i32 0, %struct.PSSceneIn addrspace(1)* @dx.typevar.0, !4, %struct.HSPerVertexData addrspace(1)* @dx.typevar.1, !8, %struct.HSPerPatchData addrspace(1)* @dx.typevar.2, !10, %class.OutputPatch addrspace(1)* @dx.typevar.3, !13}
!4 = !{i32 44, !5, !6, !7}
!5 = !{i32 3, i32 0, i32 4, !"SV_Position", i32 6, !"pos", i32 7, i32 9}
!6 = !{i32 3, i32 16, i32 4, !"TEXCOORD0", i32 6, !"tex", i32 7, i32 9}
!7 = !{i32 3, i32 32, i32 4, !"NORMAL", i32 6, !"norm", i32 7, i32 9}
!8 = !{i32 44, !9}
!9 = !{i32 3, i32 0, i32 6, !"v"}
!10 = !{i32 40, !11, !12}
!11 = !{i32 3, i32 0, i32 4, !"SV_TessFactor", i32 6, !"edges", i32 7, i32 9}
!12 = !{i32 3, i32 36, i32 4, !"SV_InsideTessFactor", i32 6, !"inside", i32 7, i32 9}
!13 = !{i32 140, !14}
!14 = !{i32 3, i32 0, i32 6, !"h"}
!15 = !{i32 1, void (<3 x float>, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x float]*, float*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !16}
!16 = !{!17, !19, !22, !24, !26, !28, !31, !33, !34, !35}
!17 = !{i32 0, !18, !18}
!18 = !{}
!19 = !{i32 0, !20, !21}
!20 = !{i32 4, !"SV_DomainLocation", i32 7, i32 9}
!21 = !{i32 0}
!22 = !{i32 4, !23, !21}
!23 = !{i32 4, !"SV_Position", i32 7, i32 9}
!24 = !{i32 4, !25, !21}
!25 = !{i32 4, !"TEXCOORD0", i32 7, i32 9}
!26 = !{i32 4, !27, !21}
!27 = !{i32 4, !"NORMAL", i32 7, i32 9}
!28 = !{i32 0, !29, !30}
!29 = !{i32 4, !"SV_TessFactor", i32 7, i32 9}
!30 = !{i32 0, i32 1, i32 2}
!31 = !{i32 0, !32, !21}
!32 = !{i32 4, !"SV_InsideTessFactor", i32 7, i32 9}
!33 = !{i32 1, !23, !21}
!34 = !{i32 1, !25, !21}
!35 = !{i32 1, !27, !21}
!36 = !{void (<3 x float>, [3 x <4 x float>]*, [3 x <2 x float>]*, [3 x <3 x float>]*, [3 x float]*, float*, <4 x float>*, <2 x float>*, <3 x float>*)* @main.flat, !"", !37, null, !46}
!37 = !{!38, !38, !42}
!38 = !{!39, !40, !41}
!39 = !{i32 0, !"SV_Position", i8 9, i8 3, !21, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!40 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !21, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!41 = !{i32 2, !"NORMAL", i8 9, i8 0, !21, i8 2, i32 1, i8 3, i32 2, i8 0, null}
!42 = !{!44, !45}
!44 = !{i32 0, !"SV_TessFactor", i8 9, i8 25, !30, i8 0, i32 3, i8 1, i32 0, i8 0, null}
!45 = !{i32 1, !"SV_InsideTessFactor", i8 9, i8 26, !21, i8 0, i32 1, i8 1, i32 3, i8 0, null}
!46 = !{i32 2, !47}
!47 = !{i32 4, i32 36}

