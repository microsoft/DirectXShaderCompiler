// RUN: %dxc /T vs_6_0 /E main > %s | FileCheck %s | XFail

AppendStructuredBuffer<int2> results;

void main()
{
  int1x1 variable, result;
  
  // Post-increment
  // CHECK: i32 11, i32 10
  variable = int1x1(10);
  result = variable++;
  results.Append(int2(variable._11, result._11));
  
  // Post-decrement
  // CHECK: i32 9, i32 10
  variable = int1x1(10);
  result = variable--;
  results.Append(int2(variable._11, result._11));
  
  // Pre-increment
  // CHECK: i32 11, i32 11
  variable = int1x1(10);
  result = ++variable;
  results.Append(int2(variable._11, result._11));
  
  // Pre-decrement
  // CHECK: i32 9, i32 9
  variable = int1x1(10);
  result = --variable;
  results.Append(int2(variable._11, result._11));
}