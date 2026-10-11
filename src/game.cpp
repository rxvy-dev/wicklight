// WICKLIGHT - game flow: progression, persistence, world objects, story
#include "game.h"
#include "platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

extern int g_dmgKind;
void collectExtra(Game& g, Pickup& p);
bool talkExtra(Game& g, int ti);
void fennLampLit(Game& g);

// ------------------------------------------------------------------ text
const char* levelName(LevelId id) {
    static const char* n[] = {"Lumen Hollow", "The Sunken Wharf", "The Clockwood", "The Ashen Vaults", "The Lighthouse Spire", "The Junkyard of Lost Machines", "The Endless Dark", "Wick's Island", "Lumen City"};
    return (id >= 0 && id < LV_COUNT) ? n[id] : "?";
}

std::string memoryText(int i) {
    static const char* M[10] = {
        "HARBOUR LOG, YEAR 31 OF THE LAMP:\nThe Lamp burns brighter every spring. Ships no longer\nneed the stars to steer by. Nobody looks up anymore.",
        "MARROW'S NOTEBOOK:\nDoubled the wick again. The whole city cheered.\nOdd - beneath the Spire the shade has grown so deep\nthat the moss there has died.",
        "A CHILD'S LETTER, NEVER SENT:\nDear Mister Lamplighter, my shadow keeps following me\neven when I stand still. It is very tall now.\nIs that normal?",
        "CLOCKMAKERS' GUILD CHARTER:\nThe Pendulum Warden shall keep time for the Great Lamp,\nso that the Lamp need never rest, nor dim,\nnor know the hour of night.",
        "MARROW'S NOTEBOOK:\nNight is merely a failure of engineering.\nGive me ten more years and I will abolish it.",
        "TOWER WATCH REGISTER:\nReports of shadows pooling beneath the Spire.\nShadows with weight. Shadows that hum.\nThe watchmen refuse the night rounds.",
        "FUNERAL RITE OF LUMEN HOLLOW:\n'Every flame casts a shadow. Love the light,\nand keep the dark as its companion. Neither\nmay ever be made to stand alone.'",
        "MARROW'S NOTEBOOK:\nIt spoke to me tonight. It said it was the shadow of\nmy Lamp - grown exactly as tall as my pride.\nIt said it was tired of being stood upon.",
        "RUSTY'S SCRAPBOOK:\nFound a lamp-lens in the scrap today. Still warm.\nThe big rat wanted it for his crown. I said no.\nHe said SQUEAK. That means war, apparently.",
        "MARROW'S LAST PAGE:\nI cannot put it out without putting out the light.\nPerhaps something small could do what I could not.\nSomething with only a little flame.",
    };
    // index 9 is the Junkyard candle (stored at position 8 above); the old last page stays index 8
    static const int ORDER[10] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 8};
    return (i >= 0 && i < 10) ? M[ORDER[i]] : "";
}

std::string actionName(int a) {
    static const char* n[A_COUNT] = {"Move forward", "Move back", "Move left", "Move right", "Jump", "Dash", "Throw / Recall", "Warp", "Bloom", "Interact", "Pause", "Journal", "Ember Grapple", "Firewall", "Overclock"};
    return (a >= 0 && a < A_COUNT) ? n[a] : "?";
}

std::string bindingName(int code) {
    if (code >= 1000) {
        int b = code - 1000;
        if (b == 1) return "LMB";
        if (b == 2) return "MMB";
        if (b == 3) return "RMB";
        return "Mouse" + std::to_string(b);
    }
    if (sdl::GetScancodeName) {
        std::string s = sdl::GetScancodeName((SDL_Scancode)code);
        if (!s.empty()) return s;
    }
    return "Key" + std::to_string(code);
}

Settings::Settings() {
    keys[A_FWD] = SDL_SCANCODE_W; keys[A_BACK] = SDL_SCANCODE_S; keys[A_LEFT] = SDL_SCANCODE_A; keys[A_RIGHT] = SDL_SCANCODE_D;
    keys[A_JUMP] = SDL_SCANCODE_SPACE; keys[A_DASH] = SDL_SCANCODE_LSHIFT; keys[A_THROW] = 1000 + 1; keys[A_WARP] = 1000 + 3;
    keys[A_BLOOM] = SDL_SCANCODE_Q; keys[A_INTERACT] = SDL_SCANCODE_E; keys[A_PAUSE] = SDL_SCANCODE_ESCAPE; keys[A_JOURNAL] = SDL_SCANCODE_TAB;
    keys[A_GRAPPLE] = SDL_SCANCODE_F; keys[A_FIREWALL] = SDL_SCANCODE_R; keys[A_OVERCLOCK] = SDL_SCANCODE_C;
}

// ------------------------------------------------------------------ paths & persistence
static void mkdirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1) mkdir(cur.c_str(), 0755);
    }
    mkdir(path.c_str(), 0755);
}

void Game::initPaths() {
    const char* home = getenv("HOME");
    std::string h = home ? home : ".";
    const char* xd = getenv("XDG_DATA_HOME");
    const char* xc = getenv("XDG_CONFIG_HOME");
    saveDir = (xd && *xd ? std::string(xd) : h + "/.local/share") + "/wicklight";
    configDir = (xc && *xc ? std::string(xc) : h + "/.config") + "/wicklight";
    if (const char* o = getenv("WICKLIGHT_DATA_DIR")) { saveDir = o; configDir = o; }
    mkdirs(saveDir);
    mkdirs(configDir);
}

static bool atomicWrite(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (fflush(f) == 0) && ok;
    fsync(fileno(f));
    fclose(f);
    if (!ok) { remove(tmp.c_str()); return false; }
    return rename(tmp.c_str(), path.c_str()) == 0;
}

bool Game::hasSave() const {
    struct stat st;
    return stat((saveDir + "/save.dat").c_str(), &st) == 0;
}

void Game::writeSave() {
    std::ostringstream o;
    o << "WICKLIGHT_SAVE 1\n";
    o << "level " << save.level << "\n";
    o << "spawn " << save.spawn << "\n";
    o << "time " << save.playTime << "\n";
    o << "deaths " << save.deaths << "\n";
    o << "embers " << save.embers << "\n";
    o << "endless " << save.endlessDepth << " " << save.endlessBest << "\n";
    o << "forge " << save.forgeTime << "\n";
    // Wick's Island
    o << "voxseed " << save.voxSeed << "\n";
    o << "day " << save.dayT << "\n";
    if (save.hasBed) o << "bed " << save.bedX << " " << save.bedY << " " << save.bedZ << "\n";
    o << "hot"; for (int i = 0; i < 9; i++) o << " " << save.hot[i]; o << "\n";
    for (auto& kv : save.inv) if (kv.second > 0) o << "inv " << kv.first << " " << kv.second << "\n";
    for (auto& c : save.chests) { o << "chest " << c.first; for (auto& kv : c.second) if (kv.second > 0) o << " " << kv.first << " " << kv.second; o << "\n"; }
    if (!vox.blk.empty() && vox.seed == save.voxSeed && save.voxSeed) {
        std::string d = vox.saveDiff();
        save.voxLines.clear();
        std::istringstream ds(d); std::string l;
        while (std::getline(ds, l)) if (!l.empty()) save.voxLines.push_back(l);
    }
    for (auto& l : save.voxLines) o << l << "\n";
    o << "hat " << save.hat << "\n" << "fish " << save.fishCaught << "\n" << "race " << save.raceBest << "\n";
    for (auto& f : save.flags) o << "flag " << f << "\n";
    o << "end\n";
    if (!atomicWrite(saveDir + "/save.dat", o.str())) {
        fprintf(stderr, "[save] could not write %s/save.dat: %s\n", saveDir.c_str(), strerror(errno));
        statusMsg = "Warning: could not write save file"; statusT = 3;
    }
}

bool Game::loadGame() {
    std::ifstream in(saveDir + "/save.dat");
    if (!in) return false;
    SaveData s;
    std::string line;
    bool header = false, ended = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string key; ls >> key;
        if (!header) { int v = 0; ls >> v; if (key != "WICKLIGHT_SAVE" || v != 1) break; header = true; continue; }
        if (key == "level") ls >> s.level;
        else if (key == "spawn") ls >> s.spawn;
        else if (key == "time") ls >> s.playTime;
        else if (key == "deaths") ls >> s.deaths;
        else if (key == "embers") ls >> s.embers;
        else if (key == "endless") { ls >> s.endlessDepth >> s.endlessBest; if (s.endlessDepth < 1) s.endlessDepth = 1; }
        else if (key == "res") { for (int& r : s.res) { ls >> r; if (r < 0) r = 0; } }
        else if (key == "forge") ls >> s.forgeTime;
        else if (key == "build") { SaveData::Placed p; if (ls >> p.type >> p.gx >> p.gz >> p.rot) s.builds.push_back(p); }
        else if (key == "voxseed") ls >> s.voxSeed;
        else if (key == "day") { ls >> s.dayT; if (!(s.dayT >= 0 && s.dayT < 1)) s.dayT = 0.05f; }
        else if (key == "bed") { if (ls >> s.bedX >> s.bedY >> s.bedZ) s.hasBed = true; }
        else if (key == "hot") { for (int i = 0; i < 9; i++) ls >> s.hot[i]; s.hot[0] = 0; }
        else if (key == "inv") { int id = 0, n = 0; if (ls >> id >> n && n > 0) s.inv[id] = n; }
        else if (key == "chest") { int k = 0; ls >> k; auto& c = s.chests[k]; int id, n; while (ls >> id >> n) if (n > 0) c[id] = n; }
        else if (key == "vx") s.voxLines.push_back(line);
        else if (key == "hat") { ls >> s.hat; if (s.hat < 0 || s.hat > 31) s.hat = 0; }
        else if (key == "fish") ls >> s.fishCaught;
        else if (key == "race") { ls >> s.raceBest; if (!(s.raceBest >= 0)) s.raceBest = 0; }
        else if (key == "flag") { std::string f; ls >> f; if (!f.empty()) s.flags.insert(f); }
        else if (key == "end") { ended = true; break; }
    }
    if (!header || !ended || s.level < 0 || s.level >= LV_COUNT || s.spawn < 0 || s.spawn > 64 || !(s.playTime >= 0)) {
        // corrupt or truncated: keep a copy for the player, start fresh
        std::string bad = saveDir + "/save.dat.corrupt";
        rename((saveDir + "/save.dat").c_str(), bad.c_str());
        fprintf(stderr, "[save] save file was invalid; moved to %s\n", bad.c_str());
        saveCorrupt = true;
        return false;
    }
    save = s;
    vox.destroyMeshes(); vox.blk.clear(); vox.active = false;
    migrateOldIsland();
    // repair: every collected heart grants its power (older saves could miss one)
    static const char* AB[3] = {"ab.warp", "ab.bloom", "ab.sunflare"};
    for (int i = 0; i < 3; i++) if (has("heart." + std::to_string(i))) setFlag(AB[i]);
    if (has("heart.4")) setFlag("spark.core");
    if (has("heart.1")) setFlag("ab.overclock");
    if (has("pw.firewall")) setFlag("ab.firewall");
    if (has("pw.grapple")) setFlag("ab.grapple");
    return true;
}

void Game::deleteSave() { remove((saveDir + "/save.dat").c_str()); }

void Game::loadSettings() {
    std::ifstream in(configDir + "/settings.cfg");
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        float f = (float)atof(v.c_str());
        int i = atoi(v.c_str());
        if (k == "master") settings.master = clampf(f, 0, 1);
        else if (k == "music") settings.music = clampf(f, 0, 1);
        else if (k == "sfx") settings.sfx = clampf(f, 0, 1);
        else if (k == "sensitivity") settings.sensitivity = clampf(f, 0.1f, 4);
        else if (k == "invert_y") settings.invertY = i != 0;
        else if (k == "pixel_mode") settings.pixelMode = std::max(0, std::min(PIXEL_MODES - 1, i));
        else if (k == "fullscreen") settings.fullscreen = i != 0;
        else if (k == "vsync") settings.vsync = i != 0;
        else if (k == "shadows") settings.shadows = std::max(0, std::min(2, i));
        else if (k == "outlines") settings.outlines = i != 0;
        else if (k == "fov") settings.fov = clampf(f, 55, 100);
        else if (k == "shake") settings.shake = i != 0;
        else if (k == "aim_assist") settings.aimAssist = i != 0;
        else if (k == "ui_scale") settings.uiScale = std::max(0, std::min(5, i));
        else if (k.rfind("key_", 0) == 0) {
            int a = atoi(k.c_str() + 4);
            if (a >= 0 && a < A_COUNT && i > 0 && i < 1010) settings.keys[a] = i;
        }
    }
}

void Game::writeSettings() {
    std::ostringstream o;
    o << "master=" << settings.master << "\nmusic=" << settings.music << "\nsfx=" << settings.sfx << "\n";
    o << "sensitivity=" << settings.sensitivity << "\ninvert_y=" << settings.invertY << "\npixel_mode=" << settings.pixelMode << "\n";
    o << "fullscreen=" << settings.fullscreen << "\nvsync=" << settings.vsync << "\nshadows=" << settings.shadows << "\n";
    o << "outlines=" << settings.outlines << "\nfov=" << settings.fov << "\nshake=" << settings.shake << "\naim_assist=" << settings.aimAssist << "\nui_scale=" << settings.uiScale << "\n";
    for (int a = 0; a < A_COUNT; a++) o << "key_" << a << "=" << settings.keys[a] << "\n";
    atomicWrite(configDir + "/settings.cfg", o.str());
}

void Game::applySettings() {
    audio::setVolumes(settings.master, settings.music, settings.sfx);
    if (R) {
        R->outlines = settings.outlines;
        R->uiScaleOverride = settings.uiScale;
        if (R->shadowQuality != settings.shadows) R->setShadowQuality(settings.shadows);
        if (R->pixelLines != PIXEL_LINES[settings.pixelMode]) R->setPixelLines(PIXEL_LINES[settings.pixelMode]);
    }
}

// ------------------------------------------------------------------ progression helpers
int Game::countFlags(const std::string& prefix) const {
    int n = 0;
    for (auto& f : save.flags) if (f.rfind(prefix, 0) == 0) n++;
    return n;
}
float Game::maxHp() const { return 5.f + (float)(countFlags("shard.") / 2) + (float)upLevel("wax"); }

// ------------------------------------------------------------------ lifecycle
void Game::init(Renderer* r, bool headlessMode) {
    R = r;
    headless = headlessMode;
    initPaths();
    loadSettings();
    applySettings();
    buildLevel(LV_HUB);
    applyFlagsToLevel();
    pl.pos = lv.spawns.empty() ? V3(0, 1, 0) : lv.spawns[0].pos;
    camYaw = 2.4f;
    screen = SC_TITLE;
    audio::setSong(SONG_TITLE);
}

void Game::newGame() {
    save = SaveData();
    vox.destroyMeshes(); vox.blk.clear(); vox.active = false;
    deleteSave();
    screen = SC_INTRO;
    startIntro();
}

void Game::continueGame() {
    if (!loadGame()) { newGame(); return; }
    enterLevel((LevelId)save.level, save.spawn, true);
    pl.hp = maxHp();
    screen = SC_PLAY;
}

void Game::resetEncounters() {
    enemies.clear(); projs.clear(); rings.clear(); teles.clear();
    resetBoss();
    for (auto& a : lv.arenas) {
        a.active = false;
        for (int g : a.gates) { lv.dyn[g].open = true; lv.dyn[g].openAmt = 1; lv.dyn[g].offset = V3(0, -(lv.dyn[g].base.size().y) - 0.05f, 0); }
    }
    for (int g : lv.boss.gates) { lv.dyn[g].open = true; lv.dyn[g].openAmt = 1; lv.dyn[g].offset = V3(0, -(lv.dyn[g].base.size().y) - 0.05f, 0); }
    for (auto& s : lv.enemySpawns) if (s.arena < 0) spawnEnemy(s.type, s.pos, -1);
    for (auto& b : lv.braziers) if (b.bossBrazier) b.lit = false;
}

void Game::enterLevel(LevelId id, int spawn, bool fromSave) {
    (void)fromSave;
    buildLevel(id);
    applyFlagsToLevel();
    resetEncounters();
    if (spawn < 0 || spawn >= (int)lv.spawns.size()) spawn = 0;
    SpawnPt sp = lv.spawns.empty() ? SpawnPt{V3(0, 1, 0), 0} : lv.spawns[spawn];
    pl = Player();
    pl.pos = sp.pos; pl.safePos = sp.pos; pl.yaw = sp.yaw;
    pl.hp = maxHp(); pl.maxHp = maxHp();
    flame = Flame(); flame.pos = pl.pos + V3(0, 1.25f, 0);
    camYaw = sp.yaw; camPitch = -0.2f;
    camTarget = pl.pos + V3(0, 1.65f, 0);
    camDistCur = camDist;
    camOverride = 0;
    bolts.clear(); pendingSpawns.clear(); floats.clear();
    firewalls.clear(); grappling = false; ocT = 0; alarmT = 0; raceIdx = -1; fishState = 0; glitch = 0;
    mining = false; tgtValid = false; bombs.clear(); sleepT = 0;
    if (id == LV_HOME) {
        collectForges();
        voxLightsDirty = true;
        if (respawnAtBed && save.hasBed && vox.get(save.bedX, save.bedY, save.bedZ) == BK_BED) {
            pl.pos = pl.safePos = V3(save.bedX + 0.5f, save.bedY + 0.6f, save.bedZ + 0.5f);
            for (int k = 0; k < 4 && !boxFree(AABB(pl.pos - V3(PLAYER_R, 0, PLAYER_R), pl.pos + V3(PLAYER_R, PLAYER_H, PLAYER_R))); k++) pl.pos.y += 1;
            pl.safePos = pl.pos; camTarget = pl.pos + V3(0, 1.65f, 0);
        }
        if (!has("isle.welcome")) {
            setFlag("isle.welcome");
            startDialog({{"", "Welcome to Wick's Island - a whole island made of blocks, and all of it yours."},
                         {"", "Hold [" + bindingName(settings.keys[A_THROW]) + "] to MINE a block. Press [" + bindingName(settings.keys[A_WARP]) + "] to PLACE the block you're holding, or to USE beds, doors and chests."},
                         {"", "Pick blocks with [1]-[9] or the mouse wheel. [" + bindingName(settings.keys[A_JOURNAL]) + "] opens your inventory and crafting."},
                         {"", "Bramble the beaver sells building supplies for embers. At night, gloomlings come out - build a house, light lamps, or sleep in a bed!"}});
        }
    }
    respawnAtBed = false;
    for (auto& b : bots) b.pos = pl.pos + V3(0, 2, 0);
    save.level = id; save.spawn = spawn;
    audio::setSong(lv.song);
    areaTitle = lv.name; areaTitleT = 3.5f;
    transitioning = false;
    lanternSaveCool = 0;
    for (auto& l : lv.lanterns) if (distXZ(l.pos, pl.pos) < 2.5f) l.cool = 1;  // don't re-trigger on spawn
    writeSave();
}

void Game::requestTransition(LevelId id, int spawn) {
    if (transitioning) return;
    transitioning = true; transitionT = 0; pendingLevel = id; pendingSpawn = spawn;
}

// ------------------------------------------------------------------ dialog & story
void Game::startDialog(const std::vector<DialogLine>& lines, std::function<void()> done) {
    dialog = lines; dialogIdx = 0; dialogChars = 0; dialogDone = done;
}

void Game::talk(int ti) {
    if (talkExtra(*this, ti)) return;
    const Talker& t = lv.talkers[ti];
    if (t.who == 0) {  // Marrow
        int hearts = countFlags("heart.");
        std::vector<DialogLine> L;
        if (has("game.finished")) {
            L = {{"MARROW", "Look at it, Wick. A small light. Just enough to read by."},
                 {"MARROW", "And there - see? Shadows on the cobbles again. Ordinary ones."},
                 {"MARROW", "Go on, little flame. The Hollow is yours to wander."}};
        } else if (has("boss.1") && !has("talk.fbi")) {
            L = {{"MARROW", "Wick! Have you SEEN the posters? 'WANTED: THE HACKER'. With YOUR face on them!"},
                 {"MARROW", "GL1TCH must have rerouted his trail to you before he went offline. The FBI is in Lumen City, hunting for 'a small, warm suspect'."},
                 {"MARROW", "You don't have to go. But Sprocket says he's built you something special, and it's waiting at the city's helipad."},
                 {"MARROW", "The road to the city is the bridge EAST of the plaza. Mind the searchlights."}};
            setFlag("talk.fbi");
        } else if (has("hub.spire")) {
            L = {{"MARROW", "The Spire door is open. I can feel the cold pouring down from it."},
                 {"MARROW", "Wick... whatever is waiting at the top, I made it. Every bright year of it."},
                 {"MARROW", "I'm sorry I'm sending something so small to fix something so big."}};
        } else if (hearts >= 4) {
            L = {{"MARROW", "All four Beacon-Hearts! I can hear them humming from here."},
                 {"MARROW", "Set them into the pedestals at the Spire door. The way up will open."}};
        } else if (hearts == 3) {
            L = {{"MARROW", "Three hearts! But the Spire wants four. The last one was stolen by... a rat."},
                 {"MARROW", "A big one. Calls himself KING RATTLETRAP. Lives in the Junkyard behind my workshop."},
                 {"MARROW", "The gate is south-west of here. And Wick - visit Sprocket's stall first. Bring friends."}};
        } else if (hearts == 2) {
            L = {{"MARROW", "Two hearts. And your flame... it blossoms now, like a struck match."},
                 {"MARROW", "East of here, my old wax seal bars the stair to the Ashen Vaults."},
                 {"MARROW", "BLOOM your flame against it. The wax will remember me and give way."},
                 {"MARROW", "Mind the dark down there. Things that hate the light have been nesting in it."}};
        } else if (hearts == 1) {
            L = {{"MARROW", "The Wharf's heart! Ha! And look at you - you stepped right into your own flame!"},
                 {"MARROW", "The north bridge to the Clockwood broke years ago. That shouldn't stop you now."},
                 {"MARROW", "Throw your flame across the gap, then WARP to it. Simple as falling."},
                 {"MARROW", "And watch out: some HACKER from across the world has wired himself into the Clockwood's machines."},
                 {"MARROW", "Calls himself GL1TCH. The Hush is paying him. Burn his servers and he'll have nowhere to hide."}};
        } else if (!has("talk.marrow0")) {
            L = {{"MARROW", "Easy... easy. You're awake. You're really awake."},
                 {"MARROW", "I am Marrow. I kept the Great Lamp up there, for forty years."},
                 {"MARROW", "Now it's dark, and the Hush is drinking the Hollow dry."},
                 {"MARROW", "Three Beacon-Hearts fed the Lamp. The Hush scattered them, and set monsters on them."},
                 {"MARROW", "Bring them back, Wick, and the Spire will open for you."},
                 {"MARROW", "Your flame is your life - and your weapon. THROW it, and call it home again."},
                 {"MARROW", "It burns whatever it passes through. Both ways."},
                 {"MARROW", "Start with the Sunken Wharf, down the west steps. Go on now."}};
            setFlag("talk.marrow0");
        } else {
            L = {{"MARROW", "The Sunken Wharf is down the west steps, past the lamp posts."},
                 {"MARROW", "Your flame is your life. Don't let the dark keep it."}};
        }
        startDialog(L);
    } else if (t.who >= 3) {  // the people of the Hollow
        std::string key = "npc." + std::to_string(t.who);
        bool first = !has(key);
        int gift = 0;
        std::vector<DialogLine> L;
        switch (t.who) {
            case 3:  // Sprocket the bot-maker
                if (first) { L = {{"SPROCKET", "BEEP. A customer! Hello, small candle person."},
                                  {"SPROCKET", "I am Sprocket. I build ROBOT FRIENDS. They follow you and help you fight."},
                                  {"SPROCKET", "One shoots fireballs. One vacuums embers. One heals you. One blocks hits. One throws BOMBS."},
                                  {"SPROCKET", "I accept payment in embers. Smash things. Chain combos. Get rich. BEEP."}}; gift = 20; }
                else L = {{"SPROCKET", "BEEP BOOP. Let's shop."}};
                break;
            case 4:  // Gil the fisher
                if (first) { L = {{"GIL", "Hoo, a walking candle! Haven't seen a light out here in weeks."},
                                  {"GIL", "There's a great barnacled BELL at the end of the docks. Rings so loud my teeth hum."},
                                  {"GIL", "When it rings, it TILTS. Its soft glowing middle shows. Throw your fire right in there."},
                                  {"GIL", "Here - embers from my stove. Take 'em. Spend 'em on something shiny."}}; gift = 15; }
                else if (!has("quest.carp")) {
                    L = {{"GIL", "Say, you like fishing? There's a fish in these waters made of pure gold. The GOLDEN CARP."},
                         {"GIL", "Forty years I've tried. You find a fishing spot - the posts on the docks - and [" + bindingName(settings.keys[A_INTERACT]) + "] to cast."},
                         {"GIL", "When it bites, press quick! Then press again when the needle's in the green. Bring me that carp and I'll give you my lucky hat."}};
                    setFlag("quest.carp");
                } else if (has("fish.3") && !has("quest.carpdone")) {
                    L = {{"GIL", "Is that... THE GOLDEN CARP?! Forty years! And a candle does it in an afternoon!"}, {"GIL", "A deal's a deal. My lucky fisher's hat. And some embers for the bait."},
                         {"", "You received 60 embers and the FISHER'S HAT!"}};
                    setFlag("quest.carpdone"); save.embers += 60;
                    startDialog(L, [this]() { setFlag("hat.3"); save.hat = 3; showToast("NEW HAT: Fisher's Hat", "Swap hats at the mirror in Marrow's workshop"); audio::play(SFX_SHARD, 1, 1.2f); writeSave(); });
                    return;
                } else if (!has("fish.3")) L = {{"GIL", "Still after that Golden Carp? Keep casting! You've caught " + std::to_string(save.fishCaught) + " fish so far."}};
                else L = {{"GIL", "Jump the rings it makes, little candle. Jump 'em!"}};
                break;
            case 5:  // Tilda the clockmaker
                if (first) { L = {{"TILDA", "Careful! The gears up here bite. So does the HACKER."},
                                  {"TILDA", "GL1TCH, he calls himself. Hacked every clock in the wood from across the world."},
                                  {"TILDA", "His FIREWALL runs off three SERVERS. Servers HATE heat. You see where I'm going."},
                                  {"TILDA", "Here, some embers for the climb. Tick-tock!"}}; gift = 15; }
                else L = {{"TILDA", "Burn the three servers, then burn the boy. Tick-tock!"}};
                break;
            case 6:  // Mort the gravekeeper
                if (first) { L = {{"MORT", "...Oh. You're not a ghost. Pity. They're better at conversation."},
                                  {"MORT", "The Veil Matron nests at the bottom. A moth the size of a house."},
                                  {"MORT", "Moths love fire. Light all four braziers in her crypt and she'll come down to stare."},
                                  {"MORT", "Take these embers. The dead don't need them."}}; gift = 20; }
                else L = {{"MORT", "Four braziers. Bloom between the pairs. Then she's yours."}};
                break;
            case 7:  // Rusty the scrap robot
                if (first) { L = {{"RUSTY", "BZZT. Welcome to the Junkyard, flamey. Mind the rats."},
                                  {"RUSTY", "KING RATTLETRAP stole the last Beacon-Heart. Wears it like a crown. Very rude."},
                                  {"RUSTY", "Halfway in there's MEGA-BOT 3000. Armored front. Squishy back. Go around."},
                                  {"RUSTY", "The King's claw machine always gets STUCK after it slams. That's your moment."},
                                  {"RUSTY", "Here. Embers. I found them in a sofa."}}; gift = 25; }
                else L = {{"RUSTY", "Claw gets stuck - rat comes out - FIRE. Simple. BZZT."}};
                break;
            case 9:  // (old blueprint table)
                screen = SC_CRAFT; craftCol = 1; audio::play(SFX_MENU_SELECT);
                return;
            case 10:  // Bramble the beaver
                if (first) { L = {{"BRAMBLE", "Well hello, Mr. Candle! Bramble's the name, building supplies is the game."},
                                  {"BRAMBLE", "Logs, stone, glass, lamps, beds - you pay embers, I chew... I mean, I DELIVER."},
                                  {"BRAMBLE", "Or dig for it yourself! Ember ore pays out embers, crystal ore and scrap ore give crystals and gears."},
                                  {"BRAMBLE", "The workbench over there crafts the fancy stuff. Here's a starter kit, on the house!"}};
                             addItem(BK_LOG, 10, false); addItem(BK_COBBLE, 12, false); addItem(BK_BED, 1, false); addItem(BK_DOOR, 1, false);
                             addItem(BK_CHEST, 1, false); addItem(BK_CANDLE, 6, false); addItem(BK_SAPLING, 3, false);
                             L.push_back({"", "You received 10 logs, 12 cobblestone, a bed, a door, a chest, 6 candles and 3 saplings!"}); }
                else L = {{"BRAMBLE", "What'll it be? Fresh-chewed planks, finest rocks."}};
                break;
            case 8:  // Pip, a ghost kid
                if (first) { L = {{"PIP", "Boo! ...Did I scare you? Everyone's too scared to play with me since the dark came."},
                                  {"PIP", "I used to write letters to the Lamplighter. About my shadow. It got REALLY big."},
                                  {"PIP", "Wanna know a secret? Ember Shards hide in weird places. High places. Sealed places."},
                                  {"PIP", "Here, I found these. Ghosts don't have pockets anyway."}}; gift = 15; }
                else {
                    static const char* lines[] = {"Ghosts can't eat cake. Worst part.", "Try throwing your flame at the hooks! Whoosh!", "Sprocket's bomb robot is SO cool.", "If you get hit, your combo breaks. Don't get hit!"};
                    L = {{"PIP", lines[(int)(time * 3) % 4]}};
                }
                break;
        }
        bool shop = t.who == 3, supply = t.who == 10;
        if (first) { setFlag(key); if (gift > 0) { save.embers += gift; L.push_back({"", "You received " + std::to_string(gift) + " embers!"}); writeSave(); } }
        if (supply) { if (first) setFlag(key); startDialog(L, [this]() { screen = SC_SUPPLY; shopSel = 0; audio::play(SFX_CHECKPOINT); }); writeSave(); return; }
        startDialog(L, shop ? std::function<void()>([this]() { screen = SC_BOTSHOP; shopSel = 0; audio::play(SFX_CHECKPOINT); }) : nullptr);
        return;
    } else if (t.who == 1) {  // Spire pedestals
        int placedNow = 0;
        for (int i : {0, 1, 2, 4}) {
            std::string h = "heart." + std::to_string(i), p = "placed." + std::to_string(i);
            if (has(h) && !has(p)) { setFlag(p); placedNow++; }
        }
        int placed = countFlags("placed.");
        if (placedNow > 0) {
            audio::play(SFX_HEART);
            burst(t.pos + V3(0, 1.5f, 0), hexColor(0xffd070), 40, 6, 0.12f, 3, 1.0f, 0);
        }
        if (placed >= 4 && !has("hub.spire")) {
            setFlag("hub.spire");
            setFlag("hub.spiredoor");
            for (auto& d : lv.dyn) if (d.flag == "hub.spiredoor") d.open = true;
            audio::play(SFX_GATE);
            addShake(0.5f);
            startDialog({{"", "The four Beacon-Hearts blaze in their pedestals."}, {"", "With a groan of old stone, the Spire door grinds open."}});
            writeSave();
        } else if (placed >= 4) {
            startDialog({{"", "The pedestals hum with borrowed light. The Spire stands open."}});
        } else {
            startDialog({{"", "Four pedestals ring the Spire door. " + std::to_string(placed) + " of 4 hold a Beacon-Heart."},
                         {"", placedNow ? "The new heart settles into place with a warm chime." : "Recover the hearts from the Wharf, the Clockwood, the Vaults and the Junkyard."}});
            if (placedNow) writeSave();
        }
    }
}

void Game::collectPickup(Pickup& p) {
    if (p.taken) return;
    p.taken = true;
    if (!p.flag.empty()) setFlag(p.flag);
    switch (p.type) {
        case PK_EMBER:
            save.embers += p.value;
            audio::play(SFX_TICK, 0.6f, 1.6f + 0.02f * (p.value % 10));
            break;
        case PK_HAT: case PK_POWER: collectExtra(*this, p); break;
        case PK_WAX:
            pl.hp = std::min(maxHp(), pl.hp + 1);
            if (lv.id == LV_HOME) addItem(IT_WAX, 1);
            audio::play(SFX_CATCH, 1, 1.3f);
            break;
        case PK_SHARD: {
            int n = countFlags("shard.");
            audio::play(SFX_SHARD);
            if (n % 2 == 0) {
                pl.hp = maxHp();
                showToast("EMBER SHARD " + std::to_string(n) + "/7", "Your wax thickens: +1 maximum flame!");
            } else showToast("EMBER SHARD " + std::to_string(n) + "/7", "Find another to grow stronger");
            burst(p.pos, hexColor(0xff8040), 30, 5, 0.1f, 3, 0.8f, 0);
            writeSave();
        } break;
        case PK_CANDLE: {
            audio::play(SFX_CANDLE);
            int n = countFlags("candle.");
            burst(p.pos, hexColor(0xfff0c0), 24, 3, 0.08f, 3, 1.0f, -1);
            startDialog({{"MEMORY CANDLE " + std::to_string(n) + "/10", memoryText(p.index)}});
            writeSave();
        } break;
        case PK_HEART: {
            audio::play(SFX_HEART);
            addShake(0.3f);
            flashOverlay = 0.6f; flashColor = hexColor(0xfff0c0);
            burst(p.pos, hexColor(0xffd070), 80, 9, 0.14f, 3, 1.2f, 0);
            std::vector<DialogLine> L;
            if (p.index == 0) {
                setFlag("ab.warp");
                showToast("NEW POWER: WARP", "[" + bindingName(settings.keys[A_WARP]) + "] while your flame is away");
                L = {{"BEACON-HEART", "The heart of the Wharf beats against your wax, warm and impatient."},
                     {"", "Your flame remembers where it is, even when you are not with it."},
                     {"WARP", "While your flame is thrown, press [" + bindingName(settings.keys[A_WARP]) + "] to step into it."},
                     {"WARP", "Throw it onto ledges, across gaps, onto glowing brass hooks - then follow."},
                     {"", "A door of light has opened back to the Hollow."}};
            } else if (p.index == 1) {
                setFlag("ab.bloom"); setFlag("ab.overclock");
                showToast("NEW POWERS: BLOOM + OVERCLOCK", "[" + bindingName(settings.keys[A_BLOOM]) + "] detonates your flame");
                L = {{"BEACON-HEART", "The Clockwood's heart ticks once, and your flame swells."},
                     {"BLOOM", "Press [" + bindingName(settings.keys[A_BLOOM]) + "] to burst your flame wherever it is - in hand or far away."},
                     {"BLOOM", "Blooms burn everything nearby, light many braziers at once, and melt wax seals."},
                     {"OVERCLOCK", "And GL1TCH's stolen clock-cycles are yours now. Every hit you land charges OVERCLOCK."},
                     {"OVERCLOCK", "When the meter is full, press [" + bindingName(settings.keys[A_OVERCLOCK]) + "]: the whole world slows down - except you."}};
            } else if (p.index == 4) {
                setFlag("spark.core");
                showToast("SPARK CORE", "+1 robot slot, and your robots get stronger");
                L = {{"BEACON-HEART", "The Junkyard's heart buzzes like a live wire - and every robot nearby perks up."},
                     {"SPARK CORE", "You can now bring THREE robot friends at once, and they work faster."},
                     {"", "Four hearts. Take them to the pedestals at the Spire door."}};
            } else if (p.index == 2) {
                setFlag("ab.sunflare");
                showToast("NEW POWER: SUNFLARE", "Hold [" + bindingName(settings.keys[A_THROW]) + "] to charge");
                L = {{"BEACON-HEART", "The Vaults' heart is so hot it hisses against the stone."},
                     {"SUNFLARE", "Hold [" + bindingName(settings.keys[A_THROW]) + "] until your flame flashes white, then release."},
                     {"SUNFLARE", "A Sunflare flies farther, strikes three times as hard,"},
                     {"SUNFLARE", "shatters shields and crystal - and burns away shrouds of shadow."}};
            }
            startDialog(L);
            writeSave();
        } break;
    }
}

void Game::checkpoint(int li) {
    Lantern& l = lv.lanterns[li];
    bool first = !l.lit;
    l.lit = true;
    setFlag("lantern." + std::to_string(lv.id) + "." + std::to_string(li));
    save.level = lv.id; save.spawn = l.spawn;
    pl.hp = maxHp();
    writeSave();
    audio::play(SFX_CHECKPOINT);
    burst(l.pos, hexColor(0xffd070), first ? 30 : 12, 3, 0.08f, 3, 0.8f, -1);
    statusMsg = first ? "Lantern lit - your flame is restored and progress saved" : "Flame restored - progress saved";
    statusT = 2.5f;
}

void Game::lightBrazier(int i, bool byBloom) {
    Brazier& b = lv.braziers[i];
    if (b.lit) return;
    if (b.sealed && !byBloom) return;
    b.lit = true;
    b.litT = b.timed;
    audio::playAt(SFX_BRAZIER, b.pos);
    burst(b.pos, hexColor(0xffa040), 24, 4, 0.1f, 3, 0.8f, -2);
    if (b.gateGroup == 70) { fennLampLit(*this); return; }
    if (b.gateGroup >= 0) {
        bool all = true;
        for (auto& o : lv.braziers) if (o.gateGroup == b.gateGroup && !o.lit) all = false;
        if (all) openGateGroup(b.gateGroup);
        else if (b.timed > 0) { statusMsg = "Quick - light the others before it fades!"; statusT = 2; }
    }
}

void Game::openGateGroup(int group) {
    bool any = false;
    for (auto& gl : lv.gateLinks) {
        if (gl.group != group) continue;
        auto& d = lv.dyn[gl.dyn];
        if (!d.open) { d.open = true; any = true; }
        if (!gl.flag.empty()) setFlag(gl.flag);
    }
    for (auto& b : lv.braziers) if (b.gateGroup == group) b.timed = 0;
    if (any) { audio::play(SFX_GATE); addShake(0.2f); statusMsg = "A gate grinds open"; statusT = 2; }
}

// ------------------------------------------------------------------ world objects
void Game::updateWorldObjects(float dt) {
    // movers & gates
    for (int i = 0; i < (int)lv.dyn.size(); i++) {
        auto& d = lv.dyn[i];
        d.prevOffset = d.offset;
        if (d.kind == DK_MOVER) {
            float t = std::fmod(time + d.phase * d.period, d.period) / d.period;  // 0..1
            float u = t < 0.5f ? t * 2 : 2 - t * 2;
            float p = d.pause / d.period * 2;
            u = clampf((u - p) / (1 - 2 * p), 0, 1);
            u = smooth01(u);
            d.offset = d.a + (d.b - d.a) * u;
        } else if (d.kind == DK_GATE) {
            float h = d.base.size().y;
            d.openAmt = approach(d.openAmt, d.open ? 1.f : 0.f, dt * (d.open ? 0.9f : 2.5f));
            d.offset = V3(0, -(h + 0.05f) * d.openAmt, 0);
        }
        V3 delta = d.offset - d.prevOffset;
        if (len2(delta) > 0) {
            if (pl.grounded && pl.groundDyn == i) pl.pos += delta;
            // push the player out if the solid moved into them
            AABB pb(V3(pl.pos.x - PLAYER_R, pl.pos.y, pl.pos.z - PLAYER_R), V3(pl.pos.x + PLAYER_R, pl.pos.y + PLAYER_H, pl.pos.z + PLAYER_R));
            AABB db = d.box();
            if (d.collides() && pb.overlaps(db)) {
                float up = db.hi.y - pl.pos.y;
                if (up < 0.8f && d.kind == DK_MOVER) { pl.pos.y = db.hi.y + 0.001f; pl.vel.y = std::max(pl.vel.y, 0.f); }
                else {
                    float px1 = db.hi.x - pb.lo.x, px2 = pb.hi.x - db.lo.x, pz1 = db.hi.z - pb.lo.z, pz2 = pb.hi.z - db.lo.z;
                    float m = std::min(std::min(px1, px2), std::min(pz1, pz2));
                    if (m == px1) pl.pos.x += px1; else if (m == px2) pl.pos.x -= px2; else if (m == pz1) pl.pos.z += pz1; else pl.pos.z -= pz2;
                }
            }
        }
    }
    for (auto& s : lv.spinners) s.angle += s.speed * dt;
    for (auto& a : lv.anchors) a.pulse = std::max(0.f, a.pulse - dt * 2);

    // timed braziers
    for (auto& b : lv.braziers) {
        b.flicker += dt;
        if (b.lit && b.timed > 0) {
            b.litT -= dt;
            if (b.litT <= 0) { b.lit = false; audio::playAt(SFX_WHISPER, b.pos, 0.4f, 2); burst(b.pos, hexColor(0x3a3040), 10, 2, 0.12f, 2, 0.6f, -1); }
        }
    }

    if (pl.dead) return;
    // lanterns (checkpoints)
    for (int i = 0; i < (int)lv.lanterns.size(); i++) {
        auto& l = lv.lanterns[i];
        bool inside = distXZ(l.pos, pl.pos) < 3.0f && std::fabs(l.pos.y - 1.8f - pl.pos.y) < 2.5f;
        if (inside && l.cool <= 0) { checkpoint(i); l.cool = 1; }
        if (!inside) l.cool = 0;
    }
    // pickups
    for (auto& p : lv.pickups) {
        if (p.taken) continue;
        p.bob += dt;
        if (p.type == PK_WAX || p.type == PK_EMBER) {
            p.life -= dt;
            if (p.life <= 0) { p.taken = true; continue; }
            p.vel.y -= 20 * dt;
            bool g = false;
            V3 pos = p.pos - V3(0, 0.2f, 0);
            moveBox(pos, p.vel, 0.15f, 0.3f, dt, g, nullptr, false);
            p.pos = pos + V3(0, 0.2f, 0);
            if (g) { p.vel.x *= 0.8f; p.vel.z *= 0.8f; }
            // gentle magnet
            float mag = 3.f + 3.f * upLevel("magnet");
            if (p.life < 12.5f && len(p.pos - (pl.pos + V3(0, 0.6f, 0))) < mag) p.pos = lerp(p.pos, pl.pos + V3(0, 0.6f, 0), dt * 6);
        }
        if (len(p.pos - (pl.pos + V3(0, 0.6f, 0))) < (p.type == PK_HEART ? 1.8f : 1.2f)) collectPickup(p);
    }
    // arenas
    for (int ai = 0; ai < (int)lv.arenas.size(); ai++) {
        auto& a = lv.arenas[ai];
        if (a.done) continue;
        if (!a.active) {
            if (a.zone.contains(pl.pos + V3(0, 0.5f, 0))) {
                a.active = true;
                for (int g : a.gates) lv.dyn[g].open = false;
                for (auto& s : lv.enemySpawns) if (s.arena == ai) spawnEnemy(s.type, s.pos, ai);
                audio::play(SFX_ROAR, 0.6f, 1.6f);
                statusMsg = "The Hush closes in!"; statusT = 2;
            }
        } else {
            bool any = false;
            for (auto& e : enemies) if (e.alive && e.arena == ai) {
                any = true;
                // an arena enemy that slipped out of the room (knocked through a gap, wandered off) is pulled back in,
                // so the gates can never stay shut on an enemy you can't reach
                const AABB& z = a.zone;
                bool out = e.pos.x < z.lo.x - 1.5f || e.pos.x > z.hi.x + 1.5f || e.pos.z < z.lo.z - 1.5f || e.pos.z > z.hi.z + 1.5f || e.pos.y < z.lo.y - 2.f;
                if (out) {
                    burst(e.pos + V3(0, 0.8f, 0), hexColor(0x8060ff), 10, 3, 0.07f, 3, 0.4f, 1);
                    V3 c = (z.lo + z.hi) * 0.5f;
                    e.pos = V3(clampf(e.pos.x, z.lo.x + 1.5f, z.hi.x - 1.5f), z.lo.y + 1.2f, clampf(e.pos.z, z.lo.z + 1.5f, z.hi.z - 1.5f));
                    if (e.pos.y < c.y - 50) e.pos.y = c.y;
                    e.vel = V3();
                    burst(e.pos + V3(0, 0.8f, 0), hexColor(0x8060ff), 10, 3, 0.07f, 3, 0.4f, 1);
                }
            }
            if (!any) {
                a.done = true; a.active = false;
                for (int g : a.gates) lv.dyn[g].open = true;
                if (!a.flag.empty()) setFlag(a.flag);
                audio::play(SFX_GATE);
                statusMsg = "The way is clear"; statusT = 2;
            }
        }
    }
    // boss trigger
    if (lv.boss.type != B_NONE && !boss.active && !has(lv.boss.flag) && lv.boss.trigger.contains(pl.pos + V3(0, 0.5f, 0))) startBoss();
    // exits
    for (auto& e : lv.exits) {
        if (!e.zone.contains(pl.pos + V3(0, 0.5f, 0))) continue;
        if (e.needFlag.empty() || has(e.needFlag)) {
            if (e.target == LV_ENDLESS && e.spawn == 99) { if (!transitioning) completeEndlessLevel(); }
            else requestTransition(e.target, e.spawn);
        }
        else if (statusT <= 0.3f) { statusMsg = e.lockedMsg; statusT = 2; }
    }
}

// ------------------------------------------------------------------ cinematics
static V3 hubWorkshopCam(int step, float t, V3& target) {
    // shots inside Marrow's workshop (see buildHub)
    switch (step) {
        case 0: target = V3(0, 14, 30); return V3(-30 + t * 1.5f, 6 + t * 0.4f, -20);
        case 1: target = V3(0, 34, 0); return V3(-24, 8, -26 + t * 1.0f);
        case 2: target = V3(0, 24, 0); return V3(20 - t, 5, -22);
        case 3: target = V3(-6, 3, -18); return V3(8 - t * 0.5f, 6, -30);
        case 4: target = V3(0, 1.5f, -24.5f); return V3(4.5f - t * 0.3f, 3.0f, -20.5f);
        case 5: target = V3(0, 1.5f, -25.5f); return V3(1.8f, 2.0f, -23.0f + t * 0.1f);
        default: target = V3(0, 1.6f, -25.5f); return V3(1.4f, 2.0f, -23.2f);
    }
}

[[maybe_unused]] static const char* INTRO[] = {
    "Lumen Hollow was a city that had forgotten the night.",
    "Atop the Lighthouse Spire burned the Great Lamp -\nbrighter every year, until no one remembered the stars.",
    "Then, one evening, the Lamp went out.\nAnd the Hush came down the Spire like water.",
    "It drank the light from every window.\nThen it drank the people.",
    "At the foot of the Spire, in a cluttered workshop,\nthe last Lamplighter had one flame left.",
    "He gave it a body of wax.\nHe gave it a name.",
    "MARROW: ...Wick. Wake up, Wick.",
};
static const int INTRO_N = 7;

void Game::startIntro() {
    buildLevel(LV_HUB);
    applyFlagsToLevel();
    resetEncounters();
    pl = Player();
    pl.pos = lv.spawns[0].pos; pl.yaw = lv.spawns[0].yaw;
    flame = Flame();
    cineStep = 0; cineT = 0;
    audio::setSong(SONG_TITLE);
    letterbox = 1;
}

void Game::updateIntro(float dt, const InputState& in) {
    cineT += dt;
    time += dt;
    letterbox = approach(letterbox, 1, dt * 2);
    V3 tgt;
    V3 eye = hubWorkshopCam(cineStep, cineT, tgt);
    camOverrideEye = eye; camOverrideTarget = tgt; camOverride = 1;
    camEye = eye; camTarget = tgt;
    float lineDur = 5.5f;
    bool adv = in.mOk || in.pressed[A_INTERACT] || in.pressed[A_JUMP] || in.pressed[A_THROW];
    if (cineStep == 5 && cineT > 1.5f && !flame.charging) {
        flame.charging = true;  // used as "ignited" marker in the intro
        audio::play(SFX_BRAZIER, 1.2f);
        audio::play(SFX_UNLOCK, 0.8f);
        burst(pl.pos + V3(0, 1.3f, 0), hexColor(0xffc060), 50, 4, 0.1f, 3, 1.0f, -1);
        flashOverlay = 0.5f; flashColor = hexColor(0xffe0a0);
    }
    if (adv || cineT > lineDur) {
        cineStep++; cineT = 0;
        audio::play(SFX_BLIP);
    }
    if (in.mBack || in.pressed[A_PAUSE] || cineStep >= INTRO_N) {
        // begin play
        flame = Flame();
        letterbox = 0;
        camOverride = 0;
        enterLevel(LV_HUB, 0);
        screen = SC_PLAY;
        // Marrow greets the player
        for (int i = 0; i < (int)lv.talkers.size(); i++) if (lv.talkers[i].who == 0) { talk(i); break; }
    }
}

[[maybe_unused]] static const char* ENDING[] = {
    "The heart of the Hush comes apart like smoke in a draught.",
    "What remains is only an ordinary night -\nthe kind the Hollow had forgotten how to have.",
    "The Great Lamp stands cold and empty. It needs a flame.\nWick has exactly one.",
    "It is a small light. It casts a small shadow.\nThat is how it was always meant to be.",
    "Far below, one by one, the windows of Lumen Hollow begin to glow.",
    "MARROW: ...You did it, little flame. You did what I never could.",
    "Some weeks later, the old Lamplighter carved a new wick,\nand lit it from the Lamp at the top of the Spire.",
    "And Wick woke up.",
};
static const int ENDING_N = 8;

void Game::startEnding() {
    screen = SC_ENDING;
    cineStep = 0; cineT = 0;
    setFlag("game.finished");
    justFinished = true;
    enemies.clear(); projs.clear(); rings.clear(); teles.clear();
    audio::setSong(SONG_ENDING);
    letterbox = 0;
    writeSave();
}

void Game::updateEnding(float dt, const InputState& in) {
    cineT += dt;
    time += dt;
    letterbox = approach(letterbox, 1, dt * 1.5f);
    V3 C = lv.boss.center;
    float t = cineT;
    camOverride = 1;
    switch (cineStep) {
        case 0: camOverrideTarget = C + V3(0, 3, 0); camOverrideEye = C + V3(std::sin(t * 0.1f) * 12, 6, -12); break;
        case 1: camOverrideTarget = C + V3(0, 2, 0); camOverrideEye = C + V3(-10, 4 + t * 0.3f, -10); break;
        case 2: camOverrideTarget = C + V3(0, 3, 0); camOverrideEye = C + V3(0, 3, -8 + t * 0.3f); break;
        case 3: camOverrideTarget = C + V3(0, 3.5f, 0); camOverrideEye = C + V3(5, 4, -6); break;
        case 4: camOverrideTarget = C + V3(0, -40, 40); camOverrideEye = C + V3(0, 14 + t * 1.5f, -30); break;
        case 5: camOverrideTarget = C + V3(0, 3, 0); camOverrideEye = C + V3(-6, 5, -9); break;
        case 6: camOverrideTarget = C + V3(0, 3, 0); camOverrideEye = C + V3(0, 25 + t, -40); break;
        default: camOverrideTarget = C + V3(0, 3, 0); camOverrideEye = C + V3(0, 5, -10); break;
    }
    camEye = camOverrideEye; camTarget = camOverrideTarget;
    if (cineStep == 2 && t > 2.5f && !flame.charging) {
        flame.charging = true;  // the lamp ignites
        audio::play(SFX_HEART, 1.2f);
        flashOverlay = 0.8f; flashColor = hexColor(0xfff0c0);
        burst(C + V3(0, 3, 0), hexColor(0xffd070), 120, 10, 0.14f, 3, 1.5f, 0);
    }
    bool adv = in.mOk || in.pressed[A_INTERACT] || in.pressed[A_JUMP] || in.pressed[A_THROW];
    if (adv || cineT > 6.0f) { cineStep++; cineT = 0; audio::play(SFX_BLIP); }
    if (in.mBack || cineStep >= ENDING_N) {
        letterbox = 0; camOverride = 0;
        screen = SC_VICTORY; menuSel = 0; screenT = 0;
        audio::setSong(SONG_ENDING);
    }
}

// ------------------------------------------------------------------ main update
void Game::frame(float dt, InputState& in, float lookDX, float lookDY) {
    dt = std::min(dt, 0.1f);
    screenT += dt;
    if (R) { R->time += dt; R->updateParticles(dt); }
    toastT = std::max(0.f, toastT - dt);
    statusT = std::max(0.f, statusT - dt);
    areaTitleT = std::max(0.f, areaTitleT - dt);
    flashOverlay = std::max(0.f, flashOverlay - dt * 1.5f);
    respawnFade = std::max(0.f, respawnFade - dt * 1.5f);
    glitch = std::max(0.f, glitch - dt * 1.6f);
    bloomFlash = std::max(0.f, bloomFlash - dt * 2.5f);
    bloomR += dt * 14;

    if (screen == SC_PLAY) {
        if (dialog.empty() && !transitioning) {
            float sens = 0.0022f * settings.sensitivity;
            camYaw -= lookDX * sens;
            camPitch -= lookDY * sens * (settings.invertY ? -1.f : 1.f);
        }
        if (in.pressed[A_PAUSE] && dialog.empty()) { screen = SC_PAUSE; menuSel = 0; audio::play(SFX_MENU_SELECT); return; }
        if (in.pressed[A_JOURNAL] && dialog.empty() && lv.id == LV_HOME && vox.active) { craftCol = 0; screen = SC_CRAFT; audio::play(SFX_MENU_SELECT); return; }
        if (in.pressed[A_JOURNAL] && dialog.empty()) { prevJournalScreen = SC_PLAY; journalSel = 0; screen = SC_JOURNAL; audio::play(SFX_MENU_SELECT); return; }
        if (lv.id == LV_HOME && vox.active && dialog.empty()) {
            if (in.numKey) { hotSel = in.numKey - 1; hotNameT = 1.6f; audio::play(SFX_MENU_MOVE, 0.4f); }
            if (in.wheel) { hotSel = ((hotSel + in.wheel) % 9 + 9) % 9; hotNameT = 1.6f; audio::play(SFX_MENU_MOVE, 0.4f); }
            in.numKey = 0; in.wheel = 0;
        }
        static float acc = 0;
        // Button presses/releases are kept until a simulation step actually consumes them.
        // (On monitors faster than the 120 Hz simulation some frames run no step; dropping
        // their presses made jumps and throws feel unreliable.)
        static InputState carry;
        for (int a = 0; a < A_COUNT; a++) { carry.pressed[a] |= in.pressed[a]; carry.released[a] |= in.released[a]; }
        acc += dt;
        bool first = true;
        InputState stepIn = in;
        for (int a = 0; a < A_COUNT; a++) { stepIn.pressed[a] = carry.pressed[a]; stepIn.released[a] = carry.released[a]; }
        int steps = 0;
        while (acc >= SIM_DT && steps < 12) {
            update(SIM_DT, stepIn);
            acc -= SIM_DT;
            steps++;
            if (first) { stepIn.clearEdges(); carry.clearEdges(); first = false; }
            if (screen != SC_PLAY) { carry.clearEdges(); break; }
        }
        if (steps >= 12) acc = 0;
        // tutorial: looking around
        if (std::fabs(lookDX) + std::fabs(lookDY) > 2 && !has("tut.look")) setFlag("tut.look");
    } else if (screen == SC_INTRO) {
        updateIntro(dt, in);
    } else if (screen == SC_ENDING) {
        updateEnding(dt, in);
    } else {
        time += dt;
        if (screen == SC_TITLE) {
            // slow orbit over the Hollow
            float a = time * 0.05f + 2.0f;
            camEye = V3(std::cos(a) * 46, 18 + std::sin(time * 0.1f) * 3, std::sin(a) * 46);
            camTarget = V3(0, 16, 0);
            for (int i = 0; i < (int)lv.dyn.size(); i++) lv.dyn[i].prevOffset = lv.dyn[i].offset;
            for (auto& s : lv.spinners) s.angle += s.speed * dt;
        }
        updateMenus(dt, in);
    }
    audio::setListener(camEye, norm(cross(norm(camTarget - camEye), V3(0, 1, 0))));
}

void Game::update(float dt, const InputState& in) {
    // dialog freezes the world
    if (!dialog.empty()) {
        float before = dialogChars;
        dialogChars += dt * 60;
        if ((int)dialogChars / 3 != (int)before / 3) audio::play(SFX_BLIP, 0.4f);
        bool adv = in.pressed[A_INTERACT] || in.pressed[A_JUMP] || in.pressed[A_THROW] || in.mOk;
        if (adv) {
            int full = (int)dialog[dialogIdx].text.size();
            if (dialogChars < full) dialogChars = (float)full;
            else {
                dialogIdx++; dialogChars = 0;
                audio::play(SFX_MENU_MOVE, 0.6f);
                if (dialogIdx >= (int)dialog.size()) {
                    dialog.clear();
                    if (dialogDone) { auto f = dialogDone; dialogDone = nullptr; f(); }
                }
            }
        }
        updateCamera(dt);
        return;
    }
    if (hitstop > 0) { hitstop -= dt; return; }
    if (transitioning) {
        transitionT += dt;
        if (transitionT > 0.45f) { enterLevel(pendingLevel, pendingSpawn); respawnFade = 1; }
        return;
    }
    time += dt;
    save.playTime += dt;
    shake = std::max(0.f, shake - dt * 1.8f);
    pl.maxHp = maxHp();

    updateWorldObjects(dt);
    if (screen != SC_PLAY) return;
    updateCamera(dt);
    computeAim();
    bool wasFishing = fishState > 0;
    InputState pin = in;
    if (updateFishing(dt, in)) pin = InputState();  // fishing: Wick stands still
    updatePlayer(dt, pin);
    if (lv.id == LV_HOME && vox.active) {
        InputState masked = in;
        updateVoxel(dt, in, masked);
        if (screen != SC_PLAY) return;
        updateFlame(dt, masked);
    } else updateFlame(dt, pin);
    updatePowers(dt, pin);
    // Overclock: the world runs slow, Wick doesn't
    float wdt = dt * worldScale();
    updateEnemies(wdt);
    updateBots(dt);
    updateProjectiles(wdt);
    updateBoss(wdt);
    if (screen != SC_PLAY) return;
    updateExtras(dt, pin);
    if (screen != SC_PLAY) return;

    // talkers
    if (in.pressed[A_INTERACT] && !pl.dead && !wasFishing && fishState == 0) {
        bool talked = false;
        for (int i = 0; i < (int)lv.talkers.size(); i++)
            if (len(lv.talkers[i].pos - pl.pos) < 3.0f) { talk(i); talked = true; break; }
        if (!talked && !boss.active)
            for (auto& l : lv.lanterns)
                if (l.lit && distXZ(l.pos, pl.pos) < 3.2f) { screen = SC_SHOP; shopSel = 0; audio::play(SFX_CHECKPOINT); break; }
    }
    // combo timer & floating text
    if (combo > 0) { comboT -= dt; if (comboT <= 0) combo = 0; }
    for (auto& f : floats) { f.t += dt; f.pos.y += dt * 1.2f; }
    floats.erase(std::remove_if(floats.begin(), floats.end(), [](const FloatText& f) { return f.t > 1.1f; }), floats.end());
    // ambient emitters
    if (!headless && R) {
        static Rng arng(5);
        for (auto& e : lv.emitters) {
            if (distXZ(e.zone.center(), pl.pos) > 70) continue;
            e.acc += dt * e.rate;
            while (e.acc >= 1) {
                e.acc -= 1;
                Particle q;
                q.pos = V3(arng.range(e.zone.lo.x, e.zone.hi.x), arng.range(e.zone.lo.y, e.zone.hi.y), arng.range(e.zone.lo.z, e.zone.hi.z));
                if (len(q.pos - pl.pos) > 40) continue;
                switch (e.kind) {
                    case 0: q.color = hexColor(0xc8ff80); q.vel = V3(arng.range(-0.5f, 0.5f), arng.range(-0.2f, 0.4f), arng.range(-0.5f, 0.5f)); q.size = 0.06f; q.life = 3; q.kind = 3; q.alpha = 1; break;
                    case 1: q.color = hexColor(0xff8a3a); q.vel = V3(arng.range(-0.3f, 0.3f), arng.range(0.6f, 1.5f), arng.range(-0.3f, 0.3f)); q.size = 0.05f; q.life = 3; q.kind = 3; q.alpha = 1; break;
                    case 2: q.color = lv.env.fogColor * 1.6f; q.vel = V3(arng.range(-0.3f, 0.3f), 0.05f, arng.range(-0.3f, 0.3f)); q.size = 1.4f; q.grow = 0.3f; q.life = 5; q.kind = 2; q.alpha = 0.35f; break;
                    case 3: q.color = hexColor(0x8a8090); q.vel = V3(arng.range(-0.3f, 0.3f), -0.6f, arng.range(-0.3f, 0.3f)); q.size = 0.05f; q.life = 6; q.kind = 0; q.alpha = 1; break;
                    default: q.color = hexColor(0xb0a0ff); q.vel = V3(0, 0.3f, 0); q.size = 0.05f; q.life = 4; q.kind = 3; q.alpha = 1; break;
                }
                q.maxLife = q.life;
                R->spawn(q);
            }
        }
    }
}
