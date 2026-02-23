PROJECT_NAME = dungeons
RAYLIB_PATH  = C:/raylib/raylib

CC  = g++
STD = -std=c++17

SRC = src/main.cpp \
      src/input/input_handler.cpp \
      src/player/player_controller.cpp \
      src/core/collision.cpp \
      src/core/level.cpp \
      src/rendering/world.cpp \
      src/rendering/HUD/hud.cpp \
      src/rendering/shadow_system.cpp \
      src/rendering/postprocess.cpp

OBJ = $(SRC:.cpp=.o)
DEP = $(OBJ:.o=.d)

CFLAGS  = -Wall -Wno-missing-braces -O2 $(STD) \
          -Isrc \
          -Isrc/core \
          -Isrc/input \
          -Isrc/player \
          -Isrc/audio \
          -Isrc/rendering \
          -Isrc/rendering/HUD \
          -Isrc/rendering/materials \
          -Isrc/networking \
          -Isrc/physics \
          -Isrc/ai \
          -Ivendor \
          -I$(RAYLIB_PATH)/src \
          -MMD -MP

LDFLAGS = -L$(RAYLIB_PATH)/src -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32 -static -lpthread

$(PROJECT_NAME).exe: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJ) $(DEP) $(PROJECT_NAME).exe

-include $(DEP)

.PHONY: clean