#ifndef RH_DRIVE_PHYSICS_H
#define RH_DRIVE_PHYSICS_H
/* Deterministic vehicle dynamics in metres, kilograms and seconds. */
#include <stdint.h>
#define RH_DRIVE_VEHICLE_SLOTS 24

typedef enum { RH_DRIVE_TRUCK=0, RH_DRIVE_CAR=1, RH_DRIVE_VAN=2 } RH_DriveKind;
typedef struct { float mass_kg,wheelbase_m,length_m,torque_nm,power_kw;
 float max_speed_mps,wheel_radius_m,steer_max_rad,drag_coefficient;
 float ratios[6],final_drive; } RH_DriveSpec;
typedef struct { float x,z,yaw,speed_mps,rpm,engine_response,damage;
 float fuel_l,trailer_yaw,trailer_mass_kg; uint8_t gear,kind,occupied,active; } RH_DriveBody;
typedef struct { float steer,throttle,brake; uint8_t handbrake,reverse; } RH_DriveInput;
typedef struct { float x,z; int vehicle_index; uint8_t on_foot; } RH_Driver;

extern const RH_DriveSpec RH_DRIVE_SPECS[3];
void rh_drive_init(RH_DriveBody *v,int kind,float x,float z,float yaw);
void rh_drive_step(RH_DriveBody *v,RH_DriveInput input,float dt,float wetness,float road_grip,float slope);
/* Returns 1 on a collision with nonzero penetration. Both bodies are updated. */
int rh_drive_resolve_collision(RH_DriveBody *a,RH_DriveBody *b);
/* Enter/exit only at safe speeds, never teleport onto an occupied vehicle. */
int rh_drive_try_enter(RH_Driver *driver,RH_DriveBody *fleet,int count,int vehicle_index);
int rh_drive_try_exit(RH_Driver *driver,RH_DriveBody *fleet,int count);
#endif
