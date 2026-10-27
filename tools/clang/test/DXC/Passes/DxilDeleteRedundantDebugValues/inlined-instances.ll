; RUN: %dxopt %s -dxil-delete-redundant-debug-values -S | FileCheck %s

; Consecutive dbg.values of one variable are redundant except for the last one,
; but only within the same inlined instance: two instances of a function that
; is inlined at different call sites share the DILocalVariable and only differ
; in the inlinedAt of their location.

; The first value of the instance at line 12 is overwritten by the third one,
; while the instance at line 13 keeps its value.
; CHECK: %add = add i32 %x, %y
; CHECK-NEXT: call void @llvm.dbg.value(metadata i32 %x, i64 0, metadata ![[var:[0-9]+]], metadata !{{[0-9]+}}), !dbg ![[loc1:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.value(metadata i32 %y, i64 0, metadata ![[var]], metadata !{{[0-9]+}}), !dbg ![[loc0:[0-9]+]]
; CHECK-NEXT: call void @use(

; CHECK-DAG: ![[var]] = !DILocalVariable(tag: DW_TAG_auto_variable, name: "g"
; CHECK-DAG: ![[loc0]] = !DILocation(line: 3, scope: !{{[0-9]+}}, inlinedAt: ![[call0:[0-9]+]])
; CHECK-DAG: ![[loc1]] = !DILocation(line: 3, scope: !{{[0-9]+}}, inlinedAt: ![[call1:[0-9]+]])
; CHECK-DAG: ![[call0]] = distinct !DILocation(line: 12,
; CHECK-DAG: ![[call1]] = distinct !DILocation(line: 13,

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @main(i32 %x, i32 %y) {
entry:
  %add = add i32 %x, %y, !dbg !11
  call void @llvm.dbg.value(metadata i32 %x, i64 0, metadata !9, metadata !10), !dbg !11
  call void @llvm.dbg.value(metadata i32 %x, i64 0, metadata !9, metadata !10), !dbg !12
  call void @llvm.dbg.value(metadata i32 %y, i64 0, metadata !9, metadata !10), !dbg !11
  call void @use(i32 %add), !dbg !13
  ret void
}

declare void @use(i32)

declare void @llvm.dbg.value(metadata, i64, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!16, !17}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "dxc", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, subprograms: !3)
!1 = !DIFile(filename: "inlined-instances.hlsl", directory: "")
!2 = !{}
!3 = !{!4, !7}
!4 = !DISubprogram(name: "main", scope: !1, file: !1, line: 10, type: !5, isLocal: false, isDefinition: true, scopeLine: 10, isOptimized: false, function: void (i32, i32)* @main)
!5 = !DISubroutineType(types: !6)
!6 = !{null}
!7 = !DISubprogram(name: "helper", scope: !1, file: !1, line: 2, type: !5, isLocal: false, isDefinition: true, scopeLine: 2, isOptimized: false)
!8 = !DIBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!9 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "g", scope: !7, file: !1, line: 3, type: !8)
!10 = !DIExpression()
!11 = !DILocation(line: 3, scope: !7, inlinedAt: !14)
!12 = !DILocation(line: 3, scope: !7, inlinedAt: !15)
!13 = !DILocation(line: 11, scope: !4)
!14 = distinct !DILocation(line: 12, scope: !4)
!15 = distinct !DILocation(line: 13, scope: !4)
!16 = !{i32 2, !"Dwarf Version", i32 4}
!17 = !{i32 2, !"Debug Info Version", i32 3}
