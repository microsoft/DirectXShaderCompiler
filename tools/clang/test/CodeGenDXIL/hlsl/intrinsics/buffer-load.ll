; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s


target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.RWByteAddressBuffer = type { i32 }
%"class.RWStructuredBuffer<vector<float, 2> >" = type { <2 x float> }
%"class.StructuredBuffer<float [2]>" = type { [2 x float] }
%"class.StructuredBuffer<Vector<float, 2> >" = type { %"struct.Vector<float, 2>" }
%"struct.Vector<float, 2>" = type { <4 x float>, double, <2 x float> }
%"class.StructuredBuffer<matrix<float, 2, 2> >" = type { %class.matrix.float.2.2 }
%class.matrix.float.2.2 = type { [2 x <2 x float>] }
%"class.StructuredBuffer<Matrix<float, 2, 2> >" = type { %"struct.Matrix<float, 2, 2>" }
%"struct.Matrix<float, 2, 2>" = type { <4 x float>, %class.matrix.float.2.2 }
%dx.types.Handle = type { i8* }
%dx.types.ResourceProperties = type { i32, i32 }

@"\01?BabBuf@@3URWByteAddressBuffer@@A" = external global %struct.RWByteAddressBuffer, align 4
@"\01?VecBuf@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A" = external global %"class.RWStructuredBuffer<vector<float, 2> >", align 4
@"\01?ArrBuf@@3V?$StructuredBuffer@$$BY01M@@A" = external global %"class.StructuredBuffer<float [2]>", align 4
@"\01?SVecBuf@@3V?$StructuredBuffer@U?$Vector@M$01@@@@A" = external global %"class.StructuredBuffer<Vector<float, 2> >", align 8
@"\01?MatBuf@@3V?$StructuredBuffer@V?$matrix@M$01$01@@@@A" = external global %"class.StructuredBuffer<matrix<float, 2, 2> >", align 4
@"\01?SMatBuf@@3V?$StructuredBuffer@U?$Matrix@M$01$01@@@@A" = external global %"class.StructuredBuffer<Matrix<float, 2, 2> >", align 4

; Function Attrs: nounwind
define void @main(i32 %ix0) #0 {
  %1 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?BabBuf@@3URWByteAddressBuffer@@A", !dbg !66

  ; Booleans require some conversion after being loaded
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4107, i32 0 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %10, i32 %7, i32 undef, i8 3, i32 4)
  ; CHECK: [[EL0:%.*]] = extractvalue %dx.types.ResRet.i32 [[LD]], 0
  ; CHECK: [[EL1:%.*]] = extractvalue %dx.types.ResRet.i32 [[LD]], 1
  ; CHECK: [[VEC0:%.*]] = insertelement <2 x i32> undef, i32 [[EL0]], i64 0
  ; CHECK: [[VEC1:%.*]] = insertelement <2 x i32> [[VEC0]], i32 [[EL1]], i64 1
  ; CHECK: {{%.*}} = icmp ne <2 x i32> [[VEC1]], zeroinitializer
  %2 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %1), !dbg !66
  %3 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !66
  %4 = call <2 x i1> @"dx.hl.op.ro.<2 x i1> (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %3, i32 %ix0), !dbg !66
  %5 = zext <2 x i1> %4 to <2 x i32>, !dbg !70
  %6 = add i32 %ix0, 1, !dbg !71
  %7 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?BabBuf@@3URWByteAddressBuffer@@A", !dbg !72

  ; Array loads do so one element at a time.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4107, i32 0 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 undef, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 undef, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  %8 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %7), !dbg !72
  %9 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !72
  %10 = call [2 x float]* @"dx.hl.op.ro.[2 x float]* (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %9, i32 %6), !dbg !72

  %11 = getelementptr inbounds [2 x float], [2 x float]* %10, i32 0, i32 0, !dbg !72
  %12 = load float, float* %11, !dbg !72
  %13 = getelementptr inbounds [2 x float], [2 x float]* %10, i32 0, i32 1, !dbg !72
  %14 = load float, float* %13, !dbg !72
  %15 = insertelement <2 x float> undef, float %12, i32 0, !dbg !72
  %16 = insertelement <2 x float> %15, float %14, i32 1, !dbg !72
  %17 = add i32 %ix0, 3, !dbg !73
  %18 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?BabBuf@@3URWByteAddressBuffer@@A", !dbg !74

  ; Vector inside a struct is a simple load.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4107, i32 0 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 undef, i8 3, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 1
  %19 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %18), !dbg !74
  %20 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %19, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !74
  %21 = call %"struct.Vector<float, 2>"* @"dx.hl.op.ro.%\22struct.Vector<float, 2>\22* (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %20, i32 %17), !dbg !74
  %22 = getelementptr inbounds %"struct.Vector<float, 2>", %"struct.Vector<float, 2>"* %21, i32 0, i32 2, !dbg !75
  %23 = load <2 x float>, <2 x float>* %22, align 4, !dbg !75, !tbaa !76
  %24 = add i32 %ix0, 4, !dbg !79
  %25 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?BabBuf@@3URWByteAddressBuffer@@A", !dbg !80

  ; 2x2 matrix loads the full storage vector and converts the orientation.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4107, i32 0 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 undef, i8 15, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 1
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 2
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 3
  %26 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %25), !dbg !80
  %27 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %26, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !80
  %28 = call <4 x float> @"dx.hl.op.ro.<4 x float> (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %27, i32 %24), !dbg !80
  %row2col = shufflevector <4 x float> %28, <4 x float> %28, <4 x i32> <i32 0, i32 2, i32 1, i32 3>, !dbg !80
  %29 = shufflevector <4 x float> %row2col, <4 x float> %row2col, <2 x i32> <i32 1, i32 3>, !dbg !80
  %30 = add i32 %ix0, 5, !dbg !81
  %31 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?BabBuf@@3URWByteAddressBuffer@@A", !dbg !82

  ; Matrix struct members get their elements extracted with individual loads on account of already dealing with GEPs
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4107, i32 0 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 undef, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 undef, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  %32 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %31), !dbg !82
  %33 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %32, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !82
  %34 = call %"struct.Matrix<float, 2, 2>"* @"dx.hl.op.ro.%\22struct.Matrix<float, 2, 2>\22* (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %33, i32 %30), !dbg !82
  %35 = getelementptr inbounds %"struct.Matrix<float, 2, 2>", %"struct.Matrix<float, 2, 2>"* %34, i32 0, i32 1, !dbg !83
  %36 = call <2 x float>* @"dx.hl.subscript.colMajor[].rn.<2 x float>* (i32, %class.matrix.float.2.2*, i32, i32)"(i32 1, %class.matrix.float.2.2* %35, i32 1, i32 3), !dbg !82
  %37 = load <2 x float>, <2 x float>* %36, !dbg !82, !tbaa !76
  %38 = fadd <2 x float> %29, %37, !dbg !84
  %39 = fadd <2 x float> %16, %23, !dbg !85
  %40 = icmp ne <2 x i32> %5, zeroinitializer, !dbg !86
  %41 = call <2 x float> @"dx.hl.op.rn.<2 x float> (i32, <2 x i1>, <2 x float>, <2 x float>)"(i32 184, <2 x i1> %40, <2 x float> %39, <2 x float> %38), !dbg !87
  %42 = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?BabBuf@@3URWByteAddressBuffer@@A", !dbg !88

  %43 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32 0, %struct.RWByteAddressBuffer %42), !dbg !88
  %44 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32 14, %dx.types.Handle %43, %dx.types.ResourceProperties { i32 4107, i32 0 }, %struct.RWByteAddressBuffer zeroinitializer), !dbg !88
  call void @"dx.hl.op..void (i32, %dx.types.Handle, i32, <2 x float>)"(i32 277, %dx.types.Handle %44, i32 %ix0, <2 x float> %41), !dbg !88
  %45 = load %"class.RWStructuredBuffer<vector<float, 2> >", %"class.RWStructuredBuffer<vector<float, 2> >"* @"\01?VecBuf@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A", !dbg !89

  ; Normal vector. Standard load.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.RWStructuredBuffer<vector<float, 2> >"(i32 160, %"class.RWStructuredBuffer<vector<float, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4108, i32 8 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 0, i8 3, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 1
  %46 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 0, %"class.RWStructuredBuffer<vector<float, 2> >" %45), !dbg !89
  %47 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 14, %dx.types.Handle %46, %dx.types.ResourceProperties { i32 4108, i32 8 }, %"class.RWStructuredBuffer<vector<float, 2> >" zeroinitializer), !dbg !89
  %48 = call <2 x float> @"dx.hl.op.ro.<2 x float> (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %47, i32 %ix0), !dbg !89
  %49 = add i32 %ix0, 1, !dbg !90
  %50 = load %"class.StructuredBuffer<float [2]>", %"class.StructuredBuffer<float [2]>"* @"\01?ArrBuf@@3V?$StructuredBuffer@$$BY01M@@A", !dbg !91

  ; Array loads do so one element at a time.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<float [2]>"(i32 160, %"class.StructuredBuffer<float [2]>"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 12, i32 8 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 0, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 4, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  %51 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<float [2]>\22)"(i32 0, %"class.StructuredBuffer<float [2]>" %50), !dbg !91
  %52 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<float [2]>\22)"(i32 14, %dx.types.Handle %51, %dx.types.ResourceProperties { i32 12, i32 8 }, %"class.StructuredBuffer<float [2]>" zeroinitializer), !dbg !91
  %53 = call [2 x float]* @"dx.hl.op.ro.[2 x float]* (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %52, i32 %49), !dbg !91
  %54 = getelementptr inbounds [2 x float], [2 x float]* %53, i32 0, i32 0, !dbg !91
  %55 = load float, float* %54, !dbg !91
  %56 = getelementptr inbounds [2 x float], [2 x float]* %53, i32 0, i32 1, !dbg !91
  %57 = load float, float* %56, !dbg !91
  %58 = insertelement <2 x float> undef, float %55, i32 0, !dbg !91
  %59 = insertelement <2 x float> %58, float %57, i32 1, !dbg !91
  %60 = add i32 %ix0, 3, !dbg !92
  %61 = load %"class.StructuredBuffer<Vector<float, 2> >", %"class.StructuredBuffer<Vector<float, 2> >"* @"\01?SVecBuf@@3V?$StructuredBuffer@U?$Vector@M$01@@@@A", !dbg !93

  ; Vector inside a struct is a simple load.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<Vector<float, 2> >"(i32 160, %"class.StructuredBuffer<Vector<float, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 780, i32 32 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 24, i8 3, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 1
  %62 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<Vector<float, 2> >\22)"(i32 0, %"class.StructuredBuffer<Vector<float, 2> >" %61), !dbg !93
  %63 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<Vector<float, 2> >\22)"(i32 14, %dx.types.Handle %62, %dx.types.ResourceProperties { i32 780, i32 32 }, %"class.StructuredBuffer<Vector<float, 2> >" zeroinitializer), !dbg !93
  %64 = call %"struct.Vector<float, 2>"* @"dx.hl.op.ro.%\22struct.Vector<float, 2>\22* (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %63, i32 %60), !dbg !93
  %65 = getelementptr inbounds %"struct.Vector<float, 2>", %"struct.Vector<float, 2>"* %64, i32 0, i32 2, !dbg !94
  %66 = load <2 x float>, <2 x float>* %65, align 4, !dbg !94, !tbaa !76
  %67 = add i32 %ix0, 4, !dbg !95
  %68 = load %"class.StructuredBuffer<matrix<float, 2, 2> >", %"class.StructuredBuffer<matrix<float, 2, 2> >"* @"\01?MatBuf@@3V?$StructuredBuffer@V?$matrix@M$01$01@@@@A", !dbg !96

  ; 2x2 matrix loads the full storage vector and converts the orientation.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<matrix<float, 2, 2> >"(i32 160, %"class.StructuredBuffer<matrix<float, 2, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 524, i32 16 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 0, i8 15, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 1
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 2
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 3
  %69 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<matrix<float, 2, 2> >\22)"(i32 0, %"class.StructuredBuffer<matrix<float, 2, 2> >" %68), !dbg !96
  %70 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<matrix<float, 2, 2> >\22)"(i32 14, %dx.types.Handle %69, %dx.types.ResourceProperties { i32 524, i32 16 }, %"class.StructuredBuffer<matrix<float, 2, 2> >" zeroinitializer), !dbg !96
  %71 = call <4 x float> @"dx.hl.op.ro.<4 x float> (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %70, i32 %67), !dbg !96
  %row2col1 = shufflevector <4 x float> %71, <4 x float> %71, <4 x i32> <i32 0, i32 2, i32 1, i32 3>, !dbg !96
  %72 = shufflevector <4 x float> %row2col1, <4 x float> %row2col1, <2 x i32> <i32 1, i32 3>, !dbg !96
  %73 = add i32 %ix0, 5, !dbg !97
  %74 = load %"class.StructuredBuffer<Matrix<float, 2, 2> >", %"class.StructuredBuffer<Matrix<float, 2, 2> >"* @"\01?SMatBuf@@3V?$StructuredBuffer@U?$Matrix@M$01$01@@@@A", !dbg !98

  ; Matrix struct members get their elements extracted with individual loads on account of already dealing with GEPs
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<Matrix<float, 2, 2> >"(i32 160, %"class.StructuredBuffer<Matrix<float, 2, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 524, i32 32 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 20, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 28, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  %75 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<Matrix<float, 2, 2> >\22)"(i32 0, %"class.StructuredBuffer<Matrix<float, 2, 2> >" %74), !dbg !98
  %76 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<Matrix<float, 2, 2> >\22)"(i32 14, %dx.types.Handle %75, %dx.types.ResourceProperties { i32 524, i32 32 }, %"class.StructuredBuffer<Matrix<float, 2, 2> >" zeroinitializer), !dbg !98
  %77 = call %"struct.Matrix<float, 2, 2>"* @"dx.hl.op.ro.%\22struct.Matrix<float, 2, 2>\22* (i32, %dx.types.Handle, i32)"(i32 231, %dx.types.Handle %76, i32 %73), !dbg !98
  %78 = getelementptr inbounds %"struct.Matrix<float, 2, 2>", %"struct.Matrix<float, 2, 2>"* %77, i32 0, i32 1, !dbg !99
  %79 = call <2 x float>* @"dx.hl.subscript.colMajor[].rn.<2 x float>* (i32, %class.matrix.float.2.2*, i32, i32)"(i32 1, %class.matrix.float.2.2* %78, i32 1, i32 3), !dbg !98
  %80 = load <2 x float>, <2 x float>* %79, !dbg !98, !tbaa !76
  %81 = fadd <2 x float> %72, %80, !dbg !100
  %82 = fadd <2 x float> %59, %66, !dbg !101
  %83 = fcmp une <2 x float> %48, zeroinitializer, !dbg !102
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.RWStructuredBuffer<vector<float, 2> >"(i32 160, %"class.RWStructuredBuffer<vector<float, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4108, i32 8 })
  ; CHECK: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle [[ANHDL]]
  %84 = call <2 x float> @"dx.hl.op.rn.<2 x float> (i32, <2 x i1>, <2 x float>, <2 x float>)"(i32 184, <2 x i1> %83, <2 x float> %82, <2 x float> %81), !dbg !103
  %85 = load %"class.RWStructuredBuffer<vector<float, 2> >", %"class.RWStructuredBuffer<vector<float, 2> >"* @"\01?VecBuf@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A", !dbg !104

  ; Normal vector. Standard load.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.RWStructuredBuffer<vector<float, 2> >"(i32 160, %"class.RWStructuredBuffer<vector<float, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4108, i32 8 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 0, i8 3, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 1
  %86 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 0, %"class.RWStructuredBuffer<vector<float, 2> >" %85), !dbg !104
  %87 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 14, %dx.types.Handle %86, %dx.types.ResourceProperties { i32 4108, i32 8 }, %"class.RWStructuredBuffer<vector<float, 2> >" zeroinitializer), !dbg !104
  %88 = call <2 x float>* @"dx.hl.subscript.[].rn.<2 x float>* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %87, i32 %ix0), !dbg !104
  store <2 x float> %84, <2 x float>* %88, !dbg !105, !tbaa !76
  %89 = load %"class.RWStructuredBuffer<vector<float, 2> >", %"class.RWStructuredBuffer<vector<float, 2> >"* @"\01?VecBuf@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A", !dbg !106

  %90 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 0, %"class.RWStructuredBuffer<vector<float, 2> >" %89), !dbg !106
  %91 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 14, %dx.types.Handle %90, %dx.types.ResourceProperties { i32 4108, i32 8 }, %"class.RWStructuredBuffer<vector<float, 2> >" zeroinitializer), !dbg !106
  %92 = call <2 x float>* @"dx.hl.subscript.[].rn.<2 x float>* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %91, i32 %ix0), !dbg !106
  %93 = load <2 x float>, <2 x float>* %92, !dbg !106, !tbaa !76
  %94 = add i32 %ix0, 1, !dbg !107
  %95 = load %"class.StructuredBuffer<float [2]>", %"class.StructuredBuffer<float [2]>"* @"\01?ArrBuf@@3V?$StructuredBuffer@$$BY01M@@A", !dbg !108

  ; Array loads do so one element at a time.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<float [2]>"(i32 160, %"class.StructuredBuffer<float [2]>"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 12, i32 8 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 0, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 4, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  %96 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<float [2]>\22)"(i32 0, %"class.StructuredBuffer<float [2]>" %95), !dbg !108
  %97 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<float [2]>\22)"(i32 14, %dx.types.Handle %96, %dx.types.ResourceProperties { i32 12, i32 8 }, %"class.StructuredBuffer<float [2]>" zeroinitializer), !dbg !108
  %98 = call [2 x float]* @"dx.hl.subscript.[].rn.[2 x float]* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %97, i32 %94), !dbg !108
  %99 = getelementptr inbounds [2 x float], [2 x float]* %98, i32 0, i32 0, !dbg !108
  %100 = load float, float* %99, !dbg !108
  %101 = getelementptr inbounds [2 x float], [2 x float]* %98, i32 0, i32 1, !dbg !108
  %102 = load float, float* %101, !dbg !108
  %103 = insertelement <2 x float> undef, float %100, i32 0, !dbg !108
  %104 = insertelement <2 x float> %103, float %102, i32 1, !dbg !108
  %105 = add i32 %ix0, 3, !dbg !109
  %106 = load %"class.StructuredBuffer<Vector<float, 2> >", %"class.StructuredBuffer<Vector<float, 2> >"* @"\01?SVecBuf@@3V?$StructuredBuffer@U?$Vector@M$01@@@@A", !dbg !110

  ; Vector inside a struct is a simple load.
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<Vector<float, 2> >"(i32 160, %"class.StructuredBuffer<Vector<float, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 780, i32 32 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 24, i8 3, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 1
  %107 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<Vector<float, 2> >\22)"(i32 0, %"class.StructuredBuffer<Vector<float, 2> >" %106), !dbg !110
  %108 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<Vector<float, 2> >\22)"(i32 14, %dx.types.Handle %107, %dx.types.ResourceProperties { i32 780, i32 32 }, %"class.StructuredBuffer<Vector<float, 2> >" zeroinitializer), !dbg !110
  %109 = call %"struct.Vector<float, 2>"* @"dx.hl.subscript.[].rn.%\22struct.Vector<float, 2>\22* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %108, i32 %105), !dbg !110
  %110 = getelementptr inbounds %"struct.Vector<float, 2>", %"struct.Vector<float, 2>"* %109, i32 0, i32 2, !dbg !111
  %111 = load <2 x float>, <2 x float>* %110, align 4, !dbg !111, !tbaa !76
  %112 = add i32 %ix0, 4, !dbg !112
  %113 = load %"class.StructuredBuffer<matrix<float, 2, 2> >", %"class.StructuredBuffer<matrix<float, 2, 2> >"* @"\01?MatBuf@@3V?$StructuredBuffer@V?$matrix@M$01$01@@@@A", !dbg !113

  ; Subscripted matrices get their elements extracted with individual loads on account of already dealing with GEPs
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<matrix<float, 2, 2> >"(i32 160, %"class.StructuredBuffer<matrix<float, 2, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 524, i32 16 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 4, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 12, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  %114 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<matrix<float, 2, 2> >\22)"(i32 0, %"class.StructuredBuffer<matrix<float, 2, 2> >" %113), !dbg !113
  %115 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<matrix<float, 2, 2> >\22)"(i32 14, %dx.types.Handle %114, %dx.types.ResourceProperties { i32 524, i32 16 }, %"class.StructuredBuffer<matrix<float, 2, 2> >" zeroinitializer), !dbg !113
  %116 = call %class.matrix.float.2.2* @"dx.hl.subscript.[].rn.%class.matrix.float.2.2* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %115, i32 %112), !dbg !113
  %117 = call <2 x float>* @"dx.hl.subscript.colMajor[].rn.<2 x float>* (i32, %class.matrix.float.2.2*, i32, i32)"(i32 1, %class.matrix.float.2.2* %116, i32 1, i32 3), !dbg !113
  %118 = load <2 x float>, <2 x float>* %117, !dbg !113, !tbaa !76
  %119 = add i32 %ix0, 5, !dbg !114
  %120 = load %"class.StructuredBuffer<Matrix<float, 2, 2> >", %"class.StructuredBuffer<Matrix<float, 2, 2> >"* @"\01?SMatBuf@@3V?$StructuredBuffer@U?$Matrix@M$01$01@@@@A", !dbg !115

  ; Matrix struct members get their elements extracted with individual loads on account of already dealing with GEPs
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.StructuredBuffer<Matrix<float, 2, 2> >"(i32 160, %"class.StructuredBuffer<Matrix<float, 2, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 524, i32 32 })
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 20, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  ; CHECK: [[LD:%.*]] = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle [[ANHDL]], i32 {{%.*}}, i32 28, i8 1, i32 4)
  ; CHECK: {{%.*}} = extractvalue %dx.types.ResRet.f32 [[LD]], 0
  %121 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<Matrix<float, 2, 2> >\22)"(i32 0, %"class.StructuredBuffer<Matrix<float, 2, 2> >" %120), !dbg !115
  %122 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<Matrix<float, 2, 2> >\22)"(i32 14, %dx.types.Handle %121, %dx.types.ResourceProperties { i32 524, i32 32 }, %"class.StructuredBuffer<Matrix<float, 2, 2> >" zeroinitializer), !dbg !115
  %123 = call %"struct.Matrix<float, 2, 2>"* @"dx.hl.subscript.[].rn.%\22struct.Matrix<float, 2, 2>\22* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %122, i32 %119), !dbg !115
  %124 = getelementptr inbounds %"struct.Matrix<float, 2, 2>", %"struct.Matrix<float, 2, 2>"* %123, i32 0, i32 1, !dbg !116
  %125 = call <2 x float>* @"dx.hl.subscript.colMajor[].rn.<2 x float>* (i32, %class.matrix.float.2.2*, i32, i32)"(i32 1, %class.matrix.float.2.2* %124, i32 1, i32 3), !dbg !115
  %126 = load <2 x float>, <2 x float>* %125, !dbg !115, !tbaa !76
  %127 = fadd <2 x float> %118, %126, !dbg !117
  %128 = fadd <2 x float> %104, %111, !dbg !118
  %129 = fcmp une <2 x float> %93, zeroinitializer, !dbg !119
  ; CHECK: [[HDL:%.*]] = call %dx.types.Handle @"dx.op.createHandleForLib.class.RWStructuredBuffer<vector<float, 2> >"(i32 160, %"class.RWStructuredBuffer<vector<float, 2> >"
  ; CHECK: [[ANHDL:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[HDL]], %dx.types.ResourceProperties { i32 4108, i32 8 })
  ; CHECK: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle [[ANHDL]]
  %130 = call <2 x float> @"dx.hl.op.rn.<2 x float> (i32, <2 x i1>, <2 x float>, <2 x float>)"(i32 184, <2 x i1> %129, <2 x float> %128, <2 x float> %127), !dbg !120
  %131 = add i32 %ix0, 1, !dbg !121
  %132 = load %"class.RWStructuredBuffer<vector<float, 2> >", %"class.RWStructuredBuffer<vector<float, 2> >"* @"\01?VecBuf@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A", !dbg !122

  %133 = call %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 0, %"class.RWStructuredBuffer<vector<float, 2> >" %132), !dbg !122
  %134 = call %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32 14, %dx.types.Handle %133, %dx.types.ResourceProperties { i32 4108, i32 8 }, %"class.RWStructuredBuffer<vector<float, 2> >" zeroinitializer), !dbg !122
  %135 = call <2 x float>* @"dx.hl.subscript.[].rn.<2 x float>* (i32, %dx.types.Handle, i32)"(i32 0, %dx.types.Handle %134, i32 %131), !dbg !122
  store <2 x float> %130, <2 x float>* %135, !dbg !123, !tbaa !76
  ret void, !dbg !124
}

; Function Attrs: nounwind readnone
declare <2 x float>* @"dx.hl.subscript.colMajor[].rn.<2 x float>* (i32, %class.matrix.float.2.2*, i32, i32)"(i32, %class.matrix.float.2.2*, i32, i32) #1

; Function Attrs: nounwind readonly
declare <2 x i1> @"dx.hl.op.ro.<2 x i1> (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %struct.RWByteAddressBuffer)"(i32, %struct.RWByteAddressBuffer) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %struct.RWByteAddressBuffer) #1

; Function Attrs: nounwind readonly
declare [2 x float]* @"dx.hl.op.ro.[2 x float]* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %"struct.Vector<float, 2>"* @"dx.hl.op.ro.%\22struct.Vector<float, 2>\22* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %"struct.Matrix<float, 2, 2>"* @"dx.hl.op.ro.%\22struct.Matrix<float, 2, 2>\22* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind
declare void @"dx.hl.op..void (i32, %dx.types.Handle, i32, <2 x float>)"(i32, %dx.types.Handle, i32, <2 x float>) #0

; Function Attrs: nounwind readnone
declare <2 x float> @"dx.hl.op.rn.<2 x float> (i32, <2 x i1>, <2 x float>, <2 x float>)"(i32, <2 x i1>, <2 x float>, <2 x float>) #1

; Function Attrs: nounwind readonly
declare <2 x float> @"dx.hl.op.ro.<2 x float> (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32, %"class.RWStructuredBuffer<vector<float, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.RWStructuredBuffer<vector<float, 2> >\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.RWStructuredBuffer<vector<float, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<float [2]>\22)"(i32, %"class.StructuredBuffer<float [2]>") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<float [2]>\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.StructuredBuffer<float [2]>") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<Vector<float, 2> >\22)"(i32, %"class.StructuredBuffer<Vector<float, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<Vector<float, 2> >\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.StructuredBuffer<Vector<float, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<matrix<float, 2, 2> >\22)"(i32, %"class.StructuredBuffer<matrix<float, 2, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<matrix<float, 2, 2> >\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.StructuredBuffer<matrix<float, 2, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.createhandle..%dx.types.Handle (i32, %\22class.StructuredBuffer<Matrix<float, 2, 2> >\22)"(i32, %"class.StructuredBuffer<Matrix<float, 2, 2> >") #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @"dx.hl.annotatehandle..%dx.types.Handle (i32, %dx.types.Handle, %dx.types.ResourceProperties, %\22class.StructuredBuffer<Matrix<float, 2, 2> >\22)"(i32, %dx.types.Handle, %dx.types.ResourceProperties, %"class.StructuredBuffer<Matrix<float, 2, 2> >") #1

; Function Attrs: nounwind readnone
declare <2 x float>* @"dx.hl.subscript.[].rn.<2 x float>* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare [2 x float]* @"dx.hl.subscript.[].rn.[2 x float]* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %"struct.Vector<float, 2>"* @"dx.hl.subscript.[].rn.%\22struct.Vector<float, 2>\22* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %class.matrix.float.2.2* @"dx.hl.subscript.[].rn.%class.matrix.float.2.2* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %"struct.Matrix<float, 2, 2>"* @"dx.hl.subscript.[].rn.%\22struct.Matrix<float, 2, 2>\22* (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare <4 x float> @"dx.hl.op.ro.<4 x float> (i32, %dx.types.Handle, i32)"(i32, %dx.types.Handle, i32) #2

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind readonly }

!llvm.module.flags = !{!0}
!pauseresume = !{!1}
!llvm.ident = !{!2}
!dx.version = !{!3}
!dx.valver = !{!4}
!dx.shaderModel = !{!5}
!dx.typeAnnotations = !{!6, !43}
!dx.entryPoints = !{!50}
!dx.fnprops = !{!63}
!dx.options = !{!64, !65}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!2 = !{!"dxc(private) 1.8.0.4807 (longvec_bab_ldst, 88cfe61c3-dirty)"}
!3 = !{i32 1, i32 6}
!4 = !{i32 1, i32 9}
!5 = !{!"vs", i32 6, i32 6}
!6 = !{i32 0, %"class.RWStructuredBuffer<vector<float, 2> >" undef, !7, %"class.StructuredBuffer<float [2]>" undef, !12, %"class.StructuredBuffer<Vector<float, 2> >" undef, !16, %"struct.Vector<float, 2>" undef, !21, %"class.StructuredBuffer<matrix<float, 2, 2> >" undef, !29, %"class.StructuredBuffer<Matrix<float, 2, 2> >" undef, !35, %"struct.Matrix<float, 2, 2>" undef, !39}
!7 = !{i32 8, !8, !9}
!8 = !{i32 6, !"h", i32 3, i32 0, i32 7, i32 9}
!9 = !{i32 0, !10}
!10 = !{!11}
!11 = !{i32 0, <2 x float> undef}
!12 = !{i32 20, !8, !13}
!13 = !{i32 0, !14}
!14 = !{!15}
!15 = !{i32 0, [2 x float] undef}
!16 = !{i32 32, !17, !18}
!17 = !{i32 6, !"h", i32 3, i32 0}
!18 = !{i32 0, !19}
!19 = !{!20}
!20 = !{i32 0, %"struct.Vector<float, 2>" undef}
!21 = !{i32 32, !22, !23, !24, !25}
!22 = !{i32 6, !"pad1", i32 3, i32 0, i32 7, i32 9}
!23 = !{i32 6, !"pad2", i32 3, i32 16, i32 7, i32 10}
!24 = !{i32 6, !"v", i32 3, i32 24, i32 7, i32 9}
!25 = !{i32 0, !26}
!26 = !{!27, !28}
!27 = !{i32 0, float undef}
!28 = !{i32 1, i64 2}
!29 = !{i32 24, !30, !32}
!30 = !{i32 6, !"h", i32 2, !31, i32 3, i32 0, i32 7, i32 9}
!31 = !{i32 2, i32 2, i32 2}
!32 = !{i32 0, !33}
!33 = !{!34}
!34 = !{i32 0, %class.matrix.float.2.2 undef}
!35 = !{i32 40, !17, !36}
!36 = !{i32 0, !37}
!37 = !{!38}
!38 = !{i32 0, %"struct.Matrix<float, 2, 2>" undef}
!39 = !{i32 40, !22, !40, !41}
!40 = !{i32 6, !"m", i32 2, !31, i32 3, i32 16, i32 7, i32 9}
!41 = !{i32 0, !42}
!42 = !{!27, !28, !28}
!43 = !{i32 1, void (i32)* @main, !44}
!44 = !{!45, !47}
!45 = !{i32 1, !46, !46}
!46 = !{}
!47 = !{i32 0, !48, !49}
!48 = !{i32 4, !"IX0", i32 7, i32 5}
!49 = !{i32 0}
!50 = !{void (i32)* @main, !"main", null, !51, null}
!51 = !{!52, !60, null, null}
!52 = !{!53, !55, !57, !59}
!53 = !{i32 0, %"class.StructuredBuffer<float [2]>"* @"\01?ArrBuf@@3V?$StructuredBuffer@$$BY01M@@A", !"ArrBuf", i32 0, i32 3, i32 1, i32 12, i32 0, !54}
!54 = !{i32 1, i32 8}
!55 = !{i32 1, %"class.StructuredBuffer<Vector<float, 2> >"* @"\01?SVecBuf@@3V?$StructuredBuffer@U?$Vector@M$01@@@@A", !"SVecBuf", i32 0, i32 4, i32 1, i32 12, i32 0, !56}
!56 = !{i32 1, i32 32}
!57 = !{i32 2, %"class.StructuredBuffer<matrix<float, 2, 2> >"* @"\01?MatBuf@@3V?$StructuredBuffer@V?$matrix@M$01$01@@@@A", !"MatBuf", i32 0, i32 5, i32 1, i32 12, i32 0, !58}
!58 = !{i32 1, i32 16}
!59 = !{i32 3, %"class.StructuredBuffer<Matrix<float, 2, 2> >"* @"\01?SMatBuf@@3V?$StructuredBuffer@U?$Matrix@M$01$01@@@@A", !"SMatBuf", i32 0, i32 6, i32 1, i32 12, i32 0, !56}
!60 = !{!61, !62}
!61 = !{i32 0, %struct.RWByteAddressBuffer* @"\01?BabBuf@@3URWByteAddressBuffer@@A", !"BabBuf", i32 0, i32 1, i32 1, i32 11, i1 false, i1 false, i1 false, null}
!62 = !{i32 1, %"class.RWStructuredBuffer<vector<float, 2> >"* @"\01?VecBuf@@3V?$RWStructuredBuffer@V?$vector@M$01@@@@A", !"VecBuf", i32 0, i32 2, i32 1, i32 12, i1 false, i1 false, i1 false, !54}
!63 = !{void (i32)* @main, i32 1}
!64 = !{i32 64}
!65 = !{i32 -1}
!66 = !DILocation(line: 39, column: 18, scope: !67)
!67 = !DISubprogram(name: "main", scope: !68, file: !68, line: 38, type: !69, isLocal: false, isDefinition: true, scopeLine: 38, flags: DIFlagPrototyped, isOptimized: false, function: void (i32)* @main)
!68 = !DIFile(filename: "/home/grroth/dxc/tools/clang/test/CodeGenDXIL/hlsl/intrinsics/buffer-load.hlsl", directory: "")
!69 = !DISubroutineType(types: !46)
!70 = !DILocation(line: 39, column: 10, scope: !67)
!71 = !DILocation(line: 40, column: 54, scope: !67)
!72 = !DILocation(line: 40, column: 26, scope: !67)
!73 = !DILocation(line: 41, column: 53, scope: !67)
!74 = !DILocation(line: 41, column: 18, scope: !67)
!75 = !DILocation(line: 41, column: 58, scope: !67)
!76 = !{!77, !77, i64 0}
!77 = !{!"omnipotent char", !78, i64 0}
!78 = !{!"Simple C/C++ TBAA"}
!79 = !DILocation(line: 42, column: 46, scope: !67)
!80 = !DILocation(line: 42, column: 18, scope: !67)
!81 = !DILocation(line: 43, column: 55, scope: !67)
!82 = !DILocation(line: 43, column: 18, scope: !67)
!83 = !DILocation(line: 43, column: 60, scope: !67)
!84 = !DILocation(line: 44, column: 59, scope: !67)
!85 = !DILocation(line: 44, column: 48, scope: !67)
!86 = !DILocation(line: 44, column: 38, scope: !67)
!87 = !DILocation(line: 44, column: 31, scope: !67)
!88 = !DILocation(line: 44, column: 3, scope: !67)
!89 = !DILocation(line: 46, column: 17, scope: !67)
!90 = !DILocation(line: 47, column: 41, scope: !67)
!91 = !DILocation(line: 47, column: 25, scope: !67)
!92 = !DILocation(line: 48, column: 34, scope: !67)
!93 = !DILocation(line: 48, column: 17, scope: !67)
!94 = !DILocation(line: 48, column: 39, scope: !67)
!95 = !DILocation(line: 49, column: 33, scope: !67)
!96 = !DILocation(line: 49, column: 17, scope: !67)
!97 = !DILocation(line: 50, column: 34, scope: !67)
!98 = !DILocation(line: 50, column: 17, scope: !67)
!99 = !DILocation(line: 50, column: 39, scope: !67)
!100 = !DILocation(line: 51, column: 45, scope: !67)
!101 = !DILocation(line: 51, column: 34, scope: !67)
!102 = !DILocation(line: 51, column: 24, scope: !67)
!103 = !DILocation(line: 51, column: 17, scope: !67)
!104 = !DILocation(line: 51, column: 3, scope: !67)
!105 = !DILocation(line: 51, column: 15, scope: !67)
!106 = !DILocation(line: 53, column: 17, scope: !67)
!107 = !DILocation(line: 54, column: 36, scope: !67)
!108 = !DILocation(line: 54, column: 25, scope: !67)
!109 = !DILocation(line: 55, column: 29, scope: !67)
!110 = !DILocation(line: 55, column: 17, scope: !67)
!111 = !DILocation(line: 55, column: 34, scope: !67)
!112 = !DILocation(line: 56, column: 28, scope: !67)
!113 = !DILocation(line: 56, column: 17, scope: !67)
!114 = !DILocation(line: 57, column: 29, scope: !67)
!115 = !DILocation(line: 57, column: 17, scope: !67)
!116 = !DILocation(line: 57, column: 34, scope: !67)
!117 = !DILocation(line: 58, column: 47, scope: !67)
!118 = !DILocation(line: 58, column: 36, scope: !67)
!119 = !DILocation(line: 58, column: 26, scope: !67)
!120 = !DILocation(line: 58, column: 19, scope: !67)
!121 = !DILocation(line: 58, column: 13, scope: !67)
!122 = !DILocation(line: 58, column: 3, scope: !67)
!123 = !DILocation(line: 58, column: 17, scope: !67)
!124 = !DILocation(line: 59, column: 1, scope: !67)
