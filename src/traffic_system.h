#ifndef RH_TRAFFIC_SYSTEM_H
#define RH_TRAFFIC_SYSTEM_H

/* PSP-safe city traffic and patrol simulation. No allocation, no PSP headers.
 * Coordinates and speeds use metres and seconds; 1 world unit = 1 metre.
 * Call rh_traffic_step at fixed 1/60 s (or bounded variable dt <= 0.05 s).
 */
#include <stdint.h>

#define RH_TNODES 40
#define RH_TEDGES 112
#define RH_TSIGNALS 12
#define RH_TCARS 22
#define RH_TPEDESTRIANS 12
#define RH_TPATROLS 3
#define RH_TRAFFIC_INVALID 255u

typedef enum { RH_SIGNAL_NS_GREEN=0, RH_SIGNAL_NS_YELLOW=1,
 RH_SIGNAL_ALL_RED_A=2, RH_SIGNAL_EW_GREEN=3,
 RH_SIGNAL_EW_YELLOW=4, RH_SIGNAL_ALL_RED_B=5 } RH_SignalPhase;
typedef enum { RH_VIOLATION_SPEEDING=1, RH_VIOLATION_RED_LIGHT=2,
 RH_VIOLATION_COLLISION=3 } RH_Violation;

typedef struct { float x,z; uint8_t signal; uint8_t road_class; } RH_TNode;
typedef struct { uint8_t from,to; float speed_limit; uint8_t road_class; } RH_TEdge;
typedef struct { uint8_t node; uint8_t phase; float timer; } RH_TSignal;
typedef struct { float x,z,yaw,speed,damage; float progress;
 uint8_t edge,active,kind,occupied; } RH_TCar;
typedef struct { float x,z,from_x,from_z,to_x,to_z,speed;
 uint8_t signal,active,crossing; } RH_TPedestrian;
typedef struct { float x,z,yaw,speed; uint8_t active; } RH_TPatrol;
typedef struct { float x,z,speed,heading; float bank; } RH_TPlayer;
typedef struct {
 RH_TNode nodes[RH_TNODES]; RH_TEdge edges[RH_TEDGES];
 RH_TSignal signals[RH_TSIGNALS]; RH_TCar cars[RH_TCARS];
 RH_TPedestrian pedestrians[RH_TPEDESTRIANS]; RH_TPatrol patrols[RH_TPATROLS];
 uint8_t node_count,edge_count,signal_count,car_count,ped_count;
 uint8_t wanted,patrol_count,last_violation;
 float chase_time,chase_contact,patrol_cooldown,speed_violation_time;
 float fines_total; unsigned int fines_count; uint32_t rng;
 float player_prev_x,player_prev_z; uint8_t player_observed;
} RH_TrafficWorld;

void rh_traffic_init(RH_TrafficWorld *w, uint32_t seed);
int rh_traffic_add_node(RH_TrafficWorld *w,float x,float z,uint8_t road_class);
int rh_traffic_add_edge(RH_TrafficWorld *w,int from,int to,float speed_limit,uint8_t road_class);
int rh_traffic_add_signal(RH_TrafficWorld *w,int node);
int rh_traffic_spawn_car(RH_TrafficWorld *w,int edge,float progress,int kind);
int rh_traffic_spawn_pedestrian(RH_TrafficWorld *w,int signal,float from_x,float from_z,float to_x,float to_z);
void rh_traffic_build_sample_city(RH_TrafficWorld *w,float x0,float z0,float spacing);
void rh_traffic_report_violation(RH_TrafficWorld *w,RH_Violation violation,const RH_TPlayer *player);
/* Reports a fine only once on catch, deducts no more than available balance. */
void rh_traffic_step(RH_TrafficWorld *w,RH_TPlayer *player,float dt,float wetness);
int rh_traffic_signal_green(const RH_TrafficWorld *w,int signal,int edge);
int rh_traffic_active_cars(const RH_TrafficWorld *w);
int rh_traffic_active_pedestrians(const RH_TrafficWorld *w);
int rh_traffic_active_patrols(const RH_TrafficWorld *w);

#endif
