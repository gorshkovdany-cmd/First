#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspdisplay.h>
#include <string.h>
#include <stdio.h>
#include "sim.h"
#include "render_psp.h"
#include "sound_psp.h"

PSP_MODULE_INFO("RoadHaul", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
static volatile int running=1;
static int exit_callback(int arg1,int arg2,void *common){(void)arg1;(void)arg2;(void)common;running=0;return 0;}
static int callback_thread(SceSize args,void *argp){(void)args;(void)argp;
 int id=sceKernelCreateCallback("RoadHaul Exit",exit_callback,0);
 sceKernelRegisterExitCallback(id);sceKernelSleepThreadCB();return 0;
}
static void setup_callbacks(void){
 int id=sceKernelCreateThread("Callbacks",callback_thread,0x11,0x1000,0,0);
 if(id>=0)sceKernelStartThread(id,0,0);
}
static const char *savepath="ms0:/PSP/GAME/DALREYS/SAVE.DAT";
static const char *legacy_savepath="ms0:/PSP/GAME/ROADHAUL/SAVE.DAT";
static int try_load(Game *g,const char *path){
 if(rh_load(g,path))return 1;
 char backup[256];snprintf(backup,sizeof backup,"%s.BAK",path);
 return rh_load(g,backup);
}
int main(int argc,char **argv){
 (void)argc;(void)argv;setup_callbacks();
 scePowerSetClockFrequency(333,333,166);
 sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
 static Game game;rh_new_game(&game);
 if(!try_load(&game,savepath) && !try_load(&game,legacy_savepath))try_load(&game,"SAVE.DAT");
 rh_render_init();rh_sound_start();
 unsigned int previous=0,pending=0;unsigned long long tick=sceKernelGetSystemTimeWide();
 float accumulator=0,save_clock=0;
 while(running){
  SceCtrlData pad;sceCtrlPeekBufferPositive(&pad,1);
  unsigned int p=pad.Buttons,pressed=p&(~previous);previous=p;
  pending|=pressed;
  unsigned long long now=sceKernelGetSystemTimeWide();
  float elapsed=(float)(now-tick)/1000000.0f;tick=now;
  if(elapsed>.15f)elapsed=.15f;
  if(elapsed<0)elapsed=0;
  Controls in;memset(&in,0,sizeof(in));
  in.steer=(float)((int)pad.Lx-128)/127.0f;
  if(in.steer>-0.13f && in.steer<.13f)in.steer=0;
  in.throttle=(p&PSP_CTRL_CROSS)?1:0;
  in.brake=(p&PSP_CTRL_SQUARE)?1:0;
  in.reverse_request=(pending&PSP_CTRL_SQUARE)?1:0;
  in.handbrake=(p&PSP_CTRL_DOWN)?1:0;
  in.look=(p&PSP_CTRL_LTRIGGER?-1:0)+(p&PSP_CTRL_RTRIGGER?1:0);
  in.action=(pending&PSP_CTRL_CIRCLE)?1:0;
  in.toggle_camera=(pending&PSP_CTRL_TRIANGLE)?1:0;
  in.pause=(pending&PSP_CTRL_START)?1:0;
  in.map=(pending&PSP_CTRL_SELECT)?1:0;
  in.swap_truck=(pending&PSP_CTRL_UP)?1:0;
  // Fixed tick 60 Hz. Edge-triggered commands are applied only once.
  accumulator+=elapsed;int steps=0;
  while(accumulator>=1.0f/60.0f && steps<5){
   rh_step(&game,in,1.0f/60.0f);
   in.action=in.toggle_camera=in.pause=in.map=in.swap_truck=in.reverse_request=0;
   accumulator-=1.0f/60.0f;steps++;
  }
  if(steps>0)pending=0;
  if(steps==5)accumulator=0;
  save_clock+=elapsed;
  if(save_clock>45.0f){
   rh_save(&game,savepath);
   save_clock=0;
  }
  rh_sound_update(game.player.rpm,game.player.engine_t,game.player.speed_abs,game.player.fuel<.01f);
  rh_render_frame(&game,in.look);
 }
 rh_save(&game,savepath);
 rh_sound_stop();rh_render_end();sceKernelExitGame();return 0;
}
