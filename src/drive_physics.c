#include "drive_physics.h"
#include <math.h>
#include <string.h>
#define D_PI 3.14159265358979323846f
static float dmin(float a,float b){return a<b?a:b;}
static float dmax(float a,float b){return a>b?a:b;}
static float dclamp(float v,float lo,float hi){return dmin(hi,dmax(lo,v));}
static float dabs(float v){return v<0?-v:v;}
static float dangle(float a){while(a>D_PI)a-=2*D_PI;while(a< -D_PI)a+=2*D_PI;return a;}

const RH_DriveSpec RH_DRIVE_SPECS[3]={
 {9000,4.1f,8.0f,2300,340,26,.52f,.49f,.84f,
  {12.0f,7.6f,4.7f,2.9f,1.7f,1.0f},3.4f},
 {1450,2.65f,4.4f,225,110,42,.32f,.58f,.31f,
  {3.7f,2.15f,1.45f,1.0f,.8f,.65f},3.6f},
 {2900,3.2f,5.8f,390,160,34,.38f,.52f,.47f,
  {4.1f,2.7f,1.75f,1.17f,.9f,.72f},3.5f}
};
void rh_drive_init(RH_DriveBody *v,int kind,float x,float z,float yaw){
 if(!v)return;
 memset(v,0,sizeof(*v));
 if(kind<0||kind>2)kind=0;
 v->kind=(uint8_t)kind;v->x=isfinite(x)?x:0;v->z=isfinite(z)?z:0;
 v->yaw=isfinite(yaw)?dangle(yaw):0;v->trailer_yaw=v->yaw;
 v->gear=1;v->rpm=720;v->fuel_l=kind==RH_DRIVE_TRUCK?450:kind==RH_DRIVE_CAR?60:95;
 v->active=1;
}
void rh_drive_step(RH_DriveBody *v,RH_DriveInput in,float dt,float wetness,float road_grip,float slope){
 if(!v||!v->active||v->kind>2||!isfinite(dt)||!isfinite(wetness)||
 !isfinite(road_grip)||!isfinite(slope)||!isfinite(v->speed_mps)||
 !isfinite(v->x)||!isfinite(v->z)||!isfinite(v->yaw)||dt<=0)return;
 dt=dmin(dt,.05f);wetness=dclamp(wetness,0,1);road_grip=dclamp(road_grip,.1f,1);
 slope=dclamp(slope,-.3f,.3f);
 const RH_DriveSpec *s=&RH_DRIVE_SPECS[v->kind];
 float mass=s->mass_kg+dclamp(v->trailer_mass_kg,0,35000);
 float speed=dabs(v->speed_mps);
 float throttle=isfinite(in.throttle)?dclamp(in.throttle,0,1):0;
 float brake=isfinite(in.brake)?dclamp(in.brake,0,1):0;
 float steer=isfinite(in.steer)?dclamp(in.steer,-1,1):0;
 float direction=in.reverse?-1.0f:1.0f;
 if(v->speed_mps*direction<-.5f){throttle=0;brake=dmax(brake,.7f);}
 v->engine_response+=dclamp(throttle-v->engine_response,-3.1f*dt,2.4f*dt);
 v->engine_response=dclamp(v->engine_response,0,1);
 if(v->gear<1||v->gear>6)v->gear=1;
 float wheel_rpm=speed*60.0f/(2*D_PI*s->wheel_radius_m);
 float ratio=s->ratios[v->gear-1]*s->final_drive;
 float target_rpm=dclamp(wheel_rpm*ratio+v->engine_response*100,650,6200);
 v->rpm+=dclamp(target_rpm-v->rpm,-1900*dt,2200*dt);
 float up=v->kind==RH_DRIVE_TRUCK?2050:4700;
 float down=v->kind==RH_DRIVE_TRUCK?1050:1700;
 if(v->rpm>up&&v->gear<6&&speed>2){v->gear++;v->rpm*=s->ratios[v->gear-1]/s->ratios[v->gear-2];}
 else if(v->rpm<down&&v->gear>1&&speed>1){v->gear--;v->rpm*=s->ratios[v->gear-1]/s->ratios[v->gear];}
 if(speed<.4f)v->gear=1;
 ratio=s->ratios[v->gear-1]*s->final_drive;
 float peak=v->kind==RH_DRIVE_TRUCK?1450:3900;
 float falloff=dclamp(1.0f-dabs(v->rpm-peak)/(peak*2),.42f,1);
 float wheel_force=s->torque_nm*falloff*ratio*.84f*v->engine_response/s->wheel_radius_m;
 wheel_force=dmin(wheel_force,s->power_kw*1000/dmax(speed,4.0f));
 float mu=road_grip*(1.0f-.62f*wetness);
 float max_traction=mu*mass*9.81f;
 wheel_force=dmin(wheel_force,max_traction);
 if(v->fuel_l<=0)wheel_force=0;
 float drag=s->drag_coefficient*speed*speed;
 float rolling=mass*9.81f*(.012f+(1-road_grip)*.02f);
 float braking=dmin(max_traction,mass*9.81f*(brake*.75f+(in.handbrake?.25f:0)));
 float grade_force=mass*9.81f*slope*direction;
 float decel=(braking+drag+rolling+grade_force)/mass;
 float acceleration=wheel_force/mass-decel;
 float next=dmax(0,speed+acceleration*dt);
 if(v->speed_mps*direction<-.5f)next=dmax(0,speed-(braking+drag+rolling)/mass*dt);
 next=dmin(next,in.reverse?dmin(6.0f,s->max_speed_mps):s->max_speed_mps);
 v->speed_mps=(v->speed_mps*direction<-.5f)?(v->speed_mps<0?-next:next):direction*next;
 float steering=steer*s->steer_max_rad/(1.0f+next*.07f);
 v->yaw=dangle(v->yaw+v->speed_mps/s->wheelbase_m*tanf(steering)*dt);
 v->x+=sinf(v->yaw)*v->speed_mps*dt;
 v->z+=cosf(v->yaw)*v->speed_mps*dt;
 float trailer_delta=dangle(v->yaw-v->trailer_yaw);
 v->trailer_yaw=dangle(v->trailer_yaw+dclamp(trailer_delta,-.6f,.6f)*dclamp(next/10,.03f,1.1f)*dt);
 v->fuel_l=dmax(0,v->fuel_l-dt*(.0006f+.002f*v->engine_response+.00012f*next));
 v->damage=dclamp(v->damage,0,100);
}
static float body_radius(const RH_DriveBody *v){return RH_DRIVE_SPECS[v->kind].length_m*.37f;}
int rh_drive_resolve_collision(RH_DriveBody *a,RH_DriveBody *b){
 if(!a||!b||a==b||!a->active||!b->active||a->kind>2||b->kind>2)return 0;
 float dx=b->x-a->x,dz=b->z-a->z;
 float dist2=dx*dx+dz*dz;
 if(!isfinite(dist2))return 0;
 float min_dist=body_radius(a)+body_radius(b);
 if(dist2>=min_dist*min_dist)return 0;
 float dist=sqrtf(dist2),nx=dist>.001f?dx/dist:1,nz=dist>.001f?dz/dist:0;
 float pen=dclamp(min_dist-dist,0,8);
 float mass_a=RH_DRIVE_SPECS[a->kind].mass_kg+dclamp(a->trailer_mass_kg,0,35000);
 float mass_b=RH_DRIVE_SPECS[b->kind].mass_kg+dclamp(b->trailer_mass_kg,0,35000);
 float ratio_a=mass_b/(mass_a+mass_b),ratio_b=mass_a/(mass_a+mass_b);
 a->x-=nx*pen*ratio_a;a->z-=nz*pen*ratio_a;
 b->x+=nx*pen*ratio_b;b->z+=nz*pen*ratio_b;
 float va_x=sinf(a->yaw)*a->speed_mps,va_z=cosf(a->yaw)*a->speed_mps;
 float vb_x=sinf(b->yaw)*b->speed_mps,vb_z=cosf(b->yaw)*b->speed_mps;
 float closing=(va_x-vb_x)*nx+(va_z-vb_z)*nz;
 if(closing>0){
  float impulse=dmin(250000.0f,1.08f*closing/(1/mass_a+1/mass_b));
  float impulse_a=impulse/mass_a,impulse_b=impulse/mass_b;
  a->speed_mps-=impulse_a*(nx*sinf(a->yaw)+nz*cosf(a->yaw));
  b->speed_mps+=impulse_b*(nx*sinf(b->yaw)+nz*cosf(b->yaw));
  a->speed_mps=dclamp(a->speed_mps,-6,RH_DRIVE_SPECS[a->kind].max_speed_mps);
  b->speed_mps=dclamp(b->speed_mps,-6,RH_DRIVE_SPECS[b->kind].max_speed_mps);
  float damage=dmin(30.0f,closing*closing*.035f);
  a->damage=dmin(100,a->damage+damage*ratio_a);
  b->damage=dmin(100,b->damage+damage*ratio_b);
 }
 return 1;
}
int rh_drive_try_enter(RH_Driver *driver,RH_DriveBody *fleet,int count,int vehicle_index){
 if(!driver||!fleet||!driver->on_foot||count<1||count>RH_DRIVE_VEHICLE_SLOTS||
 vehicle_index<0||vehicle_index>=count)return 0;
 RH_DriveBody *v=&fleet[vehicle_index];
 if(!v->active||v->occupied||dabs(v->speed_mps)>1.2f||
 !isfinite(driver->x)||!isfinite(driver->z)||
 hypotf(driver->x-v->x,driver->z-v->z)>3.2f)return 0;
 driver->on_foot=0;driver->vehicle_index=vehicle_index;v->occupied=1;return 1;
}
int rh_drive_try_exit(RH_Driver *driver,RH_DriveBody *fleet,int count){
 if(!driver||!fleet||driver->on_foot||count<1||count>RH_DRIVE_VEHICLE_SLOTS||
 driver->vehicle_index<0||driver->vehicle_index>=count)return 0;
 RH_DriveBody *v=&fleet[driver->vehicle_index];
 if(!v->active||!v->occupied||dabs(v->speed_mps)>.6f)return 0;
 float ex=v->x+cosf(v->yaw)*3.0f,ez=v->z-sinf(v->yaw)*3.0f;
 if(ex<1||ez<1||ex>4095||ez>4095)return 0;
 for(int i=0;i<count;i++)if(i!=driver->vehicle_index&&fleet[i].active){
  float dist=hypotf(ex-fleet[i].x,ez-fleet[i].z);
  if(dist<body_radius(&fleet[i])+1.1f)return 0;
 }
 driver->x=ex;driver->z=ez;driver->on_foot=1;driver->vehicle_index=-1;
 v->occupied=0;return 1;
}
