#include "../src/sim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void valid(const Game *g){
 const Player *p=&g->player;
 assert(isfinite(p->x)&&isfinite(p->z)&&isfinite(p->v)&&isfinite(p->rpm));
 assert(isfinite(p->yaw)&&isfinite(p->fuel)&&isfinite(p->trailer_yaw));
 assert(isfinite(g->bank)&&isfinite(g->sim_t)&&isfinite(g->clock_hours));
 assert(p->model>=0&&p->model<3&&p->gear>=1&&p->gear<=8);
 assert(p->fuel>=0&&p->fuel<=RH_TRUCKS[p->model].tank_l+0.01f);
 assert(p->speed_abs<=25.001f&&p->damage<=100.01f&&p->damage>=0);
 assert(g->bank>=0);
 for(int j=0;j<g->traffic_count;j++){
  assert(isfinite(g->cars[j].x)&&isfinite(g->cars[j].z));
  assert(isfinite(g->cars[j].yaw)&&isfinite(g->cars[j].speed));
 }
}
int main(void){
 Game a,b;rh_new_game(&a);rh_new_game(&b);
 for(int i=0;i<45000;i++){
  Controls c={0};
  c.throttle=(i%1700<1160);
  c.brake=(i%1700>=1160&&i%1700<1540);
  c.steer=sinf(i*.009f)*.33f;
  c.action=(i%1917==0);
  c.swap_truck=(i%4877==0);
  c.handbrake=(i%1800>1720);
  if(i%13000==0){c.reverse_request=1;c.brake=1;c.throttle=0;}
  rh_step(&a,c,1.0f/60.0f);
  rh_step(&b,c,1.0f/60.0f);
  if((i&63)==0)valid(&a);
 }
 valid(&a);
 assert(memcmp(&a,&b,sizeof a)==0);
 assert(rh_save(&a,"stress.dat"));
 assert(rh_save(&a,"stress.dat"));
 Game other;assert(rh_load(&other,"stress.dat"));
 assert(memcmp(&a,&other,sizeof a)==0);
 assert(rh_load(&other,"stress.dat.BAK"));
 FILE *f=fopen("stress.dat","r+b");assert(f);fseek(f,3,SEEK_SET);fputc(222,f);fclose(f);
 assert(!rh_load(&other,"stress.dat"));
 assert(rh_load(&other,"stress.dat.BAK"));
 remove("stress.dat");remove("stress.dat.BAK");
 printf("ODO %.3f, fuel %.3f, speed %.3f, gear %d\n",a.player.odo,a.player.fuel,a.player.v,a.player.gear); assert(a.player.odo>0.2f);
 printf("PASS stress: 45k frames, deterministic simulation, fuel, finite positions, rollback save; %.1f km driven\n",a.player.odo);
}
