// RUN: %clang_cc1 -fsyntax-only -verify %s
// ==================================================================
// Errors are generated for node inputs that are not compatible with
// the launch type
// ==================================================================

struct RECORD
{
  uint a;
};

[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("BrOaDcasting")]                         // expected-note  {{Launch type defined here}}
void node01(GroupNodeInputRecords<RECORD> input)     // expected-error {{GroupNodeInputRecords may not be used with broadcasting launch nodes}}
{ }

[Shader("node")]
[NumThreads(1024,1,1)]
void node02(RWGroupNodeInputRecords<RECORD> input)   // expected-error {{RWGroupNodeInputRecords may not be used with broadcasting launch nodes}}
{ }

[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("Broadcasting")]                         // expected-note  {{Launch type defined here}}
void node03(ThreadNodeInputRecord<RECORD> input)     // expected-error {{ThreadNodeInputRecord may not be used with broadcasting launch nodes}}
{ }

[Shader("node")]
[NodeLaunch("Broadcasting")]                         // expected-note  {{Launch type defined here}}
[NumThreads(1024,1,1)]
void node04(RWThreadNodeInputRecord<RECORD> input)   // expected-error {{RWThreadNodeInputRecord may not be used with broadcasting launch nodes}}
{ }

[Shader("node")]
[NodeLaunch("Broadcasting")]                         // expected-note  {{Launch type defined here}}
[NumThreads(1024,1,1)]
void node05(EmptyNodeInput input)                    // expected-error {{EmptyNodeInput may not be used with broadcasting launch nodes}}
{ }

[Shader("node")]
[NumThreads(128,1,1)]
[NodeLaunch("COALESCING")]                           // expected-note  {{Launch type defined here}}
void node06(DispatchNodeInputRecord<RECORD> input)   // expected-error {{DispatchNodeInputRecord may not be used with coalescing launch nodes}}
{ }

[NodeLaunch("Coalescing")]                           // expected-note  {{Launch type defined here}}
[Shader("node")]
[NumThreads(128,1,1)]
void node07(RWDispatchNodeInputRecord<RECORD> input) // expected-error {{RWDispatchNodeInputRecord may not be used with coalescing launch nodes}}
{ }

[Shader("node")]
[NumThreads(128,1,1)]
[NodeLaunch("Coalescing")]                           // expected-note  {{Launch type defined here}}
void node08(ThreadNodeInputRecord<RECORD> input)     // expected-error {{ThreadNodeInputRecord may not be used with coalescing launch nodes}}
{ }

[NodeLaunch("Coalescing")]                           // expected-note  {{Launch type defined here}}
[Shader("node")]
[NumThreads(128,1,1)]
void node09(RWThreadNodeInputRecord<RECORD> input)   // expected-error {{RWThreadNodeInputRecord may not be used with coalescing launch nodes}}
{ }

[Shader("node")]
[NodeLaunch("Thread")]                               // expected-note  {{Launch type defined here}}
void node10(DispatchNodeInputRecord<RECORD> input)   // expected-error {{DispatchNodeInputRecord may not be used with thread launch nodes}}
{ }

[NodeLaunch("Thread")]                               // expected-note  {{Launch type defined here}}
[Shader("node")]
void node11(RWDispatchNodeInputRecord<RECORD> input) // expected-error {{RWDispatchNodeInputRecord may not be used with thread launch nodes}}
{ }

[Shader("node")]
[NodeLaunch("Thread")]                               // expected-note  {{Launch type defined here}}
void node12(GroupNodeInputRecords<RECORD> input)     // expected-error {{GroupNodeInputRecords may not be used with thread launch nodes}}
{ }

[NodeLaunch("ThREAd")]                               // expected-note  {{Launch type defined here}}
[Shader("node")]
void node13([MaxRecords(32)]
            RWGroupNodeInputRecords<RECORD> input)   // expected-error {{RWGroupNodeInputRecords may not be used with thread launch nodes}}
{ }

[Shader("node")]
[NodeLaunch("Thread")]                               // expected-note  {{Launch type defined here}}
void node14(EmptyNodeInput input)                    // expected-error {{EmptyNodeInput may not be used with thread launch nodes}}
{ }