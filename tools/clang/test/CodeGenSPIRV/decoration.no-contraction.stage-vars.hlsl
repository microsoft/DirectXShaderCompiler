// Run: %dxc -T vs_6_0 -E main -fspv-reflect

// CHECK:      OpDecorate [[aa_1:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[aa_plus_b_1:%\d+]] NoContraction

// CHECK-NEXT: OpDecorate [[aa_2:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[aa_plus_b_2:%\d+]] NoContraction

// CHECK-NEXT: OpDecorate [[ee:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[ee_plus_f:%\d+]] NoContraction

// CHECK-NEXT: OpDecorate [[cc_1:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[cc_plus_d_1:%\d+]] NoContraction

// CHECK-NEXT: OpDecorate [[cc_2:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[cc_plus_d_2:%\d+]] NoContraction

// CHECK-NEXT: OpDecorate [[cxcy_1:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[cxcy_plus_dz_1:%\d+]] NoContraction

// CHECK-NOT: OpDecorate [[cxcy_2]] NoContraction
// CHECK-NOT: OpDecorate [[cxcy_plus_dz_2]] NoContraction

// CHECK-NOT: OpDecorate [[cxcy_3]] NoContraction
// CHECK-NOT: OpDecorate [[cxcy_plus_dz_3]] NoContraction

// CHECK-NEXT: OpDecorate [[aa_3:%\d+]] NoContraction
// CHECK-NEXT: OpDecorate [[aa_plus_b_3:%\d+]] NoContraction

struct InnerInnerStruct {
  precise float4   position : SV_Position;      // -> BuiltIn Position in gl_Pervertex
};

struct InnerStruct {
  float2           clipdis1 : SV_ClipDistance1; // -> BuiltIn ClipDistance in gl_PerVertex
  InnerInnerStruct s;
};

struct VSOut {
  float4           color    : COLOR;            // -> Output variable
  InnerStruct s;
};

[[vk::builtin("PointSize")]]
float main(out VSOut  vsOut,
           out   precise float4 coord    : TEXCOORD,         // -> Input & output variable
           out   precise float3 clipdis0 : SV_ClipDistance0, // -> BuiltIn ClipDistance in gl_PerVertex
           out   precise float  culldis5 : SV_CullDistance5, // -> BuiltIn CullDistance in gl_PerVertex
           out           float  culldis3 : SV_CullDistance3, // -> BuiltIn CullDistance in gl_PerVertex
           out           float  clipdis6 : SV_ClipDistance6, // -> BuiltIn ClipDistance in gl_PerVertex
           in    precise float4 inPos    : SV_Position,      // -> Input variable
           in    precise float2 inClip   : SV_ClipDistance,  // -> Input variable
           in    precise float3 inCull   : SV_CullDistance0  // -> Input variable
         ) : PSize {
  vsOut    = (VSOut)0;
  float4 a, b;
  float3 c, d;
  float2 e, f;
    
// Output variable. coord is precise.
//
// CHECK:        [[aa_1]] = OpFMul %v4float
// CHECK: [[aa_plus_b_1]] = OpFAdd %v4float
  coord = a * a + b;

// Input variable for position is precise.
//
// CHECK:        [[aa_2]] = OpFMul %v4float
// CHECK: [[aa_plus_b_2]] = OpFAdd %v4float
  inPos = a * a + b;

// Input ClipDistance variable. inClip is precise.
//
// CHECK:        [[ee]] = OpFMul %v2float
// CHECK: [[ee_plus_f]] = OpFAdd %v2float
  inClip = e * e + f;
  
// Input CullDistance variable. inCull is precise.
//
// CHECK:        [[cc_1]] = OpFMul %v3float
// CHECK: [[cc_plus_d_1]] = OpFAdd %v3float
  inCull = c * c + d;
  
// Output ClipDistance builtin. clipdis0 is precise.
//
// CHECK:        [[cc_2]] = OpFMul %v3float
// CHECK: [[cc_plus_d_2]] = OpFAdd %v3float
  clipdis0 = c * c + d;
  
// Output CullDistance builtin. culldis5 is precise.
//
// CHECK:         [[cxcy_1]] = OpFMul %float
// CHECK: [[cxcy_plus_dz_1]] = OpFAdd %float
  culldis5 = c.x * c.y + d.z;
  
// Output CullDistance builtin. culldis3 is NOT precise.
//
// CHECK:         [[cxcy_2:%\d+]] = OpFMul %float
// CHECK: [[cxcy_plus_dz_2:%\d+]] = OpFAdd %float
  culldis3 = c.x * c.y + d.z;
  
// Output CullDistance builtin. clipdis6 is NOT precise.
//
// CHECK:         [[cxcy_3:%\d+]] = OpFMul %float
// CHECK: [[cxcy_plus_dz_3:%\d+]] = OpFAdd %float
  clipdis6 = c.x * c.y + d.z;
  
// Position builtin is precise.
//
// CHECK:        [[aa_3]] = OpFMul %v4float
// CHECK: [[aa_plus_b_3]] = OpFAdd %v4float
  vsOut.s.s.position = a * a + b;
  
  return inPos.x + inClip.x + inCull.x;
}

