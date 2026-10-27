#ifndef ROADHAUL_SIM_H
#define ROADHAUL_SIM_H
#include <stdint.h>
#define RH_ROAD_SPACING 1800.0f
#define RH_MAX_TRAFFIC 22
#define RH_SAVE_VERSION 5
#define RH_PI 3.14159265358979323846f

typedef struct { const char *name; float mass_kg, wheelbase_m, torque_nm, gears[8];
    int n_gears; float final_drive, wheel_radius, tank_l, max_steer_rad, price; float length_m; } TruckSpec;
typedef struct { float steer, throttle, brake; int handbrake, reverse_request, action, swap_truck, toggle_camera, pause, map, look; } Controls;
typedef struct { float x,z,yaw,v,speed_abs, rpm, fuel, odo, damage, trailer_yaw;
    float engine_t, gearbox_t; int gear, model, camera, reverse; } Player;
typedef struct { float x,z,yaw,speed; int axis, direction, variant; } Traffic;
typedef struct { int pickup_x, pickup_z, dest_x,dest_z, stage, cargo_tons, type;
    float distance_km, payout; } Job;
typedef struct { Player player; Job job; Traffic cars[RH_MAX_TRAFFIC];
    int traffic_count, paused, map_open, completed, last_notice, notice_ticks;
    unsigned int owned_trucks;
    float bank, drive_time, traffic_t, sim_t, clock_hours; uint32_t rng; } Game;
extern const TruckSpec RH_TRUCKS[3];
void rh_new_game(Game *g);
void rh_step(Game *g, Controls in, float dt);
void rh_set_player(Game *g, float x, float z, float yaw);
float rh_height(float x,float z);
float rh_road_distance(float x,float z);
void rh_depot(int gx,int gz,float *x,float *z);
void rh_station(int gx,int gz,float *x,float *z);
float rh_dist2(float x,float z,float x2,float z2);
float rh_job_target_distance(const Game *g);
void rh_waypoint(const Game *g,float *x,float *z);
int rh_save(const Game *g,const char *path);
int rh_load(Game *g,const char *path);
#endif
