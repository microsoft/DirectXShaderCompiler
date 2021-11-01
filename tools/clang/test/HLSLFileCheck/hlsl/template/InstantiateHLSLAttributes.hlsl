// RUN: %dxc -T lib_6_6 -enable-templates %s -ast-dump %s | FileCheck %s


template<typename U, typename FV>
struct Test {
  U col : SV_Target;
  FV pos : SV_Position;
  U vid : SV_VertexID;
  FV clip : SV_ClipDistance;
  FV cull : SV_CullDistance;
  U gid : SV_GroupID;
  U tid : SV_GroupThreadID;
  U reg : register(c0);
  U pack : packoffset(c1);
  U x0 : read();
  U x1 : read(miss);
  U x2 : read(closesthit);
  U x3 : read(miss, closesthit);
  U x4 : read(anyhit);
  U x5 : read(miss, anyhit);
  U x6 : read(closesthit, anyhit);
  U x7 : read(miss, closesthit, anyhit);
  U x8 : read(caller);
  U x9 : read(miss, caller);
  U x10 : read(closesthit, caller);
  U x11 : read(miss, closesthit, caller);
  U x12 : read(anyhit, caller);
  U x13 : read(miss, anyhit, caller);
  U x14 : read(closesthit, anyhit, caller);
  U x15 : read(miss, closesthit, anyhit, caller);
  U x16 : write(miss);
  U x17 : write(miss) : read(miss);
  U x18 : write(miss) : read(closesthit);
  U x19 : write(miss) : read(miss, closesthit);
  U x20 : write(miss) : read(anyhit);
  U x21 : write(miss) : read(miss, anyhit);
  U x22 : write(miss) : read(closesthit, anyhit);
  U x23 : write(miss) : read(miss, closesthit, anyhit);
  U x24 : write(miss) : read(caller);
  U x25 : write(miss) : read(miss, caller);
  U x26 : write(miss) : read(closesthit, caller);
  U x27 : write(miss) : read(miss, closesthit, caller);
  U x28 : write(miss) : read(anyhit, caller);
  U x29 : write(miss) : read(miss, anyhit, caller);
  U x30 : write(miss) : read(closesthit, anyhit, caller);
  U x31 : write(miss) : read(miss, closesthit, anyhit, caller);
};

Test<uint, float4> Val;

//CHECK:      FieldDecl 0x{{[0-9a-zA-Z]+}} <line:6:3, col:5> col:5 col 'U'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_Target"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:7:3, col:6> col:6 pos 'FV'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:12> "SV_Position"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:8:3, col:5> col:5 vid 'U'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_VertexID"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:9:3, col:6> col:6 clip 'FV'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:13> "SV_ClipDistance"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:10:3, col:6> col:6 cull 'FV'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:13> "SV_CullDistance"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:11:3, col:5> col:5 gid 'U'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_GroupID"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:12:3, col:5> col:5 tid 'U'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_GroupThreadID"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:13:3, col:5> col:5 reg 'U'
//CHECK-NEXT: `-RegisterAssignment 0x{{[0-9a-zA-Z]+}} <col:11> register(c0)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:14:3, col:5> col:5 pack 'U'
//CHECK-NEXT: `-ConstantPacking 0x{{[0-9a-zA-Z]+}} <col:12> packoffset(c1.x)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:15:3, col:5> col:5 x0 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> write()
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:16:3, col:5> col:5 x1 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:17:3, col:5> col:5 x2 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:18:3, col:5> col:5 x3 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:19:3, col:5> col:5 x4 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:20:3, col:5> col:5 x5 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:21:3, col:5> col:5 x6 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:22:3, col:5> col:5 x7 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:23:3, col:5> col:5 x8 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:24:3, col:5> col:5 x9 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:25:3, col:5> col:5 x10 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:26:3, col:5> col:5 x11 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(miss, closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:27:3, col:5> col:5 x12 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:28:3, col:5> col:5 x13 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(miss, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:29:3, col:5> col:5 x14 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(closesthit, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:30:3, col:5> col:5 x15 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(miss, closesthit, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:31:3, col:5> col:5 x16 'U'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:32:3, col:5> col:5 x17 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:33:3, col:5> col:5 x18 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:34:3, col:5> col:5 x19 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:35:3, col:5> col:5 x20 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:36:3, col:5> col:5 x21 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:37:3, col:5> col:5 x22 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:38:3, col:5> col:5 x23 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:39:3, col:5> col:5 x24 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:40:3, col:5> col:5 x25 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:41:3, col:5> col:5 x26 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:42:3, col:5> col:5 x27 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:43:3, col:5> col:5 x28 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:44:3, col:5> col:5 x29 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:45:3, col:5> col:5 x30 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:46:3, col:5> col:5 x31 'U'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, closesthit, anyhit, caller)

//CHECK:      FieldDecl 0x{{[0-9a-zA-Z]+}} <line:6:3, col:5> col:5 col 'unsigned int':'unsigned int'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_Target"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:7:3, col:6> col:6 pos 'vector<float, 4>':'vector<float, 4>'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:12> "SV_Position"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:8:3, col:5> col:5 vid 'unsigned int':'unsigned int'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_VertexID"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:9:3, col:6> col:6 clip 'vector<float, 4>':'vector<float, 4>'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:13> "SV_ClipDistance"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:10:3, col:6> col:6 cull 'vector<float, 4>':'vector<float, 4>'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:13> "SV_CullDistance"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:11:3, col:5> col:5 gid 'unsigned int':'unsigned int'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_GroupID"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:12:3, col:5> col:5 tid 'unsigned int':'unsigned int'
//CHECK-NEXT: `-SemanticDecl 0x{{[0-9a-zA-Z]+}} <col:11> "SV_GroupThreadID"
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:13:3, col:5> col:5 reg 'unsigned int':'unsigned int'
//CHECK-NEXT: `-RegisterAssignment 0x{{[0-9a-zA-Z]+}} <col:11> register(c0)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:14:3, col:5> col:5 pack 'unsigned int':'unsigned int'
//CHECK-NEXT: `-ConstantPacking 0x{{[0-9a-zA-Z]+}} <col:12> packoffset(c1.x)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:15:3, col:5> col:5 x0 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> write()
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:16:3, col:5> col:5 x1 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:17:3, col:5> col:5 x2 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:18:3, col:5> col:5 x3 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:19:3, col:5> col:5 x4 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:20:3, col:5> col:5 x5 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:21:3, col:5> col:5 x6 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:22:3, col:5> col:5 x7 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:23:3, col:5> col:5 x8 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:24:3, col:5> col:5 x9 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:10> read(miss, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:25:3, col:5> col:5 x10 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:26:3, col:5> col:5 x11 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(miss, closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:27:3, col:5> col:5 x12 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:28:3, col:5> col:5 x13 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(miss, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:29:3, col:5> col:5 x14 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(closesthit, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:30:3, col:5> col:5 x15 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> read(miss, closesthit, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:31:3, col:5> col:5 x16 'unsigned int':'unsigned int'
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:32:3, col:5> col:5 x17 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:33:3, col:5> col:5 x18 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:34:3, col:5> col:5 x19 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, closesthit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:35:3, col:5> col:5 x20 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:36:3, col:5> col:5 x21 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:37:3, col:5> col:5 x22 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:38:3, col:5> col:5 x23 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, closesthit, anyhit)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:39:3, col:5> col:5 x24 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:40:3, col:5> col:5 x25 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:41:3, col:5> col:5 x26 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:42:3, col:5> col:5 x27 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, closesthit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:43:3, col:5> col:5 x28 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:44:3, col:5> col:5 x29 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:45:3, col:5> col:5 x30 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(closesthit, anyhit, caller)
//CHECK-NEXT: FieldDecl 0x{{[0-9a-zA-Z]+}} <line:46:3, col:5> col:5 x31 'unsigned int':'unsigned int'
//CHECK-NEXT: |-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:11> write(miss)
//CHECK-NEXT: `-PayloadAccessQualifier 0x{{[0-9a-zA-Z]+}} <col:25> read(miss,
//closesthit, anyhit, caller)
