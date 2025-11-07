; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s

; Source HLSL:
; // dxc -T cs_6_0
; float3x2 M;
; RWStructuredBuffer<float2> SB;
; [numthreads(3,1,1)]
; void main(uint row : SV_GroupIndex) {
;     SB[row] = M[row];
; }

; Check cbuffer column-major matrix subscript codegen uses correct alloca size.

; Column-major matrix subscript is lowered in HLOperationLower by:
; Loading one column into an alloca, dynamically indexing that by row,
; then doing the same for the next column.
; Each column is 3 floats for 3 rows. Thus alloca size should be 3 floats.

; alloca size should be number of rows (3), not number of columns (2)
; CHECK: %[[alloca:[^ ]+]] = alloca [3 x float]
; CHECK: %[[row:[^ ]+]] = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)
; CHECK: %[[cb0:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[cbh:[^ ]+]], i32 0)
; CHECK: %[[_cb_ex0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb0]], 0
; CHECK: %[[_v3x:[^ ]+]] = insertelement <3 x float> undef, float %[[_cb_ex0]], i64 0
; CHECK: %[[_cb_ex1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb0]], 1
; CHECK: %[[_v3xy:[^ ]+]] = insertelement <3 x float> %[[_v3x]], float %[[_cb_ex1]], i64 1
; CHECK: %[[_cb_ex2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb0]], 2
; CHECK: %[[_v3xyz:[^ ]+]] = insertelement <3 x float> %[[_v3xy]], float %[[_cb_ex2]], i64 2
; CHECK: %[[cb0_x:[^ ]+]] = extractelement <3 x float> %[[_v3xyz]], i32 0
; CHECK: %[[_alloca_0:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 0
; CHECK: store float %[[cb0_x]], float* %[[_alloca_0]]
; CHECK: %[[cb0_y:[^ ]+]] = extractelement <3 x float> %[[_v3xyz]], i32 1
; CHECK: %[[_alloca_1:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 1
; CHECK: store float %[[cb0_y]], float* %[[_alloca_1]]
; CHECK: %[[cb0_z:[^ ]+]] = extractelement <3 x float> %[[_v3xyz]], i32 2
; CHECK: %[[_alloca_2:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 2
; CHECK: store float %[[cb0_z]], float* %[[_alloca_2]]
; CHECK: %[[_alloca_row:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 %[[row]]
; CHECK: %[[M_row_0:[^ ]+]] = load float, float* %[[_alloca_row]]
; CHECK: %[[cb1:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %[[cbh]], i32 1)
; CHECK: %[[_cb_ex0:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb1]], 0
; CHECK: %[[_v3x:[^ ]+]] = insertelement <3 x float> undef, float %[[_cb_ex0]], i64 0
; CHECK: %[[_cb_ex1:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb1]], 1
; CHECK: %[[_v3xy:[^ ]+]] = insertelement <3 x float> %[[_v3x]], float %[[_cb_ex1]], i64 1
; CHECK: %[[_cb_ex2:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb1]], 2
; CHECK: %[[_v3xyz:[^ ]+]] = insertelement <3 x float> %[[_v3xy]], float %[[_cb_ex2]], i64 2
; CHECK: %[[cb1_x:[^ ]+]] = extractelement <3 x float> %[[_v3xyz]], i32 0
; CHECK: %[[_alloca_0:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 0
; CHECK: store float %[[cb1_x]], float* %[[_alloca_0]]
; CHECK: %[[cb1_y:[^ ]+]] = extractelement <3 x float> %[[_v3xyz]], i32 1
; CHECK: %[[_alloca_1:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 1
; CHECK: store float %[[cb1_y]], float* %[[_alloca_1]]
; CHECK: %[[cb1_z:[^ ]+]] = extractelement <3 x float> %[[_v3xyz]], i32 2
; CHECK: %[[_alloca_2:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 2
; CHECK: store float %[[cb1_z]], float* %[[_alloca_2]]
; CHECK: %[[_alloca_row:[^ ]+]] = getelementptr inbounds [3 x float], [3 x float]* %[[alloca]], i32 0, i32 %[[row]]
; CHECK: %[[M_row_1:[^ ]+]] = load float, float* %[[_alloca_row]]
; CHECK: %[[M_row_v2x:[^ ]+]] = insertelement <2 x float> undef, float %[[M_row_0]], i64 0
; CHECK: %[[M_row_v2xy:[^ ]+]] = insertelement <2 x float> %[[M_row_v2x]], float %[[M_row_1]], i64 1
; CHECK: %[[ex_M_row_0:[^ ]+]] = extractelement <2 x float> %[[M_row_v2xy]], i64 0
; CHECK: %[[ex_M_row_1:[^ ]+]] = extractelement <2 x float> %[[M_row_v2xy]], i64 1
; CHECK: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %{{[^,]+}}, i32 %[[row]], i32 0, float %[[ex_M_row_0]], float %[[ex_M_row_1]], float undef, float undef, i8 3, i32 4)

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%"class.RWStructuredBuffer<vector<float, 2> >" = type { <2 x float> }
%"$Globals" = type { %class.matrix.float.3.2 }
%class.matrix.float.3.2 = type { [3 x <2 x float>] }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }

@"\01?SB@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A" = external global %"class.RWStructuredBuffer<vector<float, 2> >", align 4
@"$Globals" = external constant %"$Globals"

; Function Attrs: nounwind
define void @main(i32 %row) #0 {
entry:
  %0 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22$Globals\22*, i32)"(i32 0, %"$Globals"* @"$Globals", i32 0)
  %1 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22$Globals\22)"(i32 14, %dx.types.Handle %0, %dx.types.ResourceProperties { i32 13, i32 28 }, %"$Globals" undef)
  %2 = call %"$Globals"* @"dx.hl.subscript.cb.rn.%\22$Globals\22* (i32, %dx.types.Handle, i32)"(i32 6, %dx.types.Handle %1, i32 0)
  %3 = getelementptr inbounds %"$Globals", %"$Globals"* %2, i32 0, i32 0
  %4 = add i32 3, %row
  %5 = call <2 x float>* @"dx.hl.subscript.colMajor[].rn.<2 x float>* (i32, %class.matrix.float.3.2*, i32, i32)"(i32 1, %class.matrix.float.3.2* %3, i32 %row, i32 %4)
  %6 = load <2 x float>, <2 x float>* %5
  %7 = load %"class.RWStructuredBuffer<vector<float, 2> >", %"class.RWStructuredBuffer<vector<float, 2> >"* @"\01?SB@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A"
  %8 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 0, %"class.RWStructuredBuffer<vector<float, 2> >" %7)
  %9 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 14, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 4108, i32 8 }, %"class.RWStructuredBuffer<vector<float, 2> >" zeroinitializer)
  %10 = call <2 x float>* @"dx.hl.subscript.[].rn.<2 x float>* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %9, i32 %row)
  store <2 x float> %6, <2 x float>* %10
  ret void
}

; Function Attrs: nounwind readnone
declare <2 x float>* @"dx.hl.subscript.colMajor[].rn.<2 x float>* (i32, %class.matrix.float.3.2*, i32, i32)"(i32, %class.matrix.float.3.2*, i32, i32) #1

; Function Attrs: nounwind readnone
declare <2 x float>* @"dx.hl.subscript.[].rn.<2 x float>* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32, %"class.RWStructuredBuffer<vector<float, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.RWStructuredBuffer<vector<float, 2> >") #1

; Function Attrs: nounwind readnone
declare %"$Globals"* @"dx.hl.subscript.cb.rn.%\22$Globals\22* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22$Globals\22*, i32)"(i32, %"$Globals"*, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22$Globals\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"$Globals") #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!pauseresume = !{!0}
!llvm.ident = !{!1}
!dx.fnprops = !{!2}
!dx.options = !{!3, !4}
!dx.version = !{!5}
!dx.valver = !{!6}
!dx.shaderModel = !{!7}
!dx.resources = !{!8}
!dx.typeAnnotations = !{!14, !20}
!dx.entryPoints = !{!27}

!0 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!1 = !{!"dxc(private) 1.8.0.15080 (main, c52246122)"}
!2 = !{void (i32)* @main, i32 5, i32 3, i32 1, i32 1}
!3 = !{i32 -2147483584}
!4 = !{i32 -1}
!5 = !{i32 1, i32 0}
!6 = !{i32 1, i32 10}
!7 = !{!"cs", i32 6, i32 0}
!8 = !{null, !9, !12, null}
!9 = !{!10}
!10 = !{i32 0, %"class.RWStructuredBuffer<vector<float, 2> >"* @"\01?SB@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A", !"SB", i32 -1, i32 -1, i32 1, i32 12, i1 false, i1 false, i1 false, !11}
!11 = !{i32 1, i32 8}
!12 = !{!13}
!13 = !{i32 0, %"$Globals"* @"$Globals", !"$Globals", i32 0, i32 -1, i32 1, i32 28, null}
!14 = !{i32 0, %"class.RWStructuredBuffer<vector<float, 2> >" undef, !15, %"$Globals" undef, !17}
!15 = !{i32 8, !16}
!16 = !{i32 6, !"h", i32 3, i32 0, i32 7, i32 9}
!17 = !{i32 28, !18}
!18 = !{i32 6, !"M", i32 2, !19, i32 3, i32 0, i32 7, i32 9}
!19 = !{i32 3, i32 2, i32 2}
!20 = !{i32 1, void (i32)* @main, !21}
!21 = !{!22, !24}
!22 = !{i32 1, !23, !23}
!23 = !{}
!24 = !{i32 0, !25, !26}
!25 = !{i32 4, !"SV_GroupIndex", i32 7, i32 5}
!26 = !{i32 0}
!27 = !{void (i32)* @main, !"main", null, !8, !28}
!28 = !{i32 4, !29}
!29 = !{i32 -858993460, i32 -858993460, i32 -858993460}
