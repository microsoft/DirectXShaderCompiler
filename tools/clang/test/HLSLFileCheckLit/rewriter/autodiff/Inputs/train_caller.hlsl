float4 diffFragmentMain(float2 uv : UV,
                        float4 screenPosition : SV_Position) : SV_Target {
  ::ad::bwd::GradientContext<float3> context =
      (::ad::bwd::GradientContext<float3>)0;
  user::ad::bwd::loss(context, uv, screenPosition,
                      float3(1.0f, 1.0f, 1.0f));
  return float4(loss(uv, screenPosition), 1.0f);
}
