#include "visibility_budget.h"
#include <math.h>
#include <string.h>

static int loaded(const RH_ViewSet *s,int id){return (s->history[id>>3]>>(id&7))&1;}
static void add_history(RH_ViewSet *s,int id){s->history[id>>3]|=(uint8_t)(1u<<(id&7));}
void rh_view_init(RH_ViewSet *s){if(s)memset(s,0,sizeof(*s));}
void rh_view_collect(RH_ViewSet *s,float x,float z,float yaw,const uint8_t *zones){
    if(!s)return;
    s->count=0;
    if(!isfinite(x)||!isfinite(z)||!isfinite(yaw)||x<0||z<0||
       x>=RH_VIEW_WORLD_METRES||z>=RH_VIEW_WORLD_METRES){
        memset(s->history,0,sizeof(s->history));return;
    }
    float hx=sinf(yaw),hz=cosf(yaw);
    /* 9x9 local candidates, selected to cover a 545m forward viewing radius.
     * The 128m tile edge gives enough margin to avoid zero-distance popping. */
    int tx=(int)(x/RH_VIEW_TILE_METRES),tz=(int)(z/RH_VIEW_TILE_METRES);
    float priorities[RH_VIEW_MAX_VISIBLE];
    RH_ViewTile selected[RH_VIEW_MAX_VISIBLE];
    int count=0;
    for(int dz=-5;dz<=5;dz++)for(int dx=-5;dx<=5;dx++){
        int cx=tx+dx,cz=tz+dz;
        if(cx<0||cz<0||cx>=RH_VIEW_TILE_SIDE||cz>=RH_VIEW_TILE_SIDE)continue;
        float mx=(cx+0.5f)*RH_VIEW_TILE_METRES-x;
        float mz=(cz+0.5f)*RH_VIEW_TILE_METRES-z;
        float dist2=mx*mx+mz*mz;
        if(dist2>RH_VIEW_PREFETCH_METRES*RH_VIEW_PREFETCH_METRES)continue;
        float dist=sqrtf(dist2);
        if(dist>182.0f){
            /* forward field of view widened for truck turning and peripheral
             * streets, with near cells always rendered irrespective of yaw. */
            float front=(mx*hx+mz*hz)/dist;
            if(front<0.34202015f)continue; /* >70 degrees off heading */
        }
        int id=cz*RH_VIEW_TILE_SIDE+cx;
        uint8_t zone=zones?zones[id]:0;
        float score=dist2;
        if(zone&RH_VIEW_ZONE_CITY)score*=.87f;
        if(loaded(s,id))score*=.82f;
        /* insertion into capped sorted priority list; stable by cell id */
        int at=0;
        while(at<count && (priorities[at]<score ||
             (priorities[at]==score && selected[at].id<id)))at++;
        if(at>=RH_VIEW_MAX_VISIBLE)continue;
        if(count<RH_VIEW_MAX_VISIBLE)count++;
        for(int k=count-1;k>at;k--){selected[k]=selected[k-1];priorities[k]=priorities[k-1];}
        selected[at].id=(uint16_t)id;
        selected[at].zone=zone;
        selected[at].lod=(uint8_t)(dist<=165.0f?0:dist<=315.0f?1:2);
        priorities[at]=score;
    }
    s->count=(uint16_t)count;
    memset(s->history,0,sizeof(s->history));
    for(int i=0;i<count;i++){
        s->cells[i]=selected[i];
        add_history(s,selected[i].id);
    }
}
int rh_view_lod(const RH_ViewSet *s,int id){
    if(!s||id<0||id>=RH_VIEW_TILE_TOTAL)return -1;
    for(int i=0;i<s->count;i++)if(s->cells[i].id==id)return s->cells[i].lod;
    return -1;
}
