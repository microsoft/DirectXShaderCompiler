// RUN: %dxc -T ps_6_0 -E main -spirv -Vd

//CHECK: [[payloadTy:%\w+]] = OpTypeStruct %v4float
//CHECK-NEXT: [[payloadTyPtr:%\w+]] = OpTypePointer RayPayloadNV [[payloadTy]]
//CHECK: [[crossTy:%\w+]] = OpTypePointer CrossWorkgroup %int
//CHECK: {{%\w+}} = OpVariable [[payloadTyPtr]] RayPayloadNV
//CHECK: {{%\w+}} = OpVariable [[crossTy]] CrossWorkgroup

[[vk::ext_storage_class(/*RayPayloadNV*/5338)]]
float4 payload;

int main() : SV_Target0 {
  [[vk::ext_storage_class(/* CrossWorkgroup */ 5)]] int foo = 3;
  return foo;
}
