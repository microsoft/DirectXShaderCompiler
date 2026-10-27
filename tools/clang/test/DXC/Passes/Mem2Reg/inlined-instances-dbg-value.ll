; RUN: %dxopt %s -mem2reg -S | FileCheck %s

; A variable of a function that is inlined at two call sites has one
; dbg.declare per inlined instance. The declares share the DILocalVariable and
; only differ in the inlinedAt of their location. When the alloca is promoted,
; every instance must get its own dbg.value for the store, instead of the
; second one being treated as a duplicate of the first.

; CHECK: call void @llvm.dbg.value(metadata i32 %x, i64 0, metadata ![[var:[0-9]+]], metadata !{{[0-9]+}}), !dbg ![[loc0:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.value(metadata i32 %x, i64 0, metadata ![[var]], metadata !{{[0-9]+}}), !dbg ![[loc1:[0-9]+]]
; CHECK-NOT: call void @llvm.dbg.value(

; CHECK-DAG: ![[var]] = !DILocalVariable(tag: DW_TAG_auto_variable, name: "g"
; CHECK-DAG: ![[loc0]] = !DILocation(line: 3, scope: !{{[0-9]+}}, inlinedAt: ![[call0:[0-9]+]])
; CHECK-DAG: ![[loc1]] = !DILocation(line: 3, scope: !{{[0-9]+}}, inlinedAt: ![[call1:[0-9]+]])
; CHECK-DAG: ![[call0]] = distinct !DILocation(line: {{12|13}},
; CHECK-DAG: ![[call1]] = distinct !DILocation(line: {{12|13}},

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @main(i32 %x) {
entry:
  %g = alloca i32, align 4
  call void @llvm.dbg.declare(metadata i32* %g, metadata !9, metadata !10), !dbg !11
  call void @llvm.dbg.declare(metadata i32* %g, metadata !9, metadata !10), !dbg !12
  store i32 %x, i32* %g, align 4, !dbg !13
  %v = load i32, i32* %g, align 4, !dbg !13
  call void @use(i32 %v), !dbg !13
  ret void
}

declare void @use(i32)

declare void @llvm.dbg.declare(metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!16, !17}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "dxc", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, subprograms: !3)
!1 = !DIFile(filename: "inlined-instances-dbg-value.hlsl", directory: "")
!2 = !{}
!3 = !{!4, !7}
!4 = !DISubprogram(name: "main", scope: !1, file: !1, line: 10, type: !5, isLocal: false, isDefinition: true, scopeLine: 10, isOptimized: false, function: void (i32)* @main)
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
