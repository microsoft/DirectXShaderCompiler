// RUN: %dxc -T vs_6_9 -E RayQueryTests -verify %s
// RUN: %dxc -T vs_6_5 -E RayQueryTests2 -verify %s

// validate 2nd template argument flags
// expected-error@+1{{When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set.}}
typedef RayQuery<RAY_FLAG_FORCE_OMM_2_STATE> BadRayQuery;
// expected-error@+1{{When using 'RAY_FLAG_FORCE_OMM_2_STATE' in RayFlags, RayQueryFlags must have RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS set.}}
typedef RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, 0> BadRayQuery2;

static BadRayQuery rayQuery0a;

static BadRayQuery2 rayQuery0b;
