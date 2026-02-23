#include "hud.h"

void HUD::Draw(const Body& playerBody, int screenW, int screenH) const {
    DrawRectangle(5, 5, 330, 60, Fade(SKYBLUE, 0.5f));
    DrawRectangleLines(5, 5, 330, 60, BLUE);
    DrawText("WASD / Left Stick: Move | Mouse / Right Stick: Look", 15, 15, 10, BLACK);
    DrawText("Space / A Button: Jump | L-Ctrl / LB: Crouch", 15, 30, 10, BLACK);
    DrawText(TextFormat("Speed: %06.3f",
        Vector2Length({playerBody.velocity.x, playerBody.velocity.z})),
        15, 45, 10, BLACK);
    DrawFPS(screenW - 90, 10);
}