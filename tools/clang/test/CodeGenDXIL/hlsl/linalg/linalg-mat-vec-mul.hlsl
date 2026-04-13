// ITY represents a type that may be an interpreted type
// NTY must be an unpacked native type
// PTY is a packed type either PackedS8x32 or PackedU8x32

// Two simple initial tests
// RUN: %dxc -HV 202x -I %hlsl_headers -T lib_6_10 -enable-16bit-types -DNTY=F32 -DITY=F32 -DPTY=I8 -DCTY=I32 %s | FileCheck %s -Dntype=float -Dnty=f32 -Dnen=9 -Dnsg=true -Ditype=float -Dity=f32 -Dien=9 -Dctype=i32 -Dcty=i32 -Dcen=4 -Dpen=19
// RUN: %dxc -HV 202x -I %hlsl_headers -T lib_6_10 -enable-16bit-types -DNTY=I32 -DITY=F16 -DPTY=F8_E4M3FN -DCTY=F16 %s | FileCheck %s -Dntype=i32 -Dnty=i32 -Dnen=4 -Dnsg=true -Ditype=half -Dity=f16 -Dien=8 -Dctype=half -Dcty=f16 -Dcen=8 -Dpen=21

// More exhaustive run through of all types verifying the dimension matching
// RUN: %dxc -HV 202x -I %hlsl_headers -T lib_6_10 -enable-16bit-types -DNTY=U64 -DITY=I16 -DPTY=F8_E4M3FN -DCTY=F64 %s | FileCheck %s -Dntype=i64 -Dnty=i64 -Dnen=7 -Dnsg=false -Ditype=i16 -Dity=i16 -Dien=2 -Dctype=double -Dcty=f64 -Dcen=10 -Dpen=21
// RUN: %dxc -HV 202x -I %hlsl_headers -T lib_6_10 -enable-16bit-types -DNTY=F16 -DITY=U32 -DPTY=F8_E5M2 -DCTY=F32 %s | FileCheck %s -Dntype=half -Dnty=f16 -Dnen=8 -Dnsg=true -Ditype=i32 -Dity=i32 -Dien=5 -Dctype=float -Dcty=f32 -Dcen=9 -Dpen=22
// RUN: %dxc -HV 202x -I %hlsl_headers -T lib_6_10 -enable-16bit-types -DNTY=F32 -DITY=I64 -DPTY=I8 -DCTY=I64 %s | FileCheck %s -Dntype=float -Dnty=f32 -Dnen=9 -Dnsg=true -Ditype=i64 -Dity=i64 -Dien=6 -Dctype=i64 -Dcty=i64 -Dcen=6 -Dpen=19
// RUN: %dxc -HV 202x -I %hlsl_headers -T lib_6_10 -enable-16bit-types -DNTY=F64 -DITY=F32 -DPTY=U8 -DCTY=F32 %s | FileCheck %s -Dntype=double -Dnty=f64 -Dnen=10 -Dnsg=true -Ditype=float -Dity=f32 -Dien=9 -Dctype=float -Dcty=f32 -Dcen=9 -Dpen=20
// RUN: %dxc -HV 202x -I %hlsl_headers -T lib_6_10 -enable-16bit-types -DNTY=I16 -DITY=F64 -DPTY=F8_E4M3FN -DCTY=U32 %s | FileCheck %s -Dntype=i16 -Dnty=i16 -Dnen=2 -Dnsg=true -Ditype=double -Dity=f64 -Dien=10 -Dctype=i32 -Dcty=i32 -Dcen=5 -Dpen=21


#include <dx/linalg.h>
using namespace dx::linalg;

ByteAddressBuffer Buf;
RWByteAddressBuffer OutBuf;


using nType = __detail::ComponentTypeTraits<ComponentType::NTY>::Type;
using iType = __detail::ComponentTypeTraits<ComponentType::ITY>::Type;

// CHECK: %dx.types.LinAlgMatrixC[[ien]]M8N4U0S0 = type { i8* }

// Basic test using unpacked types and native vectors
// CHECK-LABEL: define void @"\01?NativeTest
export void NativeTest(vector<nType, 4> Input) {

  typedef Matrix<ComponentType::ITY, 8, 4, MatrixUse::A, MatrixScope::Thread> MatrixTy;

  // CHECK: [[hdl:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %{{.*}})
  // CHECK: [[buf:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[hdl]], %dx.types.ResourceProperties { i32 11, i32 0 })
  // CHECK: [[lmtx:%.*]] = call %dx.types.LinAlgMatrixC[[ien]]M8N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC[[ien]]M8N4U0S0(i32 -2147483634, %dx.types.Handle [[buf]], i32 24, i32 {{[0-9]*}}, i32 1{{.*}}
  MatrixTy Mat = MatrixTy::Load<MatrixLayout::ColMajor>(Buf, 24, 8 * sizeof(iType));

  // CHECK: [[ret:%.*]] = call <8 x [[ntype]]> @dx.op.linAlgMatVecMul.v8[[nty]].mC[[ien]]M8N4U0S0.v4[[nty]](i32 -2147483623, %dx.types.LinAlgMatrixC[[ien]]M8N4U0S0 [[lmtx]], i1 [[nsg]], <4 x [[ntype]]> %Input, i32 [[nen]])
  vector<nType, 8> OutVec = Multiply<nType>(Mat, Input);

  // CHECK: [[hdl:%.*]] = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %{{.*}})
  // CHECK: [[rwbuf:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle [[hdl]], %dx.types.ResourceProperties { i32 4107, i32 0 })
  // CHECK: call void @dx.op.linAlgVectorAccumulateToDescriptor.v8[[nty]](i32 -2147483617, <8 x [[ntype]]> [[ret]], %dx.types.Handle [[rwbuf]], i32 47)
  InterlockedAccumulate(OutVec, OutBuf, 47);
}

// CHECK-LABEL: !dx.targetTypes
// CHECK-SAME:  = !{[[md0:[!][0-9]*]]
// CHECK: [[md0]] = !{%dx.types.LinAlgMatrixC[[ien]]M8N4U0S0 undef, i32 [[ien]], i32 8, i32 4, i32 0, i32 0}
