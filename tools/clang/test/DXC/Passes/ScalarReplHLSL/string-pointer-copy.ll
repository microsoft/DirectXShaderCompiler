; RUN: %dxopt %s -hlsl-passes-resume -scalarrepl-param-hlsl -verify -S | FileCheck %s

; Copy pointer-valued leaves, rather than indexing through their pointees.
; External globals keep the copies observable without introducing other passes.
; Synthetic nonzero cbuffer-layout sizes prevent metadata loading from marking
; the pointer-only fixtures empty, isolating pointer-copy behavior.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%StringOnly = type { i8* }
%Mixed = type { i8*, i32 }
%Nested = type { [2 x %StringOnly] }

@string.src = external global %StringOnly
@string.dst = external global %StringOnly
@mixed.src = external global %Mixed
@mixed.dst = external global %Mixed
@nested.src = external global %Nested
@nested.dst = external global %Nested
@pointer.src = external global i8*
@pointer.dst = external global i8*

; CHECK-LABEL: define void @main()
; CHECK: [[STRING:%.*]] = load i8*, i8** getelementptr inbounds (%StringOnly, %StringOnly* @string.src, i32 0, i32 0)
; CHECK-NEXT: store i8* [[STRING]], i8** getelementptr inbounds (%StringOnly, %StringOnly* @string.dst, i32 0, i32 0)
; CHECK: [[TEXT:%.*]] = load i8*, i8** getelementptr inbounds (%Mixed, %Mixed* @mixed.src, i32 0, i32 0)
; CHECK-NEXT: store i8* [[TEXT]], i8** getelementptr inbounds (%Mixed, %Mixed* @mixed.dst, i32 0, i32 0)
; CHECK: [[NUMBER:%.*]] = load i32, i32* getelementptr inbounds (%Mixed, %Mixed* @mixed.src, i32 0, i32 1)
; CHECK-NEXT: store i32 [[NUMBER]], i32* getelementptr inbounds (%Mixed, %Mixed* @mixed.dst, i32 0, i32 1)
; CHECK: [[FIRST:%.*]] = load i8*, i8** getelementptr inbounds (%Nested, %Nested* @nested.src, i32 0, i32 0, i32 0, i32 0)
; CHECK-NEXT: store i8* [[FIRST]], i8** getelementptr inbounds (%Nested, %Nested* @nested.dst, i32 0, i32 0, i32 0, i32 0)
; CHECK: [[SECOND:%.*]] = load i8*, i8** getelementptr inbounds (%Nested, %Nested* @nested.src, i32 0, i32 0, i32 1, i32 0)
; CHECK-NEXT: store i8* [[SECOND]], i8** getelementptr inbounds (%Nested, %Nested* @nested.dst, i32 0, i32 0, i32 1, i32 0)
; CHECK: [[POINTER:%.*]] = load i8*, i8** @pointer.src
; CHECK-NEXT: store i8* [[POINTER]], i8** @pointer.dst
; CHECK-NEXT: ret void
define void @main() {
entry:
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* bitcast (%StringOnly* @string.dst to i8*), i8* bitcast (%StringOnly* @string.src to i8*), i64 4, i32 4, i1 false)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* bitcast (%Mixed* @mixed.dst to i8*), i8* bitcast (%Mixed* @mixed.src to i8*), i64 8, i32 4, i1 false)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* bitcast (%Nested* @nested.dst to i8*), i8* bitcast (%Nested* @nested.src to i8*), i64 8, i32 4, i1 false)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* bitcast (i8** @pointer.dst to i8*), i8* bitcast (i8** @pointer.src to i8*), i64 4, i32 4, i1 false)
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture readonly, i64, i32, i1)

!pauseresume = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.typeAnnotations = !{!4, !10}
!dx.entryPoints = !{!14}
!dx.fnprops = !{!15}
!dx.options = !{!16, !17}

!0 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 8}
!3 = !{!"cs", i32 6, i32 0}
!4 = !{i32 0, %StringOnly undef, !5, %Mixed undef, !7, %Nested undef, !9}
!5 = !{i32 4, !6}
!6 = !{i32 6, !"Text", i32 3, i32 0}
!7 = !{i32 4, !6, !8}
!8 = !{i32 6, !"Value", i32 3, i32 0, i32 7, i32 5}
!9 = !{i32 8, !18}
!10 = !{i32 1, void ()* @main, !11}
!11 = !{!12}
!12 = !{i32 1, !13, !13}
!13 = !{}
!14 = !{void ()* @main, !"main", null, null, null}
!15 = !{void ()* @main, i32 5, i32 1, i32 1, i32 1}
!16 = !{i32 64}
!17 = !{i32 -1}
!18 = !{i32 6, !"Values", i32 3, i32 0}
