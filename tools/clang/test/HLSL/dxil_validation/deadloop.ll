; RUN: %dxv %s | FileCheck %s

; CHECK: Named metadata 'dx.unused' is unknown
; CHECK: Loop must have break

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%"$Globals" = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }

@"\01?i@@3HA" = global i32 0, align 4
@"$Globals" = external constant %"$Globals"
@dx.typevar.0 = external addrspace(1) constant %"$Globals"
@llvm.used = appending global [3 x i8*] [i8* bitcast (%"$Globals"* @"$Globals" to i8*), i8* bitcast (%"$Globals"* @"$Globals" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"$Globals" addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat(<2 x float>, <3 x i32>, float* nocapture readnone) #0 {
entry:
  %3 = call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %5 = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 2, i32 0, i32 0, i1 false)
  %6 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 60, %dx.types.Handle %5, i32 0)
  %7 = extractvalue %dx.types.CBufRet.i32 %6, 0
  %cmp = icmp slt i32 %7, %3
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.body, %entry
  %s.01 = phi float [ %add, %while.body ], [ 0.000000e+00, %entry ]
  %add = fadd fast float %s.01, %4
  br label %while.body

while.end:                                        ; preds = %while.body, %entry
  %s.0.lcssa = phi float [ 0.000000e+00, %entry ], [ %add, %while.body ]
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %s.0.lcssa)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.loadInput.i32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!6, !9}
!dx.entryPoints = !{!20}
!dx.unused = !{!20}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 5, i32 1}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %"$Globals"* @"$Globals", !"$Globals", i32 0, i32 0, i32 1, i32 4, null}
!6 = !{i32 0, %"$Globals" addrspace(1)* @dx.typevar.0, !7}
!7 = !{i32 0, !8}
!8 = !{i32 3, i32 0, i32 6, !"i", i32 7, i32 4}
!9 = !{i32 1, void (<2 x float>, <3 x i32>, float*)* @main.flat, !10}
!10 = !{!11, !13, !16, !18}
!11 = !{i32 0, !12, !12}
!12 = !{}
!13 = !{i32 0, !14, !15}
!14 = !{i32 4, !"A", i32 7, i32 9}
!15 = !{i32 0}
!16 = !{i32 0, !17, !15}
!17 = !{i32 4, !"B", i32 7, i32 4}
!18 = !{i32 1, !19, !15}
!19 = !{i32 4, !"SV_Target", i32 7, i32 9}
!20 = !{void (<2 x float>, <3 x i32>, float*)* @main.flat, !"", !21, !3, null}
!21 = !{!22, !25, null}
!22 = !{!23, !24}
!23 = !{i32 0, !"A", i8 9, i8 0, !15, i8 2, i32 1, i8 2, i32 0, i8 0, null}
!24 = !{i32 1, !"B", i8 4, i8 0, !15, i8 1, i32 1, i8 3, i32 1, i8 0, null}
!25 = !{!26}
!26 = !{i32 0, !"SV_Target", i8 9, i8 16, !15, i8 0, i32 1, i8 1, i32 0, i8 0, null}
