// Run: %dxc -T vs_6_0 -E main

struct Inner {
    Texture3D    t;
};

struct Combined1 {
    Inner        others;
    SamplerState s1;
    Texture2D    t1;
    Texture2D    t2;
    SamplerState s2;
};

struct Combined2 {
    Texture3D    t;
    SamplerState s;
};

Texture3D    gTex3D;
SamplerState gSampler;
Texture2D    gTex2D;

float main() : A {

// CHECK:      [[tex3d:%\d+]] = OpLoad %type_3d_image %gTex3D
// CHECK-NEXT: [[sampl:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comb2:%\d+]] = OpCompositeConstruct %Combined2 [[tex3d]] [[sampl]]
// CHECK-NEXT:                  OpStore %comb2 [[comb2]]
    Combined2 comb2 = {gTex3D, gSampler};

// CHECK-NEXT:   [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_3d_image %comb2 %int_0
// CHECK-NEXT: [[tex3d:%\d+]] = OpLoad %type_3d_image [[ptr]]
// CHECK-NEXT: [[inner:%\d+]] = OpCompositeConstruct %Inner [[tex3d]]
// CHECK-NEXT:   [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_sampler %comb2 %int_1
// CHECK-NEXT: [[sampl1:%\d+]] = OpLoad %type_sampler [[ptr]]
// CHECK-NEXT: [[tex2d1:%\d+]] = OpLoad %type_2d_image %gTex2D
// CHECK-NEXT: [[tex2d2:%\d+]] = OpLoad %type_2d_image %gTex2D
// CHECK-NEXT: [[sampl2:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comb1:%\d+]] = OpCompositeConstruct %Combined1 [[inner]] [[sampl1]] [[tex2d1]] [[tex2d2]] [[sampl2]]
// CHECK-NEXT:                  OpStore %comb1 [[comb1]]
    Combined1 comb1 = {comb2, {gTex2D, gTex2D}, gSampler};

    return 1.0;
}
