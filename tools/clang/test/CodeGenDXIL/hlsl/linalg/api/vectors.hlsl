// REQUIRES: dxil-1-10
// RUN: %dxc -I %hlsl_headers -enable-16bit-types -T cs_6_10 %s | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixATy = Matrix<ComponentType::F16, 8, 4, MatrixUse::A, MatrixScope::Thread>;
using MatrixAccum_8_8_Ty = Matrix<ComponentType::F16, 8, 8, MatrixUse::Accumulator, MatrixScope::Thread>;
using MatrixAccum_8_4_Ty = Matrix<ComponentType::F16, 8, 4, MatrixUse::Accumulator, MatrixScope::Thread>;
using Matrix_7_15_ATy = Matrix<ComponentType::F16, 7, 15, MatrixUse::A, MatrixScope::Thread>;
using MatrixPacked_7_15_ATy = Matrix<ComponentType::F8_E4M3FN, 7, 15, MatrixUse::A, MatrixScope::Thread>;

ByteAddressBuffer BAB : register(t0);

[numthreads(4, 4, 4)]
void main(uint ID : SV_GroupID) {

// CHECK: %[[MAT1:.*]] = call %dx.types.LinAlgMatrixC8M8N4U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M8N4U0S0(
// CHECK-SAME: i32 -2147483634, %dx.types.Handle %{{[0-9]+}}, i32 0, i32 8, i32 1, i32 128)
// CHECK-SAME: ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  MatrixATy Mat1 = MatrixATy::Load<MatrixLayoutEnum::ColMajor>(BAB, 0, 8);

  vector<half, 4> vec1 = 10.3f;

// CHECK: %[[VEC2:.*]] = call <8 x half> @dx.op.linAlgMatVecMul.v8f16.mC8M8N4U0S0.v4f16(i32 -2147483623,
// CHECK-SAME: %dx.types.LinAlgMatrixC8M8N4U0S0 %[[MAT1]], i1 true, <4 x half> <half 0xH4926, half 0xH4926, half 0xH4926,
// CHECK-SAME: half 0xH4926>, i32 8) ; LinAlgMatVecMul(matrix,isOutputSigned,inputVector,interpretation)
  vector<half, 8> vec2 = Multiply<half>(Mat1, vec1);

// CHECK: %[[VEC3:.*]] = call <8 x half> @dx.op.linAlgMatVecMulAdd.v8f16.mC8M8N4U0S0.v4f16.v8f16(i32 -2147483622,
// CHECK-SAME: %dx.types.LinAlgMatrixC8M8N4U0S0 %[[MAT1]], i1 true, <4 x half> <half 0xH4926, half 0xH4926, half 0xH4926,
// CHECK-SAME: half 0xH4926>, i32 8, <8 x half> %[[VEC2]], i32 8)
// CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 8> vec3 = MultiplyAdd<half>(Mat1, vec1, vec2);

// CHECK: %[[VEC20:.*]] = shufflevector
  vector<half, 4> vec20 = (vector<half, 4>)vec2;

// CHECK: %[[VEC4:.*]] = call <8 x half> @dx.op.linAlgMatVecMulAdd.v8f16.mC8M8N4U0S0.v4f16.v8f16(i32 -2147483622,
// CHECK-SAME: %dx.types.LinAlgMatrixC8M8N4U0S0 %[[MAT1]], i1 true, <4 x half> %[[VEC20]], i32 8, <8 x half> %[[VEC3]], i32 8)
// CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  InterpretedVector<half, 4, ComponentType::F16> interpVec2 = MakeInterpretedVector<ComponentType::F16>(vec20);
  vector<half, 8> vec4 = MultiplyAdd<half>(Mat1, interpVec2, vec3);

  // CHECK: %[[RAWLOAD:.*]] = call %dx.types.ResRet.v8i16 @dx.op.rawBufferVectorLoad.v8i16(i32 303,
  // CHECK-SAME: %dx.types.Handle %{{[0-9]+}}, i32 4096, i32 undef, i32 2)  ; RawBufferVectorLoad(buf,index,elementOffset,alignment)

  // CHECK: %[[VEC_BIAS:.*]] = extractvalue %dx.types.ResRet.v8i16 %[[RAWLOAD]], 0

  // CHECK: %[[VEC5:.*]] = call <8 x half> @dx.op.linAlgMatVecMulAdd.v8f16.mC8M8N4U0S0.v4f16.v8i16(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC8M8N4U0S0 %[[MAT1]], i1 true, <4 x half> %[[VEC20]], i32 8, <8 x i16> %[[VEC_BIAS]], i32 2)
  // CHECK-SAME:; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  VectorRef<ComponentType::I16, 8> memBias = {BAB, 4096};
  vector<half, 8> vec5 = MultiplyAdd<half>(Mat1, interpVec2, memBias);

  // CHECK: %[[RAWLOAD:.*]] = call %dx.types.ResRet.v8i16 @dx.op.rawBufferVectorLoad.v8i16(i32 303,
  // CHECK-SAME: %dx.types.Handle %{{[0-9]+}}, i32 4096, i32 undef, i32 2)
  // CHECK-SAME: ; RawBufferVectorLoad(buf,index,elementOffset,alignment)

  // CHECK: %[[VEC_BIAS:.*]] = extractvalue %dx.types.ResRet.v8i16 %[[RAWLOAD]], 0

  // CHECK: %[[VEC6:.*]] = call <8 x half> @dx.op.linAlgMatVecMulAdd.v8f16.mC8M8N4U0S0.v4f16.v8i16(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC8M8N4U0S0 %[[MAT1]], i1 true, <4 x half> %[[VEC20]], i32 8, <8 x i16> %[[VEC_BIAS]], i32 2)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 8> vec6 = MultiplyAdd<half>(Mat1, interpVec2, memBias);

  // CHECK: %[[ACCUM1:.*]] = call %dx.types.LinAlgMatrixC8M8N8U2S0
  // CHECK-SAME: @dx.op.linAlgMatrixOuterProduct.mC8M8N8U2S0.v8f16.v8f16(i32 -2147483619,
  // CHECK-SAME: <8 x half> %[[VEC5]], <8 x half> %[[VEC6]])  ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  MatrixAccum_8_8_Ty AccumMatrix1 = OuterProduct<ComponentType::F16>(vec5, vec6);

  // CHECK: %[[ACCUM2:.*]] = call %dx.types.LinAlgMatrixC8M8N4U2S0 @dx.op.linAlgMatrixOuterProduct.mC8M8N4U2S0.v8f16.v4f16(
  // CHECK-SAME: i32 -2147483619, <8 x half> %[[VEC5]], <4 x half> %[[VEC20]]) ; LinAlgMatrixOuterProduct(vectorA,vectorB)
  MatrixAccum_8_4_Ty AccumMatrix2 = OuterProduct<ComponentType::F16>(vec5, vec20);

  // CHECK: %[[CONV_VEC:.*]] = call <8 x float> @dx.op.linAlgConvert.v8f32.v8f16(i32 -2147483618,
  // CHECK-SAME: <8 x half> %[[VEC6]], i32 8, i32 9)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)
  InterpretedVector<float, 8, ComponentType::F32> convertedVec;
  convertedVec = Convert<ComponentType::F32, ComponentType::F16>(vec6);

  // CHECK: call <4 x i32> @dx.op.linAlgConvert.v4i32.v16f16(i32 -2147483618, <16 x half> %21, i32 8, i32 21)
  // CHECK: ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)
  typedef vector<half, 16> half16;
  half16 srcF16 = BAB.Load<half16>(128);
  InterpretedVector<uint, 4, ComponentEnum::F8_E4M3FN> convertedPacked = Convert<ComponentEnum::F8_E4M3FN, ComponentEnum::F16>(srcF16);

  // CHECK: call <1 x i32> @dx.op.linAlgConvert.v1i32.v3f16(i32 -2147483618, <3 x half> %{{[0-9]+}}, i32 8, i32 21)
  // CHECK-SAME: ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)
  half3 ThreeF16 = BAB.Load<half3>(256);
  InterpretedVector<uint, 1, ComponentEnum::F8_E4M3FN> convertedPacked2 =
      Convert<ComponentEnum::F8_E4M3FN, ComponentEnum::F16>(ThreeF16);

  // Test MultiplyAdd with odd sizes
  //
  vector<half, 15> vecH15 = BAB.Load< vector<half, 15> >(168);
  vector<half, 7> vecH7 = BAB.Load< vector<half, 7> >(64);

  InterpretedVector<half, 15, ComponentEnum::F16> interpVecH15 = MakeInterpretedVector<ComponentEnum::F16>(vecH15);

  // CHECK: %[[MAT_7_15:.*]] = call %dx.types.LinAlgMatrixC8M7N15U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC8M7N15U0S0(i32 -2147483634,
  // CHECK-SAME: %dx.types.Handle %{{[0-9]+}}, i32 0, i32 16, i32 1, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  Matrix_7_15_ATy Mat_7_15 = Matrix_7_15_ATy::Load<MatrixLayoutEnum::ColMajor>(BAB, 0, 16);

  // CHECK: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC8M7N15U0S0.v15f16.v7f16(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC8M7N15U0S0 %[[MAT_7_15]], i1 true, <15 x half> %{{[0-9]+}}, i32 8, <7 x half> %{{[0-9]+}}, i32 8)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 7> vec7 = MultiplyAdd<half>(Mat_7_15, vecH15, vecH7);
 
  // CHECK: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC8M7N15U0S0.v15f16.v7f16(i32 -2147483622, %dx.types.LinAlgMatrixC8M7N15U0S0 %[[MAT_7_15]],
  // CHECK-SAME; i1 true, <15 x half> %{{[0-9]+}}, i32 8, <7 x half> %{{[0-9]+}}, i32 8)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 7> vec8 = MultiplyAdd<half>(Mat_7_15, interpVecH15, vecH7);

  // CHECK: %[[LOAD1:.*]] = call %dx.types.ResRet.v7f16 @dx.op.rawBufferVectorLoad.v7f16(i32 303, %dx.types.Handle %{{[0-9]+}}, i32 512, i32 undef, i32 2)
  // CHECK-SAME: ; RawBufferVectorLoad(buf,index,elementOffset,alignment)
  // CHECK: %[[MEM_BIAS1:.*]] = extractvalue %dx.types.ResRet.v7f16 %[[LOAD1]], 0
  // CHECK: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC8M7N15U0S0.v15f16.v7f16(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC8M7N15U0S0 %[[MAT_7_15]], i1 true, <15 x half> %29, i32 8, <7 x half> %37, i32 8)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  VectorRef<ComponentType::F16, 7> memBias7 = {BAB, 512};
  vector<half, 7> vec9 = MultiplyAdd<half>(Mat_7_15, vecH15, memBias7);

  // CHECK: %[[LOAD2:.*]] = call %dx.types.ResRet.v7f16 @dx.op.rawBufferVectorLoad.v7f16(i32 303, %dx.types.Handle %{{[0-9]+}}, i32 512, i32 undef, i32 2)
  // CHECK-SAME: ; RawBufferVectorLoad(buf,index,elementOffset,alignment)
  // CHECK: %[[MEM_BIAS2:.*]] = extractvalue %dx.types.ResRet.v7f16 %[[LOAD2]], 0
  // CHECK-NEXT: %dx.types.LinAlgMatrixC8M7N15U0S0 %[[MAT_7_15]], i1 true, <15 x half> %{{[0-9]+}}, i32 8, <7 x half> %[[MEM_BIAS2]], i32 8)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 7> vec10 = MultiplyAdd<half>(Mat_7_15, interpVecH15, memBias7);

  // Test MultiplyAdd with packed input vector
  //
  // CHECK: %[[INTERP_VEC_H15_PACKED:.*]] = call <4 x i32> @dx.op.linAlgConvert.v4i32.v15f16(i32 -2147483618,
  // CHECK-SAME: <15 x half> %{{[0-9]+}}, i32 8, i32 21)  ; LinAlgConvert(inputVector,inputInterpretation,outputInterpretation)
  InterpretedVector<uint, 4, ComponentEnum::F8_E4M3FN> interpVecH15Packed = Convert<ComponentEnum::F8_E4M3FN, ComponentEnum::F16>(vecH15);

  // CHECK: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC8M7N15U0S0.v4i32.v7f16(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC8M7N15U0S0 %[[MAT_7_15]], i1 true, <4 x i32> %43, i32 21, <7 x half> %31, i32 8)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 7> vec11 = MultiplyAdd<half>(Mat_7_15, interpVecH15Packed, vecH7);

  // CHECK: %[[LOAD3:.+]] = call %dx.types.ResRet.v7f16 @dx.op.rawBufferVectorLoad.v7f16(i32 303, %dx.types.Handle %45, i32 512, i32 undef, i32 2)
  // CHECK-SAME: ; RawBufferVectorLoad(buf,index,elementOffset,alignment)
  // CHECK-NEXT: %[[MEM_BIAS3:.*]] = extractvalue %dx.types.ResRet.v7f16 %46, 0
  // CHECK-NEXT: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC8M7N15U0S0.v4i32.v7f16(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC8M7N15U0S0 %[[MAT_7_15]], i1 true, <4 x i32> %[[INTERP_VEC_H15_PACKED]], i32 21, <7 x half> %[[MEM_BIAS3]], i32 8)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
   vector<half, 7> vec12 = MultiplyAdd<half>(Mat_7_15, interpVecH15Packed, memBias7);

  // Test Convert and MultiplyAdd with odd sizes and packed types

  // CHECK: %[[MAT_7_15_PACKED:.*]] = call %dx.types.LinAlgMatrixC21M7N15U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC21M7N15U0S0(i32 -2147483634,
  // CHECK-SAME: %dx.types.Handle %{{[0-9]+}}, i32 0, i32 16, i32 1, i32 128)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout,align)
  MatrixPacked_7_15_ATy MatF8_7_15 = MatrixPacked_7_15_ATy::Load<MatrixLayoutEnum::ColMajor>(BAB, 0, 16);

  // CHECK: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC21M7N15U0S0.v15f16.v7f16(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC21M7N15U0S0 %[[MAT_7_15_PACKED]], i1 true, <15 x half> %{{[0-9]+}}, i32 21, <7 x half> %{{[0-9]+}}, i32 21)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 7> vec21 = MultiplyAdd<half>(MatF8_7_15, vecH15, vecH7);

  // CHECK: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC21M7N15U0S0.v4i32.v7f16(i32 -2147483622, %dx.types.LinAlgMatrixC21M7N15U0S0 %[[MAT_7_15_PACKED]],
  // CHECK-SAME: i1 true, <4 x i32> %[[INTERP_VEC_H15_PACKED]], i32 21, <7 x half> %{{[0-9]+}}, i32 21)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 7> vec22 = MultiplyAdd<half>(MatF8_7_15, interpVecH15Packed, vecH7);

  // CHECK: %[[LOAD4:.*]] = call %dx.types.ResRet.v2i32 @dx.op.rawBufferVectorLoad.v2i32(i32 303, %dx.types.Handle %{{[0-9]+}}, i32 512, i32 undef, i32 4)
  // CHECK-SAME: ; RawBufferVectorLoad(buf,index,elementOffset,alignment)
  // CHECK: %[[MEM_BIAS_PACKED1:.*]] = extractvalue %dx.types.ResRet.v2i32 %[[LOAD4]], 0
  // CHECK: call <7 x half> @dx.op.linAlgMatVecMulAdd.v7f16.mC21M7N15U0S0.v15f16.v2i32(i32 -2147483622,
  // CHECK-SAME: %dx.types.LinAlgMatrixC21M7N15U0S0 %[[MAT_7_15_PACKED]], i1 true, <15 x half> %{{[0-9]+}}, i32 21, <2 x i32> %[[MEM_BIAS_PACKED1]], i32 21)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  VectorRef<ComponentType::F8_E4M3FN, 7> memBias7Packed = {BAB, 512};
  vector<half, 7> vec23 = MultiplyAdd<half>(MatF8_7_15, vecH15, memBias7Packed);

  // CHECK: [[LOAD5:.8]] = call %dx.types.ResRet.v2i32 @dx.op.rawBufferVectorLoad.v2i32(i32 303, %dx.types.Handle %{{[0-9]+}}, i32 512, i32 undef, i32 4)
  // CHECK-SAME: ; RawBufferVectorLoad(buf,index,elementOffset,alignment)
  // CHECK: %[[MEM_BIAS_PACKED1:.*]] = extractvalue %dx.types.ResRet.v2i32 %[[LOAD5]], 0
  // CHECK-NEXT: %dx.types.LinAlgMatrixC21M7N15U0S0 %[[MAT_7_15_PACKED]], i1 true, <4 x i32> %[[INTERP_VEC_H15_PACKED]], i32 21, <2 x i32> %[[MEM_BIAS_PACKED1]], i32 21)
  // CHECK-SAME: ; LinAlgMatVecMulAdd(matrix,isOutputSigned,inputVector,inputInterpretation,biasVector,biasInterpretation)
  vector<half, 7> vec24 = MultiplyAdd<half>(MatF8_7_15, interpVecH15Packed, memBias7Packed);
}
