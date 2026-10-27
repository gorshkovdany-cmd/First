#ifndef RH_ROUTE_PLANNER_H
#define RH_ROUTE_PLANNER_H
/* Standalone, deterministic road graph + cargo mission selector.
 * Designed for 4096 x 4096 metre PSP worlds. No PSP SDK dependency.
 * Handles actual distance along connected roads, not straight-line distance.
 */
#include <stdint.h>

#define RH_ROUTE_MAX_NODES 2048
#define RH_ROUTE_MAX_EDGES 8192
#define RH_ROUTE_WORLD_METRES 4096.0f
#define RH_ROUTE_UNREACHABLE 1.0e30f

/* Nodes are intersections, depots, terminals or roadside waypoints.
 * Route graph is directed. For two-way road, call rh_route_add_road.
 */
typedef struct { float x, z; int32_t first_edge; uint8_t depot; uint8_t reserved[3]; } RH_RouteNode;
typedef struct { uint16_t dest; int32_t next; float metres; } RH_RouteEdge;
typedef struct {
    RH_RouteNode nodes[RH_ROUTE_MAX_NODES];
    RH_RouteEdge edges[RH_ROUTE_MAX_EDGES];
    uint16_t node_count;
    uint16_t edge_count;
} RH_RouteGraph;

typedef struct {
    float metres[RH_ROUTE_MAX_NODES];
    int32_t predecessor[RH_ROUTE_MAX_NODES];
    uint16_t heap[RH_ROUTE_MAX_NODES];
    int16_t heap_pos[RH_ROUTE_MAX_NODES];
    uint16_t count;
} RH_RouteSearch;

typedef struct {
    uint16_t from_node;
    uint16_t to_node;
    float route_metres;
    float payout;
    uint8_t category; /* 0 short, 1 medium, 2 long */
} RH_RouteMission;

/* Caller owns graph/search memory (heap allocation strongly recommended).
 * Graph zero-initialisation is mandatory; init performs it.
 */
void rh_route_init(RH_RouteGraph *graph);
int rh_route_add_node(RH_RouteGraph *graph, float x, float z, int depot);
/* Explicit metres can exceed Euclidean distance; zero => use Euclidean. */
int rh_route_add_arc(RH_RouteGraph *graph, int from, int to, float metres);
int rh_route_add_road(RH_RouteGraph *graph, int from, int to, float metres);
/* Nearest waypoint or depot to a GPS position; returns -1 outside world,
 * on empty graph or when no matching depot. Pure lookup, O(node_count). */
int rh_route_nearest_node(const RH_RouteGraph *graph, float x, float z,
                          int depot_only);
/* Dijkstra: completes all reachable distances, avoids repeated solves for jobs. */
int rh_route_scan(const RH_RouteGraph *graph, RH_RouteSearch *search, int start);
/* Returns total number of waypoints (including source + destination), or
 * negative for invalid/unreachable/insufficient buffer. No buffer overflow. */
int rh_route_path(const RH_RouteGraph *graph, const RH_RouteSearch *search,
                  int end, uint16_t *output, int capacity);
/* Pickup and dropoff must be depot nodes; random draw targets 30/40/30%.
 * If no destination in selected bucket, chooses other reachable candidate.
 * Never invents distance. Pays for real distance and cargo weight.
 */
int rh_route_choose_mission(const RH_RouteGraph *graph,
                            const RH_RouteSearch *search, int start,
                            int cargo_tonnes, uint32_t *seed,
                            RH_RouteMission *mission);
/* validate: all depot ids reachable from origin, no invalid edge lengths. */
int rh_route_validate(const RH_RouteGraph *graph, int origin);
#endif
