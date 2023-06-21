// RUN: %clang_cc1 -fsyntax-only -verify %s
// ==================================================================
// CASE120 (error)
// Errors are generated for writes to members of read-only records
// ==================================================================

struct RECORD
{
  bool b;
};

[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("Broadcasting")]
void node01(DispatchNodeInputRecord<RECORD> input1)
{
  input1.Get().b = false; //expected-error{{cannot assign to return value because function 'Get' returns a const value}}
}

[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("Broadcasting")]
void node02(RWDispatchNodeInputRecord<RECORD> input2)
{
  input2.b = true; //expected-error{{no member named 'b' in 'RWDispatchNodeInputRecord<RECORD>'}}
  input2.Get().b = true;
}

[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("coalescing")]
void node03([MaxRecords(3)] GroupNodeInputRecords<RECORD> input3)
{
  input3.Get().b = false; //expected-error{{cannot assign to return value because function 'Get' returns a const value}}
  input3[0].b = false; //expected-error{{cannot assign to return value because function 'operator[]' returns a const value}}
}

[Shader("node")]
[NumThreads(1,1,1)]
[NodeLaunch("coalescing")]
void node04([MaxRecords(4)] RWGroupNodeInputRecords<RECORD> input4)
{
  input4.Get().b = true;
  input4.Get(0).b = true;
  input4[0].b = true;
}

[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("Broadcasting")]
void node05(NodeOutput<RECORD> output5)
{
  output5.b = false; //expected-error{{no member named 'b' in 'NodeOutput<RECORD>'}}
  output5.Get().b = false; //expected-error{{no member named 'Get' in 'NodeOutput<RECORD>'}}
}

// expected-note@? +{{function 'Get' which returns const-qualified type 'const RECORD &' declared here}}
// expected-note@? +{{function 'operator[]' which returns const-qualified type 'const RECORD &' declared here}}

