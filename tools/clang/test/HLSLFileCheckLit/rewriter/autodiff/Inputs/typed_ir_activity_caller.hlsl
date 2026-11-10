RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::fwd::Value<float> fwdX = ::ad::fwd::variable(2.0f);
  ::ad::fwd::Value<float> fwdResult = user::ad::fwd::f(fwdX);
  output[0] = fwdResult.value;
  output[1] = fwdResult.derivative;

  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> bwdX = ::ad::bwd::variable(context, 2.0f);
  output[2] = user::ad::bwd::f(context, bwdX, 1.0f);
  output[3] = bwdX.gradient(context);
}
