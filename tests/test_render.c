#include "../src/sim.h"
#include "../src/render_psp.h"
#include <assert.h>
#include <stdio.h>
extern int mock_frames,mock_3d,mock_2d;
int main(void){
 Game g;rh_new_game(&g);rh_render_init();
 const float samples[][2]={{0,0},{40,90},{130,130},{-110,-130},{1500,1800},
   {-1800,-1800},{4500,2700},{92000,-126000},{-500000,500000}};
 for(unsigned i=0;i<sizeof(samples)/sizeof(samples[0]);i++){
  rh_set_player(&g,samples[i][0],samples[i][1],i*.49f);
  g.player.camera=i%2;g.map_open=(i==7);g.notice_ticks=(i==8?90:0);
  mock_3d=mock_2d=0;
  rh_render_frame(&g,(int)(i%3)-1);
  assert(mock_3d>1000 && mock_3d<54001);
  assert(mock_2d>200 && mock_2d<25001);
  printf("frame %u, 3D vertices %d, HUD vertices %d\n",i,mock_3d,mock_2d);
 }
 int max3=0,max2=0;
 for(int i=0;i<128;i++){
  float x=((float)((i*1973)%40000)-20000.0f)*4.1f;
  float z=((float)((i*2671)%46000)-23000.0f)*3.7f;
  rh_set_player(&g,x,z,i*.53f);
  g.player.camera=(i&1);g.map_open=(i%11==0);g.paused=(i%17==0);
  mock_3d=mock_2d=0;
  rh_render_frame(&g,(i%3)-1);
  assert(mock_3d>1000&&mock_3d<54001);
  assert(mock_2d>200&&mock_2d<25001);
  if(mock_3d>max3)max3=mock_3d;
  if(mock_2d>max2)max2=mock_2d;
 }
 rh_render_end();printf("PASS renderer: 137 terrain/camera/weather cases; maximum vertices 3D %d / 54000, HUD %d / 25000\n",max3,max2);return 0;
}
