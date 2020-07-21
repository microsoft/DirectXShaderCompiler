// RUN: %dxc -T lib_6_6 %s  | FileCheck %s


//CHECK: define i32 @"\01?array_in_if@@YAHHH@Z"
//CHECK: alloca
//CHECK: icmp
//CHECK: br i1
//CHECK: call void @llvm.lifetime.start
//CHECK: br label
//CHECK: load i32
//CHECK: call void @llvm.lifetime.end
//CHECK: br label
//CHECK: phi i32
//CHECK: load i32
//CHECK: store i32
//CHECK: br i1
//CHECK: phi i32
//CHECK: ret i32
export
int array_in_if(int n, int c)
{
  int res = c;

  if (n > 0) {
    int arr[200];

    // Fake some dynamic initialization so the array can't be optimzed away.
    for (int i=0; i<n; ++i) {
        arr[i] = arr[c - i];
    }

    res = arr[c];
  }

  return res;
}

//CHECK: define i32 @"\01?conditional_struct_init_in_loop@@YAHHH@Z"(i32 %n, i32 %c1)
//CHECK: br i1
//CHECK: phi i32
//CHECK: ret i32
//CHECK: phi i32
//CHECK-NOT: phi i32
//CHECK: dx.op.rawBufferStore
//CHECK: br i1
//CHECK: dx.op.rawBufferLoad
//CHECK: phi i32
struct MyStruct {
  int x;
};

RWStructuredBuffer<MyStruct> g_rwbuf : register(u0);

void expensiveComputation(int i, int x)
{
  // We'd prefer this to not be inlined to better illustrate things but that's
  // not possible without more effort.
  g_rwbuf[i].x = x;
}

export
int conditional_struct_init_in_loop(int n, int c1)
{
  int res = n;

  for(int i=0; i<n; ++i) {
    expensiveComputation(i, i*c1); // s mut not be considered live here.

    MyStruct s;

    // Initialize struct conditionally.
    // NOTE: If some optimization decides to flatten the if statement or if the
    //       computation could be hoisted out of the loop, the phi with undef
    //       below will be replaced by the non-undef value (which is a valid
    //       "specialization" of undef).
    if (c1 < 0)
      s.x = g_rwbuf[i - c1].x;

    res = s.x; // i or undef.
  }

  return res; // Result is n if loop wasn't executed, n-1 if it was.
}

//CHECK: define i32 @"\01?conditional_out_assign@@YAHHH@Z"(i32 %n, i32 %c1)
//CHECK: phi i32
//CHECK-NOT: phi i32
void consume(int i, in MyStruct data)
{
  // We'd prefer this to not be inlined to better illustrate things but that's
  // not possible without more effort.
  g_rwbuf[i] = data;
}

bool produce(in int c, out MyStruct data)
{
  if (c > 0) {
    MyStruct s;
    s.x = 13;
    data = s; // <-- Conditional assignment of out-qualified parameter.
    return true;
  }
  return false; // <-- Out-qualified parameter left uninitialized.
}

export
int conditional_out_assign(int n, int c1)
{
  for (int i=0; i<n; ++i) {
    MyStruct data;
    bool valid = produce(c1, data); // <-- Without lifetimes, inlining this generates a loop phi using prior iteration's value.
    if (valid)
      consume(i, data);
    expensiveComputation(i, i*n); // <-- Said phi is alive here, inflating register pressure.
  }
  return n;
}

//CHECK: define i32 @"\01?global_constant@@YAHH@Z"(i32 %n)
//CHECK: call void @llvm.lifetime.start
//CHECK: load i32
//CHECK: call void @llvm.lifetime.end
//CHECK: ret i32
int compute(int i)
{
  int arr[] = {0, 1, 2, 3, 4, 5, -1, 13};
  return arr[i % 8];
}

export
int global_constant(int n)
{
  return compute(n);
}

//CHECK: define i32 @"\01?global_constant2@@YAHH@Z"(i32 %n)
//CHECK: phi i32
//CHECK: phi i32
//CHECK: call void @llvm.lifetime.start
//CHECK: load i32
//CHECK: call void @llvm.lifetime.end
// Constant array should be hoisted to a constant global with
// lifetime only inside the loop.
export
int global_constant2(int n)
{
  int res = 0;
  for (int i = 0; i < n; ++i)
    res += compute(i);
  return res;
}
