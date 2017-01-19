; RUN: %dxv %s | FileCheck %s

; CHECK: GS output vertex count must be [0..1024].  1025 specified
; CHECK: GS instance count must be [1..32].  33 specified
; CHECK: GS output primitive topology unrecognized
; CHECK: GS input primitive unrecognized
; CHECK: Stream index (5) must between 0 and 3

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%b = type { <2 x float> }
%struct.VSOutGSIn = type { <3 x float>, <4 x float> }
%class.TriangleStream = type { %struct.VSOut }
%struct.VSOut = type { <2 x float>, <4 x float>, <4 x float>, i32 }
%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }

@b = external constant %b
@dx.typevar.0 = external addrspace(1) constant %struct.VSOutGSIn
@dx.typevar.1 = external addrspace(1) constant %class.TriangleStream
@dx.typevar.2 = external addrspace(1) constant %struct.VSOut
@dx.typevar.3 = external addrspace(1) constant %b
@llvm.used = appending global [6 x i8*] [i8* bitcast (%b* @b to i8*), i8* bitcast (%b* @b to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.VSOutGSIn addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%class.TriangleStream addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.VSOut addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%b addrspace(1)* @dx.typevar.3 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat([1 x <3 x float>]* nocapture readnone, [1 x <4 x float>]* nocapture readnone, %class.TriangleStream* nocapture readnone, <2 x float>* nocapture readnone, <4 x float>* nocapture readnone, <4 x float>* nocapture readnone, i32* nocapture readnone) #0 {
entry:
  %verts.0 = alloca [3 x float], align 4
  %verts.1 = alloca [3 x float], align 4
  %7 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 0
  %8 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 0
  store float -5.000000e-01, float* %7, align 4
  store float -5.000000e-01, float* %8, align 4
  %9 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 1
  %10 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 1
  store float 1.500000e+00, float* %9, align 4
  store float -5.000000e-01, float* %10, align 4
  %11 = getelementptr [3 x float], [3 x float]* %verts.0, i32 0, i32 2
  %12 = getelementptr [3 x float], [3 x float]* %verts.1, i32 0, i32 2
  store float -5.000000e-01, float* %11, align 4
  store float 1.500000e+00, float* %12, align 4
  %13 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 0)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 0)
  %15 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 0)
  %16 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 0)
  %17 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 0)
  %18 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 0)
  %19 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 0)
  %load30 = load float, float* %7, align 4
  %load32 = load float, float* %8, align 4
  %mul.i0 = fmul fast float %load30, %13
  %mul.i1 = fmul fast float %load32, %13
  %add.i0 = fadd fast float %mul.i0, %14
  %add.i1 = fadd fast float %mul.i1, %15
  %20 = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 2, i32 0, i32 0, i1 false)
  %21 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 60, %dx.types.Handle %20, i32 0)
  %22 = extractvalue %dx.types.CBufRet.f32 %21, 0
  %23 = extractvalue %dx.types.CBufRet.f32 %21, 1
  %mul.i.i0 = fmul fast float %22, 2.000000e+00
  %mul.i.i1 = fmul fast float %23, 2.000000e+00
  %mul1.i.i0 = fmul fast float %mul.i.i0, %add.i0
  %mul1.i.i1 = fmul fast float %mul.i.i1, %add.i1
  %sub.i = fadd fast float %mul1.i.i0, -1.000000e+00
  %sub2.i = fsub fast float 1.000000e+00, %mul1.i.i1
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %16)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %17)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %18)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %19)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %sub.i)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %sub2.i)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float 5.000000e-01)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 1.000000e+00)
  call void @dx.op.storeOutput.i32(i32 5, i32 3, i32 0, i8 0, i32 0)
  call void @dx.op.emitStream(i32 97, i8 0)
  %load26 = load float, float* %9, align 4
  %load28 = load float, float* %10, align 4
  %mul12.i0 = fmul fast float %load26, %13
  %mul12.i1 = fmul fast float %load28, %13
  %add13.i0 = fadd fast float %mul12.i0, %14
  %add13.i1 = fadd fast float %mul12.i1, %15
  %mul1.i.29.i0 = fmul fast float %add13.i0, %mul.i.i0
  %mul1.i.29.i1 = fmul fast float %add13.i1, %mul.i.i1
  %sub.i.30 = fadd fast float %mul1.i.29.i0, -1.000000e+00
  %sub2.i.31 = fsub fast float 1.000000e+00, %mul1.i.29.i1
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 2.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %16)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %17)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %18)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %19)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %sub.i.30)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %sub2.i.31)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float 5.000000e-01)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 1.000000e+00)
  call void @dx.op.storeOutput.i32(i32 5, i32 3, i32 0, i8 0, i32 2)
  call void @dx.op.emitStream(i32 97, i8 0)
  %load23 = load float, float* %11, align 4
  %load24 = load float, float* %12, align 4
  %mul22.i0 = fmul fast float %load23, %13
  %mul22.i1 = fmul fast float %load24, %13
  %add23.i0 = fadd fast float %mul22.i0, %14
  %add23.i1 = fadd fast float %mul22.i1, %15
  %mul1.i.34.i0 = fmul fast float %add23.i0, %mul.i.i0
  %mul1.i.34.i1 = fmul fast float %add23.i1, %mul.i.i1
  %sub.i.35 = fadd fast float %mul1.i.34.i0, -1.000000e+00
  %sub2.i.36 = fsub fast float 1.000000e+00, %mul1.i.34.i1
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 2.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %16)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %17)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %18)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float %19)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %sub.i.35)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %sub2.i.36)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float 5.000000e-01)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float 1.000000e+00)
  call void @dx.op.storeOutput.i32(i32 5, i32 3, i32 0, i8 0, i32 1)
  call void @dx.op.emitStream(i32 97, i8 0)
  call void @dx.op.cutStream(i32 98, i8 0)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.i32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind
declare void @dx.op.cutStream(i32, i8) #0

; Function Attrs: nounwind
declare void @dx.op.emitStream(i32, i8) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!6, !18}
!dx.entryPoints = !{!35}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 1, i32 0}
!2 = !{!"gs", i32 6, i32 0}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %b* @b, !"b", i32 0, i32 0, i32 1, i32 8, null}
!6 = !{i32 0, %struct.VSOutGSIn addrspace(1)* @dx.typevar.0, !7, %class.TriangleStream addrspace(1)* @dx.typevar.1, !10, %struct.VSOut addrspace(1)* @dx.typevar.2, !12, %b addrspace(1)* @dx.typevar.3, !16}
!7 = !{i32 32, !8, !9}
!8 = !{i32 3, i32 0, i32 4, !"POSSIZE", i32 6, !"posSize", i32 7, i32 9}
!9 = !{i32 3, i32 16, i32 4, !"COLOR", i32 6, !"clr", i32 7, i32 9}
!10 = !{i32 52, !11}
!11 = !{i32 3, i32 0, i32 6, !"h"}
!12 = !{i32 52, !13, !9, !14, !15}
!13 = !{i32 3, i32 0, i32 4, !"TEXCOORD0", i32 6, !"uv", i32 7, i32 9}
!14 = !{i32 3, i32 32, i32 4, !"SV_Position", i32 6, !"pos", i32 7, i32 9}
!15 = !{i32 3, i32 48, i32 4, !"SV_RenderTargetArrayIndex", i32 6, !"index", i32 7, i32 5}
!16 = !{i32 0, !17}
!17 = !{i32 3, i32 0, i32 6, !"invViewportSize", i32 7, i32 9}
!18 = !{i32 1, void ([1 x <3 x float>]*, [1 x <4 x float>]*, %class.TriangleStream*, <2 x float>*, <4 x float>*, <4 x float>*, i32*)* @main.flat, !19}
!19 = !{!20, !22, !25, !27, !28, !30, !31, !33}
!20 = !{i32 0, !21, !21}
!21 = !{}
!22 = !{i32 0, !23, !24}
!23 = !{i32 4, !"POSSIZE", i32 7, i32 9}
!24 = !{i32 0}
!25 = !{i32 0, !26, !24}
!26 = !{i32 4, !"COLOR", i32 7, i32 9}
!27 = !{i32 5, !21, !21}
!28 = !{i32 5, !29, !24}
!29 = !{i32 4, !"TEXCOORD0", i32 7, i32 9}
!30 = !{i32 5, !26, !24}
!31 = !{i32 5, !32, !24}
!32 = !{i32 4, !"SV_Position", i32 7, i32 9}
!33 = !{i32 5, !34, !24}
!34 = !{i32 4, !"SV_RenderTargetArrayIndex", i32 7, i32 5}
!35 = !{void ([1 x <3 x float>]*, [1 x <4 x float>]*, %class.TriangleStream*, <2 x float>*, <4 x float>*, <4 x float>*, i32*)* @main.flat, !"", !36, !3, !44}
!36 = !{!37, !40, null}
!37 = !{!38, !39}
!38 = !{i32 0, !"POSSIZE", i8 9, i8 0, !24, i8 2, i32 1, i8 3, i32 0, i8 0, null}
!39 = !{i32 1, !"COLOR", i8 9, i8 0, !24, i8 2, i32 1, i8 4, i32 1, i8 0, null}
!40 = !{!41, !39, !42, !43}
!41 = !{i32 0, !"TEXCOORD", i8 9, i8 0, !24, i8 2, i32 1, i8 2, i32 0, i8 0, !50}
!42 = !{i32 2, !"SV_Position", i8 9, i8 3, !24, i8 4, i32 1, i8 4, i32 2, i8 0, null}
!43 = !{i32 3, !"SV_RenderTargetArrayIndex", i8 5, i8 4, !24, i8 1, i32 1, i8 1, i32 3, i8 0, null}
!44 = !{i32 0, i64 512, i32 1, !45}
!45 = !{i32 5, i32 1025, i32 1, i32 0, i32 33}
;!45 = !{i32 1, i32 3, i32 1, i32 4, i32 1}

!50 = !{i32 0, i32 5}