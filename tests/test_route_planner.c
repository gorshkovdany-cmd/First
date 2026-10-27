#include "../src/route_planner.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static RH_RouteGraph *graph(void) {RH_RouteGraph *g=malloc(sizeof(*g));assert(g);rh_route_init(g);return g;}
static RH_RouteSearch *search(void){RH_RouteSearch *s=malloc(sizeof(*s));assert(s);return s;}
static void basic_and_disconnected(void){
    RH_RouteGraph *g=graph();RH_RouteSearch *s=search();
    int a=rh_route_add_node(g,0,0,1),b=rh_route_add_node(g,100,0,1);
    int c=rh_route_add_node(g,100,100,1),d=rh_route_add_node(g,3000,4000,1);
    int e=rh_route_add_node(g,1300,100,1);
    assert(a==0&&b==1&&c==2&&d==3);
    assert(rh_route_nearest_node(g,1,1,0)==a);
    assert(rh_route_nearest_node(g,1299,101,1)==e);
    assert(rh_route_nearest_node(g,-1,1,1)==-1);
    assert(!rh_route_add_arc(g,a,a,10));
    assert(!rh_route_add_arc(g,a,b,99));
    assert(!rh_route_add_arc(g,a,b,NAN));
    assert(!rh_route_add_arc(g,a,b,INFINITY));
    assert(rh_route_add_node(g,4097,1,0)==-1);
    assert(rh_route_add_road(g,a,b,0));
    assert(rh_route_add_road(g,b,c,150)); /* actual road longer than straight line */
    assert(rh_route_scan(g,s,a));
    assert(fabsf(s->metres[b]-100)<.01f);
    assert(fabsf(s->metres[c]-250)<.01f);
    assert(s->metres[d]>=RH_ROUTE_UNREACHABLE);
    uint16_t p[10];assert(rh_route_path(g,s,c,p,10)==3&&p[0]==a&&p[1]==b&&p[2]==c);
    assert(rh_route_path(g,s,c,p,2)<0); /* insufficient buffer */
    assert(rh_route_path(g,s,d,p,10)<0);
    assert(!rh_route_validate(g,a));
    assert(rh_route_add_road(g,c,e,0));
    assert(rh_route_add_road(g,e,d,0));assert(rh_route_validate(g,a));
    assert(rh_route_scan(g,s,a));uint32_t seed=0x12345567u;RH_RouteMission job;
    int category[3]={0,0,0};
    for(int i=0;i<5000;i++){
        assert(rh_route_choose_mission(g,s,a,17,&seed,&job));
        assert(job.route_metres>=100 && job.payout>=300);
        assert(job.from_node==a && job.to_node!=a);
        category[job.category]++;
    }
    /* Distant jobs happen on actual road lengths, not fake grid units. */
    assert(category[0]>900 && category[1]>1000 && category[2]>900);
    free(s);free(g);
}
static void busy_world_route_stress(void){
    RH_RouteGraph *g=graph();RH_RouteSearch *s=search();
    enum { SIDE=40 }; /* 1600 nodes, 6240 directed arcs; fits PSP limits */
    for(int z=0;z<SIDE;z++)for(int x=0;x<SIDE;x++){
        int n=rh_route_add_node(g,55.0f+x*101.0f,55.0f+z*101.0f,((x+z)%4)==0);
        assert(n==z*SIDE+x);
    }
    for(int z=0;z<SIDE;z++)for(int x=0;x<SIDE;x++){
        int n=z*SIDE+x;
        if(x<SIDE-1)assert(rh_route_add_road(g,n,n+1,0));
        if(z<SIDE-1)assert(rh_route_add_road(g,n,n+SIDE,0));
    }
    assert(g->node_count==1600 && g->edge_count==6240);
    assert(rh_route_validate(g,0));
    assert(rh_route_scan(g,s,0));
    assert(fabsf(s->metres[1599]-7878.0f)<.01f);
    uint16_t path[100];int len=rh_route_path(g,s,1599,path,100);
    assert(len==79 && path[0]==0 && path[len-1]==1599);
    uint32_t seed=0xC0FFEEu;int category[3]={0,0,0};
    for(int i=0;i<30000;i++){
        RH_RouteMission j;assert(rh_route_choose_mission(g,s,0,18,&seed,&j));
        assert(j.route_metres>=100 && j.route_metres<=7878.01f && isfinite(j.payout));
        category[j.category]++;
    }
    assert(category[0]>5000 && category[1]>9000 && category[2]>5000);
    printf("PASS 1600-node connected urban road graph, 6240 arcs, maximum road journey %.0fm, 30k weighted jobs\n",s->metres[1599]);
    free(s);free(g);
}
static void directed_and_atomic(void){
    RH_RouteGraph *g=graph();RH_RouteSearch *s=search();
    int a=rh_route_add_node(g,1,1,1),b=rh_route_add_node(g,1,101,1),c=rh_route_add_node(g,1,201,1);
    assert(rh_route_add_arc(g,a,b,100));assert(rh_route_add_arc(g,b,c,100));
    assert(rh_route_scan(g,s,c));assert(s->metres[a]>=RH_ROUTE_UNREACHABLE); /* one-way road */
    assert(rh_route_scan(g,s,a));assert(s->metres[c]==200);
    assert(!rh_route_add_road(g,a,c,150)); /* too short for 200m direct distance */
    assert(g->edge_count==2);assert(g->nodes[a].first_edge==0); /* atomic, no linked list corruption */
    assert(rh_route_add_road(g,a,c,200));
    assert(g->edge_count==4);
    free(s);free(g);
}
static uint32_t fuzz_rand(uint32_t *seed){
    uint32_t x=*seed;x^=x<<13;x^=x>>17;x^=x<<5;return *seed=x;
}
static void fuzz_shortest_paths(void){
    /* Small random directed road graphs checked against an independent
     * all-pairs Floyd-Warshall oracle (reproducible seed). */
    uint32_t rng=0xB102F00Du;
    RH_RouteGraph *g=graph();RH_RouteSearch *s=search();
    for(int trial=0;trial<300;trial++){
        enum {N=12};float dist[N][N];rh_route_init(g);
        for(int i=0;i<N;i++){
            int x=(int)(fuzz_rand(&rng)%3600),z=(int)(fuzz_rand(&rng)%3600);
            assert(rh_route_add_node(g,(float)x,(float)z,i%5==0)==i);
        }
        for(int i=0;i<N;i++)for(int j=0;j<N;j++)dist[i][j]=i==j?0.0f:RH_ROUTE_UNREACHABLE;
        for(int attempt=0;attempt<50;attempt++){
            int a=(int)(fuzz_rand(&rng)%N),b=(int)(fuzz_rand(&rng)%N);
            if(a==b)continue;
            float dx=g->nodes[a].x-g->nodes[b].x;
            float dz=g->nodes[a].z-g->nodes[b].z;
            float length=sqrtf(dx*dx+dz*dz)+(float)(fuzz_rand(&rng)%200u);
            if(rh_route_add_arc(g,a,b,length)&&length<dist[a][b])dist[a][b]=length;
        }
        for(int k=0;k<N;k++)for(int i=0;i<N;i++)for(int j=0;j<N;j++){
            float a=dist[i][k],b=dist[k][j];
            if(a<RH_ROUTE_UNREACHABLE && b<RH_ROUTE_UNREACHABLE && a+b<dist[i][j])dist[i][j]=a+b;
        }
        for(int from=0;from<N;from++){
            assert(rh_route_scan(g,s,from));
            for(int to=0;to<N;to++){
                if(dist[from][to]>=RH_ROUTE_UNREACHABLE) assert(s->metres[to]>=RH_ROUTE_UNREACHABLE);
                else assert(fabsf(s->metres[to]-dist[from][to])<.1f);
            }
        }
    }
    printf("PASS 300 random directed graphs x 12 sources verified against independent Floyd-Warshall oracle\n");
    free(s);free(g);
}
static void capacity_guard(void){
    RH_RouteGraph *g=graph();RH_RouteSearch *s=search();
    assert(rh_route_add_node(g,0,0,1)==0);
    assert(rh_route_add_node(g,100,0,1)==1);
    for(int i=0;i<RH_ROUTE_MAX_EDGES;i++)assert(rh_route_add_arc(g,0,1,100));
    assert(g->edge_count==RH_ROUTE_MAX_EDGES);
    assert(!rh_route_add_arc(g,0,1,100));
    assert(!rh_route_add_road(g,0,1,100));
    assert(rh_route_scan(g,s,0));
    assert(s->metres[1]==100);
    free(s);free(g);
}
int main(void){basic_and_disconnected();directed_and_atomic();busy_world_route_stress();fuzz_shortest_paths();capacity_guard();
    puts("PASS PSP road mission planner: safe edge validation, 100m minimum, city routes, weighted jobs, path reconstruction");return 0;}
