#pragma once

class AudioSystem {
public:
    void Init();
    void Shutdown();
    void PlaySFX(const char* name, float pitchVariance = 0.0f);
    void PlayMusic(const char* path);
    void StopMusic();
    void Update(); // streaming tick
};
