RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  user::ad::bwd::Custom custom;
  custom.scale = 4.0f;
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> x = ::ad::bwd::variable(context, 2.0f);
  output[0] = custom.f(context, x, 3.0f);
  output[1] = x.gradient(context);
}