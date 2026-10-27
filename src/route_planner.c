#include "route_planner.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int valid_node(const RH_RouteGraph *g, int node) {
    return g && node >= 0 && node < (int)g->node_count;
}
static int valid_metre(float x){return isfinite(x) && x >= 0.0f && x <= RH_ROUTE_WORLD_METRES;}
static float geometric(const RH_RouteNode *a, const RH_RouteNode *b) {
    float dx=a->x-b->x, dz=a->z-b->z;
    return sqrtf(dx*dx+dz*dz);
}
void rh_route_init(RH_RouteGraph *g) { if(g) memset(g,0,sizeof(*g)); }
int rh_route_add_node(RH_RouteGraph *g, float x, float z, int depot) {
    if(!g || g->node_count >= RH_ROUTE_MAX_NODES || !valid_metre(x) || !valid_metre(z)) return -1;
    int i=g->node_count++;
    g->nodes[i].x=x;g->nodes[i].z=z;g->nodes[i].first_edge=-1;
    g->nodes[i].depot=depot?1:0;
    return i;
}
int rh_route_add_arc(RH_RouteGraph *g,int from,int to,float metres) {
    if(!valid_node(g,from) || !valid_node(g,to) || from==to || g->edge_count>=RH_ROUTE_MAX_EDGES || !isfinite(metres) || metres<0.0f) return 0;
    float direct=geometric(&g->nodes[from],&g->nodes[to]);
    if (direct<0.01f) return 0; /* prohibit same-coordinate zero-length routes */
    if (metres==0.0f) metres=direct;
    if (metres<direct-0.005f) return 0; /* road cannot be shorter than Euclidean */
    int e=g->edge_count++;
    g->edges[e].dest=(uint16_t)to;g->edges[e].metres=metres;
    g->edges[e].next=g->nodes[from].first_edge;g->nodes[from].first_edge=e;
    return 1;
}
int rh_route_add_road(RH_RouteGraph *g,int from,int to,float metres) {
    if(!g || g->edge_count>RH_ROUTE_MAX_EDGES-2 || !rh_route_add_arc(g,from,to,metres))return 0;
    if(!rh_route_add_arc(g,to,from,metres)) { /* rollback first insert atomically */
        --g->edge_count;
        g->nodes[from].first_edge=g->edges[g->edge_count].next;
        return 0;
    }
    return 1;
}
int rh_route_nearest_node(const RH_RouteGraph *g,float x,float z,int depot_only){
    if(!g||!valid_metre(x)||!valid_metre(z))return -1;
    float best=RH_ROUTE_UNREACHABLE;int index=-1;
    for(int i=0;i<g->node_count;++i){
        if(depot_only&&!g->nodes[i].depot)continue;
        float dx=g->nodes[i].x-x,dz=g->nodes[i].z-z;
        float dsq=dx*dx+dz*dz;
        if(dsq<best){best=dsq;index=i;}
    }
    return index;
}
static int heap_less(const RH_RouteSearch *s,int a,int b){
    float da=s->metres[s->heap[a]], db=s->metres[s->heap[b]];
    return da<db || (da==db && s->heap[a]<s->heap[b]);
}
static void swap(RH_RouteSearch *s,int a,int b){
    uint16_t na=s->heap[a],nb=s->heap[b];
    s->heap[a]=nb;s->heap[b]=na;
    s->heap_pos[na]=(int16_t)b;s->heap_pos[nb]=(int16_t)a;
}
static void heap_up(RH_RouteSearch *s,int k){
    while(k>0){int p=(k-1)/2;if(!heap_less(s,k,p))break;swap(s,k,p);k=p;}
}
static void heap_down(RH_RouteSearch *s,int k){
    for(;;){int a=k*2+1,b=a+1,best=k;
        if(a<s->count&&heap_less(s,a,best))best=a;
        if(b<s->count&&heap_less(s,b,best))best=b;
        if(best==k)break;
        swap(s,k,best);
        k=best;
    }
}
static void insert_or_update(RH_RouteSearch *s,int n){
    if(s->heap_pos[n]>=0){heap_up(s,s->heap_pos[n]);return;}
    int k=s->count++;s->heap[k]=(uint16_t)n;s->heap_pos[n]=(int16_t)k;heap_up(s,k);
}
static int remove_min(RH_RouteSearch *s){
    int n=s->heap[0];s->heap_pos[n]=-2;--s->count;
    if(s->count){s->heap[0]=s->heap[s->count];s->heap_pos[s->heap[0]]=0;heap_down(s,0);}
    return n;
}
int rh_route_scan(const RH_RouteGraph *g,RH_RouteSearch *s,int start){
    if(!valid_node(g,start)||!s)return 0;
    memset(s,0,sizeof(*s));
    for(int i=0;i<g->node_count;++i){s->metres[i]=RH_ROUTE_UNREACHABLE;s->predecessor[i]=-1;s->heap_pos[i]=-1;}
    s->metres[start]=0.0f;insert_or_update(s,start);
    while(s->count){
        int n=remove_min(s);
        for(int e=g->nodes[n].first_edge;e>=0;e=g->edges[e].next){
            int v=g->edges[e].dest;
            if(s->heap_pos[v]==-2)continue;
            float d=s->metres[n]+g->edges[e].metres;
            if(d+1e-4f<s->metres[v]){s->metres[v]=d;s->predecessor[v]=n;insert_or_update(s,v);}
        }
    }
    return 1;
}
int rh_route_path(const RH_RouteGraph *g,const RH_RouteSearch *s,int end,uint16_t *output,int capacity){
    if(!valid_node(g,end)||!s||!output||capacity<=0||s->metres[end]>=RH_ROUTE_UNREACHABLE)return -1;
    int n=0,cur=end;
    while(cur>=0 && n<=g->node_count){++n;cur=s->predecessor[cur];}
    if(n<=0||n>g->node_count||n>capacity)return -1;
    cur=end;
    for(int i=n-1;i>=0;--i){output[i]=(uint16_t)cur;cur=s->predecessor[cur];}
    return n;
}
static uint32_t rng(uint32_t *s) {
    uint32_t x=*s;
    if(!x)x=0xAD264117u;
    x^=x<<13;x^=x>>17;x^=x<<5;
    *s=x;return x;
}
static int bucket(float d){return d<800.0f?0:d<4000.0f?1:2;}
int rh_route_choose_mission(const RH_RouteGraph *g,const RH_RouteSearch *s,int start,
                            int cargo_tonnes,uint32_t *seed,RH_RouteMission *job){
    if(!valid_node(g,start)||!s||!seed||!job||cargo_tonnes<0||cargo_tonnes>100)return 0;
    /* 0–99: short 30%, medium 40%, long 30%. */
    int draw=(int)(rng(seed)%100u);int wanted=draw<30?0:draw<70?1:2;
    int counts[3]={0,0,0};
    for(int i=0;i<g->node_count;i++)if(i!=start&&g->nodes[i].depot&&s->metres[i]>=100.0f&&s->metres[i]<RH_ROUTE_UNREACHABLE)counts[bucket(s->metres[i])]++;
    /* Preserve selection bias where available; otherwise nearest nonempty bucket. */
    int chosen=wanted;
    if(!counts[chosen]){
        int best=4;
        for(int k=0;k<3;k++)if(counts[k]&&abs(k-wanted)<best){best=abs(k-wanted);chosen=k;}
        if(best==4)return 0;
    }
    int index=(int)(rng(seed)%(uint32_t)counts[chosen]);int dest=-1;
    for(int i=0;i<g->node_count;i++){
        if(i!=start&&g->nodes[i].depot&&s->metres[i]>=100.0f&&s->metres[i]<RH_ROUTE_UNREACHABLE&&bucket(s->metres[i])==chosen){
            if(index--==0){dest=i;break;}
        }
    }
    if(dest<0)return 0;
    job->from_node=(uint16_t)start;job->to_node=(uint16_t)dest;
    job->route_metres=s->metres[dest];job->category=(uint8_t)chosen;
    /* Short jobs still pay at least 300, weighted but never negative. */
    job->payout=300.0f+job->route_metres*(0.46f+0.015f*(float)cargo_tonnes);
    return 1;
}
int rh_route_validate(const RH_RouteGraph *g,int origin){
    if(!valid_node(g,origin))return 0;
    for(int i=0;i<g->node_count;i++){
        const RH_RouteNode *a=&g->nodes[i];
        if(!valid_metre(a->x)||!valid_metre(a->z))return 0;
        int loop_count=0;
        for(int e=a->first_edge;e>=0;e=g->edges[e].next){
            if(e>=g->edge_count||++loop_count>g->edge_count)return 0;
            const RH_RouteEdge *r=&g->edges[e];
            if(r->dest>=g->node_count||!isfinite(r->metres)||r->metres<geometric(a,&g->nodes[r->dest])-0.01f)return 0;
        }
    }
    RH_RouteSearch *s=malloc(sizeof(*s));
    if(!s)return 0;
    int ok=rh_route_scan(g,s,origin);
    if(ok){for(int i=0;i<g->node_count;i++)if(g->nodes[i].depot&&s->metres[i]>=RH_ROUTE_UNREACHABLE){ok=0;break;}}
    free(s);return ok;
}
