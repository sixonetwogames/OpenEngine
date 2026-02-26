#include "raylib.h"
#include "engine_config.h"
#include "input_handler.h"
#include "player_controller.h"
#include "collision.h"
#include "level.h"
#include "world.h"
#include "postprocess.h"
#include "HUD/hud.h"
#include "rlgl.h"
#include <cstdio>

namespace EC = EngineConfig;

int main() {
    InitWindow(EC::SCREEN_W, EC::SCREEN_H, "3D FPS Engine");
    SetTargetFPS(EC::TARGET_FPS);

    InputHandler     input;
    PlayerController player;
    CollisionSystem  collision;
    Level            level;
    PostProcess      pp;
    HUD              hud;

    input.Init();
    player.Init({0, 0, 0});
    level.Load(collision, EC::DEFAULT_LEVEL);
    World::Init();
    pp.Init(EC::RENDER_W, EC::RENDER_H, EC::SCREEN_W, EC::SCREEN_H);

    while (!WindowShouldClose()) {
        InputState state = input.Poll();
        player.Update(state, &collision);

        Camera camera = player.GetCamera();

        World::Update(camera);
        level.UpdateLighting(camera.position);

        pp.CheckReload();

        pp.Begin();
            rlSetClipPlanes(World::fogNear, World::fogFar);
            BeginMode3D(camera);
                rlDisableColorBlend();
                World::DrawSky(camera);
                level.Draw();
                rlEnableColorBlend();
                collision.DrawBillboards(camera);
                level.DrawBillboards(camera);
            EndMode3D();
        pp.End();
            hud.Draw(player.GetBody(), GetScreenWidth(), GetScreenHeight());
        EndDrawing();
    }

    pp.Unload();
    World::Unload();
    level.Unload();
    collision.Clear();
    CloseWindow();
    return 0;
}