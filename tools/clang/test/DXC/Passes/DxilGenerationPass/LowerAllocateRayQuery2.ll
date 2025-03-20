; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s
; generated the IR with:
; ExtractIRForPassTest.py -p dxilgen -o LowerAllocateRayQuery2.ll tools\clang\test\CodeGenDXIL\hlsl\objects\RayQuery\allocateRayQuery2.hlsl -- -T lib_6_9

; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%"class.StructuredBuffer<vector<float, 3> >" = type { <3 x float> }
%struct.RaytracingAccelerationStructure = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%struct.VSOutput = type { <4 x float> }
%struct.VSInput = type { <3 x float>, i32 }
%struct.RayDesc = type { <3 x float>, float, <3 x float>, float }
%"class.RayQuery<1024, 1>" = type { i32 }
%"class.RayQuery<1, 0>" = type { i32 }

@"\01?vertexOffsets@@3V?$StructuredBuffer@V?$vector@M$02@@@@A" = external global %"class.StructuredBuffer<vector<float, 3> >", align 4
@"\01?RTAS@@3URaytracingAccelerationStructure@@A" = external global %struct.RaytracingAccelerationStructure, align 4

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RaytracingAccelerationStructure)"(i32, %struct.RaytracingAccelerationStructure) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RaytracingAccelerationStructure)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RaytracingAccelerationStructure) #1

; Function Attrs: nounwind readnone
declare <3 x float>* @"dx.hl.subscript.[].rn.<3 x float>* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<vector<float, 3> >\22)"(i32, %"class.StructuredBuffer<vector<float, 3> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<vector<float, 3> >\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.StructuredBuffer<vector<float, 3> >") #1

; Function Attrs: nounwind
declare i32 @"dx.hl.op..i32 (i32, i32, i32)"(i32, i32, i32) #0

; Function Attrs: nounwind
define void @main(<4 x float>* noalias, <3 x float>, i32, <3 x float>, float, <3 x float>, float) #0 {
entry:
  ; CHECK: call i32 @dx.op.allocateRayQuery2(i32 258, i32 1024, i32 1)
  %rayQuery14 = call i32 @"dx.hl.op..i32 (i32, i32, i32)"(i32 4, i32 1024, i32 1), !dbg !60 ; line:25 col:79
  %7 = load %struct.RaytracingAccelerationStructure, %struct.RaytracingAccelerationStructure* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", !dbg !64 ; line:27 col:3
  %8 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RaytracingAccelerationStructure)"(i32 0, %struct.RaytracingAccelerationStructure %7), !dbg !64 ; line:27 col:3
  %9 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RaytracingAccelerationStructure)"(i32 14, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 16, i32 0 }, %struct.RaytracingAccelerationStructure zeroinitializer), !dbg !64 ; line:27 col:3
  call void @"dx.hl.op..void (i32, i32, %dx.types.Handle, i32, i32, <3 x float>, float, <3 x float>, float)"(i32 320, i32 %rayQuery14, %dx.types.Handle %9, i32 1024, i32 2, <3 x float> %3, float %4, <3 x float> %5, float %6), !dbg !64 ; line:27 col:3

  ; CHECK: call i32 @dx.op.allocateRayQuery(i32 178, i32 1)
  %rayQuery25 = call i32 @"dx.hl.op..i32 (i32, i32, i32)"(i32 4, i32 1, i32 0), !dbg !65 ; line:31 col:35
  %10 = load %struct.RaytracingAccelerationStructure, %struct.RaytracingAccelerationStructure* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", !dbg !66 ; line:32 col:3
  %11 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RaytracingAccelerationStructure)"(i32 0, %struct.RaytracingAccelerationStructure %10), !dbg !66 ; line:32 col:3
  %12 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RaytracingAccelerationStructure)"(i32 14, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 16, i32 0 }, %struct.RaytracingAccelerationStructure zeroinitializer), !dbg !66 ; line:32 col:3
  call void @"dx.hl.op..void (i32, i32, %dx.types.Handle, i32, i32, <3 x float>, float, <3 x float>, float)"(i32 320, i32 %rayQuery25, %dx.types.Handle %12, i32 0, i32 2, <3 x float> %3, float %4, <3 x float> %5, float %6), !dbg !66 ; line:32 col:3
  %13 = load %"class.StructuredBuffer<vector<float, 3> >", %"class.StructuredBuffer<vector<float, 3> >"* @"\01?vertexOffsets@@3V?$StructuredBuffer@V?$vector@M$02@@@@A", !dbg !67 ; line:37 col:35
  %14 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<vector<float, 3> >\22)"(i32 0, %"class.StructuredBuffer<vector<float, 3> >" %13), !dbg !67 ; line:37 col:35
  %15 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<vector<float, 3> >\22)"(i32 14, %dx.types.Handle %14, %dx.types.ResourceProperties { i32 12, i32 12 }, %"class.StructuredBuffer<vector<float, 3> >" zeroinitializer), !dbg !67 ; line:37 col:35
  %16 = call <3 x float>* @"dx.hl.subscript.[].rn.<3 x float>* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %15, i32 %2), !dbg !67 ; line:37 col:35
  %17 = load <3 x float>, <3 x float>* %16, !dbg !67, !tbaa !68 ; line:37 col:35
  %add = fadd <3 x float> %1, %17, !dbg !71 ; line:37 col:33
  %18 = extractelement <3 x float> %add, i64 0, !dbg !72 ; line:37 col:22
  %19 = extractelement <3 x float> %add, i64 1, !dbg !72 ; line:37 col:22
  %20 = extractelement <3 x float> %add, i64 2, !dbg !72 ; line:37 col:22
  %21 = insertelement <4 x float> undef, float %18, i64 0, !dbg !72 ; line:37 col:22
  %22 = insertelement <4 x float> %21, float %19, i64 1, !dbg !72 ; line:37 col:22
  %23 = insertelement <4 x float> %22, float %20, i64 2, !dbg !72 ; line:37 col:22
  %24 = insertelement <4 x float> %23, float 1.000000e+00, i64 3, !dbg !72 ; line:37 col:22
  store <4 x float> %24, <4 x float>* %0, !dbg !73 ; line:39 col:10
  ret void, !dbg !74 ; line:40 col:1
}

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, i32, %dx.types.Handle, i32, i32, <3 x float>, float, <3 x float>, float)"(i32, i32, %dx.types.Handle, i32, i32, <3 x float>, float, <3 x float>, float) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.module.flags = !{!0}
!pauseresume = !{!1}
!llvm.ident = !{!2}
!dx.version = !{!3}
!dx.valver = !{!3}
!dx.shaderModel = !{!4}
!dx.typeAnnotations = !{!5, !31}
!dx.entryPoints = !{!50}
!dx.fnprops = !{!57}
!dx.options = !{!58, !59}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!2 = !{!"dxc(private) 1.8.0.4853 (lowerOMM, ca5df957eb33-dirty)"}
!3 = !{i32 1, i32 9}
!4 = !{!"lib", i32 6, i32 9}
!5 = !{i32 0, %"class.StructuredBuffer<vector<float, 3> >" undef, !6, %struct.VSOutput undef, !11, %struct.VSInput undef, !13, %struct.RayDesc undef, !16, %"class.RayQuery<1024, 1>" undef, !21, %"class.RayQuery<1, 0>" undef, !27}
!6 = !{i32 12, !7, !8}
!7 = !{i32 6, !"h", i32 3, i32 0, i32 7, i32 9, i32 13, i32 3}
!8 = !{i32 0, !9}
!9 = !{!10}
!10 = !{i32 0, <3 x float> undef}
!11 = !{i32 16, !12}
!12 = !{i32 6, !"pos", i32 3, i32 0, i32 4, !"SV_POSITION", i32 7, i32 9, i32 13, i32 4}
!13 = !{i32 16, !14, !15}
!14 = !{i32 6, !"pos", i32 3, i32 0, i32 4, !"POSITION", i32 7, i32 9, i32 13, i32 3}
!15 = !{i32 6, !"id", i32 3, i32 12, i32 4, !"SV_VertexID", i32 7, i32 5}
!16 = !{i32 32, !17, !18, !19, !20}
!17 = !{i32 6, !"Origin", i32 3, i32 0, i32 7, i32 9, i32 13, i32 3}
!18 = !{i32 6, !"TMin", i32 3, i32 12, i32 7, i32 9}
!19 = !{i32 6, !"Direction", i32 3, i32 16, i32 7, i32 9, i32 13, i32 3}
!20 = !{i32 6, !"TMax", i32 3, i32 28, i32 7, i32 9}
!21 = !{i32 4, !22, !23}
!22 = !{i32 6, !"h", i32 3, i32 0, i32 7, i32 5}
!23 = !{i32 0, !24}
!24 = !{!25, !26}
!25 = !{i32 1, i64 1024}
!26 = !{i32 1, i64 1}
!27 = !{i32 4, !22, !28}
!28 = !{i32 0, !29}
!29 = !{!26, !30}
!30 = !{i32 1, i64 0}
!31 = !{i32 1, void (<4 x float>*, <3 x float>, i32, <3 x float>, float, <3 x float>, float)* @main, !32}
!32 = !{!33, !35, !38, !40, !42, !44, !46, !48}
!33 = !{i32 0, !34, !34}
!34 = !{}
!35 = !{i32 1, !36, !37}
!36 = !{i32 4, !"SV_POSITION", i32 7, i32 9}
!37 = !{i32 0}
!38 = !{i32 0, !39, !37}
!39 = !{i32 4, !"POSITION", i32 7, i32 9}
!40 = !{i32 0, !41, !37}
!41 = !{i32 4, !"SV_VertexID", i32 7, i32 5}
!42 = !{i32 0, !43, !37}
!43 = !{i32 4, !"RAYDESC", i32 7, i32 9}
!44 = !{i32 0, !43, !45}
!45 = !{i32 1}
!46 = !{i32 0, !43, !47}
!47 = !{i32 2}
!48 = !{i32 0, !43, !49}
!49 = !{i32 3}
!50 = !{null, !"", null, !51, null}
!51 = !{!52, null, null, null}
!52 = !{!53, !55}
!53 = !{i32 0, %"class.StructuredBuffer<vector<float, 3> >"* @"\01?vertexOffsets@@3V?$StructuredBuffer@V?$vector@M$02@@@@A", !"vertexOffsets", i32 0, i32 0, i32 1, i32 12, i32 0, !54}
!54 = !{i32 1, i32 12}
!55 = !{i32 1, %struct.RaytracingAccelerationStructure* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", !"RTAS", i32 -1, i32 -1, i32 1, i32 16, i32 0, !56}
!56 = !{i32 0, i32 4}
!57 = !{void (<4 x float>*, <3 x float>, i32, <3 x float>, float, <3 x float>, float)* @main, i32 1}
!58 = !{i32 -2147483584}
!59 = !{i32 -1}
!60 = !DILocation(line: 25, column: 79, scope: !61)
!61 = !DISubprogram(name: "main", scope: !62, file: !62, line: 21, type: !63, isLocal: false, isDefinition: true, scopeLine: 21, flags: DIFlagPrototyped, isOptimized: false, function: void (<4 x float>*, <3 x float>, i32, <3 x float>, float, <3 x float>, float)* @main)
!62 = !DIFile(filename: "tools\5Cclang\5Ctest\5CCodeGenDXIL\5Chlsl\5Cobjects\5CRayQuery\5CallocateRayQuery2.hlsl", directory: "")
!63 = !DISubroutineType(types: !34)
!64 = !DILocation(line: 27, column: 3, scope: !61)
!65 = !DILocation(line: 31, column: 35, scope: !61)
!66 = !DILocation(line: 32, column: 3, scope: !61)
!67 = !DILocation(line: 37, column: 35, scope: !61)
!68 = !{!69, !69, i64 0}
!69 = !{!"omnipotent char", !70, i64 0}
!70 = !{!"Simple C/C++ TBAA"}
!71 = !DILocation(line: 37, column: 33, scope: !61)
!72 = !DILocation(line: 37, column: 22, scope: !61)
!73 = !DILocation(line: 39, column: 10, scope: !61)
!74 = !DILocation(line: 40, column: 1, scope: !61)
