; RUN: opt -instcombine -S %s | FileCheck %s

; CHECK-NOT: call i32 @llvm.bswap.i32(i32 %5)
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%Consts = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }

@Consts = external constant %Consts
@llvm.used = appending global [1 x i8*] [i8* bitcast (%Consts* @Consts to i8*)], section "llvm.metadata"

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %Consts*, i32)"(i32, %Consts*, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %Consts)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %Consts) #0

; Function Attrs: nounwind
define void @main(<4 x float>* noalias) #1 {
  %2 = load %Consts, %Consts* @Consts, !dbg !3 ; line:16 col:20
  %Consts = call %dx.types.Handle @dx.op.createHandleForLib.Consts(i32 160, %Consts %2), !dbg !3 ; line:16 col:20
  %3 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %Consts, %dx.types.ResourceProperties { i32 13, i32 4 }), !dbg !3 ; line:16 col:20
  %4 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %3, i32 0), !dbg !3 ; line:16 col:20
  %5 = extractvalue %dx.types.CBufRet.i32 %4, 0, !dbg !3 ; line:16 col:20
  %6 = shl i32 %5, 24, !dbg !8 ; line:8 col:15
  %7 = and i32 %6, -16777216, !dbg !11 ; line:8 col:22
  %8 = shl i32 %5, 8, !dbg !12 ; line:9 col:15
  %9 = and i32 %8, 16711680, !dbg !13 ; line:9 col:22
  %10 = or i32 %7, %9, !dbg !14 ; line:8 col:36
  %11 = lshr i32 %5, 8, !dbg !15 ; line:10 col:15
  %12 = and i32 %11, 65280, !dbg !16 ; line:10 col:22
  %13 = or i32 %10, %12, !dbg !17 ; line:9 col:36
  %14 = lshr i32 %5, 24, !dbg !18 ; line:11 col:15
  %15 = and i32 %14, 255, !dbg !19 ; line:11 col:22
  %16 = or i32 %13, %15, !dbg !20 ; line:10 col:36
  %17 = uitofp i32 %16 to float, !dbg !21 ; line:16 col:12
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %17), !dbg !22 ; line:16 col:5
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %17), !dbg !22 ; line:16 col:5
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %17), !dbg !22 ; line:16 col:5
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %17), !dbg !22 ; line:16 col:5
  ret void, !dbg !22 ; line:16 col:5
}

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.Consts(i32, %Consts) #2

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
!2 = !{!"dxc(private) 1.7.0.14160 (main, adb2dc70fbd-dirty)"}
!3 = !DILocation(line: 16, column: 20, scope: !4)
!4 = !DISubprogram(name: "main", scope: !5, file: !5, line: 14, type: !6, isLocal: false, isDefinition: true, scopeLine: 15, flags: DIFlagPrototyped, isOptimized: false, function: void (<4 x float>*)* @main)
!5 = !DIFile(filename: "bswap.hlsl", directory: "")
!6 = !DISubroutineType(types: !7)
!7 = !{}
!8 = !DILocation(line: 8, column: 15, scope: !9, inlinedAt: !10)
!9 = !DISubprogram(name: "bswap32", scope: !5, file: !5, line: 6, type: !6, isLocal: false, isDefinition: true, scopeLine: 6, flags: DIFlagPrototyped, isOptimized: false)
!10 = distinct !DILocation(line: 16, column: 12, scope: !4)
!11 = !DILocation(line: 8, column: 22, scope: !9, inlinedAt: !10)
!12 = !DILocation(line: 9, column: 15, scope: !9, inlinedAt: !10)
!13 = !DILocation(line: 9, column: 22, scope: !9, inlinedAt: !10)
!14 = !DILocation(line: 8, column: 36, scope: !9, inlinedAt: !10)
!15 = !DILocation(line: 10, column: 15, scope: !9, inlinedAt: !10)
!16 = !DILocation(line: 10, column: 22, scope: !9, inlinedAt: !10)
!17 = !DILocation(line: 9, column: 36, scope: !9, inlinedAt: !10)
!18 = !DILocation(line: 11, column: 15, scope: !9, inlinedAt: !10)
!19 = !DILocation(line: 11, column: 22, scope: !9, inlinedAt: !10)
!20 = !DILocation(line: 10, column: 36, scope: !9, inlinedAt: !10)
!21 = !DILocation(line: 16, column: 12, scope: !4)
!22 = !DILocation(line: 16, column: 5, scope: !4)
