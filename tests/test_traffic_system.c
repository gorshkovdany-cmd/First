#include "../src/traffic_system.h"
#include "../src/drive_physics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define STEP (1.0f/60.0f)
static void advance(RH_TrafficWorld *w,RH_TPlayer *p,int frames,float wet){
 for(int i=0;i<frames;i++)rh_traffic_step(w,p,STEP,wet);
}
static void flee(RH_TrafficWorld *w,RH_TPlayer *p,int frames){
 for(int i=0;i<frames;i++){p->z+=.50f;rh_traffic_step(w,p,STEP,0);}
}
static void traffic_basics(void){
 RH_TrafficWorld w;rh_traffic_init(&w,123);
 rh_traffic_build_sample_city(&w,2700,1450,100);
 assert(w.node_count==16&&w.edge_count==48&&w.signal_count==4);
 assert(rh_traffic_active_cars(&w)==16);
 assert(rh_traffic_active_pedestrians(&w)==8);
 assert(rh_traffic_active_patrols(&w)==0);
 assert(rh_traffic_add_node(&w,NAN,0,1)==-1);
 assert(rh_traffic_add_edge(&w,-1,1,15,1)==-1);
 assert(rh_traffic_add_edge(&w,0,1,-3,1)==-1);
 assert(rh_traffic_spawn_car(&w,0,NAN,1)==-1);
 int phase=w.signals[0].phase;
 RH_TPlayer player={5000,5000,0,0,4000};
 advance(&w,&player,2000,.25f);
 assert(w.signals[0].phase!=phase);
 for(int i=0;i<w.car_count;i++)if(w.cars[i].active){
  assert(isfinite(w.cars[i].x)&&isfinite(w.cars[i].z));
  assert(w.cars[i].speed>=0&&w.cars[i].speed<=32);
 }
 assert(rh_traffic_active_cars(&w)<=RH_TCARS);
 assert(rh_traffic_active_pedestrians(&w)<=RH_TPEDESTRIANS);
 printf("city graph: %u nodes, %u edges, %u signals, %d cars, %d pedestrians\n",
 w.node_count,w.edge_count,w.signal_count,rh_traffic_active_cars(&w),rh_traffic_active_pedestrians(&w));
}
static void red_light_stop(void){
 RH_TrafficWorld w;rh_traffic_init(&w,1);
 int a=rh_traffic_add_node(&w,100,100,1);
 int b=rh_traffic_add_node(&w,100,200,1);
 int e=rh_traffic_add_edge(&w,a,b,15,1);
 int sig=rh_traffic_add_signal(&w,b);
 assert(e==0&&sig==0);
 w.signals[sig].phase=RH_SIGNAL_EW_GREEN;
 int c=rh_traffic_spawn_car(&w,e,.4f,0);assert(c>=0);
 RH_TPlayer player={900,900,0,0,500};
 advance(&w,&player,600,0);
 printf("red stop progress=%.3f speed=%.3f phase=%u\n",w.cars[c].progress,w.cars[c].speed,w.signals[sig].phase);
 assert(w.cars[c].progress<=.946f);
 assert(w.cars[c].speed<.15f);
 w.signals[sig].phase=RH_SIGNAL_NS_GREEN;w.signals[sig].timer=0;
 advance(&w,&player,60,0);
 assert(w.cars[c].progress>.946f);
 printf("red signal stop and green release: PASS\n");
}
static void pedestrian_crossing(void){
 RH_TrafficWorld w;rh_traffic_init(&w,2);
 int a=rh_traffic_add_node(&w,100,100,1),sig=rh_traffic_add_signal(&w,a);
 int p=rh_traffic_spawn_pedestrian(&w,sig,90,100,110,100);
 assert(p==0);
 RH_TPlayer player={500,500,0,0,500};
 /* East-west pedestrian may cross only when north-south cars are stopped. */
 w.signals[sig].phase=RH_SIGNAL_NS_GREEN;
 advance(&w,&player,60,0);
 assert(w.pedestrians[p].x==90);
 w.signals[sig].phase=RH_SIGNAL_EW_GREEN;w.signals[sig].timer=0;
 advance(&w,&player,60,0);
 assert(w.pedestrians[p].x>90);
 printf("signal-controlled pedestrian crossing: PASS\n");
}
static void police_chase(void){
 RH_TrafficWorld w;rh_traffic_init(&w,77);
 RH_TPlayer player={2000,1800,0,0,1400};
 rh_traffic_report_violation(&w,RH_VIOLATION_RED_LIGHT,&player);
 assert(w.wanted&&rh_traffic_active_patrols(&w)==1);
 flee(&w,&player,60*13);
 assert(w.wanted&&rh_traffic_active_patrols(&w)==2);
 flee(&w,&player,60*13);
 assert(w.wanted&&rh_traffic_active_patrols(&w)==3);
 flee(&w,&player,60*35);
 assert(!w.wanted&&rh_traffic_active_patrols(&w)==0&&w.fines_count==0);
 /* A fresh pursuit after cooldown must deduct the correct bounded fine. */
 advance(&w,&player,60*13,0);
 player.bank=200;
 rh_traffic_report_violation(&w,RH_VIOLATION_COLLISION,&player);
 assert(w.wanted);
 w.patrols[0].x=player.x;w.patrols[0].z=player.z;
 advance(&w,&player,60,0);
 assert(!w.wanted&&w.fines_count==1&&fabsf(w.fines_total-200)<.01f);
 assert(player.bank==0);
 assert(rh_traffic_active_patrols(&w)==0);
 printf("patrol pursuit 60s timeout, reinforcements, catch and capped fine: PASS\n");
}
static void police_exact_timeout(void){
 RH_TrafficWorld w;rh_traffic_init(&w,13);
 RH_TPlayer player={2000,2000,30,0,900};
 rh_traffic_report_violation(&w,RH_VIOLATION_SPEEDING,&player);
 flee(&w,&player,3594); /* 59.9 seconds */
 assert(w.wanted&&rh_traffic_active_patrols(&w)==3);
 flee(&w,&player,6); /* 60.0 seconds */
 assert(!w.wanted&&rh_traffic_active_patrols(&w)==0);
 assert(w.fines_count==0);
 printf("police pursuit timeout at 60.0 s, not 59.9 s: PASS\n");
}
static void automatic_violations(void){
 RH_TrafficWorld w;rh_traffic_init(&w,88);
 int a=rh_traffic_add_node(&w,100,100,1);
 int b=rh_traffic_add_node(&w,100,200,1);
 assert(rh_traffic_add_edge(&w,a,b,10,1)>=0);
 int sig=rh_traffic_add_signal(&w,b);
 RH_TPlayer player={100,110,20,0,1000};
 for(int i=0;i<100;i++){
  player.z+=player.speed*STEP;
  rh_traffic_step(&w,&player,STEP,0);
 }
 assert(w.wanted&&w.last_violation==RH_VIOLATION_SPEEDING);
 RH_TrafficWorld lights;rh_traffic_init(&lights,99);
 a=rh_traffic_add_node(&lights,100,100,1);
 b=rh_traffic_add_node(&lights,100,200,1);
 assert(rh_traffic_add_edge(&lights,a,b,15,1)>=0);
 sig=rh_traffic_add_signal(&lights,b);
 lights.signals[sig].phase=RH_SIGNAL_EW_GREEN;
 player.x=100;player.z=193;player.speed=6;player.heading=0;
 rh_traffic_step(&lights,&player,STEP,0);
 player.z=195;
 rh_traffic_step(&lights,&player,STEP,0);
 assert(lights.wanted&&lights.last_violation==RH_VIOLATION_RED_LIGHT);
 printf("automatic speeding and red-light enforcement: PASS\n");
}
static void deterministic_stress(void){
 RH_TrafficWorld a,b;rh_traffic_init(&a,27182);rh_traffic_init(&b,27182);
 rh_traffic_build_sample_city(&a,2700,1450,75);
 rh_traffic_build_sample_city(&b,2700,1450,75);
 RH_TPlayer p={5000,5000,0,0,1000};
 for(int i=0;i<108000;i++){
  rh_traffic_step(&a,&p,STEP,(float)(i%1200)/1200);
  rh_traffic_step(&b,&p,STEP,(float)(i%1200)/1200);
 }
 assert(memcmp(&a,&b,sizeof(a))==0);
 for(int i=0;i<a.car_count;i++){
  assert(isfinite(a.cars[i].x)&&isfinite(a.cars[i].z));
  assert(a.cars[i].progress>=0&&a.cars[i].progress<=1.0f);
 }
 assert(sizeof(a)<16384);
 printf("30-minute deterministic stress, fixed world memory %lu bytes: PASS\n",(unsigned long)sizeof(a));
}
static void vehicle_physics(void){
 RH_DriveBody dry,wet,heavy;
 rh_drive_init(&dry,RH_DRIVE_TRUCK,200,200,0);
 rh_drive_init(&wet,RH_DRIVE_TRUCK,200,200,0);
 rh_drive_init(&heavy,RH_DRIVE_TRUCK,200,200,0);
 heavy.trailer_mass_kg=24000;
 RH_DriveInput gas={0,1,0,0,0};
 float dry_early=0,wet_early=0,heavy_early=0;
 for(int i=0;i<900;i++){
  rh_drive_step(&dry,gas,STEP,0,1,0);
  rh_drive_step(&wet,gas,STEP,1,1,0);
  rh_drive_step(&heavy,gas,STEP,0,1,0);
  if(i==180){dry_early=dry.speed_mps;wet_early=wet.speed_mps;heavy_early=heavy.speed_mps;}
 }
 assert(dry.speed_mps>4&&dry.speed_mps<=26);
 assert(wet_early<dry_early);
 assert(heavy_early<dry_early);
 float before=dry.speed_mps;
 RH_DriveInput brake={0,0,1,0,0};
 for(int i=0;i<400;i++)rh_drive_step(&dry,brake,STEP,0,1,0);
 assert(dry.speed_mps<.1f);
 assert(dry.fuel_l>0&&isfinite(dry.x)&&isfinite(dry.yaw));
 printf("vehicle dynamics: dry %.1f m/s, wet %.1f m/s, heavy %.1f m/s, braking %.1f -> %.1f\n",
 before,wet.speed_mps,heavy.speed_mps,before,dry.speed_mps);
}
static void collisions_and_entry(void){
 RH_DriveBody fleet[3];
 rh_drive_init(&fleet[0],RH_DRIVE_CAR,100,100,0);
 rh_drive_init(&fleet[1],RH_DRIVE_TRUCK,102,100,3.14159265f);
 rh_drive_init(&fleet[2],RH_DRIVE_CAR,104,100,0);
 fleet[0].speed_mps=15;fleet[1].speed_mps=7;
 assert(rh_drive_resolve_collision(&fleet[0],&fleet[1]));
 assert(fleet[0].damage>0||fleet[1].damage>0);
 assert(isfinite(fleet[0].speed_mps)&&isfinite(fleet[1].speed_mps));
 fleet[0].speed_mps=0;fleet[1].speed_mps=0;
 fleet[1].x=500;fleet[1].z=550;
 RH_Driver d={100,100,-1,1};
 assert(rh_drive_try_enter(&d,fleet,3,0));
 assert(!d.on_foot&&fleet[0].occupied);
 assert(!rh_drive_try_enter(&d,fleet,3,1));
 /* Exit blocked when another vehicle occupies the door space. */
 fleet[2].x=fleet[0].x+3;fleet[2].z=fleet[0].z;
 assert(!rh_drive_try_exit(&d,fleet,3));
 fleet[2].x=500;fleet[2].z=500;
 assert(rh_drive_try_exit(&d,fleet,3));
 assert(d.on_foot&&!fleet[0].occupied);
 printf("bounded collision impulses, vehicle entry/exit safety: PASS\n");
}
int main(void){
 traffic_basics();red_light_stop();pedestrian_crossing();police_chase();police_exact_timeout();
 automatic_violations();deterministic_stress();
 vehicle_physics();collisions_and_entry();
 puts("PASS: PSP traffic system deterministic smoke and safety tests");return 0;
}
