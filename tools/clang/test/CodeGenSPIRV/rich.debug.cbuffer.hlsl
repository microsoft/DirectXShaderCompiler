// Run: %dxc -T vs_6_0 -E main -fspv-debug=rich

struct S {
    float  f1;  // Size: 32,  Offset: [ 0 -  32]
    float3 f2;  // Size: 96,  Offset: [32 - 128]
};

cbuffer MyCbuffer : register(b1) {
    bool     a;    // Size:  32, Offset: [  0 -  32]
    int      b;    // Size:  32, Offset: [ 32 -  64]
    uint2    c;    // Size:  64, Offset: [ 64 - 128]
    float3x4 d;    // Size: 384, Offset: [128 - 512]
    S        s;    // Size: 128, Offset: [640 - 768]
    float    t[4]; // Size: 128, Offset: [768 - 896]
};

cbuffer AnotherCBuffer : register(b2) {
    float3 m;  // Size:  96, Offset: [ 0  -  96]
    float4 n;  // Size: 128, Offset: [128 - 256]
}

// CHECK: [[AnotherCBuffer:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugTypeComposite {{%\d+}} Structure {{%\d+}} 0 0 {{%\d+}} {{%\d+}} %uint_256 FlagIsProtected|FlagIsPrivate [[m:%\d+]] [[n:%\d+]]
// CHECK: [[n]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[AnotherCBuffer]] %uint_128 %uint_128
// CHECK: [[m]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[AnotherCBuffer]] %uint_0 %uint_96

// CHECK: [[MyCbuffer:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite {{%\d+}} Structure {{%\d+}} 0 0 {{%\d+}} {{%\d+}} %uint_896 FlagIsProtected|FlagIsPrivate [[a:%\d+]] [[b:%\d+]] [[c:%\d+]] [[d:%\d+]] [[s:%\d+]] [[t:%\d+]]
// CHECK: [[t]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_768 %uint_128

// CHECK: [[S:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite {{%\d+}} Structure {{%\d+}} 3 1 {{%\d+}} {{%\d+}} %uint_128 FlagIsProtected|FlagIsPrivate [[f1:%\d+]] [[f2:%\d+]]
// CHECK: [[s]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} [[S]] {{%\d+}} 0 0 [[MyCbuffer]] %uint_640 %uint_128
// CHECK: [[d]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_128 %uint_384
// CHECK: [[c]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_64 %uint_64
// CHECK: [[b]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_32 %uint_32
// CHECK: [[a]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 0 0 [[MyCbuffer]] %uint_0 %uint_32
// CHECK: [[f2]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 5 5 [[S]] %uint_32 %uint_96
// CHECK: [[f1]] = OpExtInst %void [[ext]] DebugTypeMember {{%\d+}} {{%\d+}} {{%\d+}} 4 5 [[S]] %uint_0 %uint_32

// CHECK: {{%\d+}} = OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} {{%\d+}} {{%\d+}} 17 9 {{%\d+}} {{%\d+}} %AnotherCBuffer
// CHECK: {{%\d+}} = OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} {{%\d+}} {{%\d+}} 8 9 {{%\d+}} {{%\d+}} %MyCbuffer

float  main() : A {
  return t[0] + m[0];
}
