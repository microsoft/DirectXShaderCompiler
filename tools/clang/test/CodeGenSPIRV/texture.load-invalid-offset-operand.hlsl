// Run: %dxc -T ps_6_0 -E main

Texture1D       <float4> t1 : register(t1);
Texture2DMS     <float>  t2 : register(t2);

float4 main(int3 location: A, int offset: B) : SV_Target {
    uint status;

// CHECK: error: Texture instructions must take offset which can resolve to integer literal in the range -8 to 7
    float4 val1 = t1.Load(int2(1, 2), offset);

    int sampleIndex = 7;
    int2 pos2 = int2(2, 3);
    int2 offset2 = int2(1, 2);

// CHECK: error: Texture instructions must take offset which can resolve to integer literal in the range -8 to 7
    float val2 = t2.Load(pos2, sampleIndex, offset2);

    return 1.0;
}
