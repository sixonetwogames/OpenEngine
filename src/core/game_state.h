#pragma once

enum class GamePhase { LOADING, MENU, PLAYING, PAUSED, GAMEOVER };

class GameState {
public:
    void SetPhase(GamePhase phase);
    GamePhase GetPhase() const;
    bool IsPlaying() const;
    void TogglePause();
    float GetPlayTime() const;
    void Update();

private:
    GamePhase phase    = GamePhase::LOADING;
    float     playTime = 0.0f;
};
