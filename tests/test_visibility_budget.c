#include "../src/visibility_budget.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void assert_consistent(const RH_ViewSet *s){
    assert(s->count<=RH_VIEW_MAX_VISIBLE);
    char used[RH_VIEW_TILE_TOTAL]={0};
    for(int i=0;i<s->count;i++){
        int id=s->cells[i].id;
        assert(id>=0&&id<RH_VIEW_TILE_TOTAL);
        assert(s->cells[i].lod<=2);
        assert(!used[id]);used[id]=1;
        assert(rh_view_lod(s,id)==s->cells[i].lod);
    }
}
static uint32_t next(uint32_t *s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;}
int main(void){
    RH_ViewSet v;rh_view_init(&v);uint8_t zones[RH_VIEW_TILE_TOTAL]={0};
    for(int row=9;row<=25;row++)for(int col=19;col<32;col++)zones[row*32+col]=RH_VIEW_ZONE_CITY;
    rh_view_collect(&v,2600,2000,0.0f,zones);
    assert_consistent(&v);
    assert(v.count>10);
    int copy_count=v.count;RH_ViewTile copy[RH_VIEW_MAX_VISIBLE];memcpy(copy,v.cells,sizeof(copy));
    /* no camera motion: byte-identical chosen tiles */
    rh_view_collect(&v,2600,2000,0.0f,zones);
    assert(v.count==copy_count);
    assert(memcmp(copy,v.cells,sizeof(copy))==0);
    assert_consistent(&v);
    /* near behind always kept */
    int tx=(int)(2600.0f/128.0f),tz=(int)(2000.0f/128.0f);
    int own=tz*32+tx;
    assert(rh_view_lod(&v,own)==0);
    /* invalid GPS/rotation inputs must never touch out-of-range tiles */
    rh_view_collect(&v,4096,2000,0,zones);assert(v.count==0);
    rh_view_collect(&v,-1,2000,0,zones);assert(v.count==0);
    rh_view_collect(&v,10,NAN,0,zones);assert(v.count==0);
    rh_view_collect(&v,10,10,NAN,zones);assert(v.count==0);
    assert(rh_view_lod(&v,-1)==-1);
    /* 100k reproducible camera poses across ocean, city, docks and edges. */
    uint32_t seed=0xAE0DE123u;int max_count=0;
    for(int i=0;i<100000;i++){
        float px=(float)(next(&seed)%409600u)*.01f;
        float pz=(float)(next(&seed)%409600u)*.01f;
        float yaw=(float)(next(&seed)%62832u)*.0001f;
        rh_view_collect(&v,px,pz,yaw,zones);
        assert_consistent(&v);
        if(v.count>max_count)max_count=v.count;
    }
    printf("PASS 100,000 PSP camera positions, deterministic 128m chunk selection, max active %d / %d\n",max_count,RH_VIEW_MAX_VISIBLE);
    return 0;
}
