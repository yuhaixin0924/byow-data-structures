#include "session.hpp"
#include <iostream>
#include <stdexcept>

using namespace relic;
using namespace std::chrono_literals;

void expect(bool condition, const char* problem) {
    if (!condition) throw std::runtime_error(problem);
}

char directionTo(int from, int to) {
    const int delta = to - from;
    return delta == 1 ? 'D' : delta == -1 ? 'A' : delta == WIDTH ? 'S' : 'W';
}

void testFog() {
    GameSession session(3142, 1, {true}, 45);
    const auto& patches = session.fogPatches();
    expect(patches.size() == 3 && !session.fogAt(session.game.player), "fog must form three patches away from spawn");
    for (std::size_t i = 0; i < patches.size(); ++i)
        for (std::size_t j = i + 1; j < patches.size(); ++j)
            expect(manhattan(patches[i].center, patches[j].center) > 4, "fog must not overlap");
    auto& game = session.game;
    game.enemies.clear();
    int entry = -1, outside = -1;
    for (int cell = 0; cell < CELLS && entry < 0; ++cell) if (session.fogAt(cell))
        for (int neighbor : game.world.adjacency[cell]) if (!session.fogAt(neighbor)) {
            entry = cell;
            outside = neighbor;
            break;
        }
    expect(entry >= 0, "fixture needs a walkable fog boundary");
    game.player = outside;
    session.setTarget(entry);
    session.updateTime(1s);
    expect(!session.moveTowardTarget() && game.player == entry, "fog must interrupt auto-walk on entry");
    expect(session.stunLeft() == 5s && !session.fogAt(entry), "entry must stun and disperse fog");
    session.updateTime(5999ms);
    session.act(directionTo(entry, outside), true);
    session.attack();
    expect(game.turn == 1 && !session.releaseUltimate() && session.stunLeft() == 1ms, "stun must block all attacks and jumps");
    session.updateTime(6s);
    expect(session.stunLeft() == 0ms && !session.fogAt(entry), "stun should expire before fog reforms");
    session.updateTime(13s);
    expect(session.fogAt(entry) && session.stunLeft() == 0ms, "reforming fog must not stun a stationary player");
    session.act(directionTo(entry, outside));
    session.act(directionTo(outside, entry));
    expect(session.stunLeft() == 5s, "entering the reformed fog must stun again");
}

void testRain() {
    GameSession session(836, 2, {}, 93);
    auto& game = session.game;
    const auto initial = session.rainCenter();
    expect(std::abs(initial[0] - point(game.player).x) + std::abs(initial[1] - point(game.player).y) > 2,
           "rain should start away from spawn");
    session.updateTime(240ms);
    expect(initial == session.rainCenter(), "rain should begin its move at the first tracking tick");
    session.updateTime(360ms);
    auto halfway = session.rainCenter();
    expect(std::abs(halfway[0] - initial[0]) + std::abs(halfway[1] - initial[1]) == .5f,
           "rain must interpolate between tiles");
    const auto player = point(game.player);
    expect(std::abs(halfway[0] - player.x) + std::abs(halfway[1] - player.y) <
           std::abs(initial[0] - player.x) + std::abs(initial[1] - player.y), "rain must move toward the player");

    GameSession damage(836, 2, {}, 93);
    auto& wet = damage.game;
    wet.enemies.clear();
    const int center = Point{static_cast<int>(initial[0]), static_cast<int>(initial[1])}.id();
    wet.player = center;
    damage.updateTime(0s);
    expect(wet.health == 90, "entering rain must deal ten damage");
    damage.updateTime(1999ms);
    expect(wet.health == 90, "rain must not deal repeated instant damage");
    damage.updateTime(2s);
    expect(wet.health == 80, "staying in rain must deal damage after two seconds");
    wet.health = 10;
    damage.updateTime(4s);
    expect(wet.health == 0 && wet.status == "lost", "fatal rain must end the session");
}

void testMirror() {
    GameSession session(5017, 3, {}, 795);
    auto& game = session.game;
    game.enemies.clear();
    const auto reachable = game.world.distanceFrom(game.world.start);
    int mirror = -1, count = 0, floors = 0;
    for (int cell = 0; cell < CELLS; ++cell) {
        if (reachable[cell] >= 0) ++floors;
        if (!session.mirrorAt(cell)) continue;
        expect(reachable[cell] >= 0, "mirror must be reachable");
        ++count;
        if (!game.world.adjacency[cell].empty()) mirror = cell;
    }
    expect(count == std::lround(floors * .08) && mirror >= 0, "about eight percent of floors must hold mirrors");
    const int approach = game.world.adjacency[mirror].front();
    const auto oldFlowers = game.crystals;
    game.player = approach;
    session.setTarget(game.world.exit);
    session.act(directionTo(approach, mirror));
    expect(session.didTeleport() && !session.mirrorAt(mirror) && !session.mirrorAt(game.player) &&
           game.steps == 1 && game.spent == game.world.cost(mirror) && session.route().path.front() == game.player,
           "mirror must consume one move, disarm and refresh the route from the safe landing");
    expect(game.crystals.size() == oldFlowers.size(), "mirror must retain uncollected flower count");
    auto remaining = game.crystals;
    for (int cell : remaining)
        expect(cell != game.player && !session.mirrorAt(cell) &&
               std::count(remaining.begin(), remaining.end(), cell) == 1 &&
               std::find(oldFlowers.begin(), oldFlowers.end(), cell) == oldFlowers.end() &&
               std::count(session.flowerCells.begin(), session.flowerCells.end(), cell) == 1,
               "each flower must relocate uniquely and keep its display slot");
    const int landing = game.player;
    game.player = approach;
    session.act(directionTo(approach, mirror));
    expect(!session.didTeleport() && game.player == mirror, "used mirror must behave as ordinary terrain");
    game.player = landing;
    remaining.push_back(game.world.exit);
    for (int destination : remaining) {
        // Navigate without stepping on any active mirror.
        std::array<int, CELLS> parent{};
        parent.fill(-1);
        std::vector<int> queue{game.player};
        parent[game.player] = game.player;
        for (std::size_t i = 0; i < queue.size() && parent[destination] < 0; ++i)
            for (int next : game.world.adjacency[queue[i]]) if (parent[next] < 0 && !session.mirrorAt(next)) {
                parent[next] = queue[i];
                queue.push_back(next);
            }
        expect(parent[destination] >= 0, "exit and relocated flowers must share an ordinary route");
        std::vector<int> path;
        for (int cell = destination; cell != game.player; cell = parent[cell]) path.push_back(cell);
        for (auto step = path.rbegin(); step != path.rend(); ++step) session.act(directionTo(game.player, *step));
    }
    expect(game.status == "won" && game.collected == 4, "mirror level must remain winnable within its budgets");
}

void testWeapons() {
    GameSession::Arsenal arsenal{};
    arsenal.fill(true);
    GameSession session(2026, 0, arsenal);
    auto& game = session.game;
    const int adjacent = game.world.adjacency[game.player].front();
    const int remote = game.world.exit;
    game.enemies = {{adjacent, adjacent, adjacent, 1000}, {remote, remote, remote, 1000}};
    session.attack();
    expect(game.enemies.front().health == 990 && game.enemies.back().health == 1000, "ordinary attacks must deal ten damage");
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        game.turn = 0; // Leave guards in their fixture cells.
        const int closeHealth = game.enemies.front().health, remoteHealth = game.enemies.back().health;
        expect(session.selectWeapon(static_cast<int>(i)) && session.releaseUltimate(), "each unlocked weapon must fire");
        expect(session.cooldown(i) == 30s && !session.releaseUltimate(), "each weapon must have its own 30-second cooldown");
        const auto damage = std::lround(20 * std::pow(1.5, i));
        expect(game.enemies.front().health == closeHealth - damage &&
               game.enemies.back().health == remoteHealth - (i == 1 ? damage : 0), "weapon range or damage did not match");
    }
    session.updateTime(29999ms);
    expect(session.cooldown(0) == 1ms, "weapon should stay unavailable until exact cooldown boundary");
    session.updateTime(30s);
    expect(session.cooldown(0) == 0ms, "weapon must be usable again after thirty seconds");
    session.selectWeapon(0);
    game.enemies.front().health = 1;
    game.enemies.front().warningCell = game.player;
    expect(session.releaseUltimate() && game.kills == 1 && game.enemies.front().warningCell == -1 && game.health == 100,
           "a fatal ultimate must cancel a guard warning and count the kill");
    session.selectWeapon(2);
    expect(!session.releaseUltimate() && session.cooldown(2) == 0ms, "no target should preserve cooldown");
    GameSession locked(2026);
    expect(locked.weapon() == -1 && !locked.selectWeapon(0) && !locked.releaseUltimate(), "locked weapon must not be usable");
    for (int level : {0, 4, 5, 6, 7})
        expect(GameSession(2026, level).hazard == GameSession::Hazard::None, "other levels must not inherit hazards");
}

void testWeaponRotation() {
    GameSession session(2026, 0, {true, false, true, false, true});
    auto& game = session.game;
    const int adjacent = game.world.adjacency[game.player].front();
    game.enemies.clear();
    expect(session.weapon() == 0 && !session.releaseUltimate() && session.weapon() == 0 &&
           session.cooldown(0) == 0ms, "a missed cast must not rotate or start cooldown");
    game.enemies = {{adjacent, adjacent, adjacent, 1000}};
    expect(session.releaseUltimate() == &weapons[0] && session.weapon() == 2,
           "the first cast must advance past locked weapons");
    session.updateTime(1s);
    expect(session.releaseUltimate() == &weapons[2] && session.weapon() == 4,
           "automatic casts must advance toward higher-level weapons");
    session.selectWeapon(4);
    expect(session.releaseUltimate() == &weapons[4] && session.weapon() == 4 &&
           !session.releaseUltimate(), "manual selection must stay on the chosen weapon");
    session.resumeWeaponRotation();
    expect(!session.releaseUltimate(), "automatic rotation must wait if all weapons are cooling down");
    session.updateTime(30s);
    expect(session.weapon() == 0 && session.releaseUltimate() == &weapons[0] &&
           session.cooldown(2) == 1s, "rotation must wrap and use the first ready weapon");
    session.updateTime(31s);
    expect(session.weapon() == 2, "rotation must resume when the next weapon finishes cooldown");
}

void testLateLevels() {
    const std::array<uint32_t, 4> seeds{7129, 9953, 4309, 15941};
    const std::array<int, 4> health{120, 180, 260, 400};
    for (int level = 4; level < 8; ++level) {
        GameSession session(seeds[level - 4], level, {true, true, true}, 83, 3);
        {
            auto encounter = session;
            auto& world = encounter.game.world;
            for (auto& enemy : encounter.game.enemies)
                if (enemy.kind != EnemyKind::Boss) enemy.health = 0;
            const int origin = encounter.boss()->at;
            const int near = world.adjacency[origin].front();
            for (int cell : world.adjacency[near]) if (manhattan(origin, cell) == 2) {
                encounter.game.player = cell;
                break;
            }
            encounter.updateTime(1s);
            expect(manhattan(encounter.game.player, encounter.boss()->at) == 1, "boss approaches an idle player");
            encounter.updateTime(1100ms);
            expect(encounter.boss()->warningCell == encounter.game.player, "boss telegraphs before hitting");
            const int hp = encounter.game.health;
            encounter.updateTime(1850ms);
            expect(encounter.game.health == hp, "boss respects dodge window");
            encounter.updateTime(1900ms);
            expect(encounter.game.health == hp - encounter.boss()->damage, "idle player takes exactly one boss hit");
        }
        auto& game = session.game;
        expect(session.bossFlowerPending() && game.crystals.size() == 3 &&
               session.boss()->health == health[level - 4], "late levels must reserve the fourth flower for a boss");
        expect(std::count_if(game.enemies.begin(), game.enemies.end(), [](const Enemy& enemy) {
            return enemy.kind == EnemyKind::Guard;
        }) == level, "ordinary guard count must increase each level");
        if (level == 4) {
            expect(session.fakeFlowers().size() == 2, "level five needs two false flowers");
            const int decoy = session.fakeFlowers().front();
            game.player = game.world.adjacency[decoy].front();
            session.act(directionTo(game.player, decoy));
            expect(session.stunLeft() == 5s && game.collected == 0 && session.fakeFlowers().size() == 1,
                   "a decoy must disappear and stun without granting a flower");
            session.updateTime(5s);
        }
        if (level == 5) {
            auto middle = game.world.start;
            auto& adjacent = game.world.adjacency[middle];
            expect(adjacent.size() >= 2, "fixture needs room for two mummies");
            int count = 0;
            for (auto& enemy : game.enemies) if (enemy.kind == EnemyKind::Mummy) {
                expect(enemy.health == 40 && enemy.damage == 5, "mummy stats");
                if (count < 2) enemy.at = adjacent[count];
                ++count;
            }
            expect(count == 4, "level six needs four mummies");
            session.updateTime(100ms);
            expect(game.health == 90, "mummy proximity damage must stack");
            session.updateTime(2099ms);
            expect(game.health == 90, "a mummy must observe its own hit interval");
        }
        if (level == 6) {
            int hidden = 0, visible = 0;
            for (const auto& enemy : game.enemies) if (enemy.kind == EnemyKind::Ghost) {
                expect(enemy.health == 60 && enemy.damage == 9, "ghost stats");
                (enemy.active() ? visible : hidden)++;
                if (!enemy.active())
                    expect(std::find(session.statueCells().begin(), session.statueCells().end(), enemy.at)
                           != session.statueCells().end(), "hidden ghost must be inside a statue");
            }
            expect(hidden == 3 && visible == 2, "five ghosts with three hidden");
            const int statue = session.statueCells().front();
            game.player = game.world.adjacency[statue].front();
            session.updateTime(100ms);
            expect(std::any_of(game.enemies.begin(), game.enemies.end(), [statue](const Enemy& enemy) {
                return enemy.kind == EnemyKind::Ghost && enemy.at == statue && enemy.active();
            }), "walking near a statue must reveal its ghost");
            session.updateTime(800ms);
            expect(game.health == 91, "a revealed ghost must deal nine damage");
        }
        if (level == 7) {
            expect(session.coffinCells().size() == 3 && !session.coffinsOpened(), "three coffins start shut");
            std::vector<int> appearances;
            for (const auto& enemy : game.enemies) if (enemy.kind == EnemyKind::Puppet) {
                expect(!enemy.active() && enemy.health == 100 && enemy.appearance >= 0 && enemy.appearance < 8 &&
                       enemy.appearance != 3 && std::find(appearances.begin(), appearances.end(), enemy.appearance) == appearances.end(),
                       "coffins must contain three distinct non-player heroes");
                appearances.push_back(enemy.appearance);
            }
            expect(appearances.size() == 3, "every coffin must have a puppet");
            session.setPlayerHero(appearances.front(), 8);
            expect(std::none_of(game.enemies.begin(), game.enemies.end(), [&](const Enemy& enemy) {
                return enemy.kind == EnemyKind::Puppet && enemy.appearance == appearances.front();
            }), "changing heroes must keep the player's appearance distinct");
            game.player = game.world.adjacency[session.boss()->at].front();
            session.updateTime(100ms);
            expect(session.coffinsOpened() && std::count_if(game.enemies.begin(), game.enemies.end(), [](const Enemy& enemy) {
                return enemy.kind == EnemyKind::Puppet && enemy.active();
            }) == 3, "approaching the final boss must wake all three coffins");
        }
        for (auto& enemy : game.enemies) if (enemy.kind != EnemyKind::Boss) enemy.health = 0;
        const auto flowers = game.crystals;
        for (int cell : flowers) {
            game.player = game.world.adjacency[cell].front();
            session.act(directionTo(game.player, cell));
        }
        game.player = game.world.adjacency[game.world.exit].front();
        session.act(directionTo(game.player, game.world.exit));
        expect(game.collected == 3 && game.status == "playing", "three ground flowers must not open the exit");
        const int bossCell = session.boss()->at;
        game.player = game.world.adjacency[bossCell].front();
        for (auto& enemy : game.enemies) if (enemy.kind == EnemyKind::Boss) enemy.health = level == 4 ? 10 : 30;
        if (level == 4) session.attack();
        else {
            session.selectWeapon(1);
            expect(session.releaseUltimate(), "ultimate must also be able to defeat the boss");
        }
        expect(!session.bossFlowerPending() && game.crystals == std::vector<int>{bossCell} && game.collected == 3,
               "defeating the boss must drop exactly one pickable flower");
        session.act(directionTo(game.player, bossCell));
        game.player = game.world.adjacency[game.world.exit].front();
        session.act(directionTo(game.player, game.world.exit));
        expect(game.collected == 4 && game.status == "won" && game.crystals.empty(), "boss flower must unlock the exit");
    }
}

void testChallengeLimits() {
    GameSession countdown(2026, 7);
    const int clock = countdown.clocks.front();
    countdown.game.enemies.clear();
    countdown.game.player = countdown.game.world.adjacency[clock].front();
    const int delta = clock - countdown.game.player;
    countdown.act(delta == 1 ? 'D' : delta == -1 ? 'A' : delta == WIDTH ? 'S' : 'W');
    if (countdown.timeLimit() != 270 || countdown.clocks.size() != 1)
        throw std::runtime_error("last level clock must grant 30 seconds and disappear");
    countdown.updateTime(std::chrono::seconds(269));
    if (countdown.game.status != "playing") throw std::runtime_error("bonus time did not extend the round");
    countdown.updateTime(std::chrono::seconds(270));
    if (countdown.game.status != "lost") throw std::runtime_error("round did not end at zero");

    for (bool stepBudget : {true, false}) {
        GameSession limited(2026);
        auto& game = limited.game;
        game.enemies.clear();
        const auto destination = game.world.adjacency[game.player].front();
        const int direction = destination - game.player;
        const char command = direction == 1 ? 'D' : direction == -1 ? 'A' : direction == WIDTH ? 'S' : 'W';
        game.steps = stepBudget ? limited.limits.steps : 0;
        game.spent = stepBudget ? 0 : limited.limits.cost;
        limited.act(command);
        if (game.player == destination || game.status != "lost" || !game.limitFailure)
            throw std::runtime_error("exceeding a budget must end the round before moving");
    }
}

int main() {
    try {
        testChallengeLimits();
        testFog();
        testRain();
        testMirror();
        testWeapons();
        testWeaponRotation();
        testLateLevels();
        GameSession session(2026);
        const int start = session.game.player;
        session.setTarget(session.game.world.exit);
        for (int algorithm = 0; algorithm < 3; ++algorithm) {
            session.setAlgorithm(algorithm);
            const auto path = session.route();
            if (path.path.empty() || path.path.front() != start || path.path.back() != session.game.world.exit)
                throw std::runtime_error("selected algorithm must return a route to target");
        }
        session.setAlgorithm(0);
        session.moveTowardTarget();
        if (session.game.turn != 1 || session.game.player == start || session.notice.empty())
            throw std::runtime_error("route action must move the game session");
        const int beforeWall = session.game.player;
        session.setTarget(-1);
        if (!session.route().path.empty()) throw std::runtime_error("invalid target must clear the route");
        session.moveTowardTarget();
        if (session.game.player != beforeWall || session.game.turn != 1)
            throw std::runtime_error("missing route must not alter game state");
        const int turns = session.game.turn;
        int wallNeighbor = WIDTH;
        while (wallNeighbor < CELLS &&
               (!session.game.world.walkable(wallNeighbor) ||
                session.game.world.walkable(wallNeighbor - WIDTH))) ++wallNeighbor;
        if (wallNeighbor == CELLS) throw std::runtime_error("fixture must contain a wall boundary");
        session.game.player = wallNeighbor;
        session.act('W');
        if (session.game.turn != turns || session.notice.find(u8"挡住") == std::string::npos)
            throw std::runtime_error("blocked movement must update UI feedback without consuming a turn");
        std::cout << "PASS: native game session integration\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
