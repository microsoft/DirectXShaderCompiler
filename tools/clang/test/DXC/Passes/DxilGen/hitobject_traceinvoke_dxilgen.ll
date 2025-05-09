; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s
; REQUIRES: dxil-1-9

;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; RTAS                              texture     i32         ras      T0t4294967295,space4294967295     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.RaytracingAccelerationStructure = type { i32 }
%struct.Payload = type { <3 x float> }
%dx.types.HitObject = type { i8* }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%"class.RWStructuredBuffer<float>" = type { float }
%struct.RayDesc = type { <3 x float>, float, <3 x float>, float }
%"class.dx::HitObject" = type { i32 }

@"\01?RTAS@@3URaytracingAccelerationStructure@@A" = external global %struct.RaytracingAccelerationStructure, align 4

; Function Attrs: nounwind
define void @"\01?main@@YAXXZ"() #0 {
entry:
  %pld = alloca %struct.Payload, align 4
; CHECK: %[[PLDA:[^ ]+]] = alloca %struct.Payload, align 4
  %hit = alloca %dx.types.HitObject, align 4
; CHECK: %[[HITA:[^ ]+]] = alloca %dx.types.HitObject, align 4
; CHECK-NOT: alloca
  %0 = bitcast %struct.Payload* %pld to i8*, !dbg !31 ; line:88 col:3
  call void @llvm.lifetime.start(i64 12, i8* %0) #0, !dbg !31 ; line:88 col:3
  %dummy = getelementptr inbounds %struct.Payload, %struct.Payload* %pld, i32 0, i32 0, !dbg !35 ; line:89 col:7
  store <3 x float> <float 7.000000e+00, float 8.000000e+00, float 9.000000e+00>, <3 x float>* %dummy, align 4, !dbg !36, !tbaa !37 ; line:89 col:13
  %1 = bitcast %dx.types.HitObject* %hit to i8*, !dbg !40 ; line:91 col:3
  call void @llvm.lifetime.start(i64 4, i8* %1) #0, !dbg !40 ; line:91 col:3
  %2 = load %struct.RaytracingAccelerationStructure, %struct.RaytracingAccelerationStructure* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", !dbg !41 ; line:91 col:23
  %3 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RaytracingAccelerationStructure)"(i32 0, %struct.RaytracingAccelerationStructure %2), !dbg !41 ; line:91 col:23
  %4 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RaytracingAccelerationStructure)"(i32 14, %dx.types.Handle %3, %dx.types.ResourceProperties { i32 16, i32 0 }, %struct.RaytracingAccelerationStructure zeroinitializer), !dbg !41 ; line:91 col:23
  call void @"dx.hl.op..void (i32, %dx.types.HitObject*, %dx.types.Handle, i32, i32, i32, i32, i32, <3 x float>, float, <3 x float>, float, %struct.Payload*)"(i32 389, %dx.types.HitObject* %hit, %dx.types.Handle %4, i32 513, i32 1, i32 2, i32 4, i32 0, <3 x float> <float 0.000000e+00, float 1.000000e+00, float 2.000000e+00>, float 3.000000e+00, <3 x float> <float 4.000000e+00, float 5.000000e+00, float 6.000000e+00>, float 7.000000e+00, %struct.Payload* %pld), !dbg !41 ; line:91 col:23
; CHECK: %[[TRACEHO:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_TraceRay.struct.Payload(i32 262, %dx.types.Handle %{{[^ ]+}}, i32 513, i32 1, i32 2, i32 4, i32 0, float 0.000000e+00, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00, %struct.Payload* %[[PLDA]])
; CHECK: store %dx.types.HitObject %[[TRACEHO]], %dx.types.HitObject* %[[HITA]]
  call void @"dx.hl.op..void (i32, %dx.types.HitObject*, %struct.Payload*)"(i32 382, %dx.types.HitObject* %hit, %struct.Payload* %pld), !dbg !42 ; line:101 col:3
; CHECK: %[[INVHIT:[^ ]+]] = load %dx.types.HitObject, %dx.types.HitObject* %[[HITA]]
; CHECK: call void @dx.op.hitObject_Invoke.struct.Payload(i32 267, %dx.types.HitObject %[[INVHIT]], %struct.Payload* %[[PLDA]])
  %5 = bitcast %dx.types.HitObject* %hit to i8*, !dbg !43 ; line:102 col:1
  call void @llvm.lifetime.end(i64 4, i8* %5) #0, !dbg !43 ; line:102 col:1
  %6 = bitcast %struct.Payload* %pld to i8*, !dbg !43 ; line:102 col:1
  call void @llvm.lifetime.end(i64 12, i8* %6) #0, !dbg !43 ; line:102 col:1
  ret void, !dbg !43 ; line:102 col:1
}

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #0

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RaytracingAccelerationStructure)"(i32, %struct.RaytracingAccelerationStructure) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RaytracingAccelerationStructure)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RaytracingAccelerationStructure) #1

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, %dx.types.HitObject*, %struct.Payload*)"(i32, %dx.types.HitObject*, %struct.Payload*) #0

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, %dx.types.HitObject*, %dx.types.Handle, i32, i32, i32, i32, i32, <3 x float>, float, <3 x float>, float, %struct.Payload*)"(i32, %dx.types.HitObject*, %dx.types.Handle, i32, i32, i32, i32, i32, <3 x float>, float, <3 x float>, float, %struct.Payload*) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.module.flags = !{!0}
!pauseresume = !{!1}
!dx.version = !{!2}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.typeAnnotations = !{!4, !19}
!dx.entryPoints = !{!23}
!dx.fnprops = !{!28}
!dx.options = !{!29, !30}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!2 = !{i32 1, i32 9}
!3 = !{!"lib", i32 6, i32 9}
!4 = !{i32 0, %"class.RWStructuredBuffer<float>" undef, !5, %struct.RayDesc undef, !10, %struct.Payload undef, !15, %"class.dx::HitObject" undef, !17}
!5 = !{i32 4, !6, !7}
!6 = !{i32 6, !"h", i32 3, i32 0, i32 7, i32 9}
!7 = !{i32 0, !8}
!8 = !{!9}
!9 = !{i32 0, float undef}
!10 = !{i32 32, !11, !12, !13, !14}
!11 = !{i32 6, !"Origin", i32 3, i32 0, i32 7, i32 9, i32 13, i32 3}
!12 = !{i32 6, !"TMin", i32 3, i32 12, i32 7, i32 9}
!13 = !{i32 6, !"Direction", i32 3, i32 16, i32 7, i32 9, i32 13, i32 3}
!14 = !{i32 6, !"TMax", i32 3, i32 28, i32 7, i32 9}
!15 = !{i32 12, !16}
!16 = !{i32 6, !"dummy", i32 3, i32 0, i32 7, i32 9, i32 13, i32 3}
!17 = !{i32 4, !18}
!18 = !{i32 6, !"h", i32 3, i32 0, i32 7, i32 4}
!19 = !{i32 1, void ()* @"\01?main@@YAXXZ", !20}
!20 = !{!21}
!21 = !{i32 1, !22, !22}
!22 = !{}
!23 = !{null, !"", null, !24, null}
!24 = !{!25, null, null, null}
!25 = !{!26}
!26 = !{i32 0, %struct.RaytracingAccelerationStructure* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", !"RTAS", i32 -1, i32 -1, i32 1, i32 16, i32 0, !27}
!27 = !{i32 0, i32 4}
!28 = !{void ()* @"\01?main@@YAXXZ", i32 7}
!29 = !{i32 -2147483584}
!30 = !{i32 -1}
!31 = !DILocation(line: 88, column: 3, scope: !32)
!32 = !DISubprogram(name: "main", scope: !33, file: !33, line: 81, type: !34, isLocal: false, isDefinition: true, scopeLine: 81, flags: DIFlagPrototyped, isOptimized: false, function: void ()* @"\01?main@@YAXXZ")
!33 = !DIFile(filename: "tools\5Cclang\5Ctest\5CCodeGenDXIL\5Chlsl\5Cobjects\5CHitObject\5Chitobject_traceinvoke.hlsl", directory: "")
!34 = !DISubroutineType(types: !22)
!35 = !DILocation(line: 89, column: 7, scope: !32)
!36 = !DILocation(line: 89, column: 13, scope: !32)
!37 = !{!38, !38, i64 0}
!38 = !{!"omnipotent char", !39, i64 0}
!39 = !{!"Simple C/C++ TBAA"}
!40 = !DILocation(line: 91, column: 3, scope: !32)
!41 = !DILocation(line: 91, column: 23, scope: !32)
!42 = !DILocation(line: 101, column: 3, scope: !32)
!43 = !DILocation(line: 102, column: 1, scope: !32)
