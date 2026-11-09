// RUN: %dxc -T cs_6_0 -E main -HV 202x %s | FileCheck %s --check-prefixes=CHECK,OPT
// RUN: %dxc -T cs_6_0 -E main -HV 202x -Od %s | FileCheck %s --check-prefixes=CHECK,NOOPT
// RUN: %dxc -T cs_6_0 -E main -HV 202x -Zi -Qembed_debug %s | FileCheck %s --check-prefixes=CHECK,OPT
// RUN: %dxc -T cs_6_0 -E main -HV 2021 %s | FileCheck %s --check-prefixes=CHECK,OPT
// RUN: %dxc -T cs_6_0 -E main -HV 202x -fcgl %s | FileCheck %s --check-prefix=FCGL
// RUN: %dxc -T cs_6_0 -E main -HV 2021 -fcgl %s | FileCheck %s --check-prefix=FCGL

// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 0, i32 undef, i32 3,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 4, i32 undef, i32 4,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 8, i32 undef, i32 5,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 12, i32 undef, i32 3,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 16, i32 undef, i32 6,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 20, i32 undef, i32 3,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 24, i32 undef, i32 7,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 28, i32 undef, i32 6,
// OPT: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 32, i32 undef, i32 5,
// NOOPT: [[FLOAT:%[^ ]+]] = uitofp i32 5 to float
// NOOPT: [[INT:%[^ ]+]] = fptoui float [[FLOAT]] to i32
// NOOPT: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 32, i32 undef, i32 [[INT]],

// FCGL-DAG: %struct.StringState = type { i8*, i32 }
// FCGL-DAG: %struct.StringOnly = type { i8* }
// FCGL-DAG: %struct.NestedState = type { [2 x %struct.StringState] }
// FCGL-DAG: %struct.DerivedState = type { %struct.StringState, i8* }
// FCGL-DAG: @GlobalText = internal constant i8* getelementptr inbounds ([6 x i8], [6 x i8]* [[HELLO:@"[^"]+"]], i32 0, i32 0)
// FCGL-DAG: @OnlyText = internal constant %struct.StringOnly { i8* getelementptr inbounds ([6 x i8], [6 x i8]* [[HELLO]], i32 0, i32 0) }
// FCGL-DAG: @GlobalLiteral = internal global %struct.StringState { i8* getelementptr inbounds ([6 x i8], [6 x i8]* [[HELLO]], i32 0, i32 0), i32 3 }
// FCGL-DAG: [[HELLO]] = linkonce_odr unnamed_addr constant [6 x i8] c"hello\00"
// FCGL-LABEL: define internal void {{.*CopyText.*}}(i8* %Text, i8** noalias dereferenceable(4) %Result)
// FCGL: [[SLOT:%[^ ]+]] = alloca i8*
// FCGL: store i8* %Text, i8** [[SLOT]]
// FCGL: [[VALUE:%[^ ]+]] = load i8*, i8** [[SLOT]]
// FCGL: store i8* [[VALUE]], i8** %Result
// FCGL-LABEL: define internal void {{.*ConvertState.*}}
// FCGL: [[TEXT_PTR:%[^ ]+]] = getelementptr inbounds %struct.StringState, %struct.StringState* %State, i32 0, i32 0
// FCGL: [[TEXT:%[^ ]+]] = load i8*, i8** [[TEXT_PTR]]
// FCGL: store i8* [[TEXT]], i8**
// FCGL: uitofp i32

struct StringOnly {
  string Text;
};

struct StringState {
  string Text;
  uint Value;
};

struct FloatStringState {
  string Text;
  float Value;
};

typedef StringState StateType;

struct NestedState {
  StringState Values[2];
};

struct FloatNestedState {
  FloatStringState Values[2];
};

struct DerivedState : StringState {
  string Other;
};

static const string GlobalText = "hello";
static StringState GlobalLiteral = {"hello", 3};
static const StateType GlobalReference = {GlobalText, 4};
static NestedState Nested = {{{"hello", 5}, GlobalLiteral}};
static DerivedState Derived = {GlobalLiteral, GlobalText};
static const StringOnly OnlyText = {"hello"};

RWByteAddressBuffer Output : register(u0);

StringState Copy(StringState State) {
  return State;
}

void Set(out StringState State) {
  State.Text = "hello";
  State.Value = 6;
}

void CopyText(string Text, out string Result) {
  Result = Text;
}

FloatStringState ConvertState(StringState State) {
  return (FloatStringState)State;
}

[numthreads(1, 1, 1)]
void main() {
  StringState Local = Copy(GlobalLiteral);
  Output.Store(0, Local.Value);
  Output.Store(4, GlobalReference.Value);
  Output.Store(8, Nested.Values[0].Value);
  Output.Store(12, Derived.Value);
  StringOnly Text = OnlyText;
  CopyText(Derived.Other, Text.Text);
  Local.Text = Text.Text;
  Set(Local);
  Output.Store(16, Local.Value);
  Output.Store(20, Nested.Values[1].Value);
  static const StringState StaticLocal = {"hello", 7};
  Output.Store(24, StaticLocal.Value);
  FloatStringState Converted = ConvertState(Local);
  Output.Store(28, (uint)Converted.Value);
  FloatNestedState ConvertedNested = (FloatNestedState)Nested;
  Output.Store(32, (uint)ConvertedNested.Values[0].Value);
}
