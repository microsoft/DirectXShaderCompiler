// RUN: %clang_cc1 -fsyntax-only -verify %s



[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1025,1,1)] // expected-error {{NumThreads group size may not exceed 1024 (x * y * z)}}
void node17()
{ }

[Shader("node")]
[NumThreads(128,8,2)] // expected-error {{NumThreads group size may not exceed 1024 (x * y * z)}}
[NodeLaunch("coalescing")]
void node18()
{ }
