// Status/cmd files for Banana Lobby when wifi.lobbyctl_dir is set (headless room daemon).
#pragma once

#include <string>

namespace bananads {

class Config;
class Emu;

void lobbyctl_init(const Config& cfg);
void lobbyctl_sync(const Emu& emu);
void lobbyctl_apply(Emu& emu);
void lobbyctl_tick(Emu& emu);
// The headless room and a game are the same process. play <rom> asks it to show that game;
// stop asks it to leave the session and exit. front is "room" or "game".
void lobbyctl_set_front(const char* front);
bool lobbyctl_has_play();
std::string lobbyctl_take_play();
bool lobbyctl_stop_requested();
void lobbyctl_note_played();

}  // namespace bananads