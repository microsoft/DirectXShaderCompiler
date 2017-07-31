// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-DAG: %float = OpTypeFloat 32
// CHECK-DAG: %_ptr_Function_float = OpTypePointer Function %float
    float1 a;
// CHECK-DAG: %v2float = OpTypeVector %float 2
// CHECK-DAG: %_ptr_Function_v2float = OpTypePointer Function %v2float
    float2 b;
// CHECK-DAG: %v3float = OpTypeVector %float 3
// CHECK-DAG: %_ptr_Function_v3float = OpTypePointer Function %v3float
    float3 c;
// CHECK-DAG: %v4float = OpTypeVector %float 4
// CHECK-DAG: %_ptr_Function_v4float = OpTypePointer Function %v4float
    float4 d;

// CHECK: %a = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %b = OpVariable %_ptr_Function_v2float Function
// CHECK-NEXT: %c = OpVariable %_ptr_Function_v3float Function
// CHECK-NEXT: %d = OpVariable %_ptr_Function_v4float Function
}
