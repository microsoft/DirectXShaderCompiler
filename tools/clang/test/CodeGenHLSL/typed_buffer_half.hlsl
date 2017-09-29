// RUN: %dxc -E includedFunc2 -T ps_6_0 %s

// CHECK: call void @dx.op.bufferStore.f16


RWBuffer<half4> g_tb: register(u1);

float4 main() : SV_Target {
    g_tb[3] = half4(1,2,3,4);
    return 1;
}