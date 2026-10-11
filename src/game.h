// WICKLIGHT - core game data structures
#pragma once
#include "mathx.h"
#include "render.h"
#include "audio.h"
#include "voxel.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <functional>
#include <memory>

// ------------------------------------------------------------------ constants
static const float PLAYER_R = 0.32f;
static const float PLAYER_H = 1.15f;
static const float GRAVITY = 30.f;
static const float SIM_DT = 1.f / 120.f;

enum LevelId { LV_HUB = 0, LV_WHARF, LV_CLOCK, LV_VAULTS, LV_SPIRE, LV_JUNK, LV_ENDLESS, LV_HOME, LV_CITY, LV_COUNT };

// ------------------------------------------------------------------ input
enum Action { A_FWD, A_BACK, A_LEFT, A_RIGHT, A_JUMP, A_DASH, A_THROW, A_WARP, A_BLOOM, A_INTERACT, A_PAUSE, A_JOURNAL, A_GRAPPLE, A_FIREWALL, A_OVERCLOCK, A_COUNT };
struct InputState {
    float moveX = 0, moveY = 0;  // -1..1 (x right, y forward)
    bool down[A_COUNT] = {};
    bool pressed[A_COUNT] = {};
    bool released[A_COUNT] = {};
    // menu navigation (edge triggered)
    bool mUp = false, mDown = false, mLeft = false, mRight = false, mOk = false, mBack = false;
    int numKey = 0;   // 1..9 pressed this frame (hotbar)
    int wheel = 0;    // mouse wheel / shoulder buttons this frame
    void clearEdges() {
        for (int i = 0; i < A_COUNT; i++) pressed[i] = released[i] = false;
        mUp = mDown = mLeft = mRight = mOk = mBack = false;
        numKey = 0; wheel = 0;
    }
};

// ------------------------------------------------------------------ world objects
struct Solid {
    AABB box;
    int tile = T_STONEBRICK, topTile = -1;
    V3 color = V3(1, 1, 1);
    bool visible = true;
    float uvScale = 1.f;
    bool barrier = false;  // invisible extension above a wall: blocks climbing/throwing over it, ignored by the camera
};

// A solid that is not baked into the static mesh: moving platforms, gates, breakable seals.
enum DynKind { DK_MOVER, DK_GATE, DK_SEAL, DK_CRYSTAL };
struct DynSolid {
    DynKind kind = DK_MOVER;
    AABB base;                 // rest position
    V3 offset, prevOffset;     // current offset from base
    // mover
    V3 a, b; float period = 6, phase = 0, pause = 0.6f;
    bool activeMover = true;
    // gate: sinks into the ground when opened
    bool open = false; float openAmt = 0;
    // seal/crystal: broken
    bool broken = false;
    std::string flag;          // persistent flag that opens/breaks it
    int tile = T_STONEBRICK, topTile = -1; V3 color = V3(1, 1, 1);
    Mesh mesh;
    bool collides() const {
        if ((kind == DK_SEAL || kind == DK_CRYSTAL) && broken) return false;
        if (kind == DK_GATE && openAmt > 0.85f) return false;
        return true;
    }
    AABB box() const { return AABB(base.lo + offset, base.hi + offset); }
};

struct Brazier {
    V3 pos;                    // bowl centre
    bool lit = false, sealed = false;
    int gateGroup = -1;        // all braziers of a group lit -> open gates of that group
    float timed = 0;           // >0: stays lit only this long
    float litT = 0;
    bool bossBrazier = false;
    float flicker = 0;
};

struct GateLink { int group; int dyn; std::string flag; };

struct Lantern { V3 pos; int spawn; bool lit = false; float cool = 0; };

enum PickupType { PK_SHARD, PK_CANDLE, PK_HEART, PK_WAX, PK_EMBER, PK_HAT, PK_POWER };
struct Pickup {
    PickupType type; V3 pos; int index = 0; std::string flag; bool taken = false;
    float bob = 0; V3 vel; float life = -1; int value = 1;  // life>0 for dropped wax
};

struct Exit { AABB zone; LevelId target; int spawn; std::string needFlag; std::string lockedMsg; std::string label; };
struct SpawnPt { V3 pos; float yaw; };
struct Anchor { V3 pos; float pulse = 0; };
struct Hint { AABB zone; std::string text; std::string needFlag, hideFlag; };
struct Talker { V3 pos; float yaw; int who; };  // who: 0 Marrow, 1 pedestals, 3 Sprocket (bot shop), 4 Gil, 5 Tilda, 6 Mort, 7 Rusty, 8 Pip,
                                                 // 10 Bramble, 11 Mira, 12 Whiskers the cat, 13 Old Fenn, 14 Flick (race), 15 wardrobe mirror, 16 fishing spot
struct Spinner { V3 pos; float r, thick; int axis; float speed; int tile; V3 color; float angle = 0; };
struct Emitter { AABB zone; int kind; float rate; float acc = 0; };  // 0 fireflies, 1 embers, 2 mist, 3 ash snow, 4 motes, 5 data rain, 6 city rain

enum EnemyType { E_GLOOMLING, E_TARBELL, E_MOTH, E_KNIGHT, E_WRAITH, E_GLOOMKING, E_MEGABOT, E_RAT, E_VIRUS, E_DRONE, E_DECOY, E_AGENT };
struct EnemySpawn { EnemyType type; V3 pos; int arena = -1; };
struct Arena { AABB zone; std::vector<int> gates; std::string flag; bool active = false, done = false; };

enum BossType { B_NONE = -1, B_BELLWETHER, B_WARDEN, B_MATRON, B_HUSH, B_RATKING };
struct BossDef { BossType type = B_NONE; V3 center; float radius = 16; AABB trigger; std::vector<int> gates; std::string flag; };

// hacking puzzles: terminals flash a sequence, then must be burned in that order
struct Terminal { V3 pos; float yaw = 0; int puzzle = 0; int slot = 0; float flash = 0; int flashKind = 0; };  // flashKind 0 show, 1 right, 2 wrong
struct HackPuzzle {
    AABB zone; std::vector<int> seq; int group = -1; std::string flag;
    int state = 0;      // 0 idle, 1 showing, 2 waiting for input, 3 solved, 4 failed (pause before replay)
    float t = 0; int showIdx = 0, progress = 0, fails = 0;
    int reward = -1;    // PK_POWER index granted when solved (-1 none)
    V3 rewardPos;
};
struct Searchlight { V3 a, b; float period = 8, phase = 0, radius = 3.2f; V3 spot; };
struct Holdout { AABB zone; float dur = 30; std::string flag; bool active = false, done = false; float t = 0, spawnT = 0; int wave = 0; std::vector<V3> spawns; V3 pad; float rescueT = 0; };
struct RaceRing { V3 pos; float yaw; bool high = false; };

struct Grid {
    float cell = 8; int nx = 0, nz = 0; V3 origin;
    std::vector<std::vector<int>> cells;
    std::vector<int> stamp; int stampId = 0;
};

struct Level {
    LevelId id = LV_HUB;
    std::string name;
    Environment env;
    Song song = SONG_HUB;
    float waterY = -1000, killY = -40;
    V3 waterColor = hexColor(0x1a4a5a);
    AABB waterArea;
    std::vector<Solid> solids;
    MeshBuilder decor;
    std::vector<Light> lights;
    std::vector<DynSolid> dyn;
    std::vector<GateLink> gateLinks;
    std::vector<Brazier> braziers;
    std::vector<Lantern> lanterns;
    std::vector<Pickup> pickups;
    std::vector<Exit> exits;
    std::vector<SpawnPt> spawns;
    std::vector<Anchor> anchors;
    std::vector<Hint> hints;
    std::vector<Talker> talkers;
    std::vector<Spinner> spinners;
    std::vector<Emitter> emitters;
    struct RouteStep { int op; V3 p; };  // 0 goto, 1 throw+warp, 2 throw+recall, 3 fight, 4 boss, 5 bloom
    std::vector<RouteStep> route;
    struct Breakable { V3 pos; int kind = 0; bool broken = false; };
    std::vector<Breakable> breakables;
    std::vector<EnemySpawn> enemySpawns;
    std::vector<Arena> arenas;
    BossDef boss;
    std::vector<Terminal> terminals;
    std::vector<HackPuzzle> puzzles;
    std::vector<Searchlight> searchlights;
    std::vector<V3> reinforce;           // where FBI backup arrives when you're spotted
    Holdout holdout; bool hasHoldout = false;
    std::vector<RaceRing> race;
    Mesh staticMesh, waterMesh;
    Grid grid;
    bool meshesBuilt = false;
};

// ------------------------------------------------------------------ actors
enum FlameState { F_HELD, F_FLYING, F_HOVER, F_RETURNING };
struct Flame {
    FlameState st = F_HELD;
    V3 pos, vel, prevPos;
    float traveled = 0, maxDist = 16, hoverT = 0;
    bool charged = false;
    float charge = 0; bool charging = false;
    int anchored = -1;
    V3 throwOrigin;
    std::set<int> hitIds;
    float bloomCd = 0, warpCd = 0;
    float flicker = 0;
    int smother = 0;          // number of moths clinging
};

struct Player {
    V3 pos, vel;
    float yaw = 0;
    bool grounded = false; float coyote = 0, jumpBuf = 0;
    int groundDyn = -1;
    float hp = 5, maxHp = 5;
    float invuln = 0, hurtT = 0;
    float dashT = 0, dashCd = 0; V3 dashDir;
    bool dead = false; float deadT = 0;
    float walkPhase = 0, land = 0;
    V3 safePos; float safeT = 0;
    float stepT = 0;
    float airTime = 0;
    int airJumps = 0;
    float flingT = 0;   // after a grapple swing: keep momentum in the air
};

struct Enemy {
    EnemyType type; int id;
    V3 pos, vel, home;
    float yaw = 0;
    float hp = 1, maxHp = 1;
    int state = 0; float t = 0, cd = 0;
    float flash = 0, anim = 0;
    bool alive = true, grounded = false;
    float radius = 0.5f, height = 1.f;
    int arena = -1;
    bool shield = true;         // knights
    float reveal = 0;           // wraiths
    V3 target, target2;
    bool clinging = false;      // moth on flame
    float deathT = 0;
    float burnT = 0;            // firewall damage tick
    int gen = 0;                // virus: 0 splits on death, 1 doesn't
    bool alerted = false;       // agent: has seen you
};

enum ProjType { PR_TAR, PR_GEAR, PR_ORB, PR_DUST, PR_CODE, PR_BULLET };
struct Projectile { ProjType type; V3 pos, vel; float life = 5, radius = 0.4f, dmg = 1; bool gravity = false; bool alive = true; float spin = 0; };
struct Ring { V3 c; float r = 0, speed = 8, maxR = 20, dmg = 1; bool alive = true; bool hitPlayer = false; V3 color = hexColor(0x8a5aff); };
struct Telegraph { V3 pos; float radius, t = 0, delay = 1.2f, dmg = 1, linger = 0.25f; bool fired = false; bool alive = true; int kind = 0; };

struct WeakPoint { V3 pos; float r; bool vulnerable; int id; bool shrouded = false; };

struct Boss {
    BossType type = B_NONE;
    bool active = false, dead = false, introDone = false;
    float hp = 1, maxHp = 1;
    int phase = 0, state = 0; float t = 0, stateT = 0;
    V3 pos; float yaw = 0;
    float flash = 0;
    std::string name;
    // generic scratch state
    float f[16] = {};
    int i[8] = {};
    V3 v[8];
    // hush hands
    struct Hand { V3 pos, home, target; int state = 0; float t = 0; float hp = 9; bool alive = true; float shroud = 1; float flash = 0; bool down = false; } hands[2];
    std::vector<WeakPoint> wps;
    float deathT = 0;
};

// ------------------------------------------------------------------ persistent data
struct SaveData {
    int level = LV_HUB, spawn = 0;
    std::set<std::string> flags;
    float playTime = 0;
    int deaths = 0;
    int embers = 0;
    int endlessDepth = 1, endlessBest = 0;          // Endless Dark progress
    int res[4] = {0, 0, 0, 0};                      // (old saves) wood, stone, crystal, gears -> moved into inv
    struct Placed { int type, gx, gz, rot; };
    std::vector<Placed> builds;                     // (old saves) blueprint buildings -> refunded into inv
    float forgeTime = 0;
    // Wick's Island block world
    uint32_t voxSeed = 0;
    std::vector<std::string> voxLines;              // block edits ("vx x y z b m") until the island is loaded
    std::map<int, int> inv;                         // item id -> count
    int hot[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};       // hotbar (slot 0 is always the flame)
    std::map<int, std::map<int, int>> chests;       // block index -> items
    float dayT = 0.05f;                             // 0..1 day/night cycle
    bool hasBed = false; int bedX = 0, bedY = 0, bedZ = 0;
    int hat = 0;                                    // cosmetic hat worn by Wick (0 none)
    int fishCaught = 0;
    float raceBest = 0;                             // best candle-race time (0 = none)
    bool has(const std::string& f) const { return flags.count(f) > 0; }
};

struct Settings {
    float master = 0.8f, music = 0.6f, sfx = 0.8f;
    float sensitivity = 1.0f;
    bool invertY = false;
    int pixelMode = 2;       // index into PIXEL_LINES
    bool fullscreen = false;
    bool vsync = true;
    int shadows = 2;
    bool outlines = true;
    float fov = 70;
    bool shake = true;
    bool aimAssist = true;
    int uiScale = 0;         // 0 auto, else 1..5
    int keys[A_COUNT];       // scancode, or 1000+mouse button
    Settings();
};
static const int PIXEL_LINES[] = {180, 225, 270, 360, 540};
static const int PIXEL_MODES = 5;

// ------------------------------------------------------------------ screens
enum Screen { SC_TITLE, SC_INTRO, SC_PLAY, SC_PAUSE, SC_SETTINGS, SC_CONTROLS, SC_JOURNAL, SC_ENDING, SC_VICTORY, SC_CREDITS, SC_CONFIRM_NEW, SC_HOWTO, SC_SHOP, SC_BOTSHOP, SC_CRAFT, SC_SUPPLY, SC_CHEST };

struct DialogLine { std::string who; std::string text; };

struct Game {
    // --- systems
    Settings settings;
    SaveData save;
    std::string saveDir, configDir;
    std::string statusMsg; float statusT = 0;
    bool headless = false;
    bool quit = false;
    Renderer* R = nullptr;

    // --- screens / ui
    Screen screen = SC_TITLE, prevScreen = SC_TITLE;
    int menuSel = 0;
    float screenT = 0;
    std::vector<DialogLine> dialog; int dialogIdx = 0; float dialogChars = 0;
    std::function<void()> dialogDone;
    bool rebinding = false; int rebindIdx = 0;
    std::string toast; float toastT = 0;      // big centered message ("NEW ABILITY")
    std::string toastSub;
    std::string areaTitle; float areaTitleT = 0;
    float letterbox = 0;
    bool saveCorrupt = false;
    bool displayDirty = false;
    struct MenuRect { float x, y, w, h; int idx; };
    std::vector<MenuRect> menuRects;
    float mouseX = 0, mouseY = 0; bool mouseClick = false, mouseMoved = false;
    int journalSel = 0; Screen prevJournalScreen = SC_PLAY;

    // --- world
    Level lv;
    Player pl;
    Flame flame;
    std::vector<Enemy> enemies; int nextEnemyId = 1;
    struct PendingSpawn { EnemyType t; V3 pos; int arena; int gen = 0; };
    std::vector<PendingSpawn> pendingSpawns;
    std::vector<Projectile> projs;
    std::vector<Ring> rings;
    std::vector<Telegraph> teles;
    Boss boss;
    float time = 0;
    float transitionT = 0; LevelId pendingLevel = LV_HUB; int pendingSpawn = 0; bool transitioning = false;
    float shake = 0; float hitstop = 0;
    float bloomFlash = 0; V3 bloomPos; float bloomR = 0;
    float flashOverlay = 0; V3 flashColor;
    int lastLantern = 0;
    float respawnFade = 0;
    float lanternSaveCool = 0;
    int combo = 0; float comboT = 0; int bestCombo = 0;
    struct FloatText { V3 pos; std::string text; V3 col; float t = 0; };
    std::vector<FloatText> floats;
    int shopSel = 0;
    float enemyHpMul = 1;
    // --- Endless Dark & Wick's Island
    void completeEndlessLevel();
    // Wick's Island: a block world to mine and build in
    VoxWorld vox;
    int hotSel = 0; float hotNameT = 0;
    bool tgtValid = false; int tgtX = 0, tgtY = 0, tgtZ = 0; V3 tgtN;
    float mineT = 0; int mineX = 0, mineY = 0, mineZ = 0; bool mining = false;
    float useCd = 0, mobT = 0, growT = 0;
    struct LitBomb { int x, y, z; float t; };
    std::vector<LitBomb> bombs;
    std::vector<V3> voxLights; bool voxLightsDirty = true;
    float sleepT = 0; bool respawnAtBed = false;
    int craftCol = 0, craftSel[2] = {0, 0}, craftScroll[2] = {0, 0}; int chestKey = -1;
    void ensureVox();
    void updateVoxel(float dt, const InputState& in, InputState& masked);
    void voxPlayerPhysics(float dt, const InputState& in);
    void updateDayNight(float dt);
    bool isNight() const;
    void addItem(int item, int n, bool announce = true);
    bool takeItem(int item, int n);
    int itemCount(int item) const { auto it = save.inv.find(item); return it == save.inv.end() ? 0 : it->second; }
    bool placeBlock(int item, int x, int y, int z, int facing, int ladderSide);
    bool mineBlock(int x, int y, int z, bool drops);
    bool useBlock(int x, int y, int z);
    void explodeBomb(int x, int y, int z);
    void collectForges();
    void migrateOldIsland();
    void drawHotbar();
    void drawCraft(); void updateCraft(const InputState& in);
    void drawChest(); void updateChest(const InputState& in);
    void drawSupply(); void updateSupply(const InputState& in);
    bool craft(int recipe);
    // --- new powers: Overclock (slow time), Firewall (a wall of fire), Ember Grapple (swing from hooks)
    float ocCharge = 0, ocT = 0;
    struct FireWall { V3 a, b; float t = 0, life = 6; };
    std::vector<FireWall> firewalls; float fwCd = 0;
    bool grappling = false; float ropeLen = 0; V3 ropeAnchor;
    float worldScale() const { return ocT > 0 ? 0.3f : 1.f; }
    bool fxTick(float every) const { return std::fmod(time, every) < SIM_DT; }
    void updatePowers(float dt, const InputState& in);
    void updateGrapple(float dt, const InputState& in);
    void placeFirewall();
    // --- hacking puzzles, glitch, the city, side activities
    float glitch = 0;
    float alarmT = 0;                       // Lumen City: spotted by a searchlight/agent
    int raceIdx = -1; float raceT = 0;      // candle race
    int fishState = 0, fishSpot = -1, fishKind = 0; float fishT = 0, fishWait = 0, fishNeedle = 0, fishZone = 0.5f, fishZoneW = 0.2f;
    void updateExtras(float dt, const InputState& in);
    bool updateFishing(float dt, const InputState& in);
    void updatePuzzles(float dt);
    bool hitTerminal(const V3& a, const V3& b, V3& at);
    void updateCity(float dt);
    void spotted(V3 where);
    void startRace();
    void drawExtrasHUD();
    void cycleHat();
    // --- companion bots (bought from Sprocket with embers)
    struct BotRT { V3 pos; float timer = 0; float charge = 0; float flash = 0; };
    BotRT bots[5];
    struct Bolt { V3 pos, vel, target; float life = 2; bool bomb = false; float fuse = 0; bool alive = true; };
    std::vector<Bolt> bolts;
    bool botOwned(int i) const { return has("bot.own." + std::to_string(i)); }
    bool botEquipped(int i) const { return has("bot.eq." + std::to_string(i)); }
    int botSlots() const { return has("spark.core") ? 3 : 2; }
    int botsEquipped() const { int n = 0; for (int i = 0; i < 5; i++) if (botEquipped(i)) n++; return n; }
    bool buyOrToggleBot(int i);
    void updateBots(float dt);
    void drawBotShop();
    void updateBotShop(const InputState& in);
    int upLevel(const char* name) const { return countFlags(std::string("up.") + name + "."); }
    float comboMult() const { return std::min(4.f, 1.f + combo / 6.f); }
    void addCombo(V3 at);
    void dropEmbers(V3 pos, int amount);
    void breakUrn(int i);
    void floatText(V3 p, const std::string& t, V3 col) { floats.push_back({p, t, col, 0}); }
    void drawShop();
    void updateShop(const InputState& in);

    // --- camera
    float camYaw = 0, camPitch = -0.25f, camDist = 5.5f, camDistCur = 5.5f;
    V3 camEye, camTarget, aimDir, aimPoint;
    int aimTarget = -1;  // for crosshair highlight
    V3 camOverrideEye, camOverrideTarget; float camOverride = 0;

    // --- cinematic state (intro / ending)
    int cineStep = 0; float cineT = 0;

    // --- stats for victory
    bool justFinished = false;

    // --- showcase recording: clean frame (no HUD/dialog), plus an overlay drawn on top
    bool hideHUD = false;
    std::function<void()> overlayUI;

    // --- autoplay/test hooks
    bool botActive = false;
    bool botAim = false; V3 botAimPoint;
    bool godMode = false;
    std::string testLog;

    // ================================================================ API
    void init(Renderer* r, bool headlessMode);
    void frame(float dt, InputState& in, float lookDX, float lookDY);
    void update(float dt, const InputState& in);
    void draw();

    // flags & progression
    bool has(const std::string& f) const { return save.has(f); }
    void setFlag(const std::string& f) { save.flags.insert(f); }
    bool ability(const char* a) const { return has(std::string("ab.") + a); }
    int countFlags(const std::string& prefix) const;
    float maxHp() const;

    // persistence
    void initPaths();
    bool hasSave() const;
    bool loadGame();
    void writeSave();
    void deleteSave();
    void loadSettings();
    void writeSettings();
    void applySettings();

    // levels
    void newGame();
    void continueGame();
    void enterLevel(LevelId id, int spawn, bool fromSave = false);
    void requestTransition(LevelId id, int spawn);
    void buildLevel(LevelId id);
    void applyFlagsToLevel();
    void resetEncounters();
    void spawnEnemy(EnemyType t, V3 pos, int arena);

    // collision
    void gridBuild();
    void querySolids(const AABB& q, std::vector<AABB>& out, std::vector<int>* dynIds = nullptr);
    bool boxFree(const AABB& q);
    bool moveBox(V3& pos, V3& vel, float r, float h, float dt, bool& grounded, int* groundDyn, bool stepUp);
    bool wardBox(const DynSolid& d, AABB& out) const;
    bool wardBlocks(const V3& a, const V3& b) const;
    float raycast(const V3& o, const V3& d, float maxT, V3* nrm = nullptr, int* dynHit = nullptr, bool ignoreCrystals = false);
    float groundHeightBelow(const V3& p, float maxDown);

    // player
    void updatePlayer(float dt, const InputState& in);
    void updateFlame(float dt, const InputState& in);
    void updateCamera(float dt);
    void computeAim();
    void hurtPlayer(float dmg, V3 from);
    void killPlayer();
    void respawnAtLantern();
    void doWarp();
    void doBloom();
    void throwFlame(bool charged);
    void recallFlame();
    bool flameHitTest(const V3& a, const V3& b, const V3& c, float r);

    // enemies
    void updateEnemies(float dt);
    void damageEnemy(Enemy& e, float dmg, V3 from, bool charged);
    void updateProjectiles(float dt);
    void dropWax(V3 pos);

    // bosses
    void startBoss();
    void updateBoss(float dt);
    void bossDamage(int wp, float dmg, bool charged);
    void bossDefeated();
    void resetBoss();
    std::vector<WeakPoint>& bossWeakPoints();

    // interactions
    void updateWorldObjects(float dt);
    void lightBrazier(int i, bool byBloom);
    void openGateGroup(int group);
    void talk(int talker);
    void startDialog(const std::vector<DialogLine>& lines, std::function<void()> done = nullptr);
    void showToast(const std::string& a, const std::string& b) { toast = a; toastSub = b; toastT = 3.5f; }
    void collectPickup(Pickup& p);
    void checkpoint(int lanternIdx);

    // cinematics
    void startIntro();
    void updateIntro(float dt, const InputState& in);
    void startEnding();
    void updateEnding(float dt, const InputState& in);

    // fx helpers
    void burst(V3 p, V3 col, int n, float speed, float size, int kind = 3, float life = 0.6f, float grav = 4);
    void addShake(float s) { if (settings.shake) shake = std::max(shake, s); }

    // ui
    void updateMenus(float dt, const InputState& in);
    void drawUI();
    void drawHUD();
    void drawMenu(const char* title, const std::vector<std::string>& items, int sel, float y0);
    void finishRebind(int code);
    std::string currentObjective() const;
    void drawTutorial();
    void drawHowTo();
};

std::string memoryText(int index);   // lore candles
const char* levelName(LevelId id);
std::string actionName(int a);
std::string bindingName(int code);
