; RUN: %opt %s -dxil-remove-dead-blocks -S | FileCheck %s

; Run the pass, to make sure that %val.0 is deleted since its only use is multiplied by 0.
;  %val.0 = phi float [ 1.000000e+00, %if.then ], [ 0.000000e+00, %entry ]
;  %mul = fmul fast float %val.0, 0.000000e+00, !dbg !28

; CHECK: @main
; CHECK-NOT: phi float

; ModuleID = 'F:\dxc\tools\clang\test\HLSLFileCheck\passes\dxil\dxil_remove_dead_pass\delete_constant_dce.hlsl'
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%cb = type { float }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }

@cb = external constant %cb
@llvm.used = appending global [1 x i8*] [i8* bitcast (%cb* @cb to i8*)], section "llvm.metadata"

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %cb*, i32)"(i32, %cb*, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %cb)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %cb) #0

; Function Attrs: convergent
declare void @dx.noop() #1

; Function Attrs: nounwind
define void @main(float* noalias) #2 {
entry:
  %1 = load %cb, %cb* @cb
  %cb = call %dx.types.Handle @dx.op.createHandleForLib.cb(i32 160, %cb %1)
  %2 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %cb, %dx.types.ResourceProperties { i32 13, i32 4 })
  call void @dx.noop(), !dbg !18
  call void @llvm.dbg.value(metadata float 0.000000e+00, i64 0, metadata !19, metadata !20), !dbg !18
  %3 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 0), !dbg !21
  %4 = extractvalue %dx.types.CBufRet.f32 %3, 0, !dbg !21
  %tobool = fcmp fast une float %4, 0.000000e+00, !dbg !21
  br i1 %tobool, label %if.then, label %if.end, !dbg !23

if.then:                                          ; preds = %entry
  call void @dx.noop(), !dbg !24
  call void @llvm.dbg.value(metadata float 1.000000e+00, i64 0, metadata !19, metadata !20), !dbg !18
  br label %if.end, !dbg !25

if.end:                                           ; preds = %if.then, %entry
  %val.0 = phi float [ 1.000000e+00, %if.then ], [ 0.000000e+00, %entry ]
  call void @llvm.dbg.value(metadata float %val.0, i64 0, metadata !19, metadata !20), !dbg !18
  call void @dx.noop(), !dbg !26
  call void @llvm.dbg.value(metadata float 0.000000e+00, i64 0, metadata !27, metadata !20), !dbg !26
  %mul = fmul fast float %val.0, 0.000000e+00, !dbg !28
  call void @dx.noop(), !dbg !29
  call void @llvm.dbg.value(metadata float %mul, i64 0, metadata !30, metadata !20), !dbg !29
  call void @dx.noop(), !dbg !31
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %mul), !dbg !31
  ret void, !dbg !31
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.value(metadata, i64, metadata, metadata) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #2

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #3

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.cb(i32, %cb) #3

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #0

attributes #0 = { nounwind readnone }
attributes #1 = { convergent }
attributes #2 = { nounwind }
attributes #3 = { nounwind readonly }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!11, !12}
!pauseresume = !{!13}
!llvm.ident = !{!14}
!dx.source.contents = !{!15}
!dx.source.defines = !{!2}
!dx.source.mainFileName = !{!16}
!dx.source.args = !{!17}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "clang version 3.7 (tags/RELEASE_370/final)", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, subprograms: !3, globals: !8)
!1 = !DIFile(filename: "F:\5Cdxc\5Ctools\5Cclang\5Ctest\5CHLSLFileCheck\5Cpasses\5Cdxil\5Cdxil_remove_dead_pass\5Cdelete_constant_dce.hlsl", directory: "")
!2 = !{}
!3 = !{!4}
!4 = !DISubprogram(name: "main", scope: !1, file: !1, line: 18, type: !5, isLocal: false, isDefinition: true, scopeLine: 18, flags: DIFlagPrototyped, isOptimized: false, function: void (float*)* @main)
!5 = !DISubroutineType(types: !6)
!6 = !{!7}
!7 = !DIBasicType(name: "float", size: 32, align: 32, encoding: DW_ATE_float)
!8 = !{!9}
!9 = !DIGlobalVariable(name: "foo", linkageName: "\01?foo@cb@@3MB", scope: !0, file: !1, line: 14, type: !10, isLocal: false, isDefinition: true)
!10 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !7)
!11 = !{i32 2, !"Dwarf Version", i32 4}
!12 = !{i32 2, !"Debug Info Version", i32 3}
!13 = !{!"hlsl-dxilemit", !"hlsl-dxilload"}
!14 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!15 = !{!"F:\5Cdxc\5Ctools\5Cclang\5Ctest\5CHLSLFileCheck\5Cpasses\5Cdxil\5Cdxil_remove_dead_pass\5Cdelete_constant_dce.hlsl", !"// RUN: %dxc %s -T ps_6_0 -Od | FileCheck %s\0D\0A\0D\0A// This test verifies the fix for a deficiency in RemoveDeadBlocks where:\0D\0A//\0D\0A// - Value 'ret' that can be reduced to constant by DxilValueCache is removed\0D\0A// - It held on uses for a PHI 'val', but 'val' was not removed\0D\0A// - 'val' is not used, but also not DCE'ed until after DeleteDeadRegion is run\0D\0A// - DeleteDeadRegion cannot delete 'if (foo)' because 'val' still exists.\0D\0A\0D\0A// CHECK: @main\0D\0A// CHECK-NOT: phi\0D\0A\0D\0Acbuffer cb : register(b0) {\0D\0A  float foo;\0D\0A}\0D\0A\0D\0A[RootSignature(\22\22)]\0D\0Afloat main() : SV_Target {\0D\0A  float val = 0;\0D\0A  if (foo)\0D\0A    val = 1;\0D\0A\0D\0A  float zero = 0;\0D\0A  float ret = val * zero;\0D\0A\0D\0A  return ret;\0D\0A}\0D\0A"}
!16 = !{!"F:\5Cdxc\5Ctools\5Cclang\5Ctest\5CHLSLFileCheck\5Cpasses\5Cdxil\5Cdxil_remove_dead_pass\5Cdelete_constant_dce.hlsl"}
!17 = !{!"-E", !"main", !"-T", !"ps_6_0", !"/Od", !"/Zi", !"-Qembed_debug"}
!18 = !DILocation(line: 19, column: 9, scope: !4)
!19 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "val", scope: !4, file: !1, line: 19, type: !7)
!20 = !DIExpression()
!21 = !DILocation(line: 20, column: 7, scope: !22)
!22 = distinct !DILexicalBlock(scope: !4, file: !1, line: 20, column: 7)
!23 = !DILocation(line: 20, column: 7, scope: !4)
!24 = !DILocation(line: 21, column: 9, scope: !22)
!25 = !DILocation(line: 21, column: 5, scope: !22)
!26 = !DILocation(line: 23, column: 9, scope: !4)
!27 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "zero", scope: !4, file: !1, line: 23, type: !7)
!28 = !DILocation(line: 24, column: 19, scope: !4)
!29 = !DILocation(line: 24, column: 9, scope: !4)
!30 = !DILocalVariable(tag: DW_TAG_auto_variable, name: "ret", scope: !4, file: !1, line: 24, type: !7)
!31 = !DILocation(line: 26, column: 3, scope: !4)
