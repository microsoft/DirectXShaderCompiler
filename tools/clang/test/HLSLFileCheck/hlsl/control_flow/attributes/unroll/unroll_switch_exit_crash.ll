; RUN: %opt %s -dxil-loop-unroll -S | FileCheck %s

; CHECK: call float @dx.op.unary.f32(i32 13
; CHECK: call float @dx.op.unary.f32(i32 13
; CHECK: call float @dx.op.unary.f32(i32 13
; CHECK: call float @dx.op.unary.f32(i32 13
; CHECK-NOT: call float @dx.op.unary.f32(i32 13

; ModuleID = 'F:\dxc\tools\clang\test\HLSLFileCheck\hlsl\control_flow\attributes\unroll\unroll_switch_exit_crash.hlsl'
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%"class.Texture1D<float>" = type { float, %"class.Texture1D<float>::mips_type" }
%"class.Texture1D<float>::mips_type" = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }

@"\01?t0@@3V?$Texture1D@M@@A" = external global %"class.Texture1D<float>", align 4
@llvm.used = appending global [1 x i8*] [i8* bitcast (%"class.Texture1D<float>"* @"\01?t0@@3V?$Texture1D@M@@A" to i8*)], section "llvm.metadata"

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.Texture1D<float>\22)"(i32, %"class.Texture1D<float>") #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.Texture1D<float>\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.Texture1D<float>") #0

; Function Attrs: nounwind
define void @main(float* noalias, i32) #1 {
entry:
  %2 = load %"class.Texture1D<float>", %"class.Texture1D<float>"* @"\01?t0@@3V?$Texture1D@M@@A", !dbg !3
  %3 = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 undef), !dbg !10
  br label %for.body.i, !dbg !10

for.body.i:                                       ; preds = %entry, %sw.epilog.i
  %i.i.0 = phi i32 [ 0, %entry ], [ %inc.i, %sw.epilog.i ]
  %ret.i.0 = phi float [ 0.000000e+00, %entry ], [ %add7.i, %sw.epilog.i ]
  %add.i = fadd fast float %ret.i.0, 1.000000e+00, !dbg !11
  switch i32 %3, label %"\01?foo@@YAMH@Z.exit" [
    i32 0, label %sw.bb.i
    i32 1, label %sw.bb.3.i
  ], !dbg !12

sw.bb.i:                                          ; preds = %for.body.i
  %add2.i = fadd fast float %add.i, 1.000000e+01, !dbg !13
  br label %sw.epilog.i, !dbg !14

sw.bb.3.i:                                        ; preds = %for.body.i
  %add4.i = fadd fast float %add.i, 2.000000e+01, !dbg !15
  br label %sw.epilog.i, !dbg !16

sw.epilog.i:                                      ; preds = %sw.bb.3.i, %sw.bb.i
  %ret.i.1 = phi float [ %add4.i, %sw.bb.3.i ], [ %add2.i, %sw.bb.i ]
  %add5.i = add nsw i32 %3, %i.i.0, !dbg !17
  %4 = call %dx.types.Handle @"dx.op.createHandleForLib.class.Texture1D<float>"(i32 160, %"class.Texture1D<float>" %2), !dbg !3
  %5 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %4, %dx.types.ResourceProperties { i32 1, i32 265 }), !dbg !3
  %TextureLoad = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %5, i32 0, i32 %add5.i, i32 undef, i32 undef, i32 undef, i32 undef, i32 undef), !dbg !3
  %6 = extractvalue %dx.types.ResRet.f32 %TextureLoad, 0, !dbg !3
  %Sin = call float @dx.op.unary.f32(i32 13, float %6), !dbg !18
  %add7.i = fadd fast float %ret.i.1, %Sin, !dbg !19
  %inc.i = add nsw i32 %i.i.0, 1, !dbg !20
  %cmp.i = icmp slt i32 %inc.i, 4, !dbg !21
  br i1 %cmp.i, label %for.body.i, label %"\01?foo@@YAMH@Z.exit", !dbg !10, !llvm.loop !22

"\01?foo@@YAMH@Z.exit":                           ; preds = %sw.epilog.i, %for.body.i
  %retval.i.0 = phi float [ 4.200000e+01, %for.body.i ], [ %add7.i, %sw.epilog.i ]
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %retval.i.0), !dbg !24
  ret void, !dbg !24
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.Handle @"dx.op.createHandleForLib.class.Texture1D<float>"(i32, %"class.Texture1D<float>") #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #0

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.module.flags = !{!0}
!pauseresume = !{!1}
!llvm.ident = !{!2}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{!"hlsl-dxilemit", !"hlsl-dxilload"}
!2 = !{!"dxc(private) 1.7.0.14003 (main, a5b0488bc-dirty)"}
!3 = !DILocation(line: 29, column: 16, scope: !4, inlinedAt: !8)
!4 = !DISubprogram(name: "foo", scope: !5, file: !5, line: 14, type: !6, isLocal: false, isDefinition: true, scopeLine: 14, flags: DIFlagPrototyped, isOptimized: false)
!5 = !DIFile(filename: "F:\5Cdxc\5Ctools\5Cclang\5Ctest\5CHLSLFileCheck\5Chlsl\5Ccontrol_flow\5Cattributes\5Cunroll\5Cunroll_switch_exit_crash.hlsl", directory: "")
!6 = !DISubroutineType(types: !7)
!7 = !{}
!8 = distinct !DILocation(line: 37, column: 10, scope: !9)
!9 = !DISubprogram(name: "main", scope: !5, file: !5, line: 35, type: !6, isLocal: false, isDefinition: true, scopeLine: 35, flags: DIFlagPrototyped, isOptimized: false, function: void (float*, i32)* @main)
!10 = !DILocation(line: 17, column: 3, scope: !4, inlinedAt: !8)
!11 = !DILocation(line: 18, column: 9, scope: !4, inlinedAt: !8)
!12 = !DILocation(line: 19, column: 5, scope: !4, inlinedAt: !8)
!13 = !DILocation(line: 21, column: 13, scope: !4, inlinedAt: !8)
!14 = !DILocation(line: 22, column: 9, scope: !4, inlinedAt: !8)
!15 = !DILocation(line: 24, column: 13, scope: !4, inlinedAt: !8)
!16 = !DILocation(line: 25, column: 9, scope: !4, inlinedAt: !8)
!17 = !DILocation(line: 29, column: 24, scope: !4, inlinedAt: !8)
!18 = !DILocation(line: 29, column: 12, scope: !4, inlinedAt: !8)
!19 = !DILocation(line: 29, column: 9, scope: !4, inlinedAt: !8)
!20 = !DILocation(line: 17, column: 27, scope: !4, inlinedAt: !8)
!21 = !DILocation(line: 17, column: 21, scope: !4, inlinedAt: !8)
!22 = distinct !{!22, !23}
!23 = !{!"llvm.loop.unroll.full"}
!24 = !DILocation(line: 38, column: 3, scope: !9)