#include "traffic_system.h"
#include <math.h>
#include <string.h>

#define RH_TPI 3.14159265358979323846f
static float tmin(float a,float b){return a<b?a:b;}
static float tmax(float a,float b){return a>b?a:b;}
static float tclamp(float v,float lo,float hi){return tmin(hi,tmax(lo,v));}
static float tdist(float x,float z,float a,float b){float dx=x-a,dz=z-b;return sqrtf(dx*dx+dz*dz);}
static float tangle(float a){while(a>RH_TPI)a-=2*RH_TPI;while(a< -RH_TPI)a+=2*RH_TPI;return a;}
static uint32_t trand(RH_TrafficWorld *w){uint32_t v=w->rng;v^=v<<13;v^=v>>17;v^=v<<5;return w->rng=v?v:0x735a21u;}
static float phase_duration(unsigned p){static const float duration[6]={14,3,1,14,3,1};return duration[p%6];}

void rh_traffic_init(RH_TrafficWorld *w,uint32_t seed){
 if(!w)return;
 memset(w,0,sizeof(*w));w->rng=seed?seed:0x735a21u;
 for(int i=0;i<RH_TNODES;i++)w->nodes[i].signal=RH_TRAFFIC_INVALID;
}
int rh_traffic_add_node(RH_TrafficWorld *w,float x,float z,uint8_t road_class){
 if(!w||w->node_count>=RH_TNODES||!isfinite(x)||!isfinite(z))return -1;
 int n=w->node_count++;w->nodes[n].x=x;w->nodes[n].z=z;
 w->nodes[n].signal=RH_TRAFFIC_INVALID;w->nodes[n].road_class=road_class;
 return n;
}
int rh_traffic_add_edge(RH_TrafficWorld *w,int from,int to,float speed_limit,uint8_t road_class){
 if(!w||w->edge_count>=RH_TEDGES||from<0||to<0||from>=w->node_count||to>=w->node_count||from==to||
 !isfinite(speed_limit)||speed_limit<1||speed_limit>45)return -1;
 const RH_TNode *a=&w->nodes[from],*b=&w->nodes[to];
 if(tdist(a->x,a->z,b->x,b->z)<2)return -1;
 int e=w->edge_count++;w->edges[e].from=(uint8_t)from;w->edges[e].to=(uint8_t)to;
 w->edges[e].speed_limit=speed_limit;w->edges[e].road_class=road_class;
 return e;
}
int rh_traffic_add_signal(RH_TrafficWorld *w,int node){
 if(!w||node<0||node>=w->node_count||w->signal_count>=RH_TSIGNALS||
 w->nodes[node].signal!=RH_TRAFFIC_INVALID)return -1;
 int id=w->signal_count++;w->nodes[node].signal=(uint8_t)id;
 w->signals[id].node=(uint8_t)node;w->signals[id].phase=(uint8_t)(id%2?3:0);
 w->signals[id].timer=(float)(id%3)*1.7f;
 return id;
}
static void place_car(const RH_TrafficWorld *w,RH_TCar *c){
 const RH_TEdge *e=&w->edges[c->edge];
 const RH_TNode *a=&w->nodes[e->from],*b=&w->nodes[e->to];
 float dx=b->x-a->x,dz=b->z-a->z;
 c->x=a->x+dx*c->progress;c->z=a->z+dz*c->progress;
 c->yaw=atan2f(dx,dz);
 /* Lane offset is perpendicular to travel; opposite directions get opposite lanes. */
 c->x+=cosf(c->yaw)*2.8f;c->z-=sinf(c->yaw)*2.8f;
}
int rh_traffic_spawn_car(RH_TrafficWorld *w,int edge,float progress,int kind){
 if(!w||edge<0||edge>=w->edge_count||!isfinite(progress)||progress<0||progress>1||kind<0||kind>3)return -1;
 for(int i=0;i<RH_TCARS;i++)if(!w->cars[i].active){
  RH_TCar *c=&w->cars[i];memset(c,0,sizeof(*c));
  c->active=1;c->edge=(uint8_t)edge;c->kind=(uint8_t)kind;c->progress=progress;
  place_car(w,c);if(i>=w->car_count)w->car_count=(uint8_t)(i+1);return i;
 }
 return -1;
}
int rh_traffic_spawn_pedestrian(RH_TrafficWorld *w,int signal,float from_x,float from_z,float to_x,float to_z){
 if(!w||signal<0||signal>=w->signal_count||!isfinite(from_x)||!isfinite(from_z)||
 !isfinite(to_x)||!isfinite(to_z)||tdist(from_x,from_z,to_x,to_z)<2)return -1;
 for(int i=0;i<RH_TPEDESTRIANS;i++)if(!w->pedestrians[i].active){
  RH_TPedestrian *p=&w->pedestrians[i];memset(p,0,sizeof(*p));
  p->x=p->from_x=from_x;p->z=p->from_z=from_z;
  p->to_x=to_x;p->to_z=to_z;p->signal=(uint8_t)signal;
  p->speed=1.0f+(float)(trand(w)%50u)*.01f;p->active=1;
  if(i>=w->ped_count)w->ped_count=(uint8_t)(i+1);
  return i;
 }
 return -1;
}
/* Demonstration grid. The world team can supply a larger streamed graph. */
void rh_traffic_build_sample_city(RH_TrafficWorld *w,float x0,float z0,float spacing){
 if(!w||!isfinite(x0)||!isfinite(z0)||!isfinite(spacing)||spacing<35||spacing>300)return;
 rh_traffic_init(w,w->rng);
 for(int z=0;z<4;z++)for(int x=0;x<4;x++)rh_traffic_add_node(w,x0+x*spacing,z0+z*spacing,1);
 for(int z=0;z<4;z++)for(int x=0;x<4;x++){
  int n=z*4+x;
  if(x<3){rh_traffic_add_edge(w,n,n+1,13.9f,1);rh_traffic_add_edge(w,n+1,n,13.9f,1);}
  if(z<3){rh_traffic_add_edge(w,n,n+4,13.9f,1);rh_traffic_add_edge(w,n+4,n,13.9f,1);}
  if(x>0&&x<3&&z>0&&z<3){
   int sig=rh_traffic_add_signal(w,n);
   if(sig>=0){
    const RH_TNode *node=&w->nodes[n];
    rh_traffic_spawn_pedestrian(w,sig,node->x-8,node->z-4,node->x+8,node->z-4);
    rh_traffic_spawn_pedestrian(w,sig,node->x+4,node->z-8,node->x+4,node->z+8);
   }
  }
 }
 for(int i=0;i<16;i++){
  int e=(int)(trand(w)%(uint32_t)w->edge_count);
  rh_traffic_spawn_car(w,e,.08f+(float)(trand(w)%85u)*.01f,i%4);
 }
}
int rh_traffic_signal_green(const RH_TrafficWorld *w,int signal,int edge){
 if(!w||signal<0||signal>=w->signal_count||edge<0||edge>=w->edge_count)return 0;
 const RH_TEdge *e=&w->edges[edge];
 const RH_TNode *a=&w->nodes[e->from],*b=&w->nodes[e->to];
 int east_west=fabsf(b->x-a->x)>fabsf(b->z-a->z);
 unsigned phase=w->signals[signal].phase;
 return east_west?(phase==RH_SIGNAL_EW_GREEN):(phase==RH_SIGNAL_NS_GREEN);
}
static int next_edge(RH_TrafficWorld *w,int old_edge){
 int node=w->edges[old_edge].to,back=w->edges[old_edge].from;
 int chosen=-1,count=0;
 for(int i=0;i<w->edge_count;i++)if(w->edges[i].from==node&&w->edges[i].to!=back){
  count++;if(trand(w)%(uint32_t)count==0)chosen=i;
 }
 if(chosen>=0)return chosen;
 for(int i=0;i<w->edge_count;i++)if(w->edges[i].from==node)return i;
 return -1;
}
static void update_signals(RH_TrafficWorld *w,float dt){
 for(int i=0;i<w->signal_count;i++){
  RH_TSignal *s=&w->signals[i];s->timer+=dt;
  while(s->timer>=phase_duration(s->phase)){
   s->timer-=phase_duration(s->phase);s->phase=(uint8_t)((s->phase+1)%6);
  }
 }
}
static void update_pedestrians(RH_TrafficWorld *w,float dt){
 for(int i=0;i<w->ped_count;i++){
  RH_TPedestrian *p=&w->pedestrians[i];if(!p->active)continue;
  const RH_TSignal *s=&w->signals[p->signal];
  float dx=p->to_x-p->from_x,dz=p->to_z-p->from_z;
  /* Pedestrians crossing E/W get right of way when E/W vehicles are red. */
  int east_west=fabsf(dx)>fabsf(dz);
  int walk=east_west?(s->phase==RH_SIGNAL_EW_GREEN):(s->phase==RH_SIGNAL_NS_GREEN);
  if(!p->crossing&&!walk)continue;
  p->crossing=1;
  float dist=tdist(p->x,p->z,p->to_x,p->to_z);
  if(dist<.05f){
   float a=p->from_x,b=p->from_z;
   p->from_x=p->to_x;p->from_z=p->to_z;p->to_x=a;p->to_z=b;p->crossing=0;
  }else{
   float move=tmin(dist,p->speed*dt);
   p->x+=(p->to_x-p->x)*(move/dist);p->z+=(p->to_z-p->z)*(move/dist);
  }
 }
}
static void update_cars(RH_TrafficWorld *w,const RH_TPlayer *player,float dt,float wetness){
 for(int i=0;i<w->car_count;i++){
  RH_TCar *c=&w->cars[i];if(!c->active||c->occupied)continue;
  const RH_TEdge *e=&w->edges[c->edge];
  const RH_TNode *a=&w->nodes[e->from],*b=&w->nodes[e->to];
  float length=tdist(a->x,a->z,b->x,b->z);
  if(length<2){c->active=0;continue;}
  float desired=e->speed_limit*(1.0f-.25f*wetness);
  float remaining=(1.0f-c->progress)*length;
  int signal=b->signal;
  if(signal!=RH_TRAFFIC_INVALID&&!rh_traffic_signal_green(w,signal,c->edge)){
   float safe=(remaining-5.5f)*.8f;
   desired=tmin(desired,tmax(0,safe));
  }
  /* Longitudinal following, crosswalk yielding, and a bounded O(N^2) scan. */
  float hx=sinf(c->yaw),hz=cosf(c->yaw);
  float px=player->x-c->x,pz=player->z-c->z;
  float ahead=px*hx+pz*hz,side=fabsf(px*hz-pz*hx);
  if(ahead>0&&ahead<42&&side<3.6f)desired=tmin(desired,tmax(0,(ahead-6)*.65f));
  for(int j=0;j<w->car_count;j++)if(j!=i&&w->cars[j].active){
   const RH_TCar *o=&w->cars[j];float dx=o->x-c->x,dz=o->z-c->z;
   float f=dx*hx+dz*hz,lat=fabsf(dx*hz-dz*hx);
   if(f>0&&f<40&&lat<3.4f&&cosf(o->yaw-c->yaw)>.7f)
    desired=tmin(desired,tmax(0,(f-6)*.7f));
  }
  for(int j=0;j<w->ped_count;j++)if(w->pedestrians[j].active&&w->pedestrians[j].crossing){
   const RH_TPedestrian *p=&w->pedestrians[j];
   float dx=p->x-c->x,dz=p->z-c->z,f=dx*hx+dz*hz;
   if(f>0&&f<18&&fabsf(dx*hz-dz*hx)<3.6f)
    desired=tmin(desired,tmax(0,(f-4)*.55f));
  }
  float decel=(4.5f-1.4f*wetness)*dt;
  c->speed+=tclamp(desired-c->speed,-decel,1.7f*dt);
  c->speed=tclamp(c->speed,0,32);
  float move=c->speed*dt;
  /* Stop before a red signal; no frame-dependent creeping through it. */
  if(signal!=RH_TRAFFIC_INVALID&&!rh_traffic_signal_green(w,signal,c->edge))
   move=tmin(move,tmax(0,remaining-5.5f));
  c->progress+=move/length;
  if(c->progress>=1.0f){
   int next=next_edge(w,c->edge);
   if(next<0){c->active=0;continue;}
   c->edge=(uint8_t)next;c->progress=tmin(.01f,c->progress-1.0f);
  }
  place_car(w,c);
 }
}
static void start_patrol(RH_TrafficWorld *w,const RH_TPlayer *player){
 w->wanted=1;w->chase_time=0;w->chase_contact=0;w->patrol_count=1;
 for(int i=0;i<RH_TPATROLS;i++)w->patrols[i].active=0;
 RH_TPatrol *p=&w->patrols[0];p->active=1;
 p->x=player->x-42*cosf(player->heading);p->z=player->z-42*sinf(player->heading);
 p->yaw=player->heading;p->speed=0;
}
void rh_traffic_report_violation(RH_TrafficWorld *w,RH_Violation violation,const RH_TPlayer *player){
 if(!w||!player||!isfinite(player->x)||!isfinite(player->z)||!isfinite(player->heading)||
 violation<RH_VIOLATION_SPEEDING||violation>RH_VIOLATION_COLLISION)return;
 if(w->wanted||w->patrol_cooldown>0)return;
 w->last_violation=(uint8_t)violation;
 start_patrol(w,player);
}
static void update_patrols(RH_TrafficWorld *w,RH_TPlayer *player,float dt,float wetness){
 if(w->patrol_cooldown>0)w->patrol_cooldown=tmax(0,w->patrol_cooldown-dt);
 if(!w->wanted)return;
 w->chase_time+=(double)dt;
 if(w->chase_time>=60.0f){
  w->wanted=0;w->patrol_count=0;w->chase_contact=0;
  for(int i=0;i<RH_TPATROLS;i++)w->patrols[i].active=0;
  w->patrol_cooldown=12.0f;return;
 }
 /* Escalation is capped: 1 initial patrol + two reinforcements. */
 if(w->patrol_count<2&&w->chase_time>=12.0f)w->patrol_count=2;
 if(w->patrol_count<3&&w->chase_time>=25.0f)w->patrol_count=3;
 float nearest=1.0e6f;
 for(int i=0;i<w->patrol_count;i++){
  RH_TPatrol *p=&w->patrols[i];
  if(!p->active){
   p->active=1;p->x=player->x+(i%2?70.0f:-70.0f);
   p->z=player->z+(i%2?-40.0f:40.0f);p->speed=0;p->yaw=player->heading;
  }
  float dx=player->x-p->x,dz=player->z-p->z,dist=tdist(p->x,p->z,player->x,player->z);
  nearest=tmin(nearest,dist);
  float target=atan2f(dx,dz);
  float turn=tangle(target-p->yaw);
  p->yaw=tangle(p->yaw+tclamp(turn,-1.1f*dt,1.1f*dt));
  float max_speed=23.0f*(1.0f-.18f*wetness);
  p->speed=tclamp(p->speed+2.8f*dt,0,max_speed);
  /* Patrols decelerate close to target; cannot teleport or hit infinitely hard. */
  float target_speed=tmin(max_speed,tmax(1.0f,dist*.65f));
  p->speed=tmin(p->speed,target_speed);
  p->x+=sinf(p->yaw)*p->speed*dt;p->z+=cosf(p->yaw)*p->speed*dt;
 }
 /* A catch requires contact maintained, not one bad frame near the player. */
 if(nearest<4.0f&&fabsf(player->speed)<3.5f)w->chase_contact+=dt;
 else w->chase_contact=tmax(0,w->chase_contact-dt*2);
 if(w->chase_contact>=.75f){
  float fine=w->last_violation==RH_VIOLATION_COLLISION?900.0f:
             w->last_violation==RH_VIOLATION_RED_LIGHT?600.0f:350.0f;
  fine=tmin(fine,tmax(0,player->bank));
  player->bank-=fine;w->fines_total+=fine;w->fines_count++;
  w->wanted=0;w->patrol_count=0;w->chase_contact=0;w->patrol_cooldown=12.0f;
  for(int i=0;i<RH_TPATROLS;i++)w->patrols[i].active=0;
 }
}
/* Detect actual lane-relative speeding and red-light crossings. Avoid global
 * false positives: player must be on a graph edge and driving with its flow. */
static void observe_player(RH_TrafficWorld *w,const RH_TPlayer *player,float dt){
 float nearest=1.0e8f,along=0,previous_along=0;
 int nearest_edge=-1;
 for(int i=0;i<w->edge_count;i++){
  const RH_TEdge *e=&w->edges[i];
  const RH_TNode *a=&w->nodes[e->from],*b=&w->nodes[e->to];
  float dx=b->x-a->x,dz=b->z-a->z,len2=dx*dx+dz*dz;
  if(len2<4)continue;
  float t=((player->x-a->x)*dx+(player->z-a->z)*dz)/len2;
  float tprev=((w->player_prev_x-a->x)*dx+(w->player_prev_z-a->z)*dz)/len2;
  if(t<0||t>1.02f)continue;
  float lateral=tdist(player->x,player->z,a->x+dx*t,a->z+dz*t);
  float forward=(sinf(player->heading)*dx+cosf(player->heading)*dz)/sqrtf(len2);
  if(player->speed<0)forward=-forward;
  if(lateral<nearest&&forward>.65f){nearest=lateral;nearest_edge=i;along=t;previous_along=tprev;}
 }
 if(nearest_edge>=0&&nearest<6.5f){
  const RH_TEdge *e=&w->edges[nearest_edge];
  if(fabsf(player->speed)>e->speed_limit+4.0f)
   w->speed_violation_time=tmin(2.0f,w->speed_violation_time+dt);
  else w->speed_violation_time=tmax(0,w->speed_violation_time-dt*2);
  if(w->speed_violation_time>=1.5f){
   rh_traffic_report_violation(w,RH_VIOLATION_SPEEDING,player);
   w->speed_violation_time=0;
  }
  int signal=w->nodes[e->to].signal;
  if(w->player_observed&&signal!=RH_TRAFFIC_INVALID&&previous_along<.945f&&along>=.945f&&
   fabsf(player->speed)>.5f&&!rh_traffic_signal_green(w,signal,nearest_edge))
   rh_traffic_report_violation(w,RH_VIOLATION_RED_LIGHT,player);
 }else w->speed_violation_time=0;
 w->player_prev_x=player->x;w->player_prev_z=player->z;w->player_observed=1;
}
void rh_traffic_step(RH_TrafficWorld *w,RH_TPlayer *player,float dt,float wetness){
 if(!w||!player||!isfinite(dt)||!isfinite(wetness)||!isfinite(player->x)||
 !isfinite(player->z)||!isfinite(player->speed)||!isfinite(player->bank)||
 !isfinite(player->heading))return;
 if(dt<=0)return;
 dt=tmin(dt,.05f);wetness=tclamp(wetness,0,1);
 update_signals(w,dt);
 observe_player(w,player,dt);
 update_pedestrians(w,dt);
 update_cars(w,player,dt,wetness);
 update_patrols(w,player,dt,wetness);
}
int rh_traffic_active_cars(const RH_TrafficWorld *w){int n=0;if(w)for(int i=0;i<w->car_count;i++)n+=!!w->cars[i].active;return n;}
int rh_traffic_active_pedestrians(const RH_TrafficWorld *w){int n=0;if(w)for(int i=0;i<w->ped_count;i++)n+=!!w->pedestrians[i].active;return n;}
int rh_traffic_active_patrols(const RH_TrafficWorld *w){int n=0;if(w)for(int i=0;i<RH_TPATROLS;i++)n+=!!w->patrols[i].active;return n;}
