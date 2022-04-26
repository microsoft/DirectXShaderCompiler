// RUN: %dxc -T cs_6_0 -DDEST=ro_structbuf -DIDX=    %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=ro_structbuf -DIDX=[0] %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=ro_buf -DIDX=    %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=ro_buf -DIDX=[0] %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=ro_tex -DIDX=    %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=ro_tex -DIDX=[0] %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=gs_var -DIDX=    %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=gs_arr -DIDX=[0] %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=cb_var -DIDX=    %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=cb_arr -DIDX=[0] %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=sc_var -DIDX=    %s | %FileCheck %s
// RUN: %dxc -T cs_6_0 -DDEST=sc_arr -DIDX=[0] %s | %FileCheck %s
// These two are different because they aren't const, so are caught later
// RUN: %dxc -T cs_6_0 -DDEST=loc_var -DIDX=    %s | %FileCheck %s -check-prefix=CHKLOC
// RUN: %dxc -T cs_6_0 -DDEST=loc_arr -DIDX=[0] %s | %FileCheck %s -check-prefix=CHKLOC


// Test various Interlocked ops using different invalid destination memory types
// The way the dest param of atomic ops is lowered is unique and missed a lot of
// these invalid uses. There are a few points where the lowering branches depending
// on the memory type, so this tries to cover all those branches:
// groupshared, cbuffers, structbuffers, other resources, and other non-resources

// Valid resources for atomics to create valid output that will later be manipulated to test the validator
RWStructuredBuffer<uint> rw_structbuf;
RWBuffer<uint> rw_buf;
RWTexture1D<uint> rw_tex;

StructuredBuffer<uint> ro_structbuf;
Buffer<uint> ro_buf;
Texture1D<uint> ro_tex;

RWBuffer<uint4> rw_bufswiz;
RWTexture1D<uint4> rw_texswiz;

struct TexCoords {
  uint s, t, r, q;
};

RWBuffer<TexCoords> rw_bufmemb;

struct TexCoordsBits {
  uint s : 8;
  uint t : 8;
  uint r : 8;
  uint q : 8;
};

RWBuffer<TexCoordsBits> rw_bufbits;
RWStructuredBuffer<TexCoordsBits> rw_strbits;

const groupshared uint cgs_var;

groupshared uint gs_var;
groupshared TexCoordsBits gs_bits;

RWStructuredBuffer<float4> output; // just something to keep the variables alive

cbuffer CB {
  uint cb_var;
}
uint cb_gvar;

#if __SHADER_TARGET_STAGE == __SHADER_STAGE_LIBRARY
void init(out uint i); // To force an alloca pointer to use with atomic op
#else
void init(out uint i) {i = 0;}
#endif

[numthreads(1,1,1)]
void main(uint ix : SV_GroupIndex) {

  uint res;
  init(res);

  InterlockedAdd(rw_structbuf[ix], 1);
  InterlockedCompareStore(rw_structbuf[ix], 1, 2);

  InterlockedAdd(rw_buf[ix], 1);
  InterlockedCompareStore(rw_buf[ix], 1, 2);

  InterlockedAdd(rw_tex[ix], 1);
  InterlockedCompareStore(rw_tex[ix], 1, 2);

  InterlockedAdd(gs_var, 1);
  InterlockedCompareStore(gs_var, 1, 2);

  // Token usages of the invalid resources and variables so they are available in the output
  output[ix] = ix + cb_var + cb_gvar + rw_bufbits[ix].s + rw_strbits[ix].s + cgs_var + gs_bits.q +
    rw_bufswiz[ix].x + rw_texswiz[ix].x + ro_structbuf[ix] + ro_buf[ix] + ro_tex[ix] + rw_bufmemb[ix].s;
}
