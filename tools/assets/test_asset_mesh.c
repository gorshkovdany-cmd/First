#include "../../src/asset_mesh.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
static unsigned triangles,lit;
static void check(void *ctx,const RHAssetTriangle *t){
 (void)ctx;triangles++;lit+=t->emissive;
 for(int k=0;k<3;k++){
  assert(isfinite(t->p[k].x));assert(isfinite(t->p[k].y));assert(isfinite(t->p[k].z));
  assert(fabsf(t->p[k].x)<10000);assert(fabsf(t->p[k].y)<10000);assert(fabsf(t->p[k].z)<10000);
 }
 assert(t->emissive==2 ? (t->abgr>>24)>0u && (t->abgr>>24)<255u : (t->abgr>>24)==255u);
}
int main(void){
 for(int i=0;i<1440;i++){
  RHAssetAtmosphere a=rh_asset_atmosphere(i/60.0f,.4f,.8f);
  assert(isfinite(a.ambient)&&a.ambient>.0f&&a.ambient<=1.0f);
  assert(a.direct_sun>=0&&a.direct_sun<=1);
  assert(a.lamp_strength>=0&&a.lamp_strength<=1);
  assert(a.wet_glint>=0&&a.wet_glint<=2);
  assert((a.sky_zenith>>24)==255u);
 }
 RHAssetAtmosphere dawn=rh_asset_atmosphere(6.1f,0,0);
 RHAssetAtmosphere noon=rh_asset_atmosphere(12,0,0);
 RHAssetAtmosphere dusk=rh_asset_atmosphere(18.5f,0,0);
 RHAssetAtmosphere night=rh_asset_atmosphere(1,0,0);
 assert(noon.direct_sun>dawn.direct_sun&&noon.direct_sun>dusk.direct_sun);
 assert(night.lamp_strength>noon.lamp_strength);
 assert(dawn.sky_horizon!=dusk.sky_horizon&&dawn.sky_horizon!=noon.sky_horizon);
 unsigned total=0,night_emissive=0;
 for(unsigned a=0;a<RH_ASSET_COUNT;a++){
  for(unsigned l=0;l<3;l++){
   unsigned budget=rh_asset_triangle_budget((RHAssetType)a,l);
   assert(budget>0&&budget<=1000);
   assert(rh_asset_vertex_budget((RHAssetType)a,l)<=800);
   float dist=l==0?32:l==1?110:290;
   RHAssetInstance inst={100,3,200,.5f,1.0f,a};
   triangles=lit=0;
   unsigned emitted=rh_asset_emit((RHAssetType)a,inst,dist,night,check,0);
   assert(emitted==budget&&triangles==budget);
   total+=triangles;night_emissive+=lit;
   triangles=0;
   assert(rh_asset_emit((RHAssetType)a,inst,401,night,check,0)==0);
   assert(triangles==0);
  }
 }
 assert(night_emissive>0);
 RHAssetInstance invalid={0,0,0,0,0,0};
 assert(rh_asset_emit(RH_ASSET_CAR,invalid,10,night,check,0)==0);
 assert(rh_asset_triangle_budget(RH_ASSET_CAR,99)==0);
 assert(rh_asset_triangle_budget((RHAssetType)100,0)==0);
 RHAssetInstance decal={0,0,0,0,1,0};
 triangles=0;
 assert(rh_asset_emit_shadow(RH_ASSET_CAR,decal,25,12,noon,check,0)==2);
 assert(triangles==2);
 assert(rh_asset_emit_shadow(RH_ASSET_CAR,decal,25,1,night,check,0)==0);
 triangles=0;
 assert(rh_asset_emit_light_pool(RH_ASSET_STREETLAMP,decal,25,night,check,0)==8);
 assert(triangles==8);
 assert(rh_asset_emit_light_pool(RH_ASSET_STREETLAMP,decal,25,noon,check,0)==0);
 printf("PASS: 9 model types x 3 LODs; %u emitted triangles, %u night emissive; 1440 sky steps\n",total,night_emissive);
 return 0;
}
