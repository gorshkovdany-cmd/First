# PSP artwork module — original geometry, not yet integrated

The first pass uses **original generated geometry** (no imported third-party meshes yet), because directly importing 45–90 OBJ/glTF files into PSP would exceed the intended draw-call/triangle budget without an offline conversion pipeline. Kenney Car Kit and City Kit are candidate CC0 replacements for the next iteration; no Kenney content is included in this commit.

## Contents

- `src/asset_mesh.h` — stable callback API for the PSP renderer.
- `src/asset_mesh.c` — atmosphere, palettes, lighting and mesh streaming.
- `src/asset_mesh_data.inc` — generated 16-bit indexed mesh library, 9 asset types × 3 LODs.
- `tools/assets/generate_meshes.py` — deterministic offline mesh compiler; Python stdlib only.
- `tools/assets/test_asset_mesh.c` — geometry and atmosphere regression tests.
- `tools/assets/test_assets.sh` — reproducible desktop compilation/test.

Geometry budget: 2,410 unique quantized vertices and 2,964 triangle records across all 27 LODs, approx 38.2 KiB raw geometry (`6 B` vertex + `8 B` triangle, excluding static descriptors/alignment). Runtime emits triangles into the integrator's existing fixed-capacity libGU buffer; it allocates **zero heap memory**. Near LOD <65m, mid <170m, far <400m. Assets include detailed car with chamfered hood/roof and 8-sided wheels, articulated visual truck + trailer, multi-storey lit building, warehouse, streetlamp, layered tree, grass tuft, bridge, and puddle.

The sky is a set of hand-authored color keyframes at 00:00, pre-dawn, golden sunrise, cyan day, violet sunset, blue night. Cloudiness reduces direct sun and desaturates the sky. Night windows and streetlamps use an emissive flag so the renderer can bypass its global sunlight darkening; puddles use the current sky palette as a low-cost reflection approximation. Optional `rh_asset_emit_shadow` draws two alpha-blended projected shadow triangles in daylight, and `rh_asset_emit_light_pool` draws eight warm translucent light-pool triangles at night; neither is ray tracing, PBR or a real shadow map.

## Integration contract

```c
#include "asset_mesh.h"
static void submit(void *unused,const RHAssetTriangle *tri){
    /* In src/render_psp.c, call the renderer's existing mtri(...) for each
       triangle, but DO NOT multiply tri->abgr by sunlight when tri->emissive.
       Respect MAX_3D before writing each three-vertex triangle. */
}
RHAssetAtmosphere sky=rh_asset_atmosphere(g->clock_hours,cloudiness,wetness);
RHAssetInstance car={x,rh_height(x,z),z,yaw,1.0f,variant};
rh_asset_emit(RH_ASSET_CAR,car,dist,sky,submit,0);
```

**Integrator action required:** add `src/asset_mesh.o` to `Makefile` and hook `rh_asset_emit` into the existing renderer. `src/render_psp.c` is owned by the integration role, so it is deliberately untouched here. For the sky, draw a screen-space 4-stop gradient behind the 3D scene using `sky_zenith`, `sky_mid`, `sky_horizon`, `sky_ground`; current solid clear color must be replaced by this gradient for visible effect. Dynamic shadows should be sparse projected dark polygons at near LOD only; no shadow maps. Lighting pool decals under streetlamps and headlights should be gated on `sky.lamp_strength`, and wet puddles should be shown only when wetness is nonzero. Night city can use instanced buildings and a deterministic mix of lit windows.

For convincing *approximate* dynamic shadows and lights, call `rh_asset_emit_shadow` / `rh_asset_emit_light_pool` after opaque geometry in a **separate translucent decal pass**. `RHAssetTriangle.emissive == 2` means alpha-blended decal, not a glowing opaque triangle. Render these with GU_BLEND enabled, depth test enabled, depth writes disabled, and no global daylight tint. `rh_asset_emit_shadow` is limited to 85 m and `rh_asset_emit_light_pool` to 55 m. The decals lie at `instance.y+0.045m`, so the integrator should avoid them on steep hills without local height projection. These are optional and can be disabled entirely on FPS drops.

Do not render every asset at maximum LOD: the 860-triangle close-up building is intentionally a **near-field exception**. Keep visible high-detail buildings below 6 at once and lower further when frame time rises. Use frustum and chunk culling in the integration renderer before invoking this module. The static mesh library is not itself a complete city renderer.

## Verification

Run `sh tools/assets/test_assets.sh` on desktop. This checks all 27 meshes, 1440 times of day, emitted triangle counts, color format, finite coordinates, night emissive lights, and invalid inputs. Passing desktop tests is not proof of PSP graphics or performance; test the integrated build in PPSSPP and on a PSP-3008.
