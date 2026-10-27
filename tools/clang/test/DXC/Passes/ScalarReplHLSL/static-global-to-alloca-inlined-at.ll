; RUN: %dxopt %s -hlsl-passes-resume -static-global-to-alloca -S | FileCheck %s

; A localized static global gets a dbg.declare for every subprogram it is used
; in. Each declare in an inlined subprogram must carry the inlinedAt chain back
; to main, or its location points at a different subprogram than the function
; holding it, which the verifier rejects. A subprogram inlined at two call
; sites shares one variable, with one declare per inlined instance.

; Generated from:
;
;   static uint g_index;
;
;   uint helper(uint x) {
;     return x * g_index;
;   }
;
;   uint main(uint a : A) : SV_Target {
;     g_index = a;
;     return helper(3) + helper(5);
;   }
;
; with dxc -T ps_6_0 -Zi -Od -fcgl, followed by the passes that run before
; -static-global-to-alloca.

; The order of the declares depends on the order of the global's users, so
; only check that there are three, and match their locations by scope.
; CHECK: %[[alloca:[0-9]+]] = alloca i32
; CHECK-NEXT: call void @llvm.dbg.declare(metadata i32* %[[alloca]],
; CHECK-NEXT: call void @llvm.dbg.declare(metadata i32* %[[alloca]],
; CHECK-NEXT: call void @llvm.dbg.declare(metadata i32* %[[alloca]],
; CHECK-NOT: call void @llvm.dbg.declare(metadata i32* %[[alloca]],

; CHECK-DAG: ![[main_scope:[0-9]+]] = !DISubprogram(name: "main"
; CHECK-DAG: ![[helper_scope:[0-9]+]] = !DISubprogram(name: "helper"

; One variable for main, and one for helper.
; CHECK-DAG: = !DILocalVariable(tag: DW_TAG_arg_variable, name: "global.g_index", arg: 0, scope: ![[main_scope]],
; CHECK-DAG: = !DILocalVariable(tag: DW_TAG_arg_variable, name: "global.g_index", arg: 0, scope: ![[helper_scope]],

; The declare in main itself has no inlinedAt, while each one in an inlined
; instance of helper has the location of its call site.
; CHECK-DAG: = !DILocation(line: 1, scope: ![[main_scope]])
; CHECK-DAG: = !DILocation(line: 1, scope: ![[helper_scope]], inlinedAt: ![[call0:[0-9]+]])
; CHECK-DAG: = !DILocation(line: 1, scope: ![[helper_scope]], inlinedAt: ![[call1:[0-9]+]])
; CHECK-DAG: ![[call0]] = distinct !DILocation(line: 9, column: {{10|22}}, scope: ![[main_scope]])
; CHECK-DAG: ![[call1]] = distinct !DILocation(line: 9, column: {{10|22}}, scope: ![[main_scope]])

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%ConstantBuffer = type opaque

@g_index = internal global i32 0, align 4
@"$Globals" = external constant %ConstantBuffer

; Function Attrs: nounwind
define i32 @main(i32 %a) #0 {
entry:
  %retval.i.1 = alloca i32, align 4, !dx.temp !2
  %x.addr.i.2 = alloca i32, align 4, !dx.temp !2
  %retval.i = alloca i32, align 4, !dx.temp !2
  %x.addr.i = alloca i32, align 4, !dx.temp !2
  %retval = alloca i32, align 4, !dx.temp !2
  %a.addr = alloca i32, align 4, !dx.temp !2
  store i32 %a, i32* %a.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %a.addr, metadata !35, metadata !36), !dbg !37 ; var:"a" !DIExpression()
  %0 = load i32, i32* %a.addr, align 4, !dbg !38 ; line:8 col:13
  store i32 %0, i32* @g_index, align 4, !dbg !39 ; line:8 col:11
  %1 = bitcast i32* %retval.i to i8*, !dbg !40 ; line:9 col:10
  call void @llvm.lifetime.start(i64 4, i8* %1), !dbg !40 ; line:9 col:10
  %2 = bitcast i32* %x.addr.i to i8*, !dbg !40 ; line:9 col:10
  call void @llvm.lifetime.start(i64 4, i8* %2), !dbg !40 ; line:9 col:10
  store i32 3, i32* %x.addr.i, align 4, !dbg !40 ; line:9 col:10
  %3 = load i32, i32* %x.addr.i, align 4, !dbg !41 ; line:4 col:10
  %4 = load i32, i32* @g_index, align 4, !dbg !43 ; line:4 col:14
  %mul.i = mul i32 %3, %4, !dbg !44 ; line:4 col:12
  store i32 %mul.i, i32* %retval.i, !dbg !45 ; line:4 col:3
  %5 = load i32, i32* %retval.i, !dbg !45 ; line:4 col:3
  %6 = bitcast i32* %retval.i to i8*, !dbg !45 ; line:4 col:3
  call void @llvm.lifetime.end(i64 4, i8* %6), !dbg !45 ; line:4 col:3
  %7 = bitcast i32* %x.addr.i to i8*, !dbg !45 ; line:4 col:3
  call void @llvm.lifetime.end(i64 4, i8* %7), !dbg !45 ; line:4 col:3
  %8 = bitcast i32* %retval.i.1 to i8*, !dbg !46 ; line:9 col:22
  call void @llvm.lifetime.start(i64 4, i8* %8), !dbg !46 ; line:9 col:22
  %9 = bitcast i32* %x.addr.i.2 to i8*, !dbg !46 ; line:9 col:22
  call void @llvm.lifetime.start(i64 4, i8* %9), !dbg !46 ; line:9 col:22
  store i32 5, i32* %x.addr.i.2, align 4, !dbg !46 ; line:9 col:22
  %10 = load i32, i32* %x.addr.i.2, align 4, !dbg !47 ; line:4 col:10
  %11 = load i32, i32* @g_index, align 4, !dbg !49 ; line:4 col:14
  %mul.i.3 = mul i32 %10, %11, !dbg !50 ; line:4 col:12
  store i32 %mul.i.3, i32* %retval.i.1, !dbg !51 ; line:4 col:3
  %12 = load i32, i32* %retval.i.1, !dbg !51 ; line:4 col:3
  %13 = bitcast i32* %retval.i.1 to i8*, !dbg !51 ; line:4 col:3
  call void @llvm.lifetime.end(i64 4, i8* %13), !dbg !51 ; line:4 col:3
  %14 = bitcast i32* %x.addr.i.2 to i8*, !dbg !51 ; line:4 col:3
  call void @llvm.lifetime.end(i64 4, i8* %14), !dbg !51 ; line:4 col:3
  %add = add i32 %5, %12, !dbg !52 ; line:9 col:20
  store i32 %add, i32* %retval, !dbg !53 ; line:9 col:3
  %15 = load i32, i32* %retval, !dbg !53 ; line:9 col:3
  call void @llvm.dbg.declare(metadata i32* %x.addr.i, metadata !54, metadata !36), !dbg !55 ; var:"x" !DIExpression()
  call void @llvm.dbg.declare(metadata i32* %x.addr.i.2, metadata !54, metadata !36), !dbg !56 ; var:"x" !DIExpression()
  ret i32 %15, !dbg !53 ; line:9 col:3
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!12, !13}
!pauseresume = !{!14}
!llvm.ident = !{!15}
!dx.source.contents = !{!16}
!dx.source.defines = !{!2}
!dx.source.mainFileName = !{!17}
!dx.source.args = !{!18}
!dx.version = !{!19}
!dx.valver = !{!20}
!dx.shaderModel = !{!21}
!dx.typeAnnotations = !{!22}
!dx.entryPoints = !{!28}
!dx.fnprops = !{!32}
!dx.options = !{!33, !34}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "dxc(private)", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, subprograms: !3, globals: !10)
!1 = !DIFile(filename: "static-global-to-alloca-inlined-at.hlsl", directory: "")
!2 = !{}
!3 = !{!4, !9}
!4 = !DISubprogram(name: "main", scope: !1, file: !1, line: 7, type: !5, isLocal: false, isDefinition: true, scopeLine: 7, flags: DIFlagPrototyped, isOptimized: false, function: i32 (i32)* @main)
!5 = !DISubroutineType(types: !6)
!6 = !{!7, !7}
!7 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint", file: !1, baseType: !8)
!8 = !DIBasicType(name: "unsigned int", size: 32, align: 32, encoding: DW_ATE_unsigned)
!9 = !DISubprogram(name: "helper", linkageName: "\01?helper@@YAII@Z", scope: !1, file: !1, line: 3, type: !5, isLocal: false, isDefinition: true, scopeLine: 3, flags: DIFlagPrototyped, isOptimized: false)
!10 = !{!11}
!11 = !DIGlobalVariable(name: "g_index", scope: !0, file: !1, line: 1, type: !7, isLocal: true, isDefinition: true, variable: i32* @g_index)
!12 = !{i32 2, !"Dwarf Version", i32 4}
!13 = !{i32 2, !"Debug Info Version", i32 3}
!14 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!15 = !{!"dxc(private)"}
!16 = !{!"static-global-to-alloca-inlined-at.hlsl", !"static uint g_index;\0A\0Auint helper(uint x) {\0A  return x * g_index;\0A}\0A\0Auint main(uint a : A) : SV_Target {\0A  g_index = a;\0A  return helper(3) + helper(5);\0A}\0A"}
!17 = !{!"static-global-to-alloca-inlined-at.hlsl"}
!18 = !{!"-E", !"main", !"-T", !"ps_6_0", !"-Zi", !"-Od", !"-Qembed_debug", !"-fcgl"}
!19 = !{i32 1, i32 0}
!20 = !{i32 1, i32 11}
!21 = !{!"ps", i32 6, i32 0}
!22 = !{i32 1, i32 (i32)* @main, !23}
!23 = !{!24, !26}
!24 = !{i32 1, !25, !2}
!25 = !{i32 4, !"SV_Target", i32 7, i32 5}
!26 = !{i32 0, !27, !2}
!27 = !{i32 4, !"A", i32 7, i32 5}
!28 = !{i32 (i32)* @main, !"main", null, !29, null}
!29 = !{null, null, !30, null}
!30 = !{!31}
!31 = !{i32 0, %ConstantBuffer* @"$Globals", !"$Globals", i32 0, i32 -1, i32 1, i32 0, null}
!32 = !{i32 (i32)* @main, i32 0, i1 false}
!33 = !{i32 72}
!34 = !{i32 -1}
!35 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "a", arg: 1, scope: !4, file: !1, line: 7, type: !7)
!36 = !DIExpression()
!37 = !DILocation(line: 7, column: 16, scope: !4)
!38 = !DILocation(line: 8, column: 13, scope: !4)
!39 = !DILocation(line: 8, column: 11, scope: !4)
!40 = !DILocation(line: 9, column: 10, scope: !4)
!41 = !DILocation(line: 4, column: 10, scope: !9, inlinedAt: !42)
!42 = distinct !DILocation(line: 9, column: 10, scope: !4)
!43 = !DILocation(line: 4, column: 14, scope: !9, inlinedAt: !42)
!44 = !DILocation(line: 4, column: 12, scope: !9, inlinedAt: !42)
!45 = !DILocation(line: 4, column: 3, scope: !9, inlinedAt: !42)
!46 = !DILocation(line: 9, column: 22, scope: !4)
!47 = !DILocation(line: 4, column: 10, scope: !9, inlinedAt: !48)
!48 = distinct !DILocation(line: 9, column: 22, scope: !4)
!49 = !DILocation(line: 4, column: 14, scope: !9, inlinedAt: !48)
!50 = !DILocation(line: 4, column: 12, scope: !9, inlinedAt: !48)
!51 = !DILocation(line: 4, column: 3, scope: !9, inlinedAt: !48)
!52 = !DILocation(line: 9, column: 20, scope: !4)
!53 = !DILocation(line: 9, column: 3, scope: !4)
!54 = !DILocalVariable(tag: DW_TAG_arg_variable, name: "x", arg: 1, scope: !9, file: !1, line: 3, type: !7)
!55 = !DILocation(line: 3, column: 18, scope: !9, inlinedAt: !42)
!56 = !DILocation(line: 3, column: 18, scope: !9, inlinedAt: !48)
