// Run: %dxc -T ps_6_0 -E main

struct S {
    [[vk::binding(5)]] // error
    float4 f;
    [[vk::counter_binding(11)]] // error
    float4 g;
};

[[vk::counter_binding(3)]] // error
SamplerState mySampler;
[[vk::counter_binding(4)]] // error
Texture2D    myTexture;
[[vk::counter_binding(5)]] // error
StructuredBuffer<S> mySBuffer;

[[vk::location(5)]] // error
ConsumeStructuredBuffer<S> myCSBuffer;

[[vk::location(12)]] // error
cbuffer myCBuffer {
    float field;
}

[[vk::binding(10)]] // error
float4 main([[vk::binding(15)]] float4 a: A // error
           ) : SV_Target {
 return 1.0;
}

// CHECK:   :4:7: error: 'binding' attribute only applies to global variables, cbuffers, and tbuffers
// CHECK:   :6:7: error: 'counter_binding' attribute only applies to RWStructuredBuffers, AppendStructuredBuffers, and ConsumeStructuredBuffers
// CHECK:  :10:3: error: 'counter_binding' attribute only applies to RWStructuredBuffers, AppendStructuredBuffers, and ConsumeStructuredBuffers
// CHECK:  :12:3: error: 'counter_binding' attribute only applies to RWStructuredBuffers, AppendStructuredBuffers, and ConsumeStructuredBuffers
// CHECK:  :14:3: error: 'counter_binding' attribute only applies to RWStructuredBuffers, AppendStructuredBuffers, and ConsumeStructuredBuffers
// CHECK:  :17:3: error: 'location' attribute only applies to functions, parameters, and fields
// CHECK:  :20:3: error: 'location' attribute only applies to functions, parameters, and fields
// CHECK: :26:15: error: 'binding' attribute only applies to global variables, cbuffers, and tbuffers
// CHECK:  :25:3: error: 'binding' attribute only applies to global variables, cbuffers, and tbuffers
