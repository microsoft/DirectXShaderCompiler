// RUN: %dxc -E main -T cs_5_0 %s

float fn_float_arr(float arr[2]) {
  arr[0] = 123;
  return arr[0];
}


[numthreads(8,8,1)]
void main() {

  float arr2[2] = { 1, 2 };
  float m = fn_float_arr(arr2);
}