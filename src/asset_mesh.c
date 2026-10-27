#include "asset_mesh.h"
#include <math.h>
#include <stddef.h>

typedef struct {int16_t x,y,z;} RHAssetVertexQ;
typedef struct {uint16_t a,b,c;uint8_t material,emissive;} RHAssetFaceQ;
typedef struct {const RHAssetVertexQ *v;const RHAssetFaceQ *f;unsigned nv,nf;} RHAssetMeshQ;
#include "asset_mesh_data.inc"

#define RH_RGB(r,g,b) (0xff000000u|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))
#define RH_MIN(a,b) ((a)<(b)?(a):(b))
#define RH_MAX(a,b) ((a)>(b)?(a):(b))
static float clamp01(float x){return x<0?0:(x>1?1:x);}
static float smooth(float t){t=clamp01(t);return t*t*(3.0f-2.0f*t);}
static uint32_t mix_color(uint32_t a,uint32_t b,float t){
 t=clamp01(t);uint32_t r=0xff000000u;
 for(int shift=0;shift<24;shift+=8){
  float x=(float)((a>>shift)&255u),y=(float)((b>>shift)&255u);
  unsigned v=(unsigned)(x+(y-x)*t+.5f);if(v>255)v=255;
  r|=v<<shift;
 }
 return r;
}
static uint32_t shade(uint32_t color,float factor){
 if(factor<0)factor=0;
 if(factor>2)factor=2;
 uint32_t out=0xff000000u;
 for(int sh=0;sh<24;sh+=8){
  int c=(int)(((color>>sh)&255u)*factor+.5f);if(c>255)c=255;
  out|=(uint32_t)c<<sh;
 }
 return out;
}
typedef struct {float h;uint32_t top,mid,horizon,ground;float ambient,sun;} SkyKey;
/* Color stops deliberately include saturated gold sunrise, cyan daylight,
 * rose-violet sunset and deep-blue night; lerp in linear time, smooth per stop. */
static const SkyKey SKY[] = {
 {0.0f, RH_RGB(4,10,33), RH_RGB(13,20,48), RH_RGB(31,36,69), RH_RGB(11,16,31),.22f,0},
 {4.8f, RH_RGB(11,20,61), RH_RGB(42,33,88), RH_RGB(140,62,100),RH_RGB(41,31,57),.29f,0},
 {5.6f, RH_RGB(33,48,122),RH_RGB(160,76,131),RH_RGB(255,131,83),RH_RGB(113,73,80),.42f,.12f},
 {6.4f, RH_RGB(40,99,188),RH_RGB(236,143,132),RH_RGB(255,192,111),RH_RGB(175,116,85),.60f,.35f},
 {7.5f, RH_RGB(24,116,211),RH_RGB(78,168,231),RH_RGB(184,222,237),RH_RGB(129,166,159),.77f,.70f},
 {11.0f,RH_RGB(13,104,207),RH_RGB(54,166,235),RH_RGB(147,215,244),RH_RGB(147,177,166),.91f,1.00f},
 {15.5f,RH_RGB(23,112,208),RH_RGB(81,173,229),RH_RGB(177,219,230),RH_RGB(152,166,143),.86f,.88f},
 {17.5f,RH_RGB(47,80,170),RH_RGB(188,108,153),RH_RGB(255,167,104),RH_RGB(157,106,104),.67f,.50f},
 {18.7f,RH_RGB(23,33,103),RH_RGB(120,61,125),RH_RGB(239,93,112),RH_RGB(80,56,92),.43f,.12f},
 {19.8f,RH_RGB(7,18,59),RH_RGB(37,35,82),RH_RGB(103,58,101),RH_RGB(25,30,54),.28f,0},
 {22.0f,RH_RGB(4,11,37),RH_RGB(17,23,56),RH_RGB(39,41,72),RH_RGB(13,19,36),.22f,0},
 {24.0f,RH_RGB(4,10,33),RH_RGB(13,20,48),RH_RGB(31,36,69),RH_RGB(11,16,31),.22f,0}
};
RHAssetAtmosphere rh_asset_atmosphere(float hour,float cloudiness,float wetness){
 RHAssetAtmosphere r;
 if(!isfinite(hour))hour=12.0f;
 hour=fmodf(hour,24.0f);if(hour<0)hour+=24.0f;
 cloudiness=clamp01(cloudiness);wetness=clamp01(wetness);
 unsigned i=0;while(i+2<sizeof(SKY)/sizeof(SKY[0])&&hour>SKY[i+1].h)i++;
 const SkyKey *a=&SKY[i],*b=&SKY[i+1];
 float t=smooth((hour-a->h)/(b->h-a->h));
 r.sky_zenith=mix_color(a->top,b->top,t);
 r.sky_mid=mix_color(a->mid,b->mid,t);
 r.sky_horizon=mix_color(a->horizon,b->horizon,t);
 r.sky_ground=mix_color(a->ground,b->ground,t);
 float daylight=a->ambient+(b->ambient-a->ambient)*t;
 r.direct_sun=(a->sun+(b->sun-a->sun)*t)*(1.0f-.80f*cloudiness);
 r.ambient=daylight*(1.0f-.23f*cloudiness);
 float night=smooth((.58f-r.direct_sun)/.48f);
 r.lamp_strength=night;
 r.window_strength=night*.90f;
 r.wet_glint=wetness*(.20f+.8f*r.direct_sun+.40f*night);
 uint32_t overcast=RH_RGB(106,127,149);
 r.sky_zenith=mix_color(r.sky_zenith,overcast,cloudiness*.66f);
 r.sky_mid=mix_color(r.sky_mid,overcast,cloudiness*.62f);
 r.sky_horizon=mix_color(r.sky_horizon,RH_RGB(163,172,178),cloudiness*.57f);
 r.sky_ground=mix_color(r.sky_ground,RH_RGB(104,113,120),cloudiness*.42f);
 return r;
}
static uint32_t material_color(unsigned material,unsigned variant,
 RHAssetAtmosphere light,unsigned face,unsigned *emissive){
 uint32_t c=RH_RGB(143,155,164);
 float ambient=light.ambient;
 *emissive=0;
 switch(material){
  case 4:{
   unsigned lit=((face*747796405u+variant*2891336453u)>>8)%5u<3u;
   if(lit&&light.window_strength>.32f){
    c=RH_RGB(255,205+(face%3u)*12u,128+(face%4u)*16u);
    *emissive=1;return shade(c,light.window_strength*.75f+.25f);
   }
   return shade(RH_RGB(49,83,108),.40f+.50f*ambient);
  }
  case 23:
   if(light.lamp_strength>.20f){*emissive=1;return shade(RH_RGB(255,228,151),light.lamp_strength*.9f+.1f);}
   return RH_RGB(93,103,103);
  case 14:
   if(light.lamp_strength>.2f){*emissive=1;return RH_RGB(255,247,203);}
   return RH_RGB(204,209,192);
  case 15:
   if(light.lamp_strength>.2f){*emissive=1;return RH_RGB(241,54,38);}
   return RH_RGB(158,42,34);
  case 0:c=RH_RGB(173,171,160);break;
  case 6:c=RH_RGB(53,59,63);break;
  case 7:c=RH_RGB(123,136,141);break;
  case 8:c=RH_RGB(83,142,55);break;
  case 9:c=RH_RGB(43,110,58);break;
  case 10:c=RH_RGB(91,70,49);break;
  case 11:c=RH_RGB(35,38,42);break;
  case 12:c=RH_RGB(164,175,183);break;
  case 13:c=RH_RGB(68,126,157);break;
  case 16:
   switch(variant%6u){
    case 0:c=RH_RGB(32,122,187);break;
    case 1:c=RH_RGB(187,47,52);break;
    case 2:c=RH_RGB(221,218,209);break;
    case 3:c=RH_RGB(44,127,97);break;
    case 4:c=RH_RGB(208,150,46);break;
    default:c=RH_RGB(88,92,105);break;
   }break;
  case 17:
   switch(variant%6u){
    case 0:c=RH_RGB(25,85,136);break;
    case 1:c=RH_RGB(121,35,43);break;
    case 2:c=RH_RGB(151,153,152);break;
    case 3:c=RH_RGB(31,84,68);break;
    case 4:c=RH_RGB(145,96,32);break;
    default:c=RH_RGB(59,64,73);break;
   }break;
  case 18:
   return shade(mix_color(light.sky_horizon,RH_RGB(240,246,247),.62f),
                .34f+light.wet_glint*.7f);
  case 19:
   return shade(mix_color(light.sky_zenith,light.sky_horizon,.56f),
                .58f+light.wet_glint*.70f);
  case 20:c=RH_RGB(207,216,213);break;
  case 21:c=RH_RGB(234,193,62);break;
  case 22:c=RH_RGB(37,46,52);break;
  case 24:c=RH_RGB(168,173,169);break;
  case 25:c=RH_RGB(119,125,123);break;
  default:break;
 }
 return shade(c,.24f+.76f*ambient);
}
unsigned rh_asset_triangle_budget(RHAssetType type,unsigned lod){
 if((unsigned)type>=RH_ASSET_COUNT||lod>=3)return 0;
 return rh_asset_meshes[type][lod].nf;
}
unsigned rh_asset_vertex_budget(RHAssetType type,unsigned lod){
 if((unsigned)type>=RH_ASSET_COUNT||lod>=3)return 0;
 return rh_asset_meshes[type][lod].nv;
}
unsigned rh_asset_emit(RHAssetType type,RHAssetInstance inst,float distance_m,
 RHAssetAtmosphere light,RHAssetEmit emit,void *ctx){
 if((unsigned)type>=RH_ASSET_COUNT||!emit||!isfinite(distance_m)||distance_m>400.0f||distance_m<0||
    !isfinite(inst.x)||!isfinite(inst.y)||!isfinite(inst.z)||!isfinite(inst.yaw)||
    !isfinite(inst.scale)||inst.scale<=0||inst.scale>64)return 0;
 unsigned lod=distance_m<65.0f?0u:(distance_m<170.0f?1u:2u);
 const RHAssetMeshQ *m=&rh_asset_meshes[type][lod];
 const float s=sinf(inst.yaw)*inst.scale,c=cosf(inst.yaw)*inst.scale;
 const float k=inst.scale/64.0f;
 RHAssetTriangle tri;
 for(unsigned i=0;i<m->nf;i++){
  const RHAssetFaceQ *f=&m->f[i];
  unsigned emissive=0;
  tri.abgr=material_color(f->material,inst.variant,light,i,&emissive);
  tri.emissive=(uint8_t)emissive;
  unsigned ix[3]={f->a,f->b,f->c};
  for(unsigned j=0;j<3;j++){
   const RHAssetVertexQ *v=&m->v[ix[j]];
   const float x=(float)v->x/64.0f,z=(float)v->z/64.0f;
   tri.p[j].x=inst.x+x*c+z*s;
   tri.p[j].y=inst.y+(float)v->y*k;
   tri.p[j].z=inst.z-x*s+z*c;
  }
  emit(ctx,&tri);
 }
 return m->nf;
}

/* The following optional decals use alpha in the ABGR vertex color.
 * Integrator MUST use GU_BLEND and GU depth test with depth writes off.
 * Opaque triangle submission would make these look like dark rectangles. */
static int valid_decal(RHAssetInstance a,float distance_m,float cutoff,RHAssetEmit emit){
 return emit&&isfinite(distance_m)&&distance_m>=0&&distance_m<=cutoff&&
 isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z)&&isfinite(a.yaw)&&
 isfinite(a.scale)&&a.scale>0&&a.scale<=64;
}
unsigned rh_asset_emit_shadow(RHAssetType type,RHAssetInstance inst,
 float distance_m,float hour,RHAssetAtmosphere light,RHAssetEmit emit,void *ctx){
 if(!valid_decal(inst,distance_m,85.0f,emit)||
    (unsigned)type>=RH_ASSET_COUNT||light.direct_sun<.13f||
    type==RH_ASSET_PUDDLE||type==RH_ASSET_GRASS)return 0;
 const float width[]={1.9f,2.6f,15.0f,27.0f,.35f,3.6f,0,15.0f,0};
 const float length[]={4.2f,12.5f,13.0f,18.0f,.35f,3.6f,0,48.0f,0};
 const float height[]={1.6f,4.0f,18.0f,9.0f,7.3f,7.0f,0,5.0f,0};
 float h=height[type]*inst.scale;
 float t=fmodf(hour,24.0f);if(t<0)t+=24.0f;
 float az=t*.2617993878f;
 float sun_height=sinf((t-6.0f)*.2617993878f);
 if(sun_height<.18f)sun_height=.18f;
 float reach=h*(1.0f-sun_height)/(.25f+sun_height);
 if(reach>35.0f)reach=35.0f;
 float dx=-cosf(az)*reach,dz=-sinf(az)*reach;
 float hw=width[type]*inst.scale*.5f,hl=length[type]*inst.scale*.5f;
 float c=cosf(inst.yaw),s=sinf(inst.yaw);
 RHAssetPoint corners[4];
 for(unsigned k=0;k<4;k++){
  float xx=(k==0||k==3)?-hw:hw;
  float zz=(k<2)?-hl:hl;
  corners[k].x=inst.x+xx*c+zz*s;
  corners[k].y=inst.y+.045f;
  corners[k].z=inst.z-xx*s+zz*c;
 }
 unsigned alpha=(unsigned)(30.0f+RH_MIN(light.direct_sun,1.0f)*70.0f);
 RHAssetTriangle tri;
 tri.abgr=(alpha<<24)|0x000c1018u;tri.emissive=2;
 /* Extend a silhouette from its far edge, not a huge solid square. */
 tri.p[0]=corners[0];tri.p[1]=corners[1];
 tri.p[2]=corners[2];tri.p[2].x+=dx;tri.p[2].z+=dz;emit(ctx,&tri);
 tri.p[0]=corners[0];tri.p[1]=corners[2];tri.p[1].x+=dx;tri.p[1].z+=dz;
 tri.p[2]=corners[3];tri.p[2].x+=dx;tri.p[2].z+=dz;emit(ctx,&tri);
 return 2;
}
unsigned rh_asset_emit_light_pool(RHAssetType type,RHAssetInstance inst,
 float distance_m,RHAssetAtmosphere light,RHAssetEmit emit,void *ctx){
 if(!valid_decal(inst,distance_m,55.0f,emit)||light.lamp_strength<.32f||
   (type!=RH_ASSET_STREETLAMP&&type!=RH_ASSET_CAR&&type!=RH_ASSET_TRUCK))return 0;
 float off=type==RH_ASSET_STREETLAMP?1.8f:(type==RH_ASSET_CAR?2.8f:3.7f);
 float radius=type==RH_ASSET_STREETLAMP?4.8f:(type==RH_ASSET_CAR?3.2f:4.3f);
 float s=sinf(inst.yaw),c=cosf(inst.yaw);
 float x=inst.x+off*s*inst.scale,z=inst.z+off*c*inst.scale;
 RHAssetTriangle tri;
 unsigned alpha=(unsigned)(17.0f+30.0f*light.lamp_strength);
 tri.abgr=(alpha<<24)|(type==RH_ASSET_STREETLAMP?0x008bc8ffu:0x00d8e8ffu);
 tri.emissive=2;
 for(unsigned k=0;k<8;k++){
  float a=(float)k*.7853981634f,b=(float)(k+1)*.7853981634f;
  tri.p[0]=(RHAssetPoint){x,inst.y+.055f,z};
  tri.p[1]=(RHAssetPoint){x+radius*cosf(a),inst.y+.055f,z+radius*sinf(a)};
  tri.p[2]=(RHAssetPoint){x+radius*cosf(b),inst.y+.055f,z+radius*sinf(b)};
  emit(ctx,&tri);
 }
 return 8;
}
