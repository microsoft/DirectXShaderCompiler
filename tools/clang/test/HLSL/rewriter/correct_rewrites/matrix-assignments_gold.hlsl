// Rewrite unchanged result:





float pick_one(float2x2 f2) {

  return 1;
}

float2x2 ret_f22() {
  float2x2 result = 0;
  return result;
}







void main() {

float2 f2;



float1x1 f11_default;
float2x2 f22_default;
matrix<int, 2, 3> i23_default;







float2x2 f22_target = float2x2(0.1f, 0.2f, 0.3f, 0.4f);
float2x2 f22_target_clone = float2x2(f22_target);


float2x2 f22_target_mix = float2x2(0.1f, f11_default, f2);







matrix<float, 2, 2> f22_copy = f22_default;
float f = pick_one(f22_default);
matrix<float, 2, 2> f22_copy_ret = ret_f22();
float1x2 f22_arr[2] = { 1, 2, 10, 20 };
float2x2 f22_list_copy = { 1, 2, 3, 4 };
int2x2 i22_list_narrowing = { 1.5f, 1.5f, 1.5f, 1.5f };

}
