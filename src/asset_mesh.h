#ifndef RH_ASSET_MESH_H
#define RH_ASSET_MESH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* PSP-friendly static, indexed and 1/64-metre quantized mesh library.
 * No heap allocation, no textures, no PSP headers: usable in native unit tests.
 * Position axes: X right, Y up, Z forward; world is metres. */
typedef enum {
 RH_ASSET_CAR=0, RH_ASSET_TRUCK, RH_ASSET_BUILDING, RH_ASSET_WAREHOUSE,
 RH_ASSET_STREETLAMP, RH_ASSET_TREE, RH_ASSET_GRASS, RH_ASSET_BRIDGE,
 RH_ASSET_PUDDLE, RH_ASSET_COUNT
} RHAssetType;
typedef struct {float x,y,z,yaw,scale;unsigned variant;} RHAssetInstance;
typedef struct {float x,y,z;} RHAssetPoint;
typedef struct {RHAssetPoint p[3];uint32_t abgr;uint8_t emissive;} RHAssetTriangle;
typedef void (*RHAssetEmit)(void *ctx,const RHAssetTriangle *triangle);
typedef struct {
 uint32_t sky_zenith,sky_mid,sky_horizon,sky_ground;
 float ambient, direct_sun, lamp_strength, window_strength, wet_glint;
} RHAssetAtmosphere;
/* Hours wrap to [0,24); cloudiness and wetness are clamped [0,1].
 * Returns smooth, rich sunrise/sunset/day/night palette without shaders. */
RHAssetAtmosphere rh_asset_atmosphere(float hour,float cloudiness,float wetness);
/* Emits transformed triangles with near <65m, medium <170m, far <=400m.
 * Returns emitted triangle count; zero when culled, invalid, or no callback.
 * The receiver MUST NOT retain the pointer passed to emit (stack storage).
 * Emissive triangles must skip renderer global daylight multiplication.
 * Caller supplies base world Y, e.g. rh_height(x,z). */
unsigned rh_asset_emit(RHAssetType type,RHAssetInstance instance,float distance_m,
 RHAssetAtmosphere light,RHAssetEmit emit,void *ctx);
unsigned rh_asset_triangle_budget(RHAssetType type,unsigned lod);
unsigned rh_asset_vertex_budget(RHAssetType type,unsigned lod);
#ifdef __cplusplus
}
#endif
#endif
