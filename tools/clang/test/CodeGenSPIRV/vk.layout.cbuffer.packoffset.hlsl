// Run: %dxc -T vs_6_0 -E main

cbuffer MyCBuffer {
    float4 data1;
    float4 data2 : packoffset(c1);
    float  data3 : packoffset(c2.y);
}

float4 main() : A {
    return data1 + data2 + data3;
}

// CHECK: :5:20: warning: packoffset ignored since not supported
// CHECK: :6:20: warning: packoffset ignored since not supported
