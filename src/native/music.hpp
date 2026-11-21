#pragma once

#include <filesystem>
#include <memory>

class Music {
    struct Playback;
    std::unique_ptr<Playback> playback;
    bool silent = false;

public:
    enum class Cue { Stone, Sand, Miss, Hit, Hurt, Click, Tap, Count };
    Music(const std::filesystem::path& originalTheme, const std::filesystem::path& menuTheme);
    ~Music();
    bool available() const { return static_cast<bool>(playback); }
    bool muted() const { return silent; }
    void toggle();
    void setExpedition(bool active);
    void play(Cue cue);
    void playUltimate(std::size_t weapon);
};
