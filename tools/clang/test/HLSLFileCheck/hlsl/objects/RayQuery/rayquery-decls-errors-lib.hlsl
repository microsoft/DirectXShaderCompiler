// RUN: %dxc -T vs_6_5 -E main -verify %s

int retNum(){
  // expected-warning@+1{{potential misuse of built-in constant RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS in shader model vs_6_5; introduced in shader model 6.9}}
  return RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS;
}

void main(uint i : IDX, RayDesc rayDesc : RAYDESC) {
  int num = retNum();
  if (num > 2){
    RayQuery<0> r1;
    RayQuery<0, 0> r2;
  }
  return;  
}
