RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  user::ad::bwd::Functions functions;
  functions.scale = 4.0f;
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> value = ::ad::bwd::variable(context, 2.0f);
  output[0] = functions.f(context, value, 3.0f);
  output[1] = value.gradient(context);
}
