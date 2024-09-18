// Nothing special about the output, but want to make sure it does not fail to run.
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 %s

template <typename T>
class C {
  C cast();
};

template <class T>
C<T>
C<T>::cast() {
  C result;
  return result;
}

[numthreads(64, 1, 1)] void main() {}
