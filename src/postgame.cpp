// WICKLIGHT - things to do forever (mostly after the ending):
//  the Night Board's daily bounties (3 new ones every day), 30 trophies, the Boss Rush at the Echo Statue,
//  and Wren's flame dyes (a big ember sink: recolour your flame).
#include "builder.h"
#include <cstdio>
#include <ctime>
#include <algorithm>

const char* hatName(int h);
static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }
static const V4 COL_TEXT = C4(0xf0e6d0), COL_DIM = C4(0x8a8098), COL_HI = C4(0xffc060), COL_BORDER = C4(0x6a5040);

// ------------------------------------------------------------------ daily bounties
enum BountyKind { BK_KILL, BK_COMBO, BK_FISH, BK_DEPTH, BK_URN, BK_BRAZIER, BK_BOSS, BK_EMBERS, BK_RACE, BK_KINDS };
struct BountyDef { int lo, hi; const char* fmt; };
static const BountyDef BOUNTY[BK_KINDS] = {
    {25, 60, "Burn %d enemies"},
    {12, 25, "Land a %d-hit combo"},
    {2, 4, "Catch %d fish at the Wharf"},
    {2, 4, "Clear %d depths of the Endless Dark"},
    {15, 30, "Smash %d urns"},
    {8, 16, "Light %d braziers"},
    {1, 2, "Defeat %d bosses (any - the Endless Dark's count)"},
    {300, 800, "Collect %d embers"},
    {1, 2, "Win silver or gold in Flick's race %d time(s)"},
};
static int today() { return (int)((long long)time(nullptr) / 86400); }
static void dayBounties(int day, int kind[3], int target[3]) {
    Rng r(hash32((uint32_t)day * 977u + 5u));
    std::vector<int> pool;
    for (int k = 0; k < BK_KINDS; k++) pool.push_back(k);
    for (int i = 0; i < 3; i++) {
        int j = r.irange(0, (int)pool.size() - 1);
        kind[i] = pool[j]; pool.erase(pool.begin() + j);
        const BountyDef& d = BOUNTY[kind[i]];
        target[i] = d.lo + r.irange(0, d.hi - d.lo);
        if (kind[i] == BK_EMBERS) target[i] = target[i] / 50 * 50;
    }
}

void Game::ensureBounties() {
    int d = getenv("WICKLIGHT_BOUNTY_DAY") ? atoi(getenv("WICKLIGHT_BOUNTY_DAY")) : today();
    if (save.bountyDay == d) return;
    save.bountyDay = d;
    for (int& p : save.bountyProg) p = 0;
    save.bountyDone = 0;
}

std::string Game::bountyLine(int i) const {
    int kind[3], target[3];
    dayBounties(save.bountyDay, kind, target);
    char b[128];
    snprintf(b, sizeof(b), BOUNTY[kind[i]].fmt, target[i]);
    std::string s = b;
    if (save.bountyDone & (1 << i)) return "[x] " + s + "   DONE";
    return "[ ] " + s + "   " + std::to_string(std::min(save.bountyProg[i], target[i])) + "/" + std::to_string(target[i]);
}

void Game::bountyEvent(int kind, int amount) {
    if (botActive && !getenv("WICKLIGHT_BOUNTIES")) return;  // keep the campaign tests' economy unchanged
    ensureBounties();
    int k[3], target[3];
    dayBounties(save.bountyDay, k, target);
    for (int i = 0; i < 3; i++) {
        if (k[i] != kind || (save.bountyDone & (1 << i))) continue;
        int& p = save.bountyProg[i];
        p = kind == BK_COMBO ? std::max(p, amount) : p + amount;
        if (p < target[i]) continue;
        save.bountyDone |= 1 << i;
        save.bounties++;
        save.embers += 150;
        bool all = save.bountyDone == 7;
        if (all) { save.embers += 300; setFlag("bounty.allday"); }
        showToast("BOUNTY COMPLETE!", all ? "+150 embers, and +300 for clearing today's board!" : "+150 embers - check the Night Board for more");
        audio::play(SFX_CHECKPOINT); audio::play(SFX_SHARD, 0.7f, 1.3f);
        writeSave();
    }
}

// ------------------------------------------------------------------ trophies
struct Trophy { const char* name; const char* desc; std::function<bool(const Game&)> got; };
static const std::vector<Trophy>& trophies() {
    static std::vector<Trophy> T = {
        {"First Light", "Relight the Great Lamp", [](const Game& g) { return g.has("game.finished"); }},
        {"Shard Hoarder", "Find all 7 Ember Shards", [](const Game& g) { return g.countFlags("shard.") >= 7; }},
        {"Remembrance", "Find all 10 Memory Candles", [](const Game& g) { return g.countFlags("candle.") >= 10; }},
        {"Hatter", "Find 5 hats", [](const Game& g) { return g.countFlags("hat.") >= 5; }},
        {"Mad Hatter", "Find all 10 hats", [](const Game& g) { return g.countFlags("hat.") >= 10; }},
        {"Angler", "Catch 10 fish", [](const Game& g) { return g.save.fishCaught >= 10; }},
        {"Solid Gold", "Catch the Golden Carp", [](const Game& g) { return g.has("fish.3"); }},
        {"Speed Candle", "Win gold in Flick's race", [](const Game& g) { return g.has("race.m3"); }},
        {"Hero of the Hollow", "Finish every side job", [](const Game& g) { return g.has("quest.catdone") && g.has("quest.fenndone") && g.has("quest.carpdone"); }},
        {"Not The Hacker", "Escape Lumen City", [](const Game& g) { return g.has("fbi.done"); }},
        {"Root Access", "Crack GL1TCH's server room", [](const Game& g) { return g.has("clock.servers"); }},
        {"Deep Dark", "Reach depth 10 of the Endless Dark", [](const Game& g) { return g.save.endlessBest >= 10; }},
        {"Deeper Dark", "Reach depth 25", [](const Game& g) { return g.save.endlessBest >= 25; }},
        {"The Abyss", "Reach depth 50", [](const Game& g) { return g.save.endlessBest >= 50; }},
        {"Raid Survivor", "Clear an FBI raid in the Endless Dark", [](const Game& g) { return g.has("endless.raid"); }},
        {"Combo Artist", "Land a 20-hit combo", [](const Game& g) { return g.save.bestCombo >= 20; }},
        {"Combo Legend", "Land a 50-hit combo", [](const Game& g) { return g.save.bestCombo >= 50; }},
        {"Exterminator", "Burn 500 enemies", [](const Game& g) { return g.save.kills >= 500; }},
        {"Inferno", "Burn 2000 enemies", [](const Game& g) { return g.save.kills >= 2000; }},
        {"Robot Army", "Own all 5 robot friends", [](const Game& g) { return g.countFlags("bot.own.") >= 5; }},
        {"Fully Lit", "Max out every lantern upgrade", [](const Game& g) { return g.countFlags("up.") >= 12; }},
        {"Home Sweet Home", "Put a bed down on Wick's Island", [](const Game& g) { return g.save.hasBed; }},
        {"Ember Baron", "Hold 5000 embers at once", [](const Game& g) { return g.save.embers >= 5000; }},
        {"Bounty Hunter", "Finish 10 bounties", [](const Game& g) { return g.save.bounties >= 10; }},
        {"Clean Board", "Finish all 3 bounties in one day", [](const Game& g) { return g.has("bounty.allday"); }},
        {"Echoes", "Finish the Boss Rush", [](const Game& g) { return g.has("rush.done"); }},
        {"Rush Hour", "Finish the Boss Rush in under 5 minutes", [](const Game& g) { return g.save.rushBest > 0 && g.save.rushBest < 300; }},
        {"Untouchable", "Beat a boss without getting hit", [](const Game& g) { return g.has("boss.flawless"); }},
        {"New Look", "Buy a flame dye from Wren", [](const Game& g) { return g.countFlags("dye.") >= 1; }},
        {"Rainbow Wick", "Buy the Rainbow flame", [](const Game& g) { return g.has("dye.7"); }},
    };
    return T;
}
int trophyCount(const Game& g) { int n = 0; for (int i = 0; i < (int)trophies().size(); i++) if (g.has("tro." + std::to_string(i))) n++; return n; }

void Game::checkTrophies() {
    if (screen != SC_PLAY || !dialog.empty() || toastT > 0.5f) return;
    auto& T = trophies();
    for (int i = 0; i < (int)T.size(); i++) {
        std::string f = "tro." + std::to_string(i);
        if (has(f) || !T[i].got(*this)) continue;
        setFlag(f);
        showToast(std::string("TROPHY: ") + T[i].name, T[i].desc + std::string("   (") + std::to_string(trophyCount(*this)) + "/" + std::to_string(T.size()) + ")");
        audio::play(SFX_HEART, 0.6f, 1.4f);
        writeSave();
        return;  // one at a time, so each gets its moment
    }
}

// ------------------------------------------------------------------ Boss Rush (the Echo Statue)
static const LevelId RUSH_LV[5] = {LV_WHARF, LV_CLOCK, LV_VAULTS, LV_JUNK, LV_SPIRE};
static const char* RUSH_NAME[5] = {"THE DROWNED BELLWETHER", "GL1TCH", "THE VEIL MATRON", "KING RATTLETRAP", "THE HUSH"};

void Game::startRush() {
    rushActive = true; rushStage = 0; rushT = 0; rushNext = 0;
    audio::play(SFX_ROAR, 0.8f, 0.7f);
    rushStageEnter();
}

void Game::rushStageEnter() {
    enterLevel(RUSH_LV[rushStage], 0);
    dialog.clear();
    V3 C = lv.boss.center;
    pl.pos = pl.safePos = C + V3(-3, 0, -9.5f);
    pl.yaw = camYaw = 0.3f; camTarget = pl.pos + V3(0, 1.65f, 0);
    flame = Flame(); flame.pos = pl.pos + V3(0, 1.25f, 0);
    pl.hp = maxHp();
    startBoss();
    boss.hp = boss.maxHp = std::ceil(boss.maxHp * 1.3f);
    showToast("BOSS RUSH  " + std::to_string(rushStage + 1) + "/5", RUSH_NAME[rushStage]);
    areaTitleT = 0;
}

// a boss in the rush went down (called from bossDefeated)
bool rushBossDown(Game& g) {
    if (!g.rushActive) return false;
    g.boss.active = false;
    for (int gi : g.lv.boss.gates) g.lv.dyn[gi].open = true;
    g.rushNext = 2.5f;
    char b[64]; snprintf(b, sizeof(b), "%d:%04.1f", (int)(g.rushT / 60), std::fmod(g.rushT, 60.f));
    g.showToast(g.rushStage < 4 ? "ECHO " + std::to_string(g.rushStage + 1) + "/5 DEFEATED" : std::string("BOSS RUSH COMPLETE!"), std::string("time ") + b);
    audio::setSong(SONG_BOSS);
    return true;
}

// died during the rush: back to the Hollow
bool rushDied(Game& g) {
    if (!g.rushActive) return false;
    int stage = g.rushStage;
    g.rushActive = false;
    g.enterLevel(LV_HUB, 4);
    g.pl.hp = g.maxHp();
    g.respawnFade = 1;
    g.showToast("THE ECHOES WIN... THIS TIME", "You reached boss " + std::to_string(stage + 1) + " of 5. The statue is waiting.");
    return true;
}

void Game::updatePostgame(float dt) {
    ensureBounties();
    trophyT += dt;
    if (trophyT > 1) { trophyT = 0; checkTrophies(); }
    if (!rushActive) return;
    if (!transitioning && lv.id != RUSH_LV[rushStage]) { rushActive = false; return; }  // left the arena some other way
    if (!transitioning && rushNext <= 0) rushT += dt;
    if (rushNext > 0) {
        rushNext -= dt;
        if (rushNext <= 0) {
            rushStage++;
            if (rushStage >= 5) {
                rushActive = false;
                bool first = !has("rush.done");
                bool best = save.rushBest <= 0 || rushT < save.rushBest;
                if (best) save.rushBest = rushT;
                setFlag("rush.done");
                enterLevel(LV_HUB, 4);
                respawnFade = 1;
                char b[64]; snprintf(b, sizeof(b), "%d:%04.1f", (int)(rushT / 60), std::fmod(rushT, 60.f));
                std::vector<DialogLine> L = {{"ECHO STATUE", std::string("All five echoes fall silent. Time: ") + b + (best ? "  - a new record!" : "")}};
                if (first) { save.embers += 1000; L.push_back({"", "You received 1000 embers for your first Boss Rush!"}); }
                else { save.embers += 250; L.push_back({"", "You received 250 embers."}); }
                startDialog(L);
                writeSave();
            } else rushStageEnter();
        }
    }
}

// ------------------------------------------------------------------ Wren's flame dyes
struct Dye { const char* name; int cost; uint32_t outer, mid, core; };
static const Dye DYES[] = {
    {"Ember (classic)", 0, 0xff7a20, 0xffb040, 0xfff0a0},
    {"Azure", 400, 0x2080ff, 0x60c0ff, 0xe0f8ff},
    {"Verdant", 600, 0x20c040, 0x70ff70, 0xe8ffe0},
    {"Violet", 900, 0x8030e0, 0xc070ff, 0xf8e0ff},
    {"Rose", 1200, 0xff3070, 0xff80b0, 0xffe8f0},
    {"Ghostly", 2000, 0x8090a8, 0xd8e8ff, 0xffffff},
    {"Molten Gold", 3000, 0xc08010, 0xffd030, 0xfffff0},
    {"Rainbow", 5000, 0xff0000, 0xff0000, 0xffffff},
};
static const int NUM_DYES = 8;

void dyeColors(int dye, float t, V3& outer, V3& mid, V3& core) {
    if (dye <= 0 || dye >= NUM_DYES) { outer = hexColor(0xff7a20); mid = hexColor(0xffb040); core = hexColor(0xfff0a0); return; }
    if (dye == 7) {  // rainbow: the hue walks around the wheel
        auto hue = [](float h) { h = h - std::floor(h); float r = clampf(std::fabs(h * 6 - 3) - 1, 0, 1), g = clampf(2 - std::fabs(h * 6 - 2), 0, 1), b = clampf(2 - std::fabs(h * 6 - 4), 0, 1); return V3(r, g, b); };
        outer = hue(t * 0.25f); mid = hue(t * 0.25f + 0.15f) * 0.6f + V3(0.4f, 0.4f, 0.4f); core = V3(1, 1, 1);
        return;
    }
    outer = hexColor(DYES[dye].outer); mid = hexColor(DYES[dye].mid); core = hexColor(DYES[dye].core);
}

void Game::updateDyes(const InputState& in) {
    int n = NUM_DYES + 1;
    if (in.mUp) { shopSel = (shopSel + n - 1) % n; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { shopSel = (shopSel + 1) % n; audio::play(SFX_MENU_MOVE); }
    if (mouseMoved) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) shopSel = r.idx;
    bool clicked = false;
    if (mouseClick) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { shopSel = r.idx; clicked = true; }
    if (in.mBack || in.pressed[A_PAUSE] || in.pressed[A_INTERACT]) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    if (!(in.mOk || clicked)) return;
    if (shopSel == NUM_DYES) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    bool owned = shopSel == 0 || has("dye." + std::to_string(shopSel));
    if (!owned) {
        if (save.embers < DYES[shopSel].cost) { statusMsg = "Not enough embers - the Endless Dark pays well"; statusT = 1.5f; audio::play(SFX_MENU_BACK); return; }
        save.embers -= DYES[shopSel].cost;
        setFlag("dye." + std::to_string(shopSel));
        audio::play(SFX_UNLOCK);
    } else audio::play(SFX_MENU_SELECT);
    save.dye = shopSel;
    V3 o, m, c; dyeColors(save.dye, time, o, m, c);
    flashOverlay = 0.3f; flashColor = m;
    burst(pl.pos + V3(0, 1.6f, 0), m, 30, 4, 0.1f, 3, 0.8f, -1);
    writeSave();
}

void Game::drawDyes() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float pw = std::min(W - 20 * s, 320 * s), px = W / 2 - pw / 2, py = H * 0.1f;
    float ph = 40 * s + (NUM_DYES + 1) * 15 * s;
    R->uiFrame(px, py, pw, ph, C4(0x0c0a14, 0.93f), COL_HI);
    R->uiTextCentered(W / 2, py + 6 * s, "WREN'S FLAME DYES", s * 1.6f, COL_HI);
    R->uiTextCentered(W / 2, py + 20 * s, "Embers: " + std::to_string(save.embers), s, C4(0xffc070));
    menuRects.clear();
    float y = py + 34 * s;
    for (int i = 0; i <= NUM_DYES; i++) {
        bool sel = i == shopSel;
        if (sel) R->uiRect(px + 3 * s, y - 2 * s, pw - 6 * s, 14 * s, C4(0x3a2418, 0.8f));
        menuRects.push_back({px, y - 2 * s, pw, 14 * s, i});
        if (i == NUM_DYES) { R->uiText(px + 10 * s, y + 2 * s, "Leave", s * 1.2f, sel ? COL_HI : COL_TEXT); break; }
        V3 o, m, c; dyeColors(i, time, o, m, c);
        R->uiRect(px + 10 * s, y + 1 * s, 8 * s, 8 * s, V4(o.x, o.y, o.z, 1));
        R->uiRect(px + 12 * s, y + 3 * s, 4 * s, 4 * s, V4(m.x, m.y, m.z, 1));
        bool owned = i == 0 || has("dye." + std::to_string(i));
        R->uiText(px + 24 * s, y + 2 * s, DYES[i].name, s * 1.2f, sel ? COL_HI : COL_TEXT);
        std::string price = save.dye == i ? "WEARING" : (owned ? "owned" : std::to_string(DYES[i].cost));
        bool afford = !owned && save.embers >= DYES[i].cost;
        R->uiText(px + pw - R->uiTextWidth(price, s * 1.2f) - 10 * s, y + 2 * s, price, s * 1.2f, save.dye == i ? C4(0x7ac070) : (owned ? COL_TEXT : (afford ? C4(0xffc070) : COL_DIM)));
        y += 15 * s;
    }
    R->uiTextCentered(W / 2, py + ph + 6 * s, "Recolour your flame - it shows on Wick, in the air and on the walls.\nEnter: buy / wear   Esc/E: leave", s, COL_DIM);
}

// ------------------------------------------------------------------ the journal's second page
void Game::drawJournalPage2() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    ensureBounties();
    float x = 20 * s, y = 36 * s;
    R->uiText(x, y, "TODAY'S BOUNTIES (Night Board, in the plaza - new ones every day)", s * 1.2f, COL_HI); y += 14 * s;
    for (int i = 0; i < 3; i++) { R->uiText(x, y, bountyLine(i), s, (save.bountyDone & (1 << i)) ? C4(0x7ac070) : COL_TEXT); y += 10 * s; }
    y += 6 * s;
    char st[200];
    snprintf(st, sizeof(st), "Enemies burned %d   Best combo %d   Bounties done %d   Endless best %d   Boss Rush best %s",
             save.kills, save.bestCombo, save.bounties, save.endlessBest,
             save.rushBest > 0 ? (std::to_string((int)(save.rushBest / 60)) + ":" + (std::fmod(save.rushBest, 60.f) < 10 ? "0" : "") + std::to_string((int)std::fmod(save.rushBest, 60.f))).c_str() : "-");
    R->uiText(x, y, st, s, COL_DIM); y += 16 * s;
    auto& T = trophies();
    R->uiText(x, y, "TROPHIES " + std::to_string(trophyCount(*this)) + "/" + std::to_string(T.size()), s * 1.2f, COL_HI); y += 14 * s;
    float colW = (W - 40 * s) / 2;
    int rows = ((int)T.size() + 1) / 2;
    for (int i = 0; i < (int)T.size(); i++) {
        bool got = has("tro." + std::to_string(i));
        float tx = x + (i / rows) * colW, ty = y + (i % rows) * 10 * s;
        R->uiText(tx, ty, std::string(got ? "* " : "- ") + T[i].name, s, got ? C4(0xffd070) : COL_TEXT);
        R->uiText(tx + 100 * s, ty, T[i].desc, s, got ? C4(0xc0a870) : COL_DIM);
    }
    R->uiTextCentered(W / 2, H - 12 * s, "Left/Right: page   Esc/Tab: close", s, COL_DIM);
}

// ------------------------------------------------------------------ the new folk: the Night Board, the Echo Statue, Wren
bool talkPostgame(Game& g, int ti) {
    const Talker& t = g.lv.talkers[ti];
    if (t.who == 17) {  // the Night Board
        g.ensureBounties();
        std::vector<DialogLine> L = {{"NIGHT BOARD", "Today's bounties (new ones every day, 150 embers each, +300 for all three):"}};
        for (int i = 0; i < 3; i++) L.push_back({"NIGHT BOARD", g.bountyLine(i)});
        g.startDialog(L);
        return true;
    }
    if (t.who == 18) {  // the Echo Statue: Boss Rush
        if (!g.has("game.finished")) { g.startDialog({{"ECHO STATUE", "A statue of a candle, cold and silent. It is waiting for the Great Lamp to burn again."}}); return true; }
        char best[64];
        if (g.save.rushBest > 0) snprintf(best, sizeof(best), "Your best time: %d:%04.1f.", (int)(g.save.rushBest / 60), std::fmod(g.save.rushBest, 60.f));
        else snprintf(best, sizeof(best), "Nobody has beaten them all yet.");
        g.startDialog({{"ECHO STATUE", "The statue's flame flickers. Five echoes stir inside it - every guardian you ever beat, stronger now."},
                       {"ECHO STATUE", std::string("BOSS RUSH: all five, back to back, against the clock. Die and the rush is over. ") + best},
                       {"ECHO STATUE", "...The echoes are coming. Good luck, little flame."}},
                      [&g]() { g.startRush(); });
        return true;
    }
    if (t.who == 19) {  // Wren the dyer
        std::string key = "npc.19";
        if (!g.has(key)) {
            g.setFlag(key);
            g.startDialog({{"WREN", "Ooh, a flame that walks! Bit plain though, isn't it? Orange, orange, orange."},
                           {"WREN", "I mix dyes from crystal dust and moth wings. Blue fire, green fire, ROSE fire. Rainbow, if you're rich."},
                           {"WREN", "Embers only. The Endless Dark is full of them, I hear."}},
                          [&g]() { g.screen = SC_DYES; g.shopSel = g.save.dye; audio::play(SFX_CHECKPOINT); });
        } else { g.screen = SC_DYES; g.shopSel = g.save.dye; audio::play(SFX_CHECKPOINT); }
        return true;
    }
    return false;
}

void drawPostgameNPC(Renderer* R, int who, V3 pos, float yaw, float t, const Game& g) {
    M4 b = M4::translate(pos) * M4::rotY(yaw);
    if (who == 17) {  // a notice board on two posts, pinned with bounty notes
        for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(1.1f * s, 1.1f, 0)) * M4::scale(V3(0.15f, 2.2f, 0.15f)), T_DARKWOOD, V3(1, 1, 1));
        R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.6f, 0)) * M4::scale(V3(2.4f, 1.4f, 0.1f)), T_PLANK, V3(1, 1, 1));
        R->drawPart(PM_CUBE, b * M4::translate(V3(0, 2.45f, 0)) * M4::scale(V3(2.7f, 0.18f, 0.35f)), T_ROOF, V3(1, 1, 1));
        for (int i = 0; i < 3; i++) {
            bool done = (g.save.bountyDone & (1 << i)) != 0;
            R->drawPart(PM_CUBE, b * M4::translate(V3(-0.7f + i * 0.7f, 1.6f + (i == 1 ? 0.1f : 0), 0.06f)) * M4::rotZ(0.06f * (i - 1)) * M4::scale(V3(0.5f, 0.65f, 0.02f)), T_WHITE, done ? hexColor(0x8ac080) : hexColor(0xe8d8a8), 0.4f);
            R->drawPart(PM_CUBE, b * M4::translate(V3(-0.7f + i * 0.7f, 1.88f + (i == 1 ? 0.1f : 0), 0.08f)) * M4::scale(V3(0.06f, 0.06f, 0.03f)), T_WHITE, hexColor(0xd03030), 1.0f);
        }
        R->drawPart(PM_CUBE, b * M4::translate(V3(0, 2.25f, 0.06f)) * M4::scale(V3(1.6f, 0.16f, 0.02f)), T_WHITE, hexColor(0xffc060), 1.5f);
    } else if (who == 18) {  // the Echo Statue: a stone Wick with a violet echo-flame
        R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0.4f, 0)) * M4::scale(V3(1.6f, 0.8f, 1.6f)), T_MARBLE, V3(1, 1, 1));
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.5f, 0)) * M4::scale(V3(0.9f, 1.4f, 0.85f)), T_MARBLE, V3(0.85f, 0.85f, 0.95f));
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.45f, 0)) * M4::scale(V3(0.75f, 0.5f, 0.75f)), T_MARBLE, V3(0.85f, 0.85f, 0.95f));
        bool awake = g.has("game.finished");
        float f = std::sin(t * 9) * 0.06f;
        V3 col = awake ? hexColor(0xa060ff) : hexColor(0x404050);
        R->drawPart(PM_OCTA, b * M4::translate(V3(0, 3.1f, 0)) * M4::rotY(t * 2) * M4::scale(V3(0.4f + f, 0.8f, 0.4f + f)), T_WHITE, col, awake ? 2.5f : 0, 0, awake ? 1.f : 0.5f);
        for (int k = 0; k < 5 && awake; k++) {
            float a = t * 0.8f + k * TAU / 5;
            R->drawPart(PM_OCTA, M4::translate(pos + V3(std::sin(a) * 1.3f, 2.4f + 0.2f * std::sin(t * 2 + k), std::cos(a) * 1.3f)) * M4::scale(V3(0.12f, 0.2f, 0.12f)), T_WHITE, hexColor(0xc090ff), 2.0f);
        }
    } else if (who == 19) {  // Wren the dyer: a human in a splashy apron, with pots of coloured fire
        float bob = std::sin(t * 2 + who) * 0.03f;
        R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.7f, 0)) * M4::scale(V3(0.9f, 1.4f, 0.8f)), T_CLOTH, hexColor(0x6a4a8a));
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.2f + bob, 0)) * M4::scale(V3(0.55f, 0.6f, 0.45f)), T_CLOTH, hexColor(0xe8e0d0));
        for (int k = 0; k < 4; k++) R->drawPart(PM_CUBE, b * M4::translate(V3(-0.15f + k * 0.1f, 1.1f + bob + (k % 2) * 0.15f, 0.23f)) * M4::scale(V3(0.08f, 0.08f, 0.02f)), T_WHITE, hexColor(k == 0 ? 0x3080ff : k == 1 ? 0x30d050 : k == 2 ? 0xff3070 : 0xa040ff), 1.0f);
        R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.72f + bob, 0)) * M4::scale(V3(0.44f, 0.48f, 0.44f)), T_WHITE, hexColor(0xc89070));
        for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.1f * s, 1.76f + bob, 0.21f)) * M4::scale(V3(0.07f, 0.05f, 0.03f)), T_WHITE, hexColor(0x101010));
        R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.95f + bob, -0.05f)) * M4::scale(V3(0.5f, 0.3f, 0.5f)), T_CLOTH, hexColor(0xff6a30));
        for (int k = 0; k < 3; k++) {
            V3 p = pos + V3(std::cos(yaw) * (0.9f + k * 0.5f), 0, -std::sin(yaw) * (0.9f + k * 0.5f));
            R->drawPart(PM_CYL, M4::translate(p + V3(0, 0.3f, 0)) * M4::scale(V3(0.4f, 0.6f, 0.4f)), T_METAL, V3(0.6f, 0.6f, 0.65f));
            V3 o, m, c; dyeColors(k == 0 ? 1 : k == 1 ? 2 : 4, t, o, m, c);
            R->drawPart(PM_OCTA, M4::translate(p + V3(0, 0.75f, 0)) * M4::rotY(t * 3 + k) * M4::scale(V3(0.22f, 0.4f + 0.05f * std::sin(t * 11 + k), 0.22f)), T_WHITE, m, 2.5f);
        }
    }
}
