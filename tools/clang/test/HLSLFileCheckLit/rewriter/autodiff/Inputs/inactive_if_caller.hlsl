RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> x = ::ad::bwd::variable(context, -0.25f);
  output[0] = user::ad::bwd::f(context, x, 3.0f);
  output[1] = x.gradient(context);
}
