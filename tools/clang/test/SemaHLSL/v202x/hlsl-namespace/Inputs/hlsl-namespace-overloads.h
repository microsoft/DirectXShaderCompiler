namespace hlsl {
struct HeaderResult {
  float value;
};

HeaderResult sin();

HeaderResult sin() {
  HeaderResult result = {42};
  return result;
}

HeaderResult sin(int value) {
  HeaderResult result = {value};
  return result;
}

void cos();
void cos() {}

struct HeaderArgument {
  float value;
};

HeaderResult headerFunction(HeaderArgument value) {
  HeaderResult result = {value.value};
  return result;
}

float callIntrinsicFromHeader(float value) {
  return cos(value);
}

template <typename T>
HeaderResult callFromHeader(T value) {
  return headerFunction(value);
}

template <typename T>
T callIntrinsicFromHeaderTemplate(T value) {
  return cos(value);
}

HeaderResult TraceRay(int a, int b, int c, int d, int e, int f, int g, int h) {
  HeaderResult result = {42};
  return result;
}

HeaderResult TraceRay(int a, int b, int c, int d, int e, int f, RayDesc ray,
                      int payload) {
  HeaderResult result = {42};
  return result;
}
} // namespace hlsl

namespace associated {
struct Argument {
  float value;
};

hlsl::HeaderResult cos(Argument value) {
  hlsl::HeaderResult result = {value.value};
  return result;
}

hlsl::HeaderResult headerFunction(Argument value) {
  hlsl::HeaderResult result = {value.value};
  return result;
}
} // namespace associated
