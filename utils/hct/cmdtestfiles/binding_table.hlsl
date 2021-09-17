// List of things tested in this:
//  - Arbitrary capitalization of the headers
//  - Quotes
//  - Optional trailing commas
//  - Arbitrary spaces
//  - Resources with the same names (but different classes)

cbuffer cb {
  float a;
};
cbuffer resource {
  float b;
};

SamplerState samp0;
Texture2D resource;
RWTexture1D<float> uav_0;

[RootSignature("CBV(b10,space=30), CBV(b42,space=999), DescriptorTable(Sampler(s1,space=2)), DescriptorTable(SRV(t1,space=2)), DescriptorTable(UAV(u1,space=2))")]
float main(float2 uv : UV, uint i : I) :SV_Target {
  return a + b + resource.Sample(samp0, uv).r + uav_0[i];
}
