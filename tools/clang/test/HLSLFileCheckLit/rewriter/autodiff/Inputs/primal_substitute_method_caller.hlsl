RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  Functions primal;
  primal.scale = 4.0f;
  output[0] = primal.original(2.0f);

  user::ad::bwd::Functions functions;
  functions.scale = 4.0f;
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> x = ::ad::bwd::variable(context, 2.0f);
  output[1] = functions.f(context, x, 3.0f);
  output[2] = x.gradient(context);
}
