COMPILER = gcc

GAME_TITLE = Sus Shooter
GAME_VERSION = "0.0.1"

PROGRAM_INFO = -DMVERSION=$(GAME_VERSION) -DMTITLE="$(GAME_TITLE)"

BUILD_OPTIONS = -g3 -Wpedantic -D_DEBUG
RELEASE_OPTIONS = -O2 -Wpedantic

SOURCE_LIBS = -Ivendor/include/

CFILES = src2/*.c

WORKSPACE = $(shell pwd)

WIN_OPT = -O2 -Lvendor/lib/win/ ./vendor/lib/win/libraylib.a -lopengl32 -lgdi32 -lwinmm
WIN_OUT = -o ".bin/build_win"

LIN_OPT = -O2 -Lvendor/lib/lin/ -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
LIN_OUT = -o ".bin/build_lin"

setup: 
	mkdir .bin


build_win:
	$(COMPILER) $(BUILD_OPTIONS) $(SOURCE_LIBS) $(CFILES) $(WIN_OUT) $(WIN_OPT)

release_win:
	$(COMPILER) $(RELEASE_OPTIONS) $(SOURCE_LIBS) $(CFILES) $(WIN_OUT) $(WIN_OPT)

build_lin:
	$(COMPILER) $(BUILD_OPTIONS) $(SOURCE_LIBS) $(CFILES) $(LIN_OUT) $(LIN_OPT)

release_lin:
	$(COMPILER) $(RELEASE_OPTIONS) $(SOURCE_LIBS) $(CFILES) $(LIN_OUT) $(LIN_OPT)


	
