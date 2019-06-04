// Run: %dxc -T ps_6_0 -E main -fvk-bind-register u10 2 10 1

struct S { float4 val; };
RWStructuredBuffer<S>  MyBuffer : register(u10, space2);

float4 main() : SV_Target {
  return MyBuffer[0].val;
}

// CHECK: :4:24: error: -fvk-bind-register for RW/Append/Consume StructuredBuffer unimplemented
