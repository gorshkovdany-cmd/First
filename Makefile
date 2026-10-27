TARGET = RoadHaul
OBJS = src/main_psp.o src/render_psp.o src/sim.o src/sound_psp.o
CFLAGS = -O2 -G0 -Wall -Wextra -ffast-math -fno-strict-aliasing
CXXFLAGS = $(CFLAGS)
LIBS = -lpspgum -lpspgu -lpspge -lpspdisplay -lpspctrl -lpsppower -lpspaudio -lm
BUILD_PRX = 1
PSP_EBOOT_TITLE = ДАЛЬНИЙ РЕЙС 1.0
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_ICON = ICON0.PNG
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
