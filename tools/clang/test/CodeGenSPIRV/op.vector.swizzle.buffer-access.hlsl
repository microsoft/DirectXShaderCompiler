// Run: %dxc -T ps_6_0 -E main

Buffer<float>  myBuffer1;
Buffer<float3> myBuffer3;

float4 main(in float4 pos : SV_Position) : SV_Target0
{
    uint index = (uint)pos.x;

// CHECK:        [[img:%\d+]] = OpLoad %type_buffer_image %myBuffer1
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}}
// CHECK-NEXT:   [[val:%\d+]] = OpCompositeExtract %float [[fetch]] 0
// CHECK-NEXT:                  OpStore %a [[val]]
    float  a = myBuffer1[index].x;

// CHECK:        [[img:%\d+]] = OpLoad %type_buffer_image %myBuffer1
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}}
// CHECK-NEXT:    [[ex:%\d+]] = OpCompositeExtract %float [[fetch]] 0
// CHECK-NEXT:   [[val:%\d+]] = OpCompositeConstruct %v2float [[ex]] [[ex]]
// CHECK-NEXT:                  OpStore %b [[val]]
    float2 b = myBuffer1[index].xx;

// CHECK:        [[img:%\d+]] = OpLoad %type_buffer_image_0 %myBuffer3
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}}
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v3float [[fetch]] [[fetch]] 0 1 2
// CHECK-NEXT:       OpStore %c [[val]]
    float3 c = myBuffer3[index].xyz;

// CHECK:        [[img:%\d+]] = OpLoad %type_buffer_image_0 %myBuffer3
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}}
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v3float [[fetch]] [[fetch]] 0 1 2
// CHECK-NEXT:     [[d:%\d+]] = OpVectorShuffle %v4float [[val]] [[val]] 1 1 0 2
// CHECK-NEXT:                  OpStore %d [[d]]
    float4 d = myBuffer3[index].yyxz;

    return float4(c, a) + d + float4(b, b);
}
