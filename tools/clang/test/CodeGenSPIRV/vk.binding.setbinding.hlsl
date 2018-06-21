// Run: %dxc -T ps_6_0 -E main

struct S {
    float4 f;
};

// vk::binding w/vk::set w/:register
[[vk::binding(1, 0), vk::set(2)]]
SamplerState bindVsSetVsReg      : register(s3, space4);
// CHECK:      9:14: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/vk::binding w/:register
[[vk::set(2), vk::binding(1, 0)]]
SamplerState setVsBindVsReg      : register(s3, space4);
// CHECK:      14:14: error: Invalid inclusion of vk::set with vk::binding

// explicit vk::binding w/vk::set
[[vk::binding(6,5), vk::set(7)]]
Texture2D<float4> ExBindVsSet;
// CHECK:      19:19: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/explicit vk::binding
[[vk::set(7), vk::binding(6,5)]]
Texture2D<float4> SetVsExBind;
// CHECK:      24:19: error: Invalid inclusion of vk::set with vk::binding

// implicit vk::binding w/vk::set
[[vk::binding(7), vk::set(9)]]
Texture3D<float4> ImpBindVsSet;
// CHECK:      29:19: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/implicit vk::binding
[[vk::set(9), vk::binding(7)]]
Texture3D<float4> SetVsImpBind;
// CHECK:      34:19: error: Invalid inclusion of vk::set with vk::binding

// vk::binding w/vk::set w/:register explicit counter
[[vk::binding(2, 1), vk::set(2), vk::counter_binding(3)]]
RWStructuredBuffer<S> BindVsSetVsRegExCt : register(u3, space4);
// CHECK:      39:23: error: Invalid inclusion of vk::set with vk::binding

// vk::binding w/vk::set w/:register explicit counter
[[vk::set(2), vk::binding(2, 1), vk::counter_binding(3)]]
RWStructuredBuffer<S> setVsBindVsRegExCt : register(u3, space4);
// CHECK:      44:23: error: Invalid inclusion of vk::set with vk::binding

// explicit vk::binding w/vk::set explicit counter
[[vk::binding(5, 6), vk::set(7), vk::counter_binding(3)]]
AppendStructuredBuffer<S> ExBindVsSetExCt;
// CHECK:      49:27: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/explicit vk::binding explicit counter
[[vk::set(7), vk::binding(5, 6), vk::counter_binding(3)]]
AppendStructuredBuffer<S> SetVsExBindExCt;
// CHECK:      54:27: error: Invalid inclusion of vk::set with vk::binding

// implicit vk::binding w/vk::set explicit counter
[[vk::binding(9), vk::set(11), vk::counter_binding(10)]]
ConsumeStructuredBuffer<S> impBindVsSetExCt;
// CHECK:      59:28: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/implicit vk::binding explicit counter
[[vk::set(11), vk::binding(9), vk::counter_binding(10)]]
ConsumeStructuredBuffer<S> setVsImpBindExCt;
// CHECK:      64:28: error: Invalid inclusion of vk::set with vk::binding

// vk::binding w/vk::set w/:register implicit counter (main part)
[[vk::binding(5, 3), vk::set(4)]]
RWStructuredBuffer<S> bindVsSetVsRegImpCt : register(u9, space2);
// CHECK:      69:23: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/vk::binding w/:register implicit counter (main part)
[[vk::set(4), vk::binding(5, 3)]]
RWStructuredBuffer<S> setVsBindVsRegImpCt : register(u9, space2);
// CHECK:      74:23: error: Invalid inclusion of vk::set with vk::binding

// explicit vk::binding w/vk::set implicit counter (main part)
[[vk::binding(4), vk::set(6)]]
AppendStructuredBuffer<S> exBindVsSetImpCt;
// CHECK:      79:27: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/explicit vk::binding implicit counter (main part)
[[vk::set(6), vk::binding(4)]]
AppendStructuredBuffer<S> setVsExBindImpCt;
// CHECK:      84:27: error: Invalid inclusion of vk::set with vk::binding

// implicit vk::binding w/vk::set implicit counter (main part)
[[vk::binding(11), vk::set(7)]]
AppendStructuredBuffer<S> impBindVsSetImpCt;
// CHECK:      89:27: error: Invalid inclusion of vk::set with vk::binding

// vk::set w/implicit vk::binding implicit counter (main part)
[[vk::set(7), vk::binding(11)]]
AppendStructuredBuffer<S> SetVsImpBindImpCt;
// CHECK:      94:27: error: Invalid inclusion of vk::set with vk::binding

float4 main() : SV_Target {
    return  1.0;
}
