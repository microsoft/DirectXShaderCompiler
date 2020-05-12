// Run: %dxc -T vs_6_0 -E main -fspv-debug=rich

struct S {
    float  f1;
    float3 f2;
};

cbuffer MyCbuffer : register(b1) {
    bool     a;
    int      b;
    uint2    c;
    float3x4 d;
    S        s;
    float    t[4];
};

cbuffer AnotherCBuffer : register(b2) {
    float3 m;
    float4 n;
}

// CHECK: [[S:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugTypeComposite {{%\d+}} Structure {{%\d+}} 3 1 {{%\d+}} {{%\d+}} %uint_100 FlagIsProtected|FlagIsPrivate [[f1:%\d+]] [[f2:%\d+]]
// CHECK: [[MyCbuffer:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite {{%\d+}} Structure {{%\d+}} 0 0 {{%\d+}} {{%\d+}} %uint_400 FlagIsProtected|FlagIsPrivate [[a:%\d+]] [[b:%\d+]] [[c:%\d+]] [[d:%\d+]] [[s:%\d+]] [[t:%\d+]]
// CHECK: [[AnotherCBuffer:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite {{%\d+}} Structure {{%\d+}} 0 0 {{%\d+}} {{%\d+}} %uint_144 FlagIsProtected|FlagIsPrivate [[m:%\d+]] [[n:%\d+]]

// CHECK: {{%\d+}} = OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} {{%\d+}} {{%\d+}} 8 9 {{%\d+}} {{%\d+}} %MyCbuffer
// CHECK: {{%\d+}} = OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} {{%\d+}} {{%\d+}} 17 9 {{%\d+}} {{%\d+}} %AnotherCBuffer

// CHECK: [[f1]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 4 5 [[S]] %uint_0 %uint_32
// CHECK: [[f2]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 5 5 [[S]] %uint_4 %uint_96
// CHECK: [[a]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_0 %uint_32
// CHECK: [[b]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_4 %uint_32
// CHECK: [[c]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_8 %uint_64
// CHECK: [[d]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_16 %uint_384
// CHECK: [[s]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} [[S]] {{%\d+}} 0 0 [[MyCbuffer]] %uint_80 %uint_100
// CHECK: [[t]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_96 %uint_128
// CHECK: [[m]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[AnotherCBuffer]] %uint_0 %uint_96
// CHECK: [[n]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[AnotherCBuffer]] %uint_16 %uint_128

float  main() : A {
  return t[0] + m[0];
}
