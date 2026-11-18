; RUN: %dxopt %s -hlsl-passes-resume -dxil-elim-vector -S | FileCheck %s

; Rewrite a sparse vector dbg.value into scalar bit pieces. The original
; expression starts at bit 64, so lanes 1 and 3 begin at bits 96 and 160.
define i32 @rewrite_sparse_debug_value(i32 %lane1, i32 %lane3) !dbg !4 {
entry:
  %v0 = insertelement <4 x i32> zeroinitializer, i32 %lane1, i32 1
  %v1 = insertelement <4 x i32> %v0, i32 %lane3, i32 3
  call void @llvm.dbg.value(metadata <4 x i32> %v1, i64 0, metadata !9, metadata !10), !dbg !11
  %result = extractelement <4 x i32> %v1, i32 1
  ret i32 %result, !dbg !11
}

; CHECK-LABEL: define i32 @rewrite_sparse_debug_value(
; CHECK-NOT: insertelement
; CHECK-NOT: extractelement
; CHECK-DAG: call void @llvm.dbg.value(metadata i32 %lane1, i64 0, metadata ![[VAR:[0-9]+]], metadata ![[LANE1:[0-9]+]])
; CHECK-DAG: call void @llvm.dbg.value(metadata i32 %lane3, i64 0, metadata ![[VAR]], metadata ![[LANE3:[0-9]+]])
; CHECK-NOT: metadata <4 x i32>
; CHECK: ret i32 %lane1
; CHECK-DAG: ![[LANE1]] = !DIExpression(DW_OP_bit_piece, 96, 32)
; CHECK-DAG: ![[LANE3]] = !DIExpression(DW_OP_bit_piece, 160, 32)

declare void @llvm.dbg.value(metadata, i64, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!12, !13}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "dxil-eliminate-vector test", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, retainedTypes: !2, subprograms: !3)
!1 = !DIFile(filename: "debug-value.ll", directory: "")
!2 = !{}
!3 = !{!4}
!4 = !DISubprogram(name: "rewrite_sparse_debug_value", scope: !1, file: !1, line: 1, type: !5, isLocal: false, isDefinition: true, scopeLine: 1, flags: DIFlagPrototyped, isOptimized: false, function: i32 (i32, i32)* @rewrite_sparse_debug_value)
!5 = !DISubroutineType(types: !6)
!6 = !{!7, !7, !7}
!7 = !DIBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!8 = !DICompositeType(tag: DW_TAG_array_type, baseType: !7, size: 256, align: 32, elements: !14)
!9 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "vector", scope: !4, file: !1, line: 2, type: !8)
!10 = !DIExpression(DW_OP_bit_piece, 64, 128)
!11 = !DILocation(line: 2, column: 1, scope: !4)
!12 = !{i32 2, !"Dwarf Version", i32 4}
!13 = !{i32 2, !"Debug Info Version", i32 3}
!14 = !{!15}
!15 = !DISubrange(count: 8)
