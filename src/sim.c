#include "sim.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const TruckSpec RH_TRUCKS[3] = {
 {"СЕВЕР 18", 7600, 3.65f, 1750, {12.1f, 8.4f, 5.9f, 4.2f, 2.9f, 1.9f, 1.3f, 1.0f},8,3.7f,.51f,430,.52f,0,7.0f},
 {"ТАЙГА 26", 8300, 3.85f, 2150, {11.7f, 8.2f, 5.7f, 4.0f, 2.8f, 1.85f, 1.25f, .96f},8,3.35f,.53f,600,.50f,28000,7.4f},
 {"АТЛАНТ 34", 9500, 4.10f, 2700, {13.1f, 9.1f, 6.3f, 4.4f, 3.0f, 2.0f, 1.34f, .95f},8,3.15f,.55f,800,.47f,65000,7.9f}
};
static float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
static float ab(float f){return f<0?-f:f;}
static float wrapped(float a){while(a>RH_PI)a-=2*RH_PI;while(a< -RH_PI)a+=2*RH_PI;return a;}
static uint32_t next_rng(Game *g) { uint32_t x=g->rng; x^=x<<13; x^=x>>17; x^=x<<5; return g->rng=x?x:238843u; }
static int rndi(Game *g,int n){return (int)(next_rng(g)%(uint32_t)n);}
float rh_dist2(float x,float z,float x2,float z2){float dx=x-x2,dz=z-z2;return sqrtf(dx*dx+dz*dz);}
static float nearest_road(float v){return v-RH_ROAD_SPACING*floorf(v/RH_ROAD_SPACING+.5f);}
float rh_road_distance(float x,float z) {
 float dx=ab(nearest_road(x)),dz=ab(nearest_road(z));
 // Two access roads attached to the main north-south route at every node.
 int cx=(int)floorf(x/RH_ROAD_SPACING+.5f),cz=(int)floorf(z/RH_ROAD_SPACING+.5f);
 float xx=x-cx*RH_ROAD_SPACING, zz=z-cz*RH_ROAD_SPACING;
 float spur_a=(xx>=-7 && xx<=124)?ab(zz-135):9999;
 float spur_b=(xx>=-124 && xx<=7)?ab(zz+130):9999;
 float min=dx<dz?dx:dz;
 if(spur_a<min)min=spur_a;
 if(spur_b<min)min=spur_b;
 return min;
}
static float raw_height(float x,float z) {
 return 4.3f*sinf(x*.00155f)+5.6f*cosf(z*.00125f)+
  3.0f*sinf((x+z)*.0018f)+1.6f*sinf(x*.0081f)*cosf(z*.0072f);
}
float rh_height(float x,float z) {
 // Keep a smooth road-grade profile under roads and access spurs.
 float d=rh_road_distance(x,z);float smooth=clampf((d-7)/37.0f,0,1);
 float road=4.3f*sinf(x*.00155f)+5.6f*cosf(z*.00125f)+3.0f*sinf((x+z)*.0018f);
 return road+smooth*(raw_height(x,z)-road);
}
void rh_depot(int gx,int gz,float *x,float *z) {*x=(float)gx*RH_ROAD_SPACING+114;*z=(float)gz*RH_ROAD_SPACING+135;}
void rh_station(int gx,int gz,float *x,float *z) {*x=(float)gx*RH_ROAD_SPACING-113;*z=(float)gz*RH_ROAD_SPACING-130;}
void rh_set_player(Game *g,float x,float z,float yaw){g->player.x=x;g->player.z=z;g->player.yaw=yaw;g->player.v=0;g->player.speed_abs=0;g->player.reverse=0;g->player.gearbox_t=0;g->player.trailer_yaw=yaw;}
static void init_job(Game *g,int px,int pz) {
 Job *j=&g->job;int dx=0,dz=0,attempt;
 for(attempt=0;attempt<30;attempt++){
  dx=rndi(g,13)-6;dz=rndi(g,13)-6;
  if(abs(dx)+abs(dz)>=5 && abs(dx)+abs(dz)<=10)break;
 }
 if(abs(dx)+abs(dz)<5){dx=4;dz=3;}
 j->pickup_x=px;j->pickup_z=pz;j->dest_x=px+dx;j->dest_z=pz+dz;
 j->cargo_tons=8+rndi(g,20);j->type=rndi(g,4);j->stage=0;
 j->distance_km=(ab((float)dx)+ab((float)dz))*RH_ROAD_SPACING/1000.0f;
 j->payout=(850.0f+35.0f*j->cargo_tons)*j->distance_km;
}
static void spawn_traffic(Game *g,int i){
 Traffic *t=&g->cars[i];Player *p=&g->player;
 int axis=rndi(g,2),dir=rndi(g,2)?1:-1;
 int roadx=(int)floorf(p->x/RH_ROAD_SPACING+.5f)+(rndi(g,3)-1);
 int roadz=(int)floorf(p->z/RH_ROAD_SPACING+.5f)+(rndi(g,3)-1);
 float offset=130+rndi(g,750);if(rndi(g,2))offset=-offset;
 t->axis=axis;t->direction=dir;t->variant=rndi(g,4);
 if(axis==0){t->x=roadx*RH_ROAD_SPACING+(dir>0?2.8f:-2.8f);t->z=p->z+offset;t->yaw=dir>0?0:RH_PI;}
 else {t->z=roadz*RH_ROAD_SPACING+(dir>0?-2.8f:2.8f);t->x=p->x+offset;t->yaw=dir>0?RH_PI/2:-RH_PI/2;}
 t->speed=(12.0f+rndi(g,120)*.1f)*dir;
}
void rh_new_game(Game *g){
 memset(g,0,sizeof(*g));g->rng=0x786423ad;g->bank=6500;g->clock_hours=8.0f;g->owned_trucks=1u;
 g->player.model=0;g->player.fuel=RH_TRUCKS[0].tank_l;
 rh_set_player(g,2,98,0);g->player.rpm=650;g->player.gear=1;
 init_job(g,0,0);g->traffic_count=RH_MAX_TRAFFIC;
 for(int i=0;i<g->traffic_count;i++)spawn_traffic(g,i);
}
static float engine_torque(const TruckSpec *t,float rpm){
 // Plateau near 1100-1500 RPM, modest low-end and high-RPM fall-off.
 float f= rpm<700?.58f+rpm/700*.22f : rpm<1150?.8f+(rpm-700)/450*.2f :
     rpm<1500?1.0f : 1.0f-(rpm-1500)/900*.49f;
 return t->torque_nm*clampf(f,.35f,1.0f);
}
static void physics_step(Game *g,Controls in,float dt) {
 Player *p=&g->player;const TruckSpec *t=&RH_TRUCKS[p->model];
 float mass=t->mass_kg+4600.0f+(g->job.stage==1?(float)g->job.cargo_tons*1000.0f:0.0f);
 float old_v=p->v,speed=ab(old_v),road_dist=rh_road_distance(p->x,p->z);
 float steer=clampf(in.steer,-1,1),throttle=clampf(in.throttle,0,1);
 float brake=clampf(in.brake,0,1);
 if(in.reverse_request && speed<.35f && throttle<.15f)p->reverse=!p->reverse;
 if(throttle>.15f)p->reverse=0;
 if(p->reverse){throttle=brake;brake=clampf(in.throttle,0,1);}
 /* Diesel pedal and turbo response: a short but tangible ramp. */
 p->engine_t+=clampf(throttle-p->engine_t,-dt*3.5f,dt*2.8f);
 p->engine_t=clampf(p->engine_t,0,1);
 float wheelrpm=speed*60.0f/(6.2831853f*t->wheel_radius);
 float ratio=t->gears[p->gear-1]*t->final_drive;
 float idealrpm=wheelrpm*ratio;
 float targetrpm=clampf(fmaxf(620,idealrpm)+p->engine_t*90.0f,620,2200);
 p->rpm+=clampf((targetrpm-p->rpm)*dt*4.2f,-dt*1400,dt*1650);
 /* The automatic transmission briefly interrupts power when shifting. */
 if(p->gearbox_t>0)p->gearbox_t=fmaxf(0,p->gearbox_t-dt);
 else if(p->rpm>1830 && p->gear<t->n_gears && speed>3.0f){
  p->gear++;p->gearbox_t=.37f;p->rpm=clampf(p->rpm*t->gears[p->gear-1]/t->gears[p->gear-2],650,1800);
 }else if(p->rpm<900 && p->gear>1 && speed>1.7f){
  p->gear--;p->gearbox_t=.29f;p->rpm=clampf(p->rpm*t->gears[p->gear-1]/t->gears[p->gear],650,2050);
 }
 if(speed<.35f && p->gear!=1){p->gear=1;p->gearbox_t=0;}
 ratio=t->gears[p->gear-1]*t->final_drive;
 float torque=engine_torque(t,p->rpm)*p->engine_t;
 /* Nominal diesel power limits: NORD 280 kW, BOREAL 350, ATLAS 430.
    A heavy laden tractor cannot accelerate as if its torque were infinite. */
 const float maxkw[3]={280.0f,350.0f,430.0f};
 float wheel_force=torque*ratio*.85f/t->wheel_radius;
 float powercap=maxkw[p->model]*1000.0f/fmaxf(speed,5.5f);
 wheel_force=fminf(wheel_force,powercap);
 float wet=.5f+.5f*sinf(g->sim_t/210.0f+1.3f);
 float grip=road_dist<8.0f?.17f:road_dist<20.0f?.095f:.057f;
 grip*=1.0f-.20f*wet;
 wheel_force=fminf(wheel_force,grip*mass*9.81f);
 if(p->gearbox_t>0)wheel_force*=.16f;
 wheel_force*=1.0f-.006f*p->damage;
 if(p->fuel<.01f)wheel_force=0;
 float slope=(rh_height(p->x+sinf(p->yaw)*2.0f,p->z+cosf(p->yaw)*2.0f)-
              rh_height(p->x-sinf(p->yaw)*2.0f,p->z-cosf(p->yaw)*2.0f))/4.0f;
 float rolling=mass*9.81f*(road_dist<8.0f?.009f:.049f);
 float drag=.74f*speed*speed;
 /* 0.44-0.65 g road deceleration: air brakes + parking brake. */
 float braking=mass*9.81f*(brake*.49f+(in.handbrake?.18f:0));
 if(road_dist>9.0f)braking*=.68f;
 float brake_force=braking+rolling+drag;
 float grade_force=mass*9.81f*slope;
 /* Signed speed: negative only in selected reverse. */
 float direction=p->reverse?-1.0f:1.0f;
 float effective=wheel_force-brake_force-direction*grade_force;
 float speed_new=fmaxf(0.0f,speed+effective/mass*dt);
 if(p->fuel<.01f)speed_new=fmaxf(0,speed_new-1.3f*dt);
 speed_new=fminf(speed_new,p->reverse?4.6f:25.0f);
 float v=direction*speed_new;
 /* Reduce steering authority at speed and smooth yaw under heavy loads. */
 float steer_angle=steer*t->max_steer_rad/(1.0f+speed*.055f);
 float yaw_rate=v/t->wheelbase_m*tanf(steer_angle)*.67f;
 if(road_dist>12.0f)yaw_rate*=.74f;
 p->yaw=wrapped(p->yaw+yaw_rate*dt);
 p->x+=sinf(p->yaw)*v*dt;p->z+=cosf(p->yaw)*v*dt;
 p->v=v;p->speed_abs=ab(v);
 p->odo+=p->speed_abs*dt/1000.0f;
 float angle=wrapped(p->yaw-p->trailer_yaw);
 float trailer_response=clampf(speed/(8.0f+(g->job.stage==1?g->job.cargo_tons*.17f:0)),.07f,1.2f);
 p->trailer_yaw=wrapped(p->trailer_yaw+clampf(angle,-.65f,.65f)*trailer_response*dt);
 float fuel_l=dt*(.0007f+p->engine_t*.0014f)+p->speed_abs*dt*(.00008f+p->engine_t*.00011f)*(mass/19000.0f);
 p->fuel=fmaxf(0,p->fuel-fuel_l);
 if(road_dist>25.0f && speed>11.0f)p->damage=fminf(100,p->damage+dt*.019f*(speed-11));
 (void)old_v;
}
static void update_traffic(Game *g,float dt){
 for(int i=0;i<g->traffic_count;i++){
  Traffic *t=&g->cars[i];
  if(rh_dist2(t->x,t->z,g->player.x,g->player.z)>1150){spawn_traffic(g,i);continue;}
  /* Each NPC tracks a lane, brakes for a leading car and the player. */
  float speed=ab(t->speed),desired=15.5f+(t->variant*1.5f);
  float hx=sinf(t->yaw),hz=cosf(t->yaw);
  float dx=g->player.x-t->x,dz=g->player.z-t->z;
  float ahead=dx*hx+dz*hz,lateral=ab(dx*hz-dz*hx);
  if(ahead>0 && ahead<52 && lateral<4.2f){
   desired=fminf(desired,clampf((ahead-7)*.68f,0,desired));
  }
  for(int j=0;j<g->traffic_count;j++)if(j!=i){
   const Traffic *o=&g->cars[j];float ox=o->x-t->x,oz=o->z-t->z;
   float f=ox*hx+oz*hz,side=ab(ox*hz-oz*hx);
   if(f>0&&f<45&&side<2.8f&&cosf(o->yaw-t->yaw)>.7f)
    desired=fminf(desired,clampf((f-6)*.66f,0,desired));
  }
  speed+=clampf(desired-speed,-4.2f*dt,1.3f*dt);
  speed=fmaxf(0,speed);
  t->speed=speed*t->direction;
  if(t->axis==0)t->z+=t->speed*dt;
  else t->x+=t->speed*dt;
  float delta=rh_dist2(t->x,t->z,g->player.x,g->player.z);
  if(delta<4.5f && g->player.speed_abs>1.0f){
   /* Impact is energy-dependent rather than frame-rate dependent. */
   float closing=fabsf(g->player.v-t->speed);
   g->player.damage=fminf(100,g->player.damage+dt*(1.0f+closing*.35f));
   g->player.v*=clampf(1.0f-1.8f*dt,.7f,1.0f);
   g->player.speed_abs=ab(g->player.v);
  }
 }
}
float rh_job_target_distance(const Game *g){
 float x,z;int px=g->job.stage?g->job.dest_x:g->job.pickup_x;
 int pz=g->job.stage?g->job.dest_z:g->job.pickup_z;
 rh_depot(px,pz,&x,&z);return rh_dist2(g->player.x,g->player.z,x,z);
}
void rh_waypoint(const Game *g,float *x,float *z){
 const Player *p=&g->player;
 float spacing=RH_ROAD_SPACING;
 if(g->job.stage==0){rh_depot(g->job.pickup_x,g->job.pickup_z,x,z);return;}
 float gx=g->job.dest_x*spacing,gz=g->job.dest_z*spacing;
 int cx=(int)floorf(p->x/spacing+.5f),cz=(int)floorf(p->z/spacing+.5f);
 float nx=cx*spacing,nz=cz*spacing;
 // Escape company driveways by approaching the nearby public junction.
 if(rh_dist2(p->x,p->z,nx,nz)<235 &&
     (fabsf(p->x-nx)>38||fabsf(p->z-nz)>38) &&
     (fabsf(p->x-nx)>17 && fabsf(p->z-nz)>17)){
  *x=nx;*z=nz;return;
 }
 // Grid routing: travel east/west on a horizontal road, then north/south.
 if(fabsf(p->x-gx)>140){
  if(fabsf(p->z-nz)>18 && fabsf(p->x-nx)<17){*x=nx;*z=nz;}
  else {*x=gx;*z=nz;}
  return;
 }
 if(fabsf(p->z-gz)>140){*x=gx;*z=gz;return;}
 rh_depot(g->job.dest_x,g->job.dest_z,x,z);
}
static void interact(Game *g){
 Player *p=&g->player;int cx=(int)floorf(p->x/RH_ROAD_SPACING+.5f);
 int cz=(int)floorf(p->z/RH_ROAD_SPACING+.5f);float x,z;
 rh_station(cx,cz,&x,&z);
 if(rh_dist2(p->x,p->z,x,z)<27 && p->speed_abs<2.0f){
  float refill=RH_TRUCKS[p->model].tank_l-p->fuel;float affordable=fminf(refill,g->bank/2.15f);
  if(affordable>0){p->fuel+=affordable;g->bank-=affordable*2.15f;g->last_notice=3;g->notice_ticks=180;}
  if(p->damage>0 && g->bank>0){float repair=fminf(p->damage,g->bank/43.0f);
   g->bank-=repair*43.0f;p->damage-=repair;g->last_notice=6;g->notice_ticks=150;}
  return;
 }
 int tx=g->job.stage?g->job.dest_x:g->job.pickup_x;
 int tz=g->job.stage?g->job.dest_z:g->job.pickup_z;
 rh_depot(tx,tz,&x,&z);
 if(rh_dist2(p->x,p->z,x,z)<32 && p->speed_abs<1.4f){
  if(g->job.stage==0){g->job.stage=1;g->last_notice=1;g->notice_ticks=220;}
  else {g->bank+=g->job.payout;g->completed++;g->last_notice=2;g->notice_ticks=240;
   init_job(g,tx,tz);}
 }
}
void rh_step(Game *g,Controls in,float dt){
 if(in.pause)g->paused=!g->paused;
 if(in.toggle_camera)g->player.camera=(g->player.camera+1)%2;
 if(in.map)g->map_open=!g->map_open;
 if(g->paused||g->map_open)return;
 if(dt<0)dt=0;
 if(dt>.07f)dt=.07f;
 if(in.swap_truck){
  int cx=(int)floorf(g->player.x/RH_ROAD_SPACING+.5f);
  int cz=(int)floorf(g->player.z/RH_ROAD_SPACING+.5f);float x,z;
  rh_depot(cx,cz,&x,&z);
  if(rh_dist2(g->player.x,g->player.z,x,z)<32 && g->player.speed_abs<1.0f){
   int model=(g->player.model+1)%3;
   float cost=(g->owned_trucks&(1u<<model))?0.0f:RH_TRUCKS[model].price;
   if(g->bank>=cost){g->bank-=cost;g->owned_trucks|=(1u<<model);
    float f=g->player.fuel/RH_TRUCKS[g->player.model].tank_l;
    g->player.model=model;g->player.fuel=f*RH_TRUCKS[model].tank_l;
    g->last_notice=4;g->notice_ticks=200;
   } else {g->last_notice=5;g->notice_ticks=200;}
  }
 }
 if(in.action)interact(g);
 if(g->player.fuel<.01f)in.throttle=0;
 // 2 substeps preserve consistent handling near 30 FPS.
 physics_step(g,in,dt*.5f);
 in.reverse_request=0;
 physics_step(g,in,dt*.5f);
 update_traffic(g,dt);g->sim_t+=dt;g->clock_hours+=dt/120.0f;
 if(g->clock_hours>=24)g->clock_hours-=24;
 if(g->notice_ticks>0)g->notice_ticks--;
}

typedef struct {uint32_t magic,version,checksum;Game g;} DiskSave;
static uint32_t checksum(const unsigned char *b,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;i++){h^=b[i];h*=16777619u;}return h;}
/* Two-generation write strategy on FAT: a failed or interrupted write
   must not overwrite the most recent complete SAVE.DAT. No flash access. */
int rh_save(const Game *g,const char *path){
 if(!g||!path||strlen(path)>230)return 0;
 DiskSave s;memset(&s,0,sizeof s);s.magic=0x52485356u;s.version=RH_SAVE_VERSION;
 s.g=*g;s.checksum=checksum((const unsigned char*)&s.g,sizeof s.g);
 char temp[256],backup[256];
 snprintf(temp,sizeof temp,"%s.NEW",path);
 snprintf(backup,sizeof backup,"%s.BAK",path);
 FILE *f=fopen(temp,"wb");if(!f)return 0;
 int okay=(fwrite(&s,1,sizeof s,f)==sizeof s);
 if(fflush(f)!=0)okay=0;
 if(fclose(f)!=0)okay=0;
 if(!okay){remove(temp);return 0;}
 /* The backup itself is never touched if a save could not be written. */
 remove(backup);
 int old_moved=(rename(path,backup)==0);
 if(rename(temp,path)!=0){
  if(old_moved)rename(backup,path);
  remove(temp);return 0;
 }
 return 1;
}
int rh_load(Game *g,const char *path){
 if(!g||!path)return 0;
 DiskSave s;FILE *f=fopen(path,"rb");if(!f)return 0;
 size_t n=fread(&s,1,sizeof s,f);fclose(f);
 if(n!=sizeof s||s.magic!=0x52485356u||s.version!=RH_SAVE_VERSION||
    s.checksum!=checksum((const unsigned char*)&s.g,sizeof s.g))return 0;
 if(s.g.player.model<0||s.g.player.model>2||s.g.player.gear<1||s.g.player.gear>8||
    s.g.owned_trucks>7u||s.g.traffic_count<0||s.g.traffic_count>RH_MAX_TRAFFIC||
    s.g.job.stage<0||s.g.job.stage>1||s.g.job.cargo_tons<0||s.g.job.cargo_tons>60||
    s.g.player.camera<0||s.g.player.camera>1||s.g.player.reverse<0||s.g.player.reverse>1)return 0;
 if(!isfinite(s.g.player.x)||!isfinite(s.g.player.z)||!isfinite(s.g.player.v)||
    !isfinite(s.g.player.fuel)||!isfinite(s.g.player.damage)||!isfinite(s.g.bank)||
    !isfinite(s.g.sim_t)||!isfinite(s.g.player.yaw)||
    fabsf(s.g.player.x)>10000000||fabsf(s.g.player.z)>10000000||
    s.g.bank<0||s.g.bank>1e9f||s.g.player.fuel<0||
    s.g.player.fuel>RH_TRUCKS[s.g.player.model].tank_l+1||
    s.g.player.damage<0||s.g.player.damage>100)return 0;
 *g=s.g;return 1;
}
