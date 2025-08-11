//UAV

RWByteAddressBuffer output1;
RWByteAddressBuffer output2;
RWByteAddressBuffer output3 	 : register(u0);
RWByteAddressBuffer output4 	 : register(space1);
RWByteAddressBuffer output5 	 : SEMA;
RWByteAddressBuffer output6;
RWByteAddressBuffer output7 	 : register(u1);
RWByteAddressBuffer output8[12]  : register(u3);
RWByteAddressBuffer output9[12];
RWByteAddressBuffer output10[33] : register(space1);
RWByteAddressBuffer output11[33] : register(space2);
RWByteAddressBuffer output12[33] : register(u0, space2);

//SRV

StructuredBuffer<float> test;
ByteAddressBuffer input13 : SEMA;
ByteAddressBuffer input14;
ByteAddressBuffer input15 		: register(t0);
ByteAddressBuffer input16[12] 	: register(t3);
ByteAddressBuffer input17[2]  	: register(space1);
ByteAddressBuffer input18[12] 	: register(t1, space1);
ByteAddressBuffer input19[3]  	: register(space1);
ByteAddressBuffer input20  		: register(space1);

//Sampler

SamplerState sampler0;
SamplerState sampler1;
SamplerState sampler2 			: register(s0);
SamplerState sampler3			: register(space1);
SamplerState sampler4 			: register(s0, space1);

//CBV

cbuffer test : register(b0) { float a; };
cbuffer test2 { float b; };
cbuffer test3 : register(space1) { float c; };
cbuffer test4 : register(space1) { float d; };

float e;

[numthreads(16, 16, 1)]
void main(uint id : SV_DispatchThreadID) {
    output2.Store(id * 4, 1);		//Only use 1 output, but this won't result into output2 receiving wrong bindings
}
