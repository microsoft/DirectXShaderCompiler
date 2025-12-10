// RUN: %dxc -T lib_6_3 -auto-binding-space 0 -fhlsl-unused-resource-bindings=reserve-all %s | %D3DReflect %s | FileCheck %s

RWByteAddressBuffer output1;
RWByteAddressBuffer output2;
RWByteAddressBuffer output3 	    : register(u0);
RWByteAddressBuffer output4 	    : register(space1);
RWByteAddressBuffer output5 	    : SEMA;
RWByteAddressBuffer output6;
RWByteAddressBuffer output7 	    : register(u1);
RWByteAddressBuffer output8[12]   : register(u3);
RWByteAddressBuffer output9[12];
RWByteAddressBuffer output10[33]  : register(space1);
RWByteAddressBuffer output11[33]  : register(space2);
RWByteAddressBuffer output12[33]  : register(u0, space2);

StructuredBuffer<float> test5;
ByteAddressBuffer input13         : SEMA;
ByteAddressBuffer input14;
ByteAddressBuffer input15 		    : register(t0);
ByteAddressBuffer input16[12] 	  : register(t3);
ByteAddressBuffer input17[2]  	  : register(space1);
ByteAddressBuffer input18[12] 	  : register(t1, space1);
ByteAddressBuffer input19[3]  	  : register(space1);
ByteAddressBuffer input20  		    : register(space1);
Texture2D tex;

SamplerState sampler0;
SamplerState sampler1;
SamplerState sampler2 			      : register(s0);
SamplerState sampler3			        : register(space1);
SamplerState sampler4 			      : register(s0, space1);

cbuffer test : register(b0) { float a; };
cbuffer test2 { float b; };
cbuffer test3 : register(space1) { float c; };
cbuffer test4 : register(space1) { float d; };

float e;    //$Global is always the first to receive bindings before cbuffers without register annotation

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint id : SV_DispatchThreadID) {
  output1.Store(0, test5[0]);
  output2.Store(id * 4, a * b * c * d * e);		//Only use 1 output, but this won't result into output2 receiving wrong bindings
  output3.Store(0, input13.Load(0));
  output4.Store(0, input14.Load(0));
  output5.Store(0, input15.Load(0));
  output6.Store(0, input16[0].Load(0));
  output7.Store(0, input17[0].Load(0));
  output8[0].Store(0, input18[0].Load(0));
  output9[0].Store(0, input19[0].Load(0));
  output10[0].Store(0, input20.Load(0));
  output11[0].Store(0, tex.SampleLevel(sampler0, 0.xx, 0));
  output12[0].Store(0, tex.SampleLevel(sampler1, 0.xx, 0) * tex.SampleLevel(sampler2, 0.xx, 0) * tex.SampleLevel(sampler3, 0.xx, 0) * tex.SampleLevel(sampler4, 0.xx, 0));
}

// CHECK: ID3D12LibraryReflection:
// CHECK:   D3D12_LIBRARY_DESC:
// CHECK:     FunctionCount: 1
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: main
// CHECK:       Shader Version: Compute 6.3
// CHECK:       BoundResources: 32
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: $Globals
// CHECK:         Type: D3D_SIT_CBUFFER
// CHECK:         BindPoint: 1
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: test
// CHECK:         Type: D3D_SIT_CBUFFER
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: test2
// CHECK:         Type: D3D_SIT_CBUFFER
// CHECK:         BindPoint: 2
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: test3
// CHECK:         Type: D3D_SIT_CBUFFER
// CHECK:         BindPoint: 0
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: test4
// CHECK:         Type: D3D_SIT_CBUFFER
// CHECK:         BindPoint: 1
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: sampler0
// CHECK:         Type: D3D_SIT_SAMPLER
// CHECK:         BindPoint: 1
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: sampler1
// CHECK:         Type: D3D_SIT_SAMPLER
// CHECK:         BindPoint: 2
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: sampler2
// CHECK:         Type: D3D_SIT_SAMPLER
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: sampler3
// CHECK:         Type: D3D_SIT_SAMPLER
// CHECK:         BindPoint: 1
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: sampler4
// CHECK:         Type: D3D_SIT_SAMPLER
// CHECK:         BindPoint: 0
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: test5
// CHECK:         Type: D3D_SIT_STRUCTURED
// CHECK:         BindPoint: 1
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input13
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 2
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input14
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 15
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input15
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input16
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 3
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input17
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 13
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input18
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 1
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input19
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 15
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: input20
// CHECK:         Type: D3D_SIT_BYTEADDRESS
// CHECK:         BindPoint: 0
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: tex
// CHECK:         BindPoint: 16
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output1
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 2
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output2
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 15
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output3
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output4
// CHECK:         BindPoint: 0
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output5
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 16
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output6
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 17
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output7
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 1
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output8
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 3
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output9
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 18
// CHECK:         Space: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output10
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 1
// CHECK:         Space: 1
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output11
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 33
// CHECK:         Space: 2
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: output12
// CHECK:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// CHECK:         BindPoint: 0
// CHECK:         Space: 2