; RUN: %dxa %s -o %t
; RUN: %dxa %t -dumpreflection | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

;struct S2 {
;    row_major int4x3 m;
;	int1x3 m1[2];
;	int3x1 m2;
;};

; Make sure cbuffer reflection is correct with High level: struct { [rows x <cols x float>] }
; CHECK: ID3D12ShaderReflectionConstantBuffer:
; CHECK-NEXT:        D3D12_SHADER_BUFFER_DESC: Name: s
; CHECK-NEXT:          Type: D3D_CT_CBUFFER
; CHECK-NEXT:          Size: 160
; CHECK-NEXT:          uFlags: 0
; CHECK-NEXT:          Num Variables: 1
; CHECK-NEXT:        {
; CHECK-NEXT:          ID3D12ShaderReflectionVariable:
; CHECK-NEXT:            D3D12_SHADER_VARIABLE_DESC: Name: s
; CHECK-NEXT:              Size: 160
; CHECK-NEXT:              StartOffset: 0
; CHECK-NEXT:              uFlags: (D3D_SVF_USED)
; CHECK-NEXT:              DefaultValue: <nullptr>
; CHECK-NEXT:            ID3D12ShaderReflectionType:
; CHECK-NEXT:              D3D12_SHADER_TYPE_DESC: Name: S2
; CHECK-NEXT:                Class: D3D_SVC_STRUCT
; CHECK-NEXT:                Type: D3D_SVT_VOID
; CHECK-NEXT:                Elements: 0
; CHECK-NEXT:                Rows: 1
; CHECK-NEXT:                Columns: 21
; CHECK-NEXT:                Members: 3
; CHECK-NEXT:                Offset: 0
; CHECK-NEXT:              {
; CHECK-NEXT:                ID3D12ShaderReflectionType:
; CHECK-NEXT:                  D3D12_SHADER_TYPE_DESC: Name: int4x3
; CHECK-NEXT:                    Class: D3D_SVC_MATRIX_ROWS
; CHECK-NEXT:                    Type: D3D_SVT_INT
; CHECK-NEXT:                    Elements: 0
; CHECK-NEXT:                    Rows: 4
; CHECK-NEXT:                    Columns: 3
; CHECK-NEXT:                    Members: 0
; CHECK-NEXT:                    Offset: 0
; CHECK-NEXT:                ID3D12ShaderReflectionType:
; CHECK-NEXT:                  D3D12_SHADER_TYPE_DESC: Name: int1x3
; CHECK-NEXT:                    Class: D3D_SVC_MATRIX_COLUMNS
; CHECK-NEXT:                    Type: D3D_SVT_INT
; CHECK-NEXT:                    Elements: 2
; CHECK-NEXT:                    Rows: 1
; CHECK-NEXT:                    Columns: 3
; CHECK-NEXT:                    Members: 0
; CHECK-NEXT:                    Offset: 64
; CHECK-NEXT:                ID3D12ShaderReflectionType:
; CHECK-NEXT:                  D3D12_SHADER_TYPE_DESC: Name: int3x1
; CHECK-NEXT:                    Class: D3D_SVC_MATRIX_COLUMNS
; CHECK-NEXT:                    Type: D3D_SVT_INT
; CHECK-NEXT:                    Elements: 0
; CHECK-NEXT:                    Rows: 3
; CHECK-NEXT:                    Columns: 1
; CHECK-NEXT:                    Members: 0
; CHECK-NEXT:                    Offset: 148
; CHECK-NEXT:              }
; CHECK-NEXT:            CBuffer: s

%hostlayout.s = type { %hostlayout.struct.S2 }

%hostlayout.struct.S2 = type { %class.matrix.int.4.3, [2 x %class.matrix.int.1.3], %class.matrix.int.3.1 }
%class.matrix.int.4.3 = type { [4 x <3 x i32>] }
%class.matrix.int.1.3 = type { [1 x <3 x i32>] }
%class.matrix.int.3.1 = type { [3 x <1 x i32>] }

@s_legacy = external global %hostlayout.s

; Function Attrs: nounwind
define float @"\01?bar@@YAMH@Z"(i32 %i) #0 {
; force use cb.
  %1 = load %hostlayout.s, %hostlayout.s* @s_legacy
  ret float 1.000000e+00
}

attributes #0 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.typeAnnotations = !{!7, !17}
!dx.entryPoints = !{!24}

!0 = !{!"dxc(private) 1.7.0.4206 (mat_ref_test, 0a141e7cc-dirty)"}
!1 = !{i32 1, i32 3}
!2 = !{i32 1, i32 7}
!3 = !{!"lib", i32 6, i32 3}
!4 = !{null, null, !5, null}
!5 = !{!6}
!6 = !{i32 0, %hostlayout.s* @s_legacy, !"s", i32 -1, i32 -1, i32 1, i32 160, null}
!7 = !{i32 0, %hostlayout.struct.S2 undef, !8, %hostlayout.s undef, !15}
!8 = !{i32 160, !9, !11, !13}
!9 = !{i32 6, !"m", i32 2, !10, i32 3, i32 0, i32 7, i32 4}
!10 = !{i32 4, i32 3, i32 1}
!11 = !{i32 6, !"m1", i32 2, !12, i32 3, i32 64, i32 7, i32 4}
!12 = !{i32 1, i32 3, i32 2}
!13 = !{i32 6, !"m2", i32 2, !14, i32 3, i32 148, i32 7, i32 4}
!14 = !{i32 3, i32 1, i32 2}
!15 = !{i32 160, !16}
!16 = !{i32 6, !"s", i32 3, i32 0}
!17 = !{i32 1, float (i32)* @"\01?bar@@YAMH@Z", !18}
!18 = !{!19, !22}
!19 = !{i32 1, !20, !21}
!20 = !{i32 7, i32 9}
!21 = !{}
!22 = !{i32 0, !23, !21}
!23 = !{i32 7, i32 4}
!24 = !{null, !"", null, !4, null}
