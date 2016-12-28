; RUN: %dxv %s | FileCheck %s

; CHECK: Multiple GS output streams are used but 'XXX' is not pointlist


target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%"$Globals" = type { i32 }
%struct.MyStruct = type { <4 x float>, <2 x float> }
%struct.MyStruct2 = type { <3 x i32>, [3 x <4 x float>], <3 x i32> }
%class.PointStream = type { %struct.MyStruct2 }
%class.TriangleStream = type { %struct.MyStruct }
%dx.types.Handle = type { i8* }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }

@"\01?g1@@3HA" = global i32 0, align 4
@"$Globals" = external constant %"$Globals"
@dx.typevar.0 = external addrspace(1) constant %struct.MyStruct
@dx.typevar.1 = external addrspace(1) constant %struct.MyStruct2
@dx.typevar.2 = external addrspace(1) constant %"$Globals"
@llvm.used = appending global [5 x i8*] [i8* bitcast (%"$Globals"* @"$Globals" to i8*), i8* bitcast (%"$Globals"* @"$Globals" to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.MyStruct addrspace(1)* @dx.typevar.0 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%struct.MyStruct2 addrspace(1)* @dx.typevar.1 to i8 addrspace(1)*) to i8*), i8* addrspacecast (i8 addrspace(1)* bitcast (%"$Globals" addrspace(1)* @dx.typevar.2 to i8 addrspace(1)*) to i8*)], section "llvm.metadata"

; Function Attrs: nounwind
define void @main.flat([1 x <4 x float>]* nocapture readnone, %class.TriangleStream* nocapture readnone, <4 x float>* nocapture readnone, <2 x float>* nocapture readnone, %class.PointStream* nocapture readnone, <3 x i32>* nocapture readnone, [3 x <4 x float>]* nocapture readnone, <3 x i32>* nocapture readnone, %class.TriangleStream* nocapture readnone, <4 x float>* nocapture readnone, <2 x float>* nocapture readnone) #0 {
entry:
  %11 = call %dx.types.Handle @dx.op.createHandle(i32 58, i8 2, i32 0, i32 0, i1 false)
  %b.1.0 = alloca [3 x float], align 4
  %b.1.1 = alloca [3 x float], align 4
  %b.1.2 = alloca [3 x float], align 4
  %b.1.3 = alloca [3 x float], align 4
  %12 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 0)
  %13 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 0)
  %14 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 0)
  %15 = getelementptr [3 x float], [3 x float]* %b.1.0, i32 0, i32 0
  %16 = getelementptr [3 x float], [3 x float]* %b.1.1, i32 0, i32 0
  %17 = getelementptr [3 x float], [3 x float]* %b.1.2, i32 0, i32 0
  %18 = getelementptr [3 x float], [3 x float]* %b.1.3, i32 0, i32 0
  store float 0.000000e+00, float* %15, align 4
  store float 0.000000e+00, float* %16, align 4
  store float 0.000000e+00, float* %17, align 4
  store float 0.000000e+00, float* %18, align 4
  %19 = getelementptr [3 x float], [3 x float]* %b.1.0, i32 0, i32 1
  %20 = getelementptr [3 x float], [3 x float]* %b.1.1, i32 0, i32 1
  %21 = getelementptr [3 x float], [3 x float]* %b.1.2, i32 0, i32 1
  %22 = getelementptr [3 x float], [3 x float]* %b.1.3, i32 0, i32 1
  store float 0.000000e+00, float* %19, align 4
  store float 0.000000e+00, float* %20, align 4
  store float 0.000000e+00, float* %21, align 4
  store float 0.000000e+00, float* %22, align 4
  %23 = getelementptr [3 x float], [3 x float]* %b.1.0, i32 0, i32 2
  %24 = getelementptr [3 x float], [3 x float]* %b.1.1, i32 0, i32 2
  %25 = getelementptr [3 x float], [3 x float]* %b.1.2, i32 0, i32 2
  %26 = getelementptr [3 x float], [3 x float]* %b.1.3, i32 0, i32 2
  %conv = fptoui float %12 to i32
  %27 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 %conv)
  %28 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 %conv)
  %29 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 %conv)
  %30 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 %conv)
  %conv3.i1 = fptoui float %13 to i32
  %conv3.i2 = fptoui float %14 to i32
  %conv5.i0 = fptoui float %27 to i32
  %conv5.i1 = fptoui float %28 to i32
  %conv5.i2 = fptoui float %29 to i32
  %mul.i0 = fmul fast float %27, 4.400000e+01
  %mul.i1 = fmul fast float %28, 4.400000e+01
  %mul.i2 = fmul fast float %29, 4.400000e+01
  %mul.i3 = fmul fast float %30, 4.400000e+01
  store float %mul.i0, float* %23, align 4
  store float %mul.i1, float* %24, align 4
  store float %mul.i2, float* %25, align 4
  store float %mul.i3, float* %26, align 4
  %31 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 60, %dx.types.Handle %11, i32 0)
  %32 = extractvalue %dx.types.CBufRet.i32 %31, 0
  %tobool = icmp eq i32 %32, 0
  br i1 %tobool, label %if.else, label %if.then

if.then:                                          ; preds = %entry
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %27)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %28)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %29)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %30)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %12)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %13)
  call void @dx.op.emitStream(i32 97, i8 0)
  call void @dx.op.cutStream(i32 98, i8 0)
  br label %if.end

if.else:                                          ; preds = %entry
  %33 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 0)
  %conv8 = fptoui float %33 to i32
  %34 = getelementptr inbounds [3 x float], [3 x float]* %b.1.0, i32 0, i32 0
  %35 = load float, float* %34, align 4
  %36 = getelementptr inbounds [3 x float], [3 x float]* %b.1.1, i32 0, i32 0
  %37 = load float, float* %36, align 4
  %38 = getelementptr inbounds [3 x float], [3 x float]* %b.1.2, i32 0, i32 0
  %39 = load float, float* %38, align 4
  %40 = getelementptr inbounds [3 x float], [3 x float]* %b.1.3, i32 0, i32 0
  %41 = load float, float* %40, align 4
  %42 = getelementptr inbounds [3 x float], [3 x float]* %b.1.0, i32 0, i32 1
  %43 = load float, float* %42, align 4
  %44 = getelementptr inbounds [3 x float], [3 x float]* %b.1.1, i32 0, i32 1
  %45 = load float, float* %44, align 4
  %46 = getelementptr inbounds [3 x float], [3 x float]* %b.1.2, i32 0, i32 1
  %47 = load float, float* %46, align 4
  %48 = getelementptr inbounds [3 x float], [3 x float]* %b.1.3, i32 0, i32 1
  %49 = load float, float* %48, align 4
  %50 = getelementptr inbounds [3 x float], [3 x float]* %b.1.0, i32 0, i32 2
  %51 = load float, float* %50, align 4
  %52 = getelementptr inbounds [3 x float], [3 x float]* %b.1.1, i32 0, i32 2
  %53 = load float, float* %52, align 4
  %54 = getelementptr inbounds [3 x float], [3 x float]* %b.1.2, i32 0, i32 2
  %55 = load float, float* %54, align 4
  %56 = getelementptr inbounds [3 x float], [3 x float]* %b.1.3, i32 0, i32 2
  %57 = load float, float* %56, align 4
  call void @dx.op.storeOutput.i32(i32 5, i32 2, i32 0, i8 0, i32 %conv8)
  call void @dx.op.storeOutput.i32(i32 5, i32 2, i32 0, i8 1, i32 %conv3.i1)
  call void @dx.op.storeOutput.i32(i32 5, i32 2, i32 0, i8 2, i32 %conv3.i2)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 0, float %35)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 1, float %37)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 2, float %39)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 3, float %41)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 0, float %43)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 1, float %45)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 2, float %47)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 3, float %49)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 0, float %51)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 1, float %53)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 2, float %55)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 3, float %57)
  call void @dx.op.storeOutput.i32(i32 5, i32 4, i32 0, i8 0, i32 %conv5.i0)
  call void @dx.op.storeOutput.i32(i32 5, i32 4, i32 0, i8 1, i32 %conv5.i1)
  call void @dx.op.storeOutput.i32(i32 5, i32 4, i32 0, i8 2, i32 %conv5.i2)
  call void @dx.op.emitStream(i32 97, i8 1)
  call void @dx.op.cutStream(i32 98, i8 1)
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %b.0.0.i0 = phi i32 [ %conv, %if.then ], [ %conv8, %if.else ]
  %58 = getelementptr inbounds [3 x float], [3 x float]* %b.1.0, i32 0, i32 0
  %59 = load float, float* %58, align 4
  %60 = getelementptr inbounds [3 x float], [3 x float]* %b.1.1, i32 0, i32 0
  %61 = load float, float* %60, align 4
  %62 = getelementptr inbounds [3 x float], [3 x float]* %b.1.2, i32 0, i32 0
  %63 = load float, float* %62, align 4
  %64 = getelementptr inbounds [3 x float], [3 x float]* %b.1.3, i32 0, i32 0
  %65 = load float, float* %64, align 4
  %66 = getelementptr inbounds [3 x float], [3 x float]* %b.1.0, i32 0, i32 1
  %67 = load float, float* %66, align 4
  %68 = getelementptr inbounds [3 x float], [3 x float]* %b.1.1, i32 0, i32 1
  %69 = load float, float* %68, align 4
  %70 = getelementptr inbounds [3 x float], [3 x float]* %b.1.2, i32 0, i32 1
  %71 = load float, float* %70, align 4
  %72 = getelementptr inbounds [3 x float], [3 x float]* %b.1.3, i32 0, i32 1
  %73 = load float, float* %72, align 4
  %74 = getelementptr inbounds [3 x float], [3 x float]* %b.1.0, i32 0, i32 2
  %75 = load float, float* %74, align 4
  %76 = getelementptr inbounds [3 x float], [3 x float]* %b.1.1, i32 0, i32 2
  %77 = load float, float* %76, align 4
  %78 = getelementptr inbounds [3 x float], [3 x float]* %b.1.2, i32 0, i32 2
  %79 = load float, float* %78, align 4
  %80 = getelementptr inbounds [3 x float], [3 x float]* %b.1.3, i32 0, i32 2
  %81 = load float, float* %80, align 4
  call void @dx.op.storeOutput.i32(i32 5, i32 2, i32 0, i8 0, i32 %b.0.0.i0)
  call void @dx.op.storeOutput.i32(i32 5, i32 2, i32 0, i8 1, i32 %conv3.i1)
  call void @dx.op.storeOutput.i32(i32 5, i32 2, i32 0, i8 2, i32 %conv3.i2)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 0, float %59)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 1, float %61)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 2, float %63)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 3, float %65)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 0, float %67)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 1, float %69)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 2, float %71)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 1, i8 3, float %73)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 0, float %75)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 1, float %77)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 2, float %79)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 2, i8 3, float %81)
  call void @dx.op.storeOutput.i32(i32 5, i32 4, i32 0, i8 0, i32 %conv5.i0)
  call void @dx.op.storeOutput.i32(i32 5, i32 4, i32 0, i8 1, i32 %conv5.i1)
  call void @dx.op.storeOutput.i32(i32 5, i32 4, i32 0, i8 2, i32 %conv5.i2)
  call void @dx.op.emitStream(i32 97, i8 1)
  call void @dx.op.cutStream(i32 98, i8 1)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 0, float %27)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 1, float %28)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 2, float %29)
  call void @dx.op.storeOutput.f32(i32 5, i32 5, i32 0, i8 3, float %30)
  call void @dx.op.storeOutput.f32(i32 5, i32 6, i32 0, i8 0, float %12)
  call void @dx.op.storeOutput.f32(i32 5, i32 6, i32 0, i8 1, float %13)
  call void @dx.op.emitStream(i32 97, i8 2)
  call void @dx.op.cutStream(i32 98, i8 2)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.i32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #1

; Function Attrs: nounwind readnone
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind
declare void @dx.op.cutStream(i32, i8) #0

; Function Attrs: nounwind
declare void @dx.op.emitStream(i32, i8) #0

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!6, !16}
!dx.entryPoints = !{!39}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"gs", i32 5, i32 1}
!3 = !{null, null, !4, null}
!4 = !{!5}
!5 = !{i32 0, %"$Globals"* @"$Globals", !"$Globals", i32 0, i32 0, i32 1, i32 4, null}
!6 = !{i32 0, %struct.MyStruct addrspace(1)* @dx.typevar.0, !7, %struct.MyStruct2 addrspace(1)* @dx.typevar.1, !10, %"$Globals" addrspace(1)* @dx.typevar.2, !14}
!7 = !{i32 24, !8, !9}
!8 = !{i32 3, i32 0, i32 4, !"SV_Position", i32 6, !"pos", i32 7, i32 9}
!9 = !{i32 3, i32 16, i32 4, !"AAA", i32 6, !"a", i32 7, i32 9}
!10 = !{i32 76, !11, !12, !13}
!11 = !{i32 3, i32 0, i32 4, !"XXX", i32 6, !"X", i32 7, i32 5}
!12 = !{i32 3, i32 16, i32 4, !"PPP", i32 6, !"p", i32 7, i32 9}
!13 = !{i32 3, i32 64, i32 4, !"YYY", i32 6, !"Y", i32 7, i32 5}
!14 = !{i32 0, !15}
!15 = !{i32 3, i32 0, i32 6, !"g1", i32 7, i32 4}
!16 = !{i32 1, void ([1 x <4 x float>]*, %class.TriangleStream*, <4 x float>*, <2 x float>*, %class.PointStream*, <3 x i32>*, [3 x <4 x float>]*, <3 x i32>*, %class.TriangleStream*, <4 x float>*, <2 x float>*)* @main.flat, !17}
!17 = !{!18, !20, !23, !24, !26, !28, !29, !31, !34, !36, !37, !38}
!18 = !{i32 0, !19, !19}
!19 = !{}
!20 = !{i32 0, !21, !22}
!21 = !{i32 4, !"COORD", i32 7, i32 9}
!22 = !{i32 0}
!23 = !{i32 5, !19, !19}
!24 = !{i32 5, !25, !22}
!25 = !{i32 4, !"SV_Position", i32 7, i32 9}
!26 = !{i32 5, !27, !22}
!27 = !{i32 4, !"AAA", i32 7, i32 9}
!28 = !{i32 6, !19, !19}
!29 = !{i32 6, !30, !22}
!30 = !{i32 4, !"XXX", i32 7, i32 5}
!31 = !{i32 6, !32, !33}
!32 = !{i32 4, !"PPP", i32 7, i32 9}
!33 = !{i32 0, i32 1, i32 2}
!34 = !{i32 6, !35, !22}
!35 = !{i32 4, !"YYY", i32 7, i32 5}
!36 = !{i32 7, !19, !19}
!37 = !{i32 7, !25, !22}
!38 = !{i32 7, !27, !22}
!39 = !{void ([1 x <4 x float>]*, %class.TriangleStream*, <4 x float>*, <2 x float>*, %class.PointStream*, <3 x i32>*, [3 x <4 x float>]*, <3 x i32>*, %class.TriangleStream*, <4 x float>*, <2 x float>*)* @main.flat, !"", !40, !3, !53}
!40 = !{!41, !43, null}
!41 = !{!42}
!42 = !{i32 0, !"COORD", i8 9, i8 0, !22, i8 2, i32 1, i8 4, i32 0, i8 0, null}
!43 = !{!44, !45, !46, !48, !49, !50, !52}
!44 = !{i32 0, !"SV_Position", i8 9, i8 3, !22, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!45 = !{i32 1, !"AAA", i8 9, i8 0, !22, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!46 = !{i32 2, !"XXX", i8 5, i8 0, !22, i8 1, i32 1, i8 3, i32 0, i8 0, !47}
!47 = !{i32 0, i32 1}
!48 = !{i32 3, !"PPP", i8 9, i8 0, !33, i8 2, i32 3, i8 4, i32 1, i8 0, !47}
!49 = !{i32 4, !"YYY", i8 5, i8 0, !22, i8 1, i32 1, i8 3, i32 4, i8 0, !47}
!50 = !{i32 5, !"SV_Position", i8 9, i8 3, !22, i8 4, i32 1, i8 4, i32 0, i8 0, !51}
!51 = !{i32 0, i32 2}
!52 = !{i32 6, !"AAA", i8 9, i8 0, !22, i8 2, i32 1, i8 2, i32 1, i8 0, !51}
!53 = !{i32 1, !54}
!54 = !{i32 1, i32 12, i32 7, i32 4, i32 1}
