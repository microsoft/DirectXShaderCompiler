// RUN: %dxc -fcgl -T vs_6_0 %s | FileCheck %s

// Test that unsigned alone is accepted and equivalent to an unsigned int

// CHECK: @"\01?g_u@@3IB" = external constant i32, align 4
unsigned g_u;

// CHECK: @"\01?buf_u@@3V?$RWBuffer@I@@A" = external global %"class.RWBuffer<unsigned int>", align 4
// CHECK: @"\01?sbuf_u@@3V?$RWStructuredBuffer@I@@A" = external global %"class.RWStructuredBuffer<unsigned int>", align 4
RWBuffer<uint> buf_u;
RWStructuredBuffer<unsigned> sbuf_u;
RWByteAddressBuffer bab_u;

unsigned doit(uint i, unsigned j);

float4 main(unsigned VID : SV_VertexID, unsigned IID : SV_InstanceID) : SV_Position {
  uint i = g_u;
  // CHECK: %call = call i32 @"\01?doit@@YAIII@Z"(i32 %{{[0-9]+}}, i32 %{{[0-9]+}})
  buf_u[0] = doit(VID, i);
  sbuf_u[0] = doit(IID, bab_u.Load<unsigned>(0));
  return doit(buf_u[1], sbuf_u[1]);
}

// CHECK: define internal i32 @"\01?doit@@YAIII@Z"(i32 %i, i32 %j)
unsigned doit(uint i, unsigned j) {
  return i + j;
}
