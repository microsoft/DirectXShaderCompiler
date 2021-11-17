// RUN: %dxc -T ps_6_0 -E main -spirv -Vd

//CHECK: {{%\w+}} = OpVariable {{%\w+}} RayPayloadNV
//CHECK: {{%\w+}} = OpVariable {{%\w+}} CrossWorkgroup

[[vk::ext_storage_class(/*RayPayloadNV*/5338)]]
float4 payload;

int main() : SV_Target0 {
    
  [[vk::ext_storage_class(/* CrossWorkgroup */ 5)]] int foo = 3;
  return foo;
}
