#include "raylib.h"
#include "input_handler.h"
#include "player_controller.h"
#include "collision.h"
#include "level.h"
#include "world.h"
#include "postprocess.h"
#include "HUD/hud.h"
#include "rlgl.h"

int main() {
    constexpr int SCREEN_W = 1920;
    constexpr int SCREEN_H = 1080;

    InitWindow(SCREEN_W, SCREEN_H, "3D FPS Engine");
    SetTargetFPS(60);

    InputHandler     input;
    PlayerController player;
    CollisionSystem  collision;
    Level            level;
    World            world;
    PostProcess      pp;
    HUD              hud;

    input.Init();
    player.Init({0, 0, 0});
    level.Load(collision, "assets/levels/demolevel.json");
    world.Init();
    pp.Init(SCREEN_W, SCREEN_H);

    while (!WindowShouldClose()) {
        InputState state = input.Poll();
        player.Update(state, &collision);

        Camera camera = player.GetCamera();

        // Compute fog color: medium grey + sky tint
        auto& fog = pp.GetFog();
        const auto& sky = world.GetLighting().skylightColor;
        float t = 0.25f;
        Color fogCol = {
            (unsigned char)(140 + sky.x * t * 255),
            (unsigned char)(142 + sky.y * t * 255),
            (unsigned char)(148 + sky.z * t * 255),
            255
        };
        fog.color = fogCol;

        // Push same color to sky shader via weather
        auto w = world.GetWeather();
        w.fogColor = fogCol;
        world.SetWeather(w);

        world.Update(camera);
        level.UpdateLighting(world.GetLighting(), camera.position);

        pp.CheckReload();

        pp.Begin();
            BeginMode3D(camera);
                rlDisableColorBlend();
                world.DrawSky(camera);
                level.Draw();
                rlEnableColorBlend();
                collision.DrawBillboards(camera);
            EndMode3D();
        pp.End();

            hud.Draw(player.GetBody(), SCREEN_W, SCREEN_H);
        EndDrawing();
    }

    pp.Unload();
    world.Unload();
    level.Unload();
    collision.Clear();
    CloseWindow();
    return 0;
}