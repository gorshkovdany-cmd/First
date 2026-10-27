#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "sim.h"
#include "font.h"
#include "render_psp.h"

#define BUF_WIDTH 512
#define SW 480
#define SH 272
#define VRAM_FRAME (BUF_WIDTH * SH * 4)
#define MAX_3D 54000
#define MAX_2D 25000
#define VERTEX_TYPE (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
typedef struct {uint32_t c;float x,y,z;} Vertex;
static unsigned int __attribute__((aligned(16))) command_list[262144];
static Vertex __attribute__((aligned(16))) mesh[MAX_3D],overlay[MAX_2D];
static int nmesh,nover;
static float cx,cz;
static float player_ground;
static float sunlight=1.0f;
static uint32_t day_color(uint32_t c){
 int r=(int)(((c)&255u)*sunlight), g=(int)(((c>>8)&255u)*sunlight),b=(int)(((c>>16)&255u)*sunlight);
 return 0xff000000u|((uint32_t)b<<16)|((uint32_t)g<<8)|(uint32_t)r;
}
#define RGB(r,g,b) (0xff000000u|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))
#define BLACK RGB(18,25,27)
#define WHITE RGB(234,238,232)
static float absf(float a){return a<0?-a:a;}
static float distance(float x,float z){float dx=x-cx,dz=z-cz;return sqrtf(dx*dx+dz*dz);}
static unsigned hash(unsigned v){v=(v^61u)^(v>>16);v*=9;v^=v>>4;v*=0x27d4eb2du;return v^(v>>15);}
static unsigned cell_hash(int x,int z){return hash((unsigned)x*101177u ^ (unsigned)z*909091u);}
static void put(Vertex *v,int *count,int limit,float x,float y,float z,uint32_t c){
 if(*count>=limit)return;
 Vertex *p=&v[(*count)++];p->c=c;p->x=x;p->y=y;p->z=z;
}
static void mtri(float x1,float y1,float z1,float x2,float y2,float z2,
 float x3,float y3,float z3,uint32_t c){
 if(nmesh>MAX_3D-3)return;
 c=day_color(c);
 put(mesh,&nmesh,MAX_3D,x1-cx,y1-player_ground,z1-cz,c);
 put(mesh,&nmesh,MAX_3D,x2-cx,y2-player_ground,z2-cz,c);
 put(mesh,&nmesh,MAX_3D,x3-cx,y3-player_ground,z3-cz,c);
}
static void quad(float x1,float y1,float z1,float x2,float y2,float z2,
 float x3,float y3,float z3,float x4,float y4,float z4,uint32_t c){
 mtri(x1,y1,z1,x2,y2,z2,x3,y3,z3,c);
 mtri(x1,y1,z1,x3,y3,z3,x4,y4,z4,c);
}
static void flat_quad(float x1,float z1,float x2,float z2,float yadd,uint32_t color){
 float y1=rh_height(x1,z1)+yadd,y2=rh_height(x2,z1)+yadd;
 float y3=rh_height(x2,z2)+yadd,y4=rh_height(x1,z2)+yadd;
 quad(x1,y1,z1,x2,y2,z1,x2,y3,z2,x1,y4,z2,color);
}
static void box3(float x,float y,float z,float w,float h,float l,float a,
 uint32_t front,uint32_t side,uint32_t roof){
 float sx=sinf(a),co=cosf(a),hx=w*.5f,hz=l*.5f;
 float X[4]={-hx,hx,hx,-hx},Z[4]={-hz,-hz,hz,hz},wx[4],wz[4];
 for(int i=0;i<4;i++){wx[i]=x+X[i]*co+Z[i]*sx;wz[i]=z-X[i]*sx+Z[i]*co;}
 // All faces explicitly drawn; GU culling disabled.
 quad(wx[0],y,wz[0],wx[1],y,wz[1],wx[1],y+h,wz[1],wx[0],y+h,wz[0],side);
 quad(wx[1],y,wz[1],wx[2],y,wz[2],wx[2],y+h,wz[2],wx[1],y+h,wz[1],side);
 quad(wx[2],y,wz[2],wx[3],y,wz[3],wx[3],y+h,wz[3],wx[2],y+h,wz[2],front);
 quad(wx[3],y,wz[3],wx[0],y,wz[0],wx[0],y+h,wz[0],wx[3],y+h,wz[3],side);
 quad(wx[0],y+h,wz[0],wx[1],y+h,wz[1],wx[2],y+h,wz[2],wx[3],y+h,wz[3],roof);
}
/* Geometry remains procedural: no missing texture/asset dependencies. */
static void local_quad(float x,float y,float z,float a,
 float x1,float y1,float z1,float x2,float y2,float z2,
 float x3,float y3,float z3,float x4,float y4,float z4,uint32_t c){
 float si=sinf(a),co=cosf(a);
 quad(x+x1*co+z1*si,y+y1,z-x1*si+z1*co,
      x+x2*co+z2*si,y+y2,z-x2*si+z2*co,
      x+x3*co+z3*si,y+y3,z-x3*si+z3*co,
      x+x4*co+z4*si,y+y4,z-x4*si+z4*co,c);
}
static void local_box(float x,float y,float z,float dx,float dy,float dz,float angle,
 float w,float h,float l,uint32_t a,uint32_t b,uint32_t c){
 float s=sinf(angle),co=cosf(angle);
 box3(x+dx*co+dz*s,y+dy,z-dx*s+dz*co,w,h,l,angle,a,b,c);
}
static void terrain(void){
 const float step=58;
 int ix=(int)floorf(cx/step),iz=(int)floorf(cz/step);
 for(int x=ix-8;x<=ix+8;x++)for(int z=iz-8;z<=iz+8;z++){
  float x1=x*step,z1=z*step,x2=x1+step,z2=z1+step;
  if(distance(x1+step*.5f,z1+step*.5f)>650)continue;
  unsigned seed=cell_hash(x,z);uint32_t color=(seed&1)?RGB(100,147,85):RGB(104,153,91);
  flat_quad(x1,z1,x2,z2,-.24f,color);
 }
}
static void roads(void){
 float sx=RH_ROAD_SPACING;
 for(int gx=(int)floorf(cx/sx+.5f)-1;gx<=(int)floorf(cx/sx+.5f)+1;gx++){
  float x=gx*sx;if(absf(x-cx)>620)continue;
  int iz=(int)floorf(cz/34.0f);
  for(int k=iz-19;k<=iz+19;k++){
   float z=k*34.0f,z2=z+34;
   if(distance(x,(z+z2)*.5f)>630)continue;
   flat_quad(x-8.7f,z,x+8.7f,z2,.040f,RGB(121,119,105));
   flat_quad(x-7,z,x+7,z2,.045f,RGB(96,99,96));
   flat_quad(x-6.2f,z,x+6.2f,z2,.055f,RGB(48,54,59));
   flat_quad(x-6.07f,z,x-5.92f,z2,.07f,RGB(221,220,200));
   flat_quad(x+5.92f,z,x+6.07f,z2,.07f,RGB(221,220,200));
   if((k&1)==0)flat_quad(x-.10f,z+4,x+.10f,z+20,.07f,RGB(237,212,127));
  }
 }
 for(int gz=(int)floorf(cz/sx+.5f)-1;gz<=(int)floorf(cz/sx+.5f)+1;gz++){
  float z=gz*sx;if(absf(z-cz)>620)continue;
  int ix=(int)floorf(cx/34.0f);
  for(int k=ix-19;k<=ix+19;k++){
   float x=k*34.0f,x2=x+34;
   if(distance((x+x2)*.5f,z)>630)continue;
   flat_quad(x,z-8.7f,x2,z+8.7f,.040f,RGB(121,119,105));
   flat_quad(x,z-7,x2,z+7,.048f,RGB(96,99,96));
   flat_quad(x,z-6.2f,x2,z+6.2f,.06f,RGB(48,54,59));
   flat_quad(x,z-6.07f,x2,z-5.92f,.075f,RGB(221,220,200));
   flat_quad(x,z+5.92f,x2,z+6.07f,.075f,RGB(221,220,200));
   if((k&1)==0)flat_quad(x+4,z-.1f,x+20,z+.1f,.08f,RGB(237,212,127));
  }
 }
 // Access lanes from the city grid to a company yard and service station.
 for(int ix=(int)floorf(cx/sx+.5f)-1;ix<=(int)floorf(cx/sx+.5f)+1;ix++)
  for(int iz=(int)floorf(cz/sx+.5f)-1;iz<=(int)floorf(cz/sx+.5f)+1;iz++){
   float bx=ix*sx,bz=iz*sx;
   if(distance(bx,bz)>620)continue;
   for(int k=0;k<6;k++){
    float a=bx+k*21.0f,b=bx+(k+1)*21.0f;
    flat_quad(a,bz+130,b,bz+140,.07f,RGB(65,70,74));
    flat_quad(bx-(k+1)*21,bz-135,bx-k*21,bz-125,.07f,RGB(65,70,74));
   }
   flat_quad(bx+75,bz+113,bx+156,bz+159,.085f,RGB(67,74,79));
   flat_quad(bx-156,bz-155,bx-75,bz-106,.085f,RGB(72,79,76));
  }
}
static void tree(float x,float z,unsigned seed){
 if(distance(x,z)>390||rh_road_distance(x,z)<20)return;
 float y=rh_height(x,z),scale=3.2f+(seed&7)*.37f;
 box3(x,y,z,.62f,scale,.62f,0,RGB(86,71,49),RGB(92,76,52),RGB(84,69,44));
 float h=scale*1.1f,y0=y+scale*.75f;
 float X[4]={-2.3f,2.3f,2.3f,-2.3f},Z[4]={-2.3f,-2.3f,2.3f,2.3f};
 uint32_t leaf=(seed&1)?RGB(51,107,55):RGB(65,120,62);
 for(int k=0;k<4;k++){int k2=(k+1)%4;
  mtri(x+X[k],y0,z+Z[k],x+X[k2],y0,z+Z[k2],x,y0+h*2,z,leaf);
 }
}
static void lamppost(float x,float z,float a){
 if(distance(x,z)>325.0f)return;
 float y=rh_height(x,z);float si=sinf(a),co=cosf(a);
 box3(x,y,z,.15f,7.4f,.15f,0,RGB(112,124,125),RGB(95,111,114),RGB(142,146,138));
 box3(x+si*1.5f,y+7.25f,z+co*1.5f,3.0f,.12f,.16f,a,
  RGB(100,114,118),RGB(99,108,113),RGB(98,112,114));
 box3(x+si*2.8f,y+7.10f,z+co*2.8f,.9f,.17f,.72f,0,
  RGB(246,227,162),RGB(210,209,175),RGB(242,235,196));
}
static void highway_sign(float x,float z,float a,int seed){
 if(distance(x,z)>355)return;
 float y=rh_height(x,z);
 box3(x,y,z,.18f,5.5f,.18f,0,RGB(113,127,125),RGB(100,117,115),RGB(151,157,145));
 local_box(x,y,z,0,5.25f,0,a,4.8f,1.3f,.16f,
   RGB(30,111,94),RGB(22,85,75),RGB(31,118,94));
 local_box(x,y,z,-1.45f,5.38f,.11f,a,1.25f,.16f,.09f,
   RGB(240,238,224),RGB(224,227,220),RGB(247,249,238));
 local_box(x,y,z,.36f,5.05f,.11f,a,2.2f,.11f,.09f,
   RGB(236,242,231),RGB(224,234,228),RGB(247,249,240));
 if(seed&1)local_box(x,y,z,.35f,5.48f,.11f,a,2.1f,.10f,.09f,
   RGB(236,242,231),RGB(224,234,228),RGB(247,249,240));
}
static void building(float x,float z,float w,float l,float h,uint32_t color){
 if(distance(x,z)>475)return;
 float y=rh_height(x,z);
 box3(x,y,z,w,h,l,0,color,RGB(123,124,120),RGB(85,101,109));
 box3(x,y+h-.18f,z,w+.5f,.28f,l+.5f,0,RGB(66,71,73),RGB(62,71,77),RGB(78,85,90));
 // Window bands, entrances and ventilators make the settlements legible.
 if(w<35){
  for(int floor=0;floor<2;floor++)for(int window=-1;window<=1;window++){
   float wx=x+window*w*.255f,wy=y+2.7f+floor*3.5f;
   if(wy+1.45f>=y+h)continue;
   box3(wx,wy,z+l*.5f+.10f,2.8f,1.55f,.09f,0,
     RGB(59,100,117),RGB(57,84,99),RGB(104,144,151));
   box3(wx,wy,z+l*.5f+.17f,.10f,1.55f,.06f,0,
     RGB(166,180,179),RGB(162,175,178),RGB(181,183,181));
  }
 }else{
  for(int k=0;k<5;k++)box3(x+(k-2)*w*.16f,y+h-2.0f,z-l*.5f-.06f,
    w*.095f,1.1f,.12f,0,RGB(66,94,103),RGB(63,88,98),RGB(75,107,115));
 }
 // Large dark loading door on the front, separate from grey facade.
 box3(x,y+.08f,z+l*.5f+.07f,w*.36f,h*.68f,.12f,0,RGB(40,48,53),RGB(42,52,57),RGB(49,58,61));
}
static void station(float bx,float bz){
 float x=bx-113,z=bz-130;if(distance(x,z)>540)return;
 float y=rh_height(x,z);
 box3(x+19,y,z,29,5,12,0,RGB(202,209,199),RGB(189,201,193),RGB(97,112,112));
 box3(x-8,y+4.6f,z,28,.65f,14,0,RGB(239,231,224),RGB(210,221,209),RGB(49,143,143));
 for(int k=-1;k<=1;k++){
  box3(x-9,y,z+k*3.9f,1.4f,4.8f,1.4f,0,WHITE,RGB(177,189,189),RGB(52,156,155));
  box3(x-9,y+1.1f,z+k*3.9f,1.5f,1.9f,.20f,0,RGB(36,41,45),RGB(40,45,47),RGB(35,52,55));
 }
 box3(x-16,y,z+6,.65f,7,.65f,0,RGB(211,209,186),RGB(202,199,176),RGB(240,232,214));
 box3(x-16,y+5.7f,z+6,5,1.2f,.32f,0,RGB(235,242,231),RGB(241,243,235),RGB(57,165,167));
 // Forecourt price display, painted kerbs and stop markers.
 box3(x+8,y+.11f,z-8,20,.20f,.30f,0,RGB(225,220,203),RGB(224,223,215),RGB(250,240,215));
 for(int k=0;k<3;k++){
  box3(x-9,y+.34f,z+k*3.9f,.80f,.65f,.75f,0,
    RGB(45,151,145),RGB(52,159,156),RGB(68,184,165));
 }

}
static void depot(float bx,float bz){
 float x=bx+114,z=bz+135;
 if(distance(x,z)>540)return;
 building(x+3,z+40,64,36,16,RGB(175,184,181));
 building(x+51,z-21,25,35,10,RGB(200,177,145));
 for(int k=0;k<5;k++){
  float yy=rh_height(x-36+k*11,z+25);
  box3(x-36+k*11,yy,z+25,1.0f,1.0f,10,0,RGB(118,102,75),RGB(128,111,91),RGB(136,117,94));
 }
 float y=rh_height(x-27,z-10);
 for(int k=0;k<4;k++)box3(x-27,y+k*1.1f,z-10,5,1,5,0,RGB(169,115,69),RGB(146,92,52),RGB(185,138,88));
 // Container stacks, pallet cages, two docks and loading-bay markings.
 for(int row=0;row<2;row++)for(int k=0;k<3;k++){
  float bx=x-58+k*12.0f,bz=z+49+row*7.0f;
  float by=rh_height(bx,bz)+.12f;
  box3(bx,by,bz,9,2.8f,5.8f,0,
    (k+row)&1?RGB(174,84,66):RGB(64,113,137),
    (k+row)&1?RGB(126,61,54):RGB(54,94,111),
    RGB(163,170,165));
  for(int stripe=-3;stripe<=3;stripe++)
   box3(bx+stripe*1.12f,by+1.2f,bz+2.99f,.12f,2.1f,.05f,0,
      RGB(201,202,186),RGB(180,185,183),RGB(214,219,200));
 }
 for(int dock=-1;dock<=1;dock++)
  box3(x+dock*13.0f,rh_height(x,z+57)+.1f,z+57,7.8f,3.5f,.18f,0,
   RGB(45,57,61),RGB(44,53,56),RGB(72,80,83));
 // Loading marker (raised yellow platform).
 box3(x,rh_height(x,z)+.08f,z,12,.15f,12,0,RGB(234,189,66),RGB(235,185,56),RGB(255,207,79));
}
static void structures(void){
 float sx=RH_ROAD_SPACING;
 for(int ix=(int)floorf(cx/sx+.5f)-1;ix<=(int)floorf(cx/sx+.5f)+1;ix++)
  for(int iz=(int)floorf(cz/sx+.5f)-1;iz<=(int)floorf(cz/sx+.5f)+1;iz++){
   float bx=ix*sx,bz=iz*sx;
   if(distance(bx,bz)>610)continue;
   depot(bx,bz);station(bx,bz);
   for(int k=-2;k<=2;k++){
    float offset=k*98.0f;
    lamppost(bx+10.3f,bz+offset,RH_PI/2);
    if((k&1)==0)lamppost(bx+offset,bz-10.3f,RH_PI);
   }
   highway_sign(bx+10.7f,bz+290.0f,RH_PI/2,(int)cell_hash(ix,iz));
   // Repeatable settlements close to road junctions, blocky low-poly style.
   unsigned seed=cell_hash(ix,iz);
   for(int k=0;k<10;k++){
    float xx=bx+(k%5-2)*57.0f,zz=bz+(k/5? -235.0f:245.0f);
    if(rh_road_distance(xx,zz)<17)continue;
    float h=7+((seed>>(k&15))&7);
    building(xx,zz,28,29,h,(k&1)?RGB(199,184,159):RGB(159,172,165));
   }
  }
 int px=(int)floorf(cx/57.0f),pz=(int)floorf(cz/57.0f);
 for(int x=px-8;x<=px+8;x++)for(int z=pz-8;z<=pz+8;z++){
  unsigned seed=cell_hash(x,z);
  if((seed&3u)>1u)continue;
  float xx=(x+.2f+(seed&15)/40.0f)*57.0f;
  float zz=(z+.17f+((seed>>8)&15)/38.0f)*57.0f;
  tree(xx,zz,seed);
 }
}
/* Radial wheel profile (10-sided) with a separate aluminium rim, much less
   blocky than the v0.1 cube-wheel placeholder. */
static void wheel(float x,float y,float z,float heading,float lx,float lz,float r,float width){
 float c=cosf(heading),s=sinf(heading);
 float wcx=x+lx*c+lz*s,wcz=z-lx*s+lz*c;
 for(int side=-1;side<=1;side+=2){
  float ox=side*width*.5f;
  for(int i=0;i<10;i++){
   float a=i*2*RH_PI/10,b=(i+1)*2*RH_PI/10;
   float y1=y+sinf(a)*r,y2=y+sinf(b)*r;
   float z1=cosf(a)*r,z2=cosf(b)*r;
   float xa=wcx+(ox*c+z1*s),za=wcz+(-ox*s+z1*c);
   float xb=wcx+(ox*c+z2*s),zb=wcz+(-ox*s+z2*c);
   float xo=wcx+ox*c,zo=wcz-ox*s;
   mtri(xa,y1,za,xb,y2,zb,xo,y,zo,RGB(31,34,36));
   float rim=.49f,ry1=y+sinf(a)*r*rim,ry2=y+sinf(b)*r*rim;
   float rz1=cosf(a)*r*rim,rz2=cosf(b)*r*rim;
   mtri(wcx+(ox*c+rz1*s),ry1,wcz+(-ox*s+rz1*c),
        wcx+(ox*c+rz2*s),ry2,wcz+(-ox*s+rz2*c),
        xo,y,zo,RGB(163,174,172));
   if(side==1){
    float bx=wcx+(-ox*c+z1*s),bz=wcz-(-ox*s-z1*c);
    float ex=wcx+(-ox*c+z2*s),ez=wcz-(-ox*s-z2*c);
    quad(xa,y1,za,xb,y2,zb,ex,y2,ez,bx,y1,bz,RGB(36,39,42));
   }
  }
 }
}
static void draw_wheels(float x,float y,float z,float yaw,float offset){
 for(int k=0;k<3;k++)for(int side=-1;side<=1;side+=2){
  wheel(x,y+.60f,z,yaw,side*(1.27f+offset),-2.25f+k*2.08f,.60f,.34f);
 }
}
static void draw_truck(float x,float z,float heading,int model,int trailer,float trailer_heading){
 if(distance(x,z)>475)return;
 float y=rh_height(x,z)+.10f;
 uint32_t body=model==0?RGB(45,131,177):(model==1?RGB(184,51,48):RGB(210,210,214));
 uint32_t dark=model==0?RGB(29,91,127):(model==1?RGB(124,34,34):RGB(149,149,157));
 local_box(x,y,z,0,.94f,-1.2f,heading,2.45f,.39f,6.0f,RGB(45,45,48),RGB(52,52,54),RGB(64,67,70));
 local_box(x,y,z,0,1.10f,1.14f,heading,2.54f,2.7f,3.25f,body,dark,body);
 // Angled windshield, a real sloped surface rather than a simple cuboid.
 local_quad(x,y,z,heading,-1.07f,1.95f,3.03f,1.07f,1.95f,3.03f,
  1.00f,3.21f,2.47f,-1.00f,3.21f,2.47f,RGB(70,117,141));
 local_quad(x,y,z,heading,-.035f,1.94f,3.05f,.035f,1.94f,3.05f,
  .035f,3.21f,2.49f,-.035f,3.21f,2.49f,RGB(41,73,91));
 local_box(x,y,z,0,1.10f,2.91f,heading,2.40f,.52f,.18f,
     RGB(52,54,57),RGB(52,54,57),RGB(70,72,75));
 local_box(x,y,z,-1.30f,2.0f,2.25f,heading,.46f,.40f,.27f,RGB(42,43,47),RGB(40,42,46),BLACK);
 local_box(x,y,z,1.30f,2.0f,2.25f,heading,.46f,.40f,.27f,RGB(42,43,47),RGB(40,42,46),BLACK);
 for(int sign=-1;sign<=1;sign+=2)
  local_box(x,y,z,sign*1.03f,1.30f,3.04f,heading,.40f,.22f,.11f,
   RGB(255,240,199),RGB(255,240,199),RGB(252,250,219));
 // Front fascia: grille slats, wind deflector, side windows, mirrors,
 // marker lamps and painted details. Colours correspond to the selected rig.
 local_box(x,y,z,0,3.83f,1.16f,heading,2.20f,.27f,2.20f,
   dark,dark,body);
 for(int k=0;k<4;k++)local_box(x,y,z,0,1.15f+k*.20f,3.052f,heading,
   1.44f,.07f,.075f,RGB(137,151,158),RGB(115,126,132),RGB(172,178,178));
 for(int side=-1;side<=1;side+=2){
  local_box(x,y,z,side*1.30f,2.49f,1.13f,heading,.07f,.85f,1.18f,
   RGB(56,97,119),RGB(40,70,89),RGB(77,123,143));
  local_box(x,y,z,side*1.39f,2.02f,2.62f,heading,.40f,.10f,.32f,
   RGB(124,130,136),RGB(109,112,120),RGB(154,156,158));
  local_box(x,y,z,side*.95f,3.74f,3.04f,heading,.21f,.12f,.15f,
   RGB(242,199,105),RGB(245,202,105),RGB(243,208,112));
  local_box(x,y,z,side*1.07f,1.25f,-3.17f,heading,.30f,.18f,.14f,
   RGB(214,55,43),RGB(192,47,38),RGB(244,77,55));
 }
 draw_wheels(x,y,z,heading,0);
 if(trailer){
  float s=sinf(heading),c=cosf(heading),tx=x-s*9.2f,tz=z-c*9.2f;
  float ty=rh_height(tx,tz)+.76f;
  uint32_t cargo=model==2?RGB(166,170,166):RGB(219,223,210);
  local_box(tx,ty,tz,0,1.25f,0,trailer_heading,2.62f,2.63f,12.8f,
   cargo,RGB(185,194,190),RGB(236,235,228));
  for(int k=0;k<3;k++)for(int side=-1;side<=1;side+=2)
   wheel(tx,ty-.12f,tz,trailer_heading,side*1.3f,-4.0f+k*1.52f,.58f,.33f);
  local_box(tx,ty,tz,0,1.6f,-6.48f,trailer_heading,2.3f,.13f,.05f,
     RGB(233,53,44),RGB(228,56,52),RGB(225,53,44));
 }
}
static void traffic(const Game *g){
 for(int i=0;i<g->traffic_count;i++){
  const Traffic *t=&g->cars[i];
  if(distance(t->x,t->z)>470)continue;
  float y=rh_height(t->x,t->z)+.13f;
  uint32_t cc=t->variant==0?RGB(76,94,152):t->variant==1?RGB(189,180,164):
    t->variant==2?RGB(132,47,51):RGB(46,106,89);
  if(t->variant==3){draw_truck(t->x,t->z,t->yaw,0,0,t->yaw);continue;}
  local_box(t->x,y,t->z,0,.53f,0,t->yaw,1.95f,1.0f,4.1f,cc,cc,cc);
  local_box(t->x,y,t->z,0,1.35f,-.12f,t->yaw,1.71f,.7f,2.3f,
    RGB(79,109,130),RGB(70,94,115),RGB(86,116,131));
  for(int zz=-1;zz<=1;zz+=2)for(int ss=-1;ss<=1;ss+=2)
   local_box(t->x,y,t->z,ss*.92f,.22f,zz*1.32f,t->yaw,.28f,.5f,.56f,
   RGB(30,31,35),RGB(27,28,29),RGB(42,44,45));
 }
}
static void tri2d(float x1,float y1,float x2,float y2,float x3,float y3,uint32_t c){
 if(nover>MAX_2D-3)return;
 put(overlay,&nover,MAX_2D,x1,y1,0,c);put(overlay,&nover,MAX_2D,x2,y2,0,c);
 put(overlay,&nover,MAX_2D,x3,y3,0,c);
}
static void rect(float x,float y,float w,float h,uint32_t c){
 tri2d(x,y,x+w,y,x+w,y+h,c);tri2d(x,y,x+w,y+h,x,y+h,c);
}
// Decode UTF-8 Russian text without PSP OS fonts or external font files.
static unsigned int rh_utf8_next(const unsigned char **text){
 unsigned int cp=*(*text)++;
 if(cp<0x80u)return cp;
 if(cp>=0xC2u&&cp<=0xDFu && **text>=0x80u && **text<=0xBFu){
  cp=((cp&0x1Fu)<<6)|(**text&0x3Fu);(*text)++;return cp;
 }
 // Unsupported or truncated sequence: replace with a visible question mark.
 while(**text>=0x80u&&**text<=0xBFu)(*text)++;
 return '?';
}
static const unsigned char *rh_font_glyph(unsigned int cp){
 if(cp>=32u&&cp<=127u)return rh_font[cp-32];
 if(cp>=0x0410u&&cp<=0x042Fu)return rh_font_ru[cp-0x0410u];
 if(cp==0x0401u)return rh_font_ru[32];
 if(cp>=0x0430u&&cp<=0x044Fu)return rh_font_ru[33+cp-0x0430u];
 if(cp==0x0451u)return rh_font_ru[65];
 return rh_font['?'-32];
}
static void label(float x,float y,const char *txt,float scale,uint32_t color){
 const unsigned char *p=(const unsigned char *)txt;
 while(*p){
  unsigned int cp=rh_utf8_next(&p);
  const unsigned char *rows=rh_font_glyph(cp);
  // One rectangle per connected horizontal run reduces the PSP HUD vertex load.
  for(int iy=0;iy<9;iy++){
   int ix=0;
   while(ix<7){
    if(!(rows[iy]&(1u<<(6-ix)))){ix++;continue;}
    int left=ix++;
    while(ix<7&&(rows[iy]&(1u<<(6-ix))))ix++;
    rect(x+left*scale,y+iy*scale,(ix-left)*scale,scale,color);
   }
  }
  x+=8*scale;
 }
}

static void minimap(const Game *g){
 const float mapx=355,mapy=7,mapw=116,maph=101,scale=.095f;
 rect(mapx,mapy,mapw,maph,RGB(16,34,43));
 rect(mapx+2,mapy+2,mapw-4,maph-4,RGB(53,91,83));
 float px=g->player.x,pz=g->player.z;
 for(int i=-2;i<=2;i++){
  float lineX=RH_ROAD_SPACING*(floorf(px/RH_ROAD_SPACING+.5f)+i);
  float lineZ=RH_ROAD_SPACING*(floorf(pz/RH_ROAD_SPACING+.5f)+i);
  float screenx=mapx+58+(lineX-px)*scale;
  float screenz=mapy+48-(lineZ-pz)*scale;
  if(screenx>mapx+3&&screenx<mapx+mapw-3)rect(screenx-1,mapy+3,2,maph-6,RGB(161,168,147));
  if(screenz>mapy+3&&screenz<mapy+maph-3)rect(mapx+3,screenz-1,mapw-6,2,RGB(161,168,147));
 }
 float x,z;rh_waypoint(g,&x,&z);
 float tx=mapx+58+(x-px)*scale,tz=mapy+48-(z-pz)*scale;
 if(tx<mapx+5)tx=mapx+5;
 if(tx>mapx+mapw-6)tx=mapx+mapw-6;
 if(tz<mapy+5)tz=mapy+5;
 if(tz>mapy+maph-6)tz=mapy+maph-6;
 rect(tx-2,tz-2,5,5,RGB(255,204,94));
 rect(mapx+55,mapy+45,6,6,RGB(252,252,251));
 label(mapx+7,mapy+84,"КАРТА",1,WHITE);
}
static void cockpit(const Game *g){
 if(!g->player.camera)return;
 // Hand-built 2D cabin overlay; matches LHD driver's seat.
 rect(0,0,10,205,RGB(30,34,35));
 rect(468,0,12,205,RGB(30,34,35));
 rect(0,185,480,87,RGB(26,31,35));
 rect(0,182,480,12,RGB(48,51,52));
 rect(10,196,460,4,RGB(61,66,68));
 rect(286,197,140,35,RGB(41,45,47));
 rect(306,204,98,17,RGB(58,70,72));
 rect(339,210,29,5,RGB(166,173,159));
 // Solid steering wheel ring, with central badge and three spokes.
 const float mx=136,my=221,r0=29,r1=23;
 for(int i=0;i<24;i++){
  float a=i*RH_PI/12,b=(i+1)*RH_PI/12;
  float c1=cosf(a),s1=sinf(a),c2=cosf(b),s2=sinf(b);
  tri2d(mx+r0*c1,my+r0*s1,mx+r0*c2,my+r0*s2,mx+r1*c2,my+r1*s2,RGB(20,23,25));
  tri2d(mx+r0*c1,my+r0*s1,mx+r1*c2,my+r1*s2,mx+r1*c1,my+r1*s1,RGB(20,23,25));
 }
 rect(116,217,40,7,RGB(30,35,36));
 rect(133,214,6,29,RGB(30,35,36));
 rect(128,213,16,14,RGB(61,66,66));
}
static void hud(const Game *g){
 char buf[160];const Player *p=&g->player;
 if(g->paused){
  rect(35,35,410,202,RGB(22,34,39));
  rect(35,35,410,29,RGB(35,84,95));
  label(51,44,"ДАЛЬНИЙ РЕЙС 1.0 : ПАУЗА",1.30f,WHITE);
  label(52,77,"СТИК : РУЛЬ",1.20f,WHITE);
  label(52,98,"КРЕСТ : ГАЗ   КВАДРАТ : ТОРМОЗ",1.02f,WHITE);
  label(52,119,"КРУГ : ДЕЙСТВИЕ   ТРЕУГ. : ВИД",1.02f,WHITE);
  label(52,140,"ВВЕРХ : ГАРАЖ   ВНИЗ : РУЧНИК",1.02f,WHITE);
  label(52,161,"ЛЕВ./ПРАВ. : ОБЗОР",1.02f,WHITE);
  label(52,182,"СЕЛЕКТ : ЗАКАЗ",1.06f,WHITE);
  label(52,207,"СТАРТ : ПРОДОЛЖИТЬ",1.19f,RGB(247,217,151));
  return;
 }
 if(g->map_open){
  rect(44,45,391,183,RGB(20,39,45));
  rect(44,45,391,26,RGB(36,88,100));
  label(60,53,"ЗАКАЗ НА ПЕРЕВОЗКУ",1.40f,WHITE);
  snprintf(buf,sizeof buf,"ПОГРУЗКА: %d, %d",g->job.pickup_x,g->job.pickup_z);
  label(64,86,buf,1.13f,WHITE);
  snprintf(buf,sizeof buf,"ВЫГРУЗКА: %d, %d",g->job.dest_x,g->job.dest_z);
  label(64,109,buf,1.13f,WHITE);
  snprintf(buf,sizeof buf,"МАРШРУТ: %.1f КМ   ГРУЗ: %d Т",g->job.distance_km,g->job.cargo_tons);
  label(64,134,buf,1.06f,RGB(246,216,143));
  snprintf(buf,sizeof buf,"ОПЛАТА: %.0f РУБ.  РЕЙСОВ: %d",g->job.payout,g->completed);
  label(64,158,buf,1.04f,WHITE);
  label(64,195,"СЕЛЕКТ : ВЕРНУТЬСЯ К ДОРОГЕ",1.12f,RGB(248,215,142));
  return;
 }
 cockpit(g);
 rect(7,7,310,43,RGB(21,30,36));
 label(14,13,"ДАЛЬНИЙ РЕЙС 1.0",1.16f,RGB(239,212,145));
 snprintf(buf,sizeof buf,"%s   %.0f Р.   %02d:%02d",RH_TRUCKS[p->model].name,g->bank,(int)g->clock_hours,(int)(g->clock_hours*60)%60);
 label(14,30,buf,1.04f,WHITE);
 if(sinf(g->sim_t/210.0f+1.3f)>.45f){
  rect(270,12,40,13,RGB(46,73,89));label(275,14,"ДОЖДЬ",.77f,RGB(177,213,229));
 }
 rect(7,218,304,47,RGB(20,30,36));
 snprintf(buf,sizeof buf,"%03d КМ/Ч   ПЕР. %d   %04d ОБ/МИН",(int)(p->speed_abs*3.6f),p->gear,(int)p->rpm);
 label(14,224,buf,.99f,WHITE);
 snprintf(buf,sizeof buf,"ТОПЛ. %3.0f Л  ПРОБЕГ %.1f КМ  ИЗНОС %d%%",p->fuel,p->odo,(int)p->damage);
 label(14,245,buf,.82f,RGB(188,210,205));
 minimap(g);
 rect(314,218,159,47,RGB(17,30,38));
 label(320,224,g->job.stage?"ДОСТАВЬ ГРУЗ":"ЗАБЕРИ ГРУЗ",.98f,RGB(247,213,128));
 snprintf(buf,sizeof buf,"%.1f КМ  %d Т",rh_job_target_distance(g)/1000.0f,g->job.cargo_tons);
 label(320,239,buf,1.00f,WHITE);
 float tx,tz;rh_waypoint(g,&tx,&tz);
 float a=atan2f(tx-p->x,tz-p->z)-p->yaw;
 while(a>RH_PI)a-=2*RH_PI;
 while(a< -RH_PI)a+=2*RH_PI;
 const char *dir=(a>0.35f)?"ПОВЕРНИ ВПРАВО":(a<-.35f)?"ПОВЕРНИ ВЛЕВО":"ЕДЬ ПРЯМО";
 label(320,252,dir,.88f,RGB(240,214,142));
 int ix=(int)floorf(p->x/RH_ROAD_SPACING+.5f);
 int iz=(int)floorf(p->z/RH_ROAD_SPACING+.5f);
 rh_station(ix,iz,&tx,&tz);
 if(rh_dist2(p->x,p->z,tx,tz)<27.0f && p->speed_abs<2.0f){
  rect(102,178,287,24,RGB(21,51,52));
  label(115,185,"КРУГ : ЗАПРАВИТЬСЯ",1.22f,WHITE);
 }
 rh_depot(g->job.stage?g->job.dest_x:g->job.pickup_x,
 g->job.stage?g->job.dest_z:g->job.pickup_z,&tx,&tz);
 if(rh_dist2(p->x,p->z,tx,tz)<32.0f && p->speed_abs<1.4f){
  rect(102,178,287,24,RGB(21,51,52));
  label(115,185,g->job.stage?"КРУГ : ВЫГРУЗИТЬ":"КРУГ : ПОГРУЗИТЬ",1.22f,WHITE);
 }
 if(g->notice_ticks){
  static const char *msg[]={"","ГРУЗ ЗАГРУЖЕН","РЕЙС ВЫПОЛНЕН","ЗАПРАВКА ОПЛАЧЕНА","ТЯГАЧ СМЕНЁН","НЕ ХВАТАЕТ ДЕНЕГ","ТЯГАЧ ОТРЕМОНТИРОВАН"};
  rect(82,78,317,25,RGB(19,45,45));
  int idx=g->last_notice;
  if(idx<0||idx>=7)idx=0;
  label(96,85,msg[idx],1.14f,RGB(248,227,158));
 }
}

void rh_render_init(void){
 sceGuInit();sceGuStart(GU_DIRECT,command_list);
 sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
 sceGuDispBuffer(SW,SH,(void*)VRAM_FRAME,BUF_WIDTH);
 sceGuDepthBuffer((void*)(VRAM_FRAME*2),BUF_WIDTH);
 sceGuOffset(2048-SW/2,2048-SH/2);sceGuViewport(2048,2048,SW,SH);
 sceGuDepthRange(0xc350,0x2710);sceGuScissor(0,0,SW,SH);
 sceGuEnable(GU_SCISSOR_TEST);sceGuEnable(GU_DEPTH_TEST);
 sceGuDepthFunc(GU_GEQUAL);sceGuDisable(GU_CULL_FACE);sceGuEnable(GU_CLIP_PLANES);
 sceGuShadeModel(GU_SMOOTH);sceGuDisable(GU_TEXTURE_2D);
 sceGuFinish();sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);sceGuDisplay(GU_TRUE);
}
void rh_render_frame(const Game *g,int looking){
 const Player *p=&g->player;cx=p->x;cz=p->z;player_ground=rh_height(cx,cz);
 nmesh=0;nover=0;
 float sun=sinf((g->clock_hours-6.0f)*(RH_PI/12.0f));
 if(sun<0)sun=0;
 sunlight=0.39f+.61f*sun;
 terrain();roads();structures();traffic(g);
 if(p->camera==0)draw_truck(cx,cz,p->yaw,p->model,1,p->trailer_yaw);
 hud(g);
 sceKernelDcacheWritebackRange(mesh,nmesh*sizeof(Vertex));
 sceKernelDcacheWritebackRange(overlay,nover*sizeof(Vertex));
 sceGuStart(GU_DIRECT,command_list);
 int day=(int)(sunlight*190.0f);
 sceGuClearColor(RGB(15+day*124/190,24+day*161/190,52+day*160/190));sceGuClearDepth(0);
 sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
 sceGumMatrixMode(GU_PROJECTION);sceGumLoadIdentity();
 sceGumPerspective(67.0f,(float)SW/(float)SH,.5f,700.0f);
 sceGumMatrixMode(GU_VIEW);sceGumLoadIdentity();
 float yaw=p->yaw+looking*.75f,fs=sinf(yaw),fc=cosf(yaw);
 ScePspFVector3 eye,target,up={0,1,0};
 if(p->camera){
  eye.x=-cosf(yaw)*.74f;eye.y=3.18f;eye.z=sinf(yaw)*.74f;
  target.x=eye.x+fs*70;target.y=eye.y-.42f;target.z=eye.z+fc*70;
 }else{
  eye.x=-fs*17;eye.y=7.3f;eye.z=-fc*17;
  target.x=fs*14;target.y=2.4f;target.z=fc*14;
 }
 sceGumLookAt(&eye,&target,&up);
 sceGumMatrixMode(GU_MODEL);sceGumLoadIdentity();
 sceGumDrawArray(GU_TRIANGLES,VERTEX_TYPE,nmesh,0,mesh);
 sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_CLIP_PLANES);
 sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,nover,0,overlay);
 sceGuEnable(GU_DEPTH_TEST);sceGuEnable(GU_CLIP_PLANES);
 sceGuFinish();sceGuSync(GU_SYNC_FINISH,GU_SYNC_WHAT_DONE);
 sceDisplayWaitVblankStart();sceGuSwapBuffers();
}
void rh_render_end(void){sceGuTerm();}
