
#define myDefine 2
#define myDefine2 21
#define myDefine3 1994
#define myDefine4 1

int func_uses_defines(int a)
{
  int b = myDefine2;
  return a + myDefine + b + myDefine3 + myDefine4;
}