// WICKLIGHT - more things to do:
//  hacking puzzles (Clockwood, Lumen City), the city's searchlights / alarms / helipad holdout,
//  side jobs in the Hollow (Mira's cat, Old Fenn's braziers, Flick's candle race, Gil's Golden Carp),
//  fishing at the Wharf, and cosmetic hats (found in secret spots, worn via Marrow's mirror).
#include "builder.h"
#include <cstdio>
#include <algorithm>

extern int g_dmgKind;
static Rng xrng(9001);

static const int NUM_HATS = 10;
const char* hatName(int h) {
    static const char* n[] = {"No hat", "Cat Ears", "Lamplighter's Cap", "Fisher's Hat", "Golden Crown", "Top Hat", "Party Hat", "Wizard Hat", "Viking Helmet", "Propeller Cap", "FBI Cap"};
    return (h >= 0 && h <= NUM_HATS) ? n[h] : "?";
}
static const char* FISH_NAMES[4] = {"MINNOW", "GLOWFIN", "GLOOMEEL", "GOLDEN CARP"};
static const int FISH_EMBERS[4] = {3, 8, 12, 30};
static const float FISH_SPEED[4] = {2.2f, 3.0f, 3.8f, 4.6f};
static const float FISH_ZONE[4] = {0.36f, 0.28f, 0.22f, 0.17f};
const float RACE_GOLD = 14, RACE_SILVER = 18, RACE_BRONZE = 26;

static float segPointDist(const V3& a, const V3& b, const V3& c) {
    V3 ab = b - a; float l2 = len2(ab);
    float t = l2 > 1e-8f ? clampf(dot(c - a, ab) / l2, 0, 1) : 0;
    return len(a + ab * t - c);
}

// ------------------------------------------------------------------ building helpers
void addPuzzle(LB& b, const AABB& zone, const std::vector<V3>& pos, const std::vector<float>& yaw, int len, uint32_t seed, int group, const std::string& flag, int reward, V3 rewardPos) {
    int pi = (int)b.L.puzzles.size();
    HackPuzzle hp; hp.zone = zone; hp.group = group; hp.flag = flag; hp.reward = reward; hp.rewardPos = rewardPos;
    Rng r(seed);
    int n = (int)pos.size(), prev = -1;
    for (int k = 0; k < len; k++) {
        int t = r.irange(0, n - 1);
        if (t == prev && n > 1) t = (t + 1 + r.irange(0, n - 2)) % n;
        hp.seq.push_back(t); prev = t;
    }
    b.L.puzzles.push_back(hp);
    for (int i = 0; i < n; i++) {
        Terminal t; t.pos = pos[i] + V3(0, 1.45f, 0); t.yaw = yaw[i]; t.puzzle = pi; t.slot = i;
        b.L.terminals.push_back(t);
        M4 m = M4::translate(pos[i]) * M4::rotY(yaw[i]);
        b.L.decor.cube(m * M4::translate(V3(0, 0.5f, 0)) * M4::scale(V3(0.8f, 1.0f, 0.55f)), T_METAL);
        b.L.decor.cube(m * M4::translate(V3(0, 1.45f, -0.08f)) * M4::scale(V3(1.15f, 0.95f, 0.22f)), T_METAL);
        b.L.decor.color = hexColor(0x203028);
        b.L.decor.cube(m * M4::translate(V3(0, 1.0f, 0.05f)) * M4::scale(V3(0.6f, 0.08f, 0.5f)), T_GRATE);
        b.L.decor.color = V3(1, 1, 1);
        b.solid(AABB(pos[i] - V3(0.42f, 0, 0.42f), pos[i] + V3(0.42f, 1.0f, 0.42f)), T_METAL, -1, V3(1, 1, 1), false);
    }
}

static const char* powerFlag(int idx) { return idx == 1 ? "pw.firewall" : idx == 2 ? "pw.grapple" : "pw.none"; }

void applyExtraFlags(Game& g) {
    Level& L = g.lv;
    for (auto& p : L.puzzles) {
        if (!g.has(p.flag)) continue;
        p.state = 3;
        if (p.reward >= 0 && !g.has(powerFlag(p.reward))) {
            Pickup k; k.type = PK_POWER; k.index = p.reward; k.pos = p.rewardPos; k.flag = powerFlag(p.reward);
            L.pickups.push_back(k);
        }
    }
    if (g.has("quest.lamps")) for (auto& b : L.braziers) if (b.gateGroup == 70) b.lit = true;
    if (L.hasHoldout && g.has(L.holdout.flag)) { L.holdout.done = true; L.holdout.rescueT = 99; }
}

// ------------------------------------------------------------------ hats & powers
static void giveHat(Game& g, int h) {
    g.setFlag("hat." + std::to_string(h));
    g.save.hat = h;
    g.showToast(std::string("NEW HAT: ") + hatName(h), "Swap hats at the mirror in Marrow's workshop");
    audio::play(SFX_SHARD, 1, 1.2f);
    g.burst(g.pl.pos + V3(0, 1.6f, 0), hexColor(0xffd070), 30, 4, 0.08f, 3, 0.8f, 0);
}

void collectExtra(Game& g, Pickup& p) {
    if (p.type == PK_HAT) {
        giveHat(g, p.index);
        g.writeSave();
    } else if (p.type == PK_POWER) {
        audio::play(SFX_HEART);
        g.addShake(0.3f);
        g.flashOverlay = 0.6f; g.flashColor = p.index == 1 ? hexColor(0xffa040) : hexColor(0xffd070);
        g.burst(p.pos, hexColor(0xffd070), 60, 8, 0.12f, 3, 1.0f, 0);
        std::vector<DialogLine> L;
        if (p.index == 1) {
            g.setFlag("ab.firewall");
            g.showToast("NEW POWER: FIREWALL", "[" + bindingName(g.settings.keys[A_FIREWALL]) + "] drops a wall of fire");
            L = {{"FIREWALL", "GL1TCH's own security program - rewritten in flame."},
                 {"FIREWALL", "Press [" + bindingName(g.settings.keys[A_FIREWALL]) + "] to drop a WALL OF FIRE in front of you."},
                 {"FIREWALL", "Enemies can't walk through it and burn while they try. Shots melt in it. It even lights braziers."}};
        } else if (p.index == 2) {
            g.setFlag("ab.grapple"); g.setFlag("fbi.done");
            g.showToast("NEW POWER: EMBER GRAPPLE", "Hold [" + bindingName(g.settings.keys[A_GRAPPLE]) + "] while your flame is on a hook");
            L = {{"SPROCKET", "BEEP. Special delivery! I built it from the hacker's stolen cables."},
                 {"EMBER GRAPPLE", "Throw your flame onto a brass hook, then HOLD [" + bindingName(g.settings.keys[A_GRAPPLE]) + "]."},
                 {"EMBER GRAPPLE", "A rope of fire pulls you in - steer to SWING, let go to fling yourself through the air."},
                 {"SPROCKET", "Now get in the chopper. The FBI has decided you are 'probably not the hacker'. BOOP."}};
        }
        g.startDialog(L);
        g.writeSave();
    }
}

void Game::cycleHat() {
    std::vector<int> owned = {0};
    for (int h = 1; h <= NUM_HATS; h++) if (has("hat." + std::to_string(h))) owned.push_back(h);
    if (owned.size() == 1) { startDialog({{"", "An old mirror. A small candle looks back at you. ...You'd look good in a hat."}, {"", "Hats are hidden in secret corners all over the world - and some people might reward you with one."}}); return; }
    int cur = 0;
    for (int i = 0; i < (int)owned.size(); i++) if (owned[i] == save.hat) cur = i;
    save.hat = owned[(cur + 1) % owned.size()];
    audio::play(SFX_MENU_SELECT, 1, 1.2f);
    burst(pl.pos + V3(0, 1.6f, 0), hexColor(0xfff0c0), 14, 2, 0.06f, 3, 0.5f, 0);
    showToast(save.hat ? std::string("WEARING: ") + hatName(save.hat) : std::string("NO HAT"), std::to_string((int)owned.size() - 1) + " of " + std::to_string(NUM_HATS) + " hats found");
    writeSave();
}

// ------------------------------------------------------------------ talking to the new folk
bool talkExtra(Game& g, int ti) {
    Talker& t = g.lv.talkers[ti];
    std::vector<DialogLine> L;
    switch (t.who) {
        case 11:  // Mira and the cat
            if (!g.has("quest.cat") && !g.has("quest.catfound")) {
                L = {{"MIRA", "Oh! A walking candle! Please, have you seen my cat? Whiskers?"},
                     {"MIRA", "He chased a moth up the old treehouse just west of here, and now he won't come down."},
                     {"MIRA", "You've got that flame trick, right? Throw it up to the hook by the treehouse and pop up there!"}};
                g.setFlag("quest.cat");
            } else if (!g.has("quest.catfound")) {
                L = {{"MIRA", "He's still up in the treehouse. West of here, by the big leafy tree. The brass hook!"}};
            } else if (!g.has("quest.catdone")) {
                L = {{"MIRA", "WHISKERS! You silly, silly thing. Thank you, little candle!"},
                     {"MIRA", "Here - I knitted these for him but he hates them. They'd look SO cute on you."}};
                g.setFlag("quest.catdone");
                g.save.embers += 40;
                L.push_back({"", "You received 40 embers and CAT EARS!"});
                g.startDialog(L, [&g]() { giveHat(g, 1); g.writeSave(); });
                return true;
            } else {
                static const char* lines[] = {"Whiskers says thank you. He said it with his eyes.", "He's been sleeping on my head ever since. Warm, huh?", "If you see any moths, DON'T tell Whiskers."};
                L = {{"MIRA", lines[(int)(g.time * 3) % 3]}};
            }
            break;
        case 12:  // Whiskers
            if (!g.has("quest.catfound")) {
                g.setFlag("quest.catfound");
                L = {{"WHISKERS", "Mrrrow?"}, {"", "Whiskers sniffs your flame, decides you're a friend, and hops down branch by branch to the plaza."}};
                t.pos = V3(-22.4f, 0, -0.6f); t.yaw = -PI / 2;
                audio::play(SFX_CATCH, 1, 1.6f);
            } else L = {{"WHISKERS", (int)(g.time * 2) % 2 ? "Purrrrr." : "Mrrp."}};
            break;
        case 13: {  // Old Fenn
            int lit = 0, total = 0;
            for (auto& b : g.lv.braziers) if (b.gateGroup == 70) { total++; if (b.lit) lit++; }
            if (!g.has("quest.fenn")) {
                L = {{"OLD FENN", "Evening, little flame. I lit the streets of the Hollow for fifty years."},
                     {"OLD FENN", "Six braziers ring the city. All cold now. My pole can't reach them anymore."},
                     {"OLD FENN", "Light all six for an old man? Your flame reaches farther than mine ever did."}};
                g.setFlag("quest.fenn");
            } else if (!g.has("quest.lamps")) {
                L = {{"OLD FENN", std::to_string(lit) + " of " + std::to_string(total) + " burning. They ring the whole Hollow - by the docks, the bridges, the back alleys."}};
            } else if (!g.has("quest.fenndone")) {
                L = {{"OLD FENN", "Look at that. Every street lit, like the old days."}, {"OLD FENN", "Here. My old lamplighter's cap. It fits a candle better than it ever fit me."},
                     {"", "You received 50 embers and the LAMPLIGHTER'S CAP!"}};
                g.setFlag("quest.fenndone");
                g.save.embers += 50;
                g.startDialog(L, [&g]() { giveHat(g, 2); g.writeSave(); });
                return true;
            } else L = {{"OLD FENN", "The streets look right again. Thank you, little flame."}};
        } break;
        case 14: {  // Flick and the candle race
            if (g.raceIdx >= 0) { L = {{"FLICK", "GO GO GO! The next ring is glowing!"}}; break; }
            char best[64];
            if (g.save.raceBest > 0) snprintf(best, sizeof(best), "Your best: %.1f seconds.", g.save.raceBest);
            else snprintf(best, sizeof(best), "Nobody's beaten my record yet.");
            if (!g.has("npc.14")) {
                g.setFlag("npc.14");
                L = {{"FLICK", "Hey! You're fast for a candle. Wanna RACE?"},
                     {"FLICK", "Twelve rings of candlelight loop the plaza. Fly through them in order - the next one glows gold."},
                     {"FLICK", "Under 26 seconds gets bronze, under 18 silver, and under 14... the GOLDEN CROWN. Dash a lot!"}};
            } else L = {{"FLICK", std::string("Another round? ") + best}};
            L.push_back({"FLICK", "Ready... set... GO!"});
            g.startDialog(L, [&g]() { g.startRace(); });
            return true;
        }
        case 15:  // the wardrobe mirror
            g.cycleHat();
            return true;
        case 16:  // a fishing spot
            if (g.fishState) return true;
            g.fishState = 1; g.fishSpot = ti; g.fishT = 0;
            g.fishWait = 1.5f + 2.5f * hashf(g.save.fishCaught, ti, 7);
            g.pl.yaw = t.yaw; g.pl.vel = V3();
            audio::play(SFX_THROW, 0.7f, 0.6f);
            g.statusMsg = "Line cast. Wait for a bite... then press [" + bindingName(g.settings.keys[A_INTERACT]) + "]!"; g.statusT = 2.5f;
            return true;
        default: return false;
    }
    g.startDialog(L);
    g.writeSave();
    return true;
}

// ------------------------------------------------------------------ fishing
bool Game::updateFishing(float dt, const InputState& in) {
    if (fishState == 0) return false;
    if (lv.id != LV_WHARF || pl.dead || fishSpot < 0 || fishSpot >= (int)lv.talkers.size()) { fishState = 0; return false; }
    bool press = in.pressed[A_INTERACT] || in.pressed[A_THROW];
    bool cancel = in.pressed[A_JUMP] || in.pressed[A_DASH];
    fishT += dt;
    V3 bob = lv.talkers[fishSpot].pos + V3(std::sin(lv.talkers[fishSpot].yaw), 0, std::cos(lv.talkers[fishSpot].yaw)) * 3.2f;
    bob.y = lv.waterY;
    switch (fishState) {
        case 1:
            if (cancel || press) { fishState = 0; statusMsg = "You reel your line back in."; statusT = 1.5f; return false; }
            if (fishT >= fishWait) {
                fishState = 2; fishT = 0;
                audio::playAt(SFX_SPLASH, bob, 1, 1.4f);
                burst(bob, hexColor(0x9ad8e8), 16, 3, 0.08f, 0, 0.5f, 8);
                floatText(bob + V3(0, 1, 0), "!", hexColor(0xfff0a0));
            }
            break;
        case 2:
            if (press) {
                fishState = 3; fishT = 0;
                float r = hashf(save.fishCaught, fishSpot, 991);
                if (has("quest.carp") && !has("fish.3") && (r < 0.4f || save.fishCaught % 3 == 2)) fishKind = 3;
                else if (r < 0.04f) fishKind = 3;
                else fishKind = r < 0.55f ? 0 : (r < 0.85f ? 1 : 2);
                fishZoneW = FISH_ZONE[fishKind];
                fishZone = 0.2f + 0.6f * hashf(save.fishCaught, 3, 11);
                audio::play(SFX_TICK, 1, 0.8f);
            } else if (cancel || fishT > 0.9f) { fishState = 0; statusMsg = "It got away..."; statusT = 1.5f; audio::play(SFX_MENU_BACK, 0.6f); return false; }
            break;
        case 3:
            fishNeedle = 0.5f - 0.5f * std::cos(fishT * FISH_SPEED[fishKind]);
            if (press) {
                fishState = 0;
                if (std::fabs(fishNeedle - fishZone) <= fishZoneW * 0.5f + 0.02f) {
                    int k = fishKind;
                    bool first = !has("fish." + std::to_string(k));
                    setFlag("fish." + std::to_string(k));
                    save.fishCaught++;
                    save.embers += FISH_EMBERS[k];
                    showToast(std::string("CAUGHT A ") + FISH_NAMES[k] + "!", "+" + std::to_string(FISH_EMBERS[k]) + " embers" + (first ? "   (new species!)" : ""));
                    audio::play(SFX_SHARD, 0.9f, k == 3 ? 0.8f : 1.2f);
                    audio::playAt(SFX_SPLASH, bob, 1, 0.9f);
                    burst(bob + V3(0, 0.5f, 0), k == 3 ? hexColor(0xffd040) : hexColor(0x9ad8e8), 30, 4, 0.1f, 3, 0.7f, 6);
                    writeSave();
                } else { statusMsg = "SNAP! The line broke."; statusT = 1.5f; audio::play(SFX_BREAK, 0.6f, 1.6f); }
                return false;
            }
            if (cancel || fishT > 8) { fishState = 0; statusMsg = "It got away..."; statusT = 1.5f; return false; }
            break;
    }
    return true;
}

// ------------------------------------------------------------------ candle race
void Game::startRace() {
    if (lv.race.empty()) return;
    raceIdx = 0; raceT = 0;
    audio::play(SFX_BELL, 0.8f, 1.6f);
    statusMsg = "GO! Through the glowing rings, in order!"; statusT = 2;
}

static void finishRace(Game& g) {
    float t = g.raceT;
    g.raceIdx = -1;
    int medal = t < RACE_GOLD ? 3 : (t < RACE_SILVER ? 2 : (t < RACE_BRONZE ? 1 : 0));
    bool best = g.save.raceBest <= 0 || t < g.save.raceBest;
    if (best) g.save.raceBest = t;
    char tb[32]; snprintf(tb, sizeof(tb), "%.2f s", t);
    static const char* M[4] = {"No medal", "BRONZE", "SILVER", "GOLD"};
    g.showToast(std::string("FINISH! ") + tb, std::string(M[medal]) + (best ? "   - new best!" : ""));
    audio::play(SFX_CHECKPOINT); audio::play(SFX_BELL, 1, 1.3f);
    int reward = 0;
    for (int m = 1; m <= medal; m++) {
        std::string f = "race.m" + std::to_string(m);
        if (!g.has(f)) { g.setFlag(f); reward += m * 20; }
    }
    std::vector<DialogLine> L;
    if (medal == 0) L = {{"FLICK", std::string(tb) + "! Not bad... but not a medal either. Talk to me to try again!"}};
    else L = {{"FLICK", std::string(tb) + "! That's " + M[medal] + "!" + (medal < 3 ? " Gold is under 14 seconds..." : " NOBODY has ever beaten that!")}};
    if (reward) { g.save.embers += reward; L.push_back({"", "You received " + std::to_string(reward) + " embers!"}); }
    if (medal == 3 && !g.has("hat.4")) {
        L.push_back({"FLICK", "The Golden Crown is yours, champ. Wear it with pride!"});
        g.startDialog(L, [&g]() { giveHat(g, 4); g.writeSave(); });
    } else g.startDialog(L);
    g.writeSave();
}

// ------------------------------------------------------------------ hacking puzzles
static int findTerminal(const Level& L, int puzzle, int slot) {
    for (int i = 0; i < (int)L.terminals.size(); i++) if (L.terminals[i].puzzle == puzzle && L.terminals[i].slot == slot) return i;
    return -1;
}

bool Game::hitTerminal(const V3& a, const V3& b, V3& at) {
    for (auto& T : lv.terminals) {
        HackPuzzle& P = lv.puzzles[T.puzzle];
        if (P.state != 0 && P.state != 2) continue;
        if (segPointDist(a, b, T.pos) > 0.85f) continue;
        at = T.pos + V3(std::sin(T.yaw), 0, std::cos(T.yaw)) * 0.6f;
        if (P.state == 0) {
            P.state = 1; P.t = 0; P.showIdx = 0;
            statusMsg = "WATCH the order the terminals flash..."; statusT = 2.5f;
            return true;
        }
        if (P.seq[P.progress] == T.slot) {
            T.flash = 0.6f; T.flashKind = 1;
            P.progress++;
            audio::playAt(SFX_TICK, T.pos, 1.2f, 1.0f + 0.18f * T.slot);
            burst(T.pos, hexColor(0x60ff90), 10, 3, 0.06f, 3, 0.4f, 0);
            if (P.progress >= (int)P.seq.size()) {
                P.state = 3;
                setFlag(P.flag);
                if (P.group >= 0) openGateGroup(P.group);
                audio::play(SFX_UNLOCK, 1, 1.2f);
                showToast("ACCESS GRANTED", P.reward >= 0 ? "Something unlocked in the server room..." : "Hack complete");
                for (auto& o : lv.terminals) if (o.puzzle == T.puzzle) { o.flash = 1.2f; o.flashKind = 1; }
                if (P.reward >= 0 && !has(powerFlag(P.reward))) {
                    Pickup k; k.type = PK_POWER; k.index = P.reward; k.pos = P.rewardPos; k.flag = powerFlag(P.reward);
                    lv.pickups.push_back(k);
                    burst(P.rewardPos, hexColor(0xffa040), 40, 6, 0.1f, 3, 0.8f, 0);
                }
                writeSave();
            } else { statusMsg = "ACCESS " + std::to_string(P.progress) + "/" + std::to_string(P.seq.size()); statusT = 1.2f; }
        } else {
            // wrong terminal: the system fights back
            for (auto& o : lv.terminals) if (o.puzzle == T.puzzle) { o.flash = 1.0f; o.flashKind = 2; }
            glitch = std::max(glitch, 0.8f);
            audio::playAt(SFX_SHIELD, T.pos, 1.2f, 0.5f);
            audio::play(SFX_ROAR, 0.5f, 2.2f);
            addShake(0.25f);
            statusMsg = "ACCESS DENIED - firewall drones incoming!"; statusT = 2.5f;
            int drones = 0;
            for (auto& e : enemies) if (e.alive && e.type == E_DRONE) drones++;
            V3 c = P.zone.center(); c.y = P.zone.lo.y + 1.0f;
            for (int k = 0; k < 2 && drones < 3; k++, drones++) spawnEnemy(E_DRONE, c + V3(k ? 3.f : -3.f, 0, 0), -1);
            P.state = 4; P.t = 0; P.fails++;
        }
        return true;
    }
    return false;
}

void Game::updatePuzzles(float dt) {
    for (auto& t : lv.terminals) t.flash = std::max(0.f, t.flash - dt);
    for (int pi = 0; pi < (int)lv.puzzles.size(); pi++) {
        HackPuzzle& P = lv.puzzles[pi];
        bool inside = P.zone.contains(pl.pos + V3(0, 0.5f, 0));
        switch (P.state) {
            case 0:
                if (inside && !pl.dead) { P.state = 1; P.t = 0; P.showIdx = 0; statusMsg = "WATCH the order the terminals flash..."; statusT = 2.5f; }
                break;
            case 1: {
                P.t += dt;
                const float first = 0.8f, gap = 0.8f;
                if (P.showIdx < (int)P.seq.size() && P.t >= first + P.showIdx * gap) {
                    int ti = findTerminal(lv, pi, P.seq[P.showIdx]);
                    if (ti >= 0) {
                        lv.terminals[ti].flash = 0.55f; lv.terminals[ti].flashKind = 0;
                        audio::playAt(SFX_BLIP, lv.terminals[ti].pos, 1.4f, 0.8f + 0.18f * lv.terminals[ti].slot);
                    }
                    P.showIdx++;
                }
                if (P.showIdx >= (int)P.seq.size() && P.t >= first + P.seq.size() * gap) {
                    P.state = 2; P.progress = 0;
                    statusMsg = "Now BURN them in that order! [" + bindingName(settings.keys[A_THROW]) + "]"; statusT = 3;
                }
            } break;
            case 2:
                if (!inside && distXZ(P.zone.center(), pl.pos) > 45) P.state = 0;
                break;
            case 4:
                P.t += dt;
                if (P.t > 2.2f) { P.state = 1; P.t = 0; P.showIdx = 0; P.progress = 0; }
                break;
            default: break;
        }
    }
}

// ------------------------------------------------------------------ Lumen City
void Game::spotted(V3 where) {
    if (alarmT > 0) { alarmT = std::max(alarmT, 6.f); return; }
    alarmT = 9;
    audio::play(SFX_ROAR, 0.6f, 2.4f);
    audio::play(SFX_BELL, 0.5f, 2.0f);
    flashOverlay = std::max(flashOverlay, 0.25f); flashColor = hexColor(0xff3030);
    statusMsg = "SPOTTED! FBI backup is on the way!"; statusT = 2.5f;
    floatText(where + V3(0, 2.5f, 0), "SPOTTED!", hexColor(0xff5050));
    int agents = 0;
    for (auto& e : enemies) if (e.alive && e.type == E_AGENT) agents++;
    std::vector<std::pair<float, V3>> pts;
    for (auto& p : lv.reinforce) { float d = distXZ(p, pl.pos); if (d > 10 && d < 45 && std::fabs(p.y - pl.pos.y) < 4) pts.push_back({d, p}); }
    std::sort(pts.begin(), pts.end(), [](const std::pair<float, V3>& a, const std::pair<float, V3>& b) { return a.first < b.first; });
    for (int k = 0; k < (int)pts.size() && k < 2 && agents < 8; k++, agents++) {
        spawnEnemy(E_AGENT, pts[k].second, -1);
        enemies.back().state = 1; enemies.back().alerted = true;
        burst(pts[k].second + V3(0, 1, 0), hexColor(0x3060ff), 12, 3, 0.1f, 3, 0.5f, 0);
    }
}

void Game::updateCity(float dt) {
    if (lv.id != LV_CITY && lv.searchlights.empty()) { alarmT = 0; return; }
    alarmT = std::max(0.f, alarmT - dt);
    for (auto& s : lv.searchlights) {
        float u = 0.5f - 0.5f * std::cos(TAU * (time / s.period + s.phase));
        s.spot = lerp(s.a, s.b, u);
        if (!pl.dead && distXZ(pl.pos, s.spot) < s.radius && std::fabs(pl.pos.y - s.spot.y) < 3) spotted(s.spot);
    }
    if (!lv.hasHoldout) return;
    Holdout& H = lv.holdout;
    if (H.done) { H.rescueT += dt; return; }
    if (!H.active) {
        if (!pl.dead && H.zone.contains(pl.pos + V3(0, 0.5f, 0))) {
            H.active = true; H.t = 0; H.spawnT = 1.5f; H.wave = 0;
            showToast("HOLD THE HELIPAD", "Sprocket's chopper is 30 seconds out!");
            audio::play(SFX_ROAR, 0.8f, 1.8f);
            alarmT = 99;
        }
        return;
    }
    H.t += dt; H.spawnT -= dt;
    alarmT = 99;
    if (H.spawnT <= 0 && H.t < H.dur - 4) {
        H.spawnT = 4.5f;
        int alive = 0;
        for (auto& e : enemies) if (e.alive) alive++;
        // spawn at the two edges farthest from Wick
        std::vector<std::pair<float, V3>> pts;
        for (auto& p : H.spawns) pts.push_back({-distXZ(p, pl.pos), p});
        std::sort(pts.begin(), pts.end(), [](const std::pair<float, V3>& a, const std::pair<float, V3>& b) { return a.first < b.first; });
        static const EnemyType W[3][3] = {{E_AGENT, E_AGENT, E_AGENT}, {E_AGENT, E_DRONE, E_DRONE}, {E_DRONE, E_DRONE, E_VIRUS}};
        int n = H.wave % 3 == 2 ? 3 : 2;
        for (int k = 0; k < n && alive < 6; k++, alive++) {
            V3 p = pts[k % pts.size()].second + V3(k >= 2 ? 1.5f : 0, 0.2f, 0);
            spawnEnemy(W[H.wave % 3][k], p, -1);
            enemies.back().state = 1; enemies.back().alerted = true;
            burst(p + V3(0, 1, 0), hexColor(0x3060ff), 14, 3, 0.1f, 3, 0.5f, 0);
        }
        H.wave++;
    }
    if (H.t >= H.dur) {
        H.done = true; H.active = false; H.rescueT = 0;
        alarmT = 0;
        for (auto& e : enemies) if (e.alive) { e.alive = false; burst(e.pos + V3(0, 1, 0), hexColor(0xffd070), 12, 4, 0.1f, 3, 0.5f, 0); }
        projs.clear(); teles.clear();
        setFlag(H.flag);
        Pickup k; k.type = PK_POWER; k.index = 2; k.pos = H.pad + V3(0, 1.2f, -4); k.flag = "pw.grapple";
        if (!has("pw.grapple")) lv.pickups.push_back(k);
        audio::play(SFX_HEART, 0.8f, 1.2f);
        showToast("SPROCKET TO THE RESCUE!", "The agents scatter from the rotor wash");
        startDialog({{"SPROCKET", "BEEP BEEP! Chopper incoming! Out of the way, agents!"},
                     {"SPROCKET", "I dropped you a present on the pad. Grab it, then hop in!"}});
        writeSave();
    }
}

// ------------------------------------------------------------------ per-step update for all of the above
void Game::updateExtras(float dt, const InputState& in) {
    (void)in;
    updatePuzzles(dt);
    updateCity(dt);
    if (raceIdx >= 0) {
        if (lv.id != LV_HUB || pl.dead || lv.race.empty()) raceIdx = -1;
        else {
            raceT += dt;
            const RaceRing& r = lv.race[raceIdx];
            if (len(pl.pos + V3(0, 0.8f, 0) - r.pos) < 1.9f) {
                audio::play(SFX_TICK, 1, 1.0f + 0.07f * raceIdx);
                burst(r.pos, hexColor(0xffd070), 16, 4, 0.08f, 3, 0.4f, 0);
                raceIdx++;
                if (raceIdx >= (int)lv.race.size()) finishRace(*this);
            } else if (raceT > 90) { raceIdx = -1; statusMsg = "Too slow! Talk to Flick to race again."; statusT = 2; }
        }
    }
    // the Clockwood flickers now and then, wherever GL1TCH's code has crept in
    if (lv.id == LV_CLOCK && fxTick(9.3f)) glitch = std::max(glitch, 0.22f);
    if (lv.id == LV_CITY && alarmT > 0 && fxTick(0.9f)) audio::play(SFX_BLIP, 0.25f, 2.6f);
}

// side-job brazier ring (Old Fenn): called when one of the group-70 braziers is lit
void fennLampLit(Game& g) {
    int lit = 0, total = 0;
    for (auto& b : g.lv.braziers) if (b.gateGroup == 70) { total++; if (b.lit) lit++; }
    if (lit >= total && !g.has("quest.lamps")) {
        g.setFlag("quest.lamps");
        g.showToast("THE STREETS ARE LIT", "Tell Old Fenn!");
        audio::play(SFX_CHECKPOINT);
        g.writeSave();
    } else if (!g.has("quest.lamps")) { g.statusMsg = "Street brazier lit: " + std::to_string(lit) + " of " + std::to_string(total); g.statusT = 2; }
}
