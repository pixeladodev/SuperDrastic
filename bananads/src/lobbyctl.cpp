// Banana Lobby reads status, writes one-line commands (host 2, join, join 10.0.0.1, off).
#include "lobbyctl.h"

#include "config.h"
#include "emu.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <ctime>
#include <sys/stat.h>

#ifdef BANANADS_LAN

namespace bananads {

static std::string g_dir;
static std::string g_play;
static std::string g_front = "room";
static bool g_stop = false;
static int g_played = 0;

static const char *mode_str(Emu::LanMode m) {
    switch (m) {
    case Emu::LanMode::Host: return "host";
    case Emu::LanMode::Join: return "join";
    case Emu::LanMode::Auto: return "auto";
    default: return "off";
    }
}

static const char *state_str(Emu::LanState s) {
    switch (s) {
    case Emu::LanState::Busy: return "busy";
    case Emu::LanState::Ready: return "ready";
    case Emu::LanState::Failed: return "failed";
    default: return "idle";
    }
}

static void write_atomic(const std::string& path, const std::string& body) {
    const std::string tmp = path + ".new";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return;
    fwrite(body.data(), 1, body.size(), f);
    fclose(f);
    rename(tmp.c_str(), path.c_str());
}

void lobbyctl_init(const Config& cfg) {
    g_dir = cfg.str("wifi.lobbyctl_dir", "");
    if (g_dir.empty()) return;
    mkdir(g_dir.c_str(), 0777);  // NOLINT
}

void lobbyctl_sync(const Emu& emu) {
    if (g_dir.empty()) return;
    const Emu::LanStatus l = emu.lan_status();
    if (!l.built) return;
    std::string out;
    char line[256];
    snprintf(line, sizeof line, "mode=%s\nstate=%s\nmax=%d\nhost_players=%d\nfront=%s\nplayed=%d\n",
             mode_str(l.mode), state_str(l.state), l.max_players, l.host_players, g_front.c_str(), g_played);
    out += line;
    if (!l.addr.empty()) {
        snprintf(line, sizeof line, "addr=%s\n", l.addr.c_str());
        out += line;
    }
    if (!l.msg.empty()) {
        snprintf(line, sizeof line, "msg=%s\n", l.msg.c_str());
        out += line;
    }
    snprintf(line, sizeof line, "nplayers=%zu\n",
             l.mode == Emu::LanMode::Host && l.state == Emu::LanState::Ready && l.players.empty() ? 1
                                                                                                 : l.players.size());
    out += line;
    if (l.mode == Emu::LanMode::Host && l.state == Emu::LanState::Ready && l.players.empty()) {
        const std::string nick = emu.cfg().str("firmware.nickname", "BananaDS");
        snprintf(line, sizeof line, "player\t%s\t1\t1\t-1\tgame_host\t%s\n", nick.c_str(),
                 l.addr.empty() ? "" : l.addr.c_str());
        out += line;
    } else {
        for (size_t i = 0; i < l.players.size(); i++) {
            const auto& p = l.players[i];
            const char* st = p.host ? "game_host" : "idle";
            snprintf(line, sizeof line, "player\t%s\t%d\t%d\t%d\t%s\t%s\n", p.name.c_str(), p.host ? 1 : 0, p.me ? 1 : 0,
                     p.ping_ms, st, p.me ? l.addr.c_str() : "");
            out += line;
        }
    }
    snprintf(line, sizeof line, "nhosts=%zu\n", l.hosts.size());
    out += line;
    for (size_t i = 0; i < l.hosts.size(); i++) {
        const auto& h = l.hosts[i];
        snprintf(line, sizeof line, "host\t%s\t%s\t%d\t%d\n", h.name.c_str(), h.addr.c_str(), h.players, h.max);
        out += line;
    }
    write_atomic(g_dir + "/status", out);
}

void lobbyctl_tick(Emu&) {
    // The room ends when Banana Lobby closes, not after a wait.
}

void lobbyctl_set_front(const char* front) {
    g_front = front && front[0] ? front : "room";
}

bool lobbyctl_has_play() { return !g_play.empty(); }

std::string lobbyctl_take_play() {
    std::string p = std::move(g_play);
    g_play.clear();
    return p;
}

bool lobbyctl_stop_requested() { return g_stop; }

void lobbyctl_note_played() { g_played++; }

void lobbyctl_apply(Emu& emu) {
    if (g_dir.empty()) return;
    const std::string path = g_dir + "/cmd";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;
    char buf[512];
    if (!fgets(buf, sizeof buf, f)) {
        fclose(f);
        return;
    }
    fclose(f);
    unlink(path.c_str());
    char* nl = strchr(buf, '\n');
    if (nl) *nl = 0;
    if (!buf[0]) return;
    if (!strcmp(buf, "off")) {
        emu.lan_request(Emu::LanMode::Off);
        return;
    }
    if (!strncmp(buf, "host", 4)) {
        int max = 4;
        sscanf(buf + 4, "%d", &max);
        if (max < 2) max = 2;
        if (max > 16) max = 16;
        emu.lan_request(Emu::LanMode::Host, "", max);
        return;
    }
    if (!strcmp(buf, "join")) {
        emu.lan_request(Emu::LanMode::Join, "", 0);
        return;
    }
    if (!strncmp(buf, "join ", 5)) {
        emu.lan_request(Emu::LanMode::Join, buf + 5, 0);
        return;
    }
    if (!strcmp(buf, "stop")) {
        g_stop = true;
        emu.lan_request(Emu::LanMode::Off);
        return;
    }
    if (!strncmp(buf, "play ", 5) && buf[5]) {
        g_play = buf + 5;
        return;
    }
}

}  // namespace bananads

#else

namespace bananads {
void lobbyctl_init(const Config&) {}
void lobbyctl_sync(const Emu&) {}
void lobbyctl_apply(Emu&) {}
void lobbyctl_tick(Emu&) {}
void lobbyctl_set_front(const char*) {}
bool lobbyctl_has_play() { return false; }
std::string lobbyctl_take_play() { return {}; }
bool lobbyctl_stop_requested() { return false; }
void lobbyctl_note_played() {}
}  // namespace bananads

#endif