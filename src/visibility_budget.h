#ifndef RH_VISIBILITY_BUDGET_H
#define RH_VISIBILITY_BUDGET_H
/* PSP city-world renderer helper: deterministic 128m sectors with fixed resident
 * count and camera-facing priority, without allocation while driving.
 * Render does not depend on this until the integrator wires it in.
 */
#include <stdint.h>
#define RH_VIEW_WORLD_METRES 4096
#define RH_VIEW_TILE_METRES 128
#define RH_VIEW_TILE_SIDE 32
#define RH_VIEW_TILE_TOTAL 1024
#define RH_VIEW_MAX_VISIBLE 48
#define RH_VIEW_PREFETCH_METRES 545.0f
#define RH_VIEW_ZONE_CITY 1u
#define RH_VIEW_ZONE_WATER 2u

typedef struct { uint16_t id; uint8_t lod; uint8_t zone; } RH_ViewTile;
typedef struct {
    RH_ViewTile cells[RH_VIEW_MAX_VISIBLE];
    uint16_t count;
    /* Remember previous loaded tiles to reduce edge-thrashing while steering. */
    uint8_t history[RH_VIEW_TILE_TOTAL/8];
} RH_ViewSet;
void rh_view_init(RH_ViewSet *state);
/* z positive is north in game coordinates; forward = (sin(yaw),cos(yaw)).
 * zones is a 1024-byte array or NULL; cells sorted by prioritized distance.
 * The renderer should instantiate only cells returned by this routine.
 */
void rh_view_collect(RH_ViewSet *state, float player_x, float player_z,
                     float heading_rad, const uint8_t *zones);
/* Returns the visible LOD (0 near, 1 medium, 2 distant), -1 if culled. */
int rh_view_lod(const RH_ViewSet *state, int tile_id);
#endif
