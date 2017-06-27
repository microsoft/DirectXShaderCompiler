//RUN: %dxc -E main -T ps_6_0 %s
shared cbuffer cb0
{
 float X;
};

[RootSignature("CBV(b0)")]
float main() : SV_Target {
 return -X;
}