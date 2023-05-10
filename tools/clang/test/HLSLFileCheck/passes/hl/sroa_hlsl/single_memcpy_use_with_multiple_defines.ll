; RUN: %opt %s -scalarrepl-param-hlsl -S | FileCheck %s

; Make sure the 

; CHECK: alloca [2 x %class.matrix.float
; CHECK: alloca [2 x %class.matrix.float
; CHECK-NOT: alloca [2 x %class.matrix.float

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%"$Globals" = type { i32 }
%class.matrix.float.2.2.Col = type { [2 x <2 x float>] }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }
%class.matrix.float.2.2.Row = type { [2 x <2 x float>] }

@"\01?idx@@3HB" = external constant i32, align 4
@"$Globals" = external constant %"$Globals"

; Function Attrs: alwaysinline nounwind
define internal %class.matrix.float.2.2.Col @"\01?lookup@@YA?AV?$matrix@M$01$01$0A@@matrix.internal@@Y01V12@H@Z"([2 x %class.matrix.float.2.2.Col]* %arr, i32 %index) #0 {
  ret %class.matrix.float.2.2.Col undef
}

; Function Attrs: nounwind readonly
declare %class.matrix.float.2.2.Col @"dx.hl.matldst.colLoad.%class.matrix.float.2.2.Col (i32, %class.matrix.float.2.2.Col*)"(i32, %class.matrix.float.2.2.Col*) #1

; Function Attrs: nounwind
declare %class.matrix.float.2.2.Col @"dx.hl.matldst.colStore.%class.matrix.float.2.2.Col (i32, %class.matrix.float.2.2.Col*, %class.matrix.float.2.2.Col)"(i32, %class.matrix.float.2.2.Col*, %class.matrix.float.2.2.Col) #2

; Function Attrs: nounwind
define %class.matrix.float.2.2.Col @main() #2 {
entry:
  %index.addr.i = alloca i32, align 4, !dx.temp !13
  %0 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22$Globals\22*, i32)"(i32 0, %"$Globals"* @"$Globals", i32 0)
  %1 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22$Globals\22)"(i32 11, %dx.types.Handle %0, %dx.types.ResourceProperties { i32 13, i32 4 }, %"$Globals" undef)
  %2 = call %"$Globals"* @"dx.hl.subscript.cb.%\22$Globals\22* (i32, %dx.types.Handle, i32)"(i32 6, %dx.types.Handle %1, i32 0)
  %3 = getelementptr inbounds %"$Globals", %"$Globals"* %2, i32 0, i32 0
  %4 = alloca [2 x %class.matrix.float.2.2.Col]
  %retval = alloca %class.matrix.float.2.2.Col, align 4, !dx.temp !13
  %retval.i = alloca %class.matrix.float.2.2.Col, align 4, !dx.temp !13
  %arr = alloca [2 x %class.matrix.float.2.2.Row], align 4
  %i = alloca i32, align 4
  %agg.tmp = alloca [2 x %class.matrix.float.2.2.Col], align 4
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %5 = load i32, i32* %i, align 4
  %cmp = icmp slt i32 %5, 2
  %tobool = icmp ne i1 %cmp, false
  %tobool1 = icmp ne i1 %tobool, false
  br i1 %tobool1, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %6 = load i32, i32* %i, align 4
  %arrayidx = getelementptr inbounds [2 x %class.matrix.float.2.2.Row], [2 x %class.matrix.float.2.2.Row]* %arr, i32 0, i32 %6
  %7 = load i32, i32* %i, align 4
  %conv = sitofp i32 %7 to float
  %8 = call %class.matrix.float.2.2.Col @"dx.hl.cast.default.%class.matrix.float.2.2.Col (i32, float)"(i32 0, float %conv)
  %9 = call %class.matrix.float.2.2.Row @"dx.hl.cast.colMatToRowMat.%class.matrix.float.2.2.Row (i32, %class.matrix.float.2.2.Col)"(i32 6, %class.matrix.float.2.2.Col %8)
  %10 = call %class.matrix.float.2.2.Row @"dx.hl.matldst.rowStore.%class.matrix.float.2.2.Row (i32, %class.matrix.float.2.2.Row*, %class.matrix.float.2.2.Row)"(i32 3, %class.matrix.float.2.2.Row* %arrayidx, %class.matrix.float.2.2.Row %9)
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %11 = load i32, i32* %i, align 4
  %inc = add nsw i32 %11, 1
  store i32 %inc, i32* %i, align 4
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %12 = getelementptr inbounds [2 x %class.matrix.float.2.2.Row], [2 x %class.matrix.float.2.2.Row]* %arr, i32 0, i32 0
  %13 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %12, <1 x i32> zeroinitializer)
  %14 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %12, <1 x i32> <i32 1>)
  %15 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %12, <1 x i32> <i32 2>)
  %16 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %12, <1 x i32> <i32 3>)
  %17 = getelementptr inbounds [2 x %class.matrix.float.2.2.Row], [2 x %class.matrix.float.2.2.Row]* %arr, i32 0, i32 1
  %18 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %17, <1 x i32> zeroinitializer)
  %19 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %17, <1 x i32> <i32 1>)
  %20 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %17, <1 x i32> <i32 2>)
  %21 = call float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32 4, %class.matrix.float.2.2.Row* %17, <1 x i32> <i32 3>)
  %22 = load float, float* %13
  %23 = load float, float* %14
  %24 = load float, float* %15
  %25 = load float, float* %16
  %26 = load float, float* %18
  %27 = load float, float* %19
  %28 = load float, float* %20
  %29 = load float, float* %21
  %30 = getelementptr inbounds [2 x %class.matrix.float.2.2.Col], [2 x %class.matrix.float.2.2.Col]* %agg.tmp, i32 0, i32 0
  %31 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %30, <1 x i32> zeroinitializer)
  %32 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %30, <1 x i32> <i32 2>)
  %33 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %30, <1 x i32> <i32 1>)
  %34 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %30, <1 x i32> <i32 3>)
  %35 = getelementptr inbounds [2 x %class.matrix.float.2.2.Col], [2 x %class.matrix.float.2.2.Col]* %agg.tmp, i32 0, i32 1
  %36 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %35, <1 x i32> zeroinitializer)
  %37 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %35, <1 x i32> <i32 2>)
  %38 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %35, <1 x i32> <i32 1>)
  %39 = call float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32 3, %class.matrix.float.2.2.Col* %35, <1 x i32> <i32 3>)
  store float %22, float* %31
  store float %23, float* %32
  store float %24, float* %33
  store float %25, float* %34
  store float %26, float* %36
  store float %27, float* %37
  store float %28, float* %38
  store float %29, float* %39
  %40 = bitcast [2 x %class.matrix.float.2.2.Col]* %4 to i8*
  %41 = bitcast [2 x %class.matrix.float.2.2.Col]* %agg.tmp to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %40, i8* %41, i64 32, i32 1, i1 false)
  %42 = load i32, i32* %3, align 4
  store i32 %42, i32* %index.addr.i, align 4
  %43 = load i32, i32* %index.addr.i, align 4
  %arrayidx.i = getelementptr inbounds [2 x %class.matrix.float.2.2.Col], [2 x %class.matrix.float.2.2.Col]* %4, i32 0, i32 %43
  %44 = call %class.matrix.float.2.2.Col @"dx.hl.matldst.colLoad.%class.matrix.float.2.2.Col (i32, %class.matrix.float.2.2.Col*)"(i32 0, %class.matrix.float.2.2.Col* %arrayidx.i) #2
  %45 = call %class.matrix.float.2.2.Col @"dx.hl.matldst.colStore.%class.matrix.float.2.2.Col (i32, %class.matrix.float.2.2.Col*, %class.matrix.float.2.2.Col)"(i32 1, %class.matrix.float.2.2.Col* %retval.i, %class.matrix.float.2.2.Col %44) #2
  %46 = call %class.matrix.float.2.2.Col @"dx.hl.matldst.colLoad.%class.matrix.float.2.2.Col (i32, %class.matrix.float.2.2.Col*)"(i32 0, %class.matrix.float.2.2.Col* %retval.i) #2
  %47 = call %class.matrix.float.2.2.Col @"dx.hl.matldst.colStore.%class.matrix.float.2.2.Col (i32, %class.matrix.float.2.2.Col*, %class.matrix.float.2.2.Col)"(i32 1, %class.matrix.float.2.2.Col* %retval, %class.matrix.float.2.2.Col %46)
  %48 = call %class.matrix.float.2.2.Col @"dx.hl.matldst.colLoad.%class.matrix.float.2.2.Col (i32, %class.matrix.float.2.2.Col*)"(i32 0, %class.matrix.float.2.2.Col* %retval)
  ret %class.matrix.float.2.2.Col %48
}

; Function Attrs: nounwind readnone
declare %class.matrix.float.2.2.Col @"dx.hl.cast.default.%class.matrix.float.2.2.Col (i32, float)"(i32, float) #3

; Function Attrs: nounwind readnone
declare %class.matrix.float.2.2.Row @"dx.hl.cast.colMatToRowMat.%class.matrix.float.2.2.Row (i32, %class.matrix.float.2.2.Col)"(i32, %class.matrix.float.2.2.Col) #3

; Function Attrs: nounwind
declare %class.matrix.float.2.2.Row @"dx.hl.matldst.rowStore.%class.matrix.float.2.2.Row (i32, %class.matrix.float.2.2.Row*, %class.matrix.float.2.2.Row)"(i32, %class.matrix.float.2.2.Row*, %class.matrix.float.2.2.Row) #2

; Function Attrs: nounwind readnone
declare float* @"dx.hl.subscript.rowMajor_m.float* (i32, %class.matrix.float.2.2.Row*, <1 x i32>)"(i32, %class.matrix.float.2.2.Row*, <1 x i32>) #3

; Function Attrs: nounwind readnone
declare float* @"dx.hl.subscript.colMajor_m.float* (i32, %class.matrix.float.2.2.Col*, <1 x i32>)"(i32, %class.matrix.float.2.2.Col*, <1 x i32>) #3

; Function Attrs: nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture readonly, i64, i32, i1) #2

; Function Attrs: nounwind
define internal %class.matrix.float.2.2.Col @"\01?main@@YA?AV?$matrix@M$01$01$0A@@matrix.internal@@XZ"() #2 {
  ret %class.matrix.float.2.2.Col undef
}

; Function Attrs: nounwind readnone
declare %"$Globals"* @"dx.hl.subscript.cb.%\22$Globals\22* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #3

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22$Globals\22*, i32)"(i32, %"$Globals"*, i32) #3

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22$Globals\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"$Globals") #3

attributes #0 = { alwaysinline nounwind }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind }
attributes #3 = { nounwind readnone }

!pauseresume = !{!0}
!llvm.ident = !{!1}
!dx.version = !{!2}
!dx.valver = !{!3}
!dx.shaderModel = !{!4}
!dx.typeAnnotations = !{!5, !8}
!dx.entryPoints = !{!21}
!dx.fnprops = !{!25}
!dx.options = !{!26, !27}

!0 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!1 = !{!"dxc(private) 1.7.0.3875 (mat_orientation, b71d96c0b)"}
!2 = !{i32 1, i32 3}
!3 = !{i32 1, i32 7}
!4 = !{!"lib", i32 6, i32 3}
!5 = !{i32 0, %"$Globals" undef, !6}
!6 = !{i32 4, !7}
!7 = !{i32 6, !"idx", i32 3, i32 0, i32 7, i32 4}
!8 = !{i32 1, %class.matrix.float.2.2.Col ([2 x %class.matrix.float.2.2.Col]*, i32)* @"\01?lookup@@YA?AV?$matrix@M$01$01$0A@@matrix.internal@@Y01V12@H@Z", !9, %class.matrix.float.2.2.Col ()* @main, !17, %class.matrix.float.2.2.Col ()* @"\01?main@@YA?AV?$matrix@M$01$01$0A@@matrix.internal@@XZ", !20}
!9 = !{!10, !14, !15}
!10 = !{i32 1, !11, !13}
!11 = !{i32 2, !12, i32 7, i32 9}
!12 = !{i32 2, i32 2, i32 2}
!13 = !{}
!14 = !{i32 0, !11, !13}
!15 = !{i32 0, !16, !13}
!16 = !{i32 7, i32 4}
!17 = !{!18}
!18 = !{i32 1, !19, !13}
!19 = !{i32 2, !12, i32 4, !"OUT", i32 7, i32 9}
!20 = !{!10}
!21 = !{null, !"", null, !22, null}
!22 = !{null, null, !23, null}
!23 = !{!24}
!24 = !{i32 0, %"$Globals"* @"$Globals", !"$Globals", i32 0, i32 -1, i32 1, i32 4, null}
!25 = !{%class.matrix.float.2.2.Col ()* @main, i32 1}
!26 = !{i32 152}
!27 = !{i32 -1}
