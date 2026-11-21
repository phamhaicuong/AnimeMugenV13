TARGET = AnimeMugenV13
OBJS = src/main.o
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
LIBS = -lpspdebug -lpspdisplay -lpspge -lpspctrl
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Anime MUGEN V13
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
