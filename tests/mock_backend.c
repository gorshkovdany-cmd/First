#include "psp_mock/mock_common.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int mock_frames=0,mock_3d=0,mock_2d=0;
typedef struct {uint32_t c;float x,y,z;} MockVertex;
void sceGuDrawArray(int type,int format,int count,const void* index,const void *vertices){
 (void)type;(void)index;
 const MockVertex *v=vertices;
 if(count<0||count>55000){fprintf(stderr,"invalid batch: %d\n",count);abort();}
 if(format & GU_TRANSFORM_2D)mock_2d+=count;else mock_3d+=count;
 for(int i=0;i<count;i++){
  if(!isfinite(v[i].x)||!isfinite(v[i].y)||!isfinite(v[i].z)){
   fprintf(stderr,"NaN/Inf vertex %d\n",i);abort();
  }
 }
}
void sceGumDrawArray(int type,int format,int count,const void* index,const void *vertices){
 sceGuDrawArray(type,format,count,index,vertices);
}
#define IMPL_VOID(name,signature) void name signature {}
IMPL_VOID(sceGuInit,(void))
IMPL_VOID(sceGuStart,(int x,void*y))
IMPL_VOID(sceGuDrawBuffer,(int x,void*y,int z))
IMPL_VOID(sceGuDispBuffer,(int x,int y,void *p,int z))
IMPL_VOID(sceGuDepthBuffer,(void *p,int w))
IMPL_VOID(sceGuOffset,(int a,int b))
IMPL_VOID(sceGuViewport,(int a,int b,int c,int d))
IMPL_VOID(sceGuDepthRange,(int a,int b))
IMPL_VOID(sceGuScissor,(int a,int b,int c,int d))
IMPL_VOID(sceGuEnable,(int a))
IMPL_VOID(sceGuDisable,(int a))
IMPL_VOID(sceGuDepthFunc,(int a))
IMPL_VOID(sceGuShadeModel,(int a))
IMPL_VOID(sceGuFinish,(void))
IMPL_VOID(sceGuSync,(int a,int b))
IMPL_VOID(sceGuDisplay,(int a))
IMPL_VOID(sceGuClearColor,(unsigned a))
IMPL_VOID(sceGuClearDepth,(int a))
IMPL_VOID(sceGuClear,(int a))
IMPL_VOID(sceGuTerm,(void))
IMPL_VOID(sceGuSwapBuffers,(void))
IMPL_VOID(sceDisplayWaitVblankStart,(void))
IMPL_VOID(sceGumMatrixMode,(int a))
IMPL_VOID(sceGumLoadIdentity,(void))
IMPL_VOID(sceGumPerspective,(float a,float b,float c,float d))
IMPL_VOID(sceGumLookAt,(const ScePspFVector3 *a,const ScePspFVector3 *b,const ScePspFVector3 *c))
IMPL_VOID(sceKernelDcacheWritebackRange,(const void *p,unsigned int n))
