// RUN: %dxc -T vs_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s

struct VSIn {
  float2x2 rotationA : ROTATIONA;
  float2 positionA : POSITIONA;
  float2x2 rotationB : ROTATIONB;
  float2 positionB : POSITIONB;
};

cbuffer SceneInput : register(b0) {
  float4x4 projection;
};

// Matrix types from vertex input attributes must not be emitted as implicitly transposed in SPIR-V,
// because they cannot be decorated with the row_major/column_major type qualifiers.
// The mul() intrinsic's operands must therefore be emitted as-is,
// which results in a double flip of matrix ordering: (1) in the vertex input attribute and (2) in the mul() intrinsic.
// These cancel each other out and result in the same matrix transformation between DXIL and SPIR-V.
// The second mul() intrinsic in this test must be emitted as before,
// with flipped operands and OpVectorTimesMatrix instruction.
float4 main(VSIn input) : SV_Position {
  // CHECK:      [[rotationA:%[0-9]+]] = OpLoad %mat2v2float {{%[a-zA-Z0-9_]+}}
  // CHECK-NEXT: [[positionA:%[0-9]+]] = OpLoad %v2float {{%[a-zA-Z0-9_]+}}
  // CHECK:      [[rotationB:%[0-9]+]] = OpLoad %mat2v2float {{%[a-zA-Z0-9_]+}}
  // CHECK-NEXT: [[positionB:%[0-9]+]] = OpLoad %v2float {{%[a-zA-Z0-9_]+}}
  // CHECK:      [[mulA:%[0-9]+]] = OpMatrixTimesVector %v2float [[rotationA]] [[positionA]]
  float4 worldSpacePositionA = float4(mul(input.rotationA, input.positionA) + input.positionA, 0.0, 1.0);

  // CHECK:      [[worldSpacePositionA:%[0-9]+]] = OpCompositeConstruct %v4float
  // CHECK:      [[mulB:%[0-9]+]] = OpVectorTimesMatrix %v2float [[positionB]] [[rotationB]]
  float4 worldSpacePositionB = float4(mul(input.positionB, input.rotationB) + input.positionB, 0.0, 1.0);

  // CHECK:      [[worldSpacePositionB:%[0-9]+]] = OpCompositeConstruct %v4float
  // CHECK:      [[projection:%[0-9]+]] = OpLoad %mat4v4float {{%[a-zA-Z0-9_]+}}
  // CHECK:      [[projectionMulA:%[0-9]+]] = OpVectorTimesMatrix %v4float [[worldSpacePositionA]] [[projection]]
  // CHECK:      [[projectionMulB:%[0-9]+]] = OpVectorTimesMatrix %v4float [[worldSpacePositionB]] [[projection]]
  return mul(projection, worldSpacePositionA) + mul(projection, worldSpacePositionB);
}
