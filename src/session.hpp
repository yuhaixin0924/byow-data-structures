#pragma once
#include "engine.hpp"

namespace relic {
struct Weapon {
    const char* name;
    const char* skill;
    int damage;
    bool wholeMaze = false;
};

inline constexpr std::array<Weapon, 7> weapons{{
    {u8"长风鞭", u8"长风裂阵", 20},
    {u8"幻影之泪", u8"幻泪之域", 30, true},
    {u8"圆月弯刀", u8"圆月回斩", 45},
    {u8"魔杖", u8"星辉迸裂", 68},
    {u8"梅花亮银枪", u8"梅影银锋", 101},
    {u8"生死牌", u8"生死判决", 152},
    {u8"虚空之剑", u8"虚空断界", 228}
}};

class GameSession {
public:
    using Time = std::chrono::milliseconds;
    using Arsenal = std::array<bool, weapons.size()>;
    enum class Hazard { None, Fog, Rain, Mirror };
    struct Fog { int center; Time reforms{}; };
    static constexpr int hazardRadius = 2;
    static constexpr auto fogReformDelay = std::chrono::seconds(12);
    static constexpr auto rainInterval = std::chrono::seconds(4);

private:
    std::array<SearchResult, 3> searches{};
    Random hazardRandom;
    const Arsenal unlocked;
    std::array<Time, weapons.size()> readyAt{};
    int manualWeapon = -1;
    std::size_t rotationStart = 0;
    Time elapsed{}, stunnedUntil{}, rainDamageAt{}, rainStep{};
    std::vector<int> reachable;
    std::vector<int> mirrorSafe;
    std::vector<Fog> fog;
    std::array<bool, CELLS> mirrors{};
    int rainFrom = -1, rainTo = -1;
    bool teleported = false;
    int bossIndex = -1;
    static constexpr int bossFlowerSlot = 3;
    static constexpr int sealedFlower = -2;
    std::vector<int> decoys, statues, coffins;
    bool puppetsAwake = false;
    void refreshRoutes() {
        static constexpr const char* names[] = {"BFS", "DIJKSTRA", "ASTAR"};
        for (int i = 0; i < 3; ++i) searches[i] = game.world.search(game.player, target, names[i]);
    }
    void shuffleCells(std::vector<int>& cells) {
        for (int n = static_cast<int>(cells.size()); n > 1; --n)
            std::swap(cells[n - 1], cells[hazardRandom.between(0, n - 1)]);
    }
    bool safeCell(int cell) const {
        const auto absent = [cell](const auto& cells) {
            return std::find(cells.begin(), cells.end(), cell) == cells.end();
        };
        return cell != game.player && cell != game.world.exit && !game.occupied(cell) &&
               absent(clocks) && absent(game.potions) && absent(game.crystals) &&
               std::none_of(game.enemies.begin(), game.enemies.end(), [cell](const Enemy& guard) {
                   return guard.health > 0 && guard.warningCell == cell;
               });
    }
    void initHazard() {
        if (hazard == Hazard::None) return;
        const auto distances = game.world.distanceFrom(game.world.start);
        for (int cell = 0; cell < CELLS; ++cell)
            if (distances[cell] >= 0) reachable.push_back(cell);
        auto candidates = reachable;
        shuffleCells(candidates);
        if (hazard == Hazard::Mirror) {
            const auto count = static_cast<std::size_t>(std::lround(reachable.size() * .08));
            // Keep at least one route through the original objectives free of mirror traps.
            std::array<bool, CELLS> reserved{};
            const auto reserveRoute = [&](int destination) {
                for (const int cell : game.world.search(game.world.start, destination, "DIJKSTRA").path)
                    reserved[cell] = true;
            };
            reserveRoute(game.world.exit);
            for (int flower : game.crystals) reserveRoute(flower);
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](int cell) {
                return reserved[cell] || cell == game.world.start || cell == game.world.exit;
            }), candidates.end());
            for (std::size_t i = 0; i < std::min(count, candidates.size()); ++i) mirrors[candidates[i]] = true;
            // Destinations and relocated flowers share the exit's trap-free component.
            std::array<bool, CELLS> visited{};
            mirrorSafe.push_back(game.world.exit);
            visited[game.world.exit] = true;
            for (std::size_t i = 0; i < mirrorSafe.size(); ++i)
                for (int next : game.world.adjacency[mirrorSafe[i]]) if (!mirrors[next] && !visited[next]) {
                    visited[next] = true;
                    mirrorSafe.push_back(next);
                }
            return;
        }
        for (int cell : candidates) {
            if (manhattan(cell, game.world.start) <= hazardRadius + 1) continue;
            if (hazard == Hazard::Rain) {
                rainFrom = rainTo = cell;
                break;
            }
            if (std::any_of(fog.begin(), fog.end(), [cell](const Fog& patch) {
                return manhattan(cell, patch.center) <= 2 * hazardRadius;
            })) continue;
            fog.push_back({cell, {}});
            if (fog.size() == 3) break;
        }
    }
    void initEncounter(int level, int hero, int heroCount) {
        if (level < 4) return;
        const int bossCell = game.crystals.back();
        game.crystals.pop_back();
        flowerCells[bossFlowerSlot] = sealedFlower;
        const auto distance = game.world.distanceFrom(game.world.start);
        const auto addEnemy = [&](int cell, EnemyKind kind, int health, int damage, bool awake = true) -> Enemy& {
            auto& enemy = game.enemies.emplace_back();
            enemy.at = enemy.home = cell;
            enemy.target = game.world.rooms[(game.enemies.size() + 5) % game.world.rooms.size()].center();
            enemy.kind = kind;
            enemy.health = health;
            enemy.damage = damage;
            enemy.awake = awake;
            return enemy;
        };
        bossIndex = static_cast<int>(game.enemies.size());
        addEnemy(bossCell, EnemyKind::Boss, bossHealth(level), 14 + (level - 4) * 2);
        std::vector<int> positions;
        for (int cell = 0; cell < CELLS; ++cell) {
            if (distance[cell] >= 6 && game.world.tile[cell] == Floor &&
                game.world.adjacency[cell].size() >= 3 && safeCell(cell)) positions.push_back(cell);
        }
        shuffleCells(positions);
        const auto takePosition = [&] {
            const int cell = positions.back();
            positions.pop_back();
            return cell;
        };
        for (int i = 3; i < guardCount(level); ++i) {
            const int tier = guardTier(level, i);
            auto& guard = addEnemy(takePosition(), EnemyKind::Guard, (2 + tier) * Game::expeditionAttackDamage,
                                   guardDamage(tier));
            guard.tier = tier;
        }
        if (level == 4) {
            for (int i = 0; i < 2; ++i) decoys.push_back(takePosition());
        } else if (level == 5) {
            for (int i = 0; i < 4; ++i) addEnemy(takePosition(), EnemyKind::Mummy, 40, 5).nextMove = std::chrono::seconds(2);
        } else if (level == 6) {
            for (int i = 0; i < 5; ++i) statues.push_back(takePosition());
            for (int i = 0; i < 5; ++i) {
                const int cell = i < 3 ? statues[i] : takePosition();
                addEnemy(cell, EnemyKind::Ghost, 60, 9, i >= 3).nextMove = std::chrono::seconds(2);
            }
        } else {
            std::vector<int> appearances;
            for (int i = 0; i < heroCount; ++i) if (i != hero) appearances.push_back(i);
            shuffleCells(appearances);
            for (int i = 0; i < 3; ++i) {
                const int cell = takePosition();
                coffins.push_back(cell);
                auto& puppet = addEnemy(cell, EnemyKind::Puppet, 100, 8, false);
                puppet.appearance = appearances[i];
            }
        }
    }
    void updateBossReward() {
        const auto* leader = boss();
        if (!leader || leader->health > 0 || !bossFlowerPending()) return;
        const int cell = leader->at;
        game.crystals.push_back(cell);
        flowerCells[bossFlowerSlot] = cell;
        if (game.status == "playing") notice = u8"首领倒下了，拾取它掉落的花。";
    }
    void updateEncounter() {
        const auto* leader = boss();
        if (!leader || game.status != "playing") return;
        using namespace std::chrono_literals;
        if (!coffins.empty() && leader->active() && !puppetsAwake &&
            (leader->health < bossHealth(7) || (manhattan(leader->at, game.player) <= 6 && game.lineOfSight(game.player, leader->at)))) {
            puppetsAwake = true;
            notice = u8"终极首领唤醒了棺材中的傀儡！";
        }
        int damage = 0;
        for (std::size_t i = 0; i < game.enemies.size(); ++i) {
            auto& enemy = game.enemies[i];
            if (enemy.health <= 0 || enemy.kind == EnemyKind::Boss || enemy.kind == EnemyKind::Guard) continue;
            if (!enemy.awake) {
                const bool awaken = enemy.kind == EnemyKind::Puppet ? puppetsAwake :
                    manhattan(enemy.at, game.player) <= 2 && game.lineOfSight(game.player, enemy.at);
                if (!awaken || enemy.at == game.player || game.occupied(enemy.at, static_cast<int>(i))) continue;
                enemy.awake = true;
                enemy.nextMove = elapsed + 700ms;
                enemy.nextHit = elapsed + 700ms;
                if (enemy.kind == EnemyKind::Ghost) notice = u8"幽魂从雕像里现身了！";
            }
            if (manhattan(enemy.at, game.player) > 1 && elapsed >= enemy.nextMove) {
                enemy.nextMove = elapsed + (enemy.kind == EnemyKind::Mummy ? 1700ms : 1300ms);
                const auto route = game.world.search(enemy.at, game.player, "BFS");
                if (route.path.size() > 2 && !game.occupied(route.path[1], static_cast<int>(i))) enemy.at = route.path[1];
            }
            if (manhattan(enemy.at, game.player) == 1 && elapsed >= enemy.nextHit) {
                enemy.nextHit = elapsed + 2s;
                damage += enemy.damage;
            }
        }
        if (damage) {
            game.health = std::max(0, game.health - damage);
            notice = u8"受到怪物攻击，生命 −" + std::to_string(damage) + "。";
            if (game.health == 0) game.status = "lost";
        }
    }
    void checkRainDamage() {
        if (rainFrom < 0 || elapsed < rainDamageAt || game.status != "playing") return;
        const auto center = rainCenter();
        const auto player = point(game.player);
        if (std::abs(center[0] - player.x) + std::abs(center[1] - player.y) > hazardRadius) return;
        rainDamageAt = elapsed + rainInterval;
        game.health = std::max(0, game.health - 8);
        if (game.health == 0) game.status = "lost";
        notice = game.health ? u8"被暴雨淋湿，生命 −8。" : u8"暴雨耗尽了生命。";
    }
    void updateRain() {
        if (rainFrom < 0) return;
        while (elapsed - rainStep >= rainInterval) {
            rainStep += rainInterval;
            rainFrom = rainTo;
            auto center = point(rainFrom);
            const auto player = point(game.player);
            const int dx = player.x - center.x, dy = player.y - center.y;
            if (dx && std::abs(dx) >= std::abs(dy)) center.x += dx > 0 ? 1 : -1;
            else if (dy) center.y += dy > 0 ? 1 : -1;
            rainTo = center.id();
        }
        checkRainDamage();
    }
    void checkEntry(int previous) {
        if (game.player == previous || game.status != "playing") return;
        const auto decoy = std::find(decoys.begin(), decoys.end(), game.player);
        if (decoy != decoys.end()) {
            decoys.erase(decoy);
            stunnedUntil = elapsed + std::chrono::seconds(5);
            notice = u8"误采假花，原地眩晕 5 秒。";
        }
        for (auto& patch : fog) {
            if (elapsed < patch.reforms || manhattan(previous, patch.center) <= hazardRadius ||
                manhattan(game.player, patch.center) > hazardRadius) continue;
            stunnedUntil = elapsed + std::chrono::seconds(5);
            patch.reforms = elapsed + fogReformDelay;
            notice = u8"迷雾散开，原地眩晕 5 秒。";
            break;
        }
        if (mirrors[game.player]) {
            mirrors[game.player] = false;
            auto candidates = mirrorSafe;
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                [&](int cell) { return !safeCell(cell); }), candidates.end());
            if (candidates.size() > game.crystals.size()) {
                shuffleCells(candidates);
                game.player = candidates.back();
                candidates.pop_back();
                for (std::size_t i = 0; i < game.crystals.size(); ++i) {
                    const auto slot = std::find(flowerCells.begin(), flowerCells.end(), game.crystals[i]);
                    if (slot != flowerCells.end()) *slot = candidates[i];
                    game.crystals[i] = candidates[i];
                }
                game.updateSight();
                teleported = true;
                notice = u8"镜像转移，未收集的花已移位。";
            }
        }
        checkRainDamage();
    }
public:
    struct Challenge { int seconds, steps, cost; };
    inline static constexpr std::array<Challenge, 8> challenges{{
        {480, 420, 490}, {445, 380, 445}, {410, 350, 410}, {375, 320, 385},
        {340, 300, 365}, {305, 280, 340}, {270, 260, 315}, {240, 240, 295}
    }};
    static constexpr int guardTier(int level, int guard) { return std::min(3, (level + guard) / 3); }
    static constexpr int guardDamage(int tier) { return 30 + 10 * tier; }
    static constexpr int clockCount(int level) { return level >= 5 ? 2 : 3; }
    static constexpr int guardCount(int level) { return level >= 4 ? level : 3; }
    static constexpr int bossHealth(int level) {
        constexpr std::array<int, 4> health{120, 180, 260, 400};
        return level >= 4 ? health[level - 4] : 0;
    }
    Game game;
    std::array<int, 4> flowerCells{};
    std::vector<int> clocks;
    int bonusSeconds = 0;
    int target = -1;
    int algorithm = 2;
    std::string notice = u8"集齐四朵花，前往出口。";
    const Challenge limits;
    const Hazard hazard;

    explicit GameSession(uint32_t seed, int level = 0, Arsenal arsenal = {}, uint32_t hazardSeed = 0,
                         int hero = 0, int heroCount = 8)
        : hazardRandom(hazardSeed), unlocked(arsenal), game(seed), limits(challenges.at(level)),
          hazard(level >= 1 && level <= 3 ? static_cast<Hazard>(level) : Hazard::None) {
        game.expeditionLevel = level;
        game.stepLimit = limits.steps;
        game.costLimit = limits.cost;
        game.world.addExpeditionTerrain(level);
        std::copy_n(game.crystals.begin(), flowerCells.size(), flowerCells.begin());
        for (std::size_t i = 0; i < game.enemies.size(); ++i) {
            auto& guard = game.enemies[i];
            guard.tier = guardTier(level, static_cast<int>(i));
            guard.health = (2 + guard.tier) * Game::expeditionAttackDamage;
            guard.damage = guardDamage(guard.tier);
        }
        for (int room : {1, 6, 9}) {
            if (clockCount(level) == 2 && room == 1) continue;
            const auto& area = game.world.rooms[room];
            const int cell = Point{area.x, area.y}.id();
            clocks.push_back(cell);
        }
        initHazard();
        initEncounter(level, hero, heroCount);
        if (bossFlowerPending()) notice = u8"沿途拾取三朵花，击败首领取得第四朵。";
    }

    bool bossFlowerPending() const { return flowerCells[bossFlowerSlot] == sealedFlower; }
    const Enemy* boss() const {
        return bossIndex >= 0 && static_cast<std::size_t>(bossIndex) < game.enemies.size() ? &game.enemies[bossIndex] : nullptr;
    }
    const std::vector<int>& fakeFlowers() const { return decoys; }
    const std::vector<int>& statueCells() const { return statues; }
    const std::vector<int>& coffinCells() const { return coffins; }
    bool coffinsOpened() const { return puppetsAwake; }
    void setPlayerHero(int hero, int heroCount) {
        // The three copies must still differ from the player after changing hero from pause.
        for (auto& puppet : game.enemies) {
            if (puppet.kind != EnemyKind::Puppet || puppet.appearance != hero) continue;
            for (int candidate = 0; candidate < heroCount; ++candidate) {
                if (candidate == hero || std::any_of(game.enemies.begin(), game.enemies.end(), [&](const Enemy& other) {
                    return other.kind == EnemyKind::Puppet && other.appearance == candidate;
                })) continue;
                puppet.appearance = candidate;
                break;
            }
        }
    }
    bool selectWeapon(int index) {
        if (index < 0 || index >= static_cast<int>(unlocked.size()) || !unlocked[index]) return false;
        manualWeapon = index;
        return true;
    }
    void resumeWeaponRotation() { manualWeapon = -1; }
    bool weaponRotationEnabled() const { return manualWeapon < 0; }
    int weapon() const {
        if (!weaponRotationEnabled()) return manualWeapon;
        int firstUnlocked = -1;
        for (std::size_t offset = 0; offset < unlocked.size(); ++offset) {
            const auto index = (rotationStart + offset) % unlocked.size();
            if (!unlocked[index]) continue;
            if (cooldown(index) == Time::zero()) return static_cast<int>(index);
            if (firstUnlocked < 0) firstUnlocked = static_cast<int>(index);
        }
        return firstUnlocked;
    }
    bool hasWeapon(std::size_t index) const { return unlocked.at(index); }
    Time cooldown(std::size_t index) const { return std::max(Time::zero(), readyAt.at(index) - elapsed); }
    Time stunLeft() const { return std::max(Time::zero(), stunnedUntil - elapsed); }
    bool didTeleport() const { return teleported; }
    bool mirrorAt(int cell) const { return mirrors[cell]; }
    const std::vector<Fog>& fogPatches() const { return fog; }
    bool fogActive(const Fog& patch) const { return elapsed >= patch.reforms; }
    bool fogAt(int cell) const {
        return std::any_of(fog.begin(), fog.end(), [&](const Fog& patch) {
            return fogActive(patch) && manhattan(cell, patch.center) <= hazardRadius;
        });
    }
    std::array<float, 2> rainCenter() const {
        const auto from = point(rainFrom), to = point(rainTo);
        const float t = std::chrono::duration<float>(elapsed - rainStep) / rainInterval;
        return {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t};
    }
    const Weapon* releaseUltimate() {
        if (game.status != "playing" || stunLeft() > Time::zero()) return nullptr;
        const int selectedWeapon = weapon();
        if (selectedWeapon < 0) { notice = u8"通关后解锁武器。"; return nullptr; }
        if (cooldown(selectedWeapon) > Time::zero()) {
            notice = weaponRotationEnabled() ? u8"已解锁的武器都在冷却。" : u8"武器冷却中，可切换其他武器。";
            return nullptr;
        }
        const auto& skill = weapons[selectedWeapon];
        bool hit = false;
        for (auto& enemy : game.enemies) if (enemy.active() &&
            (skill.wholeMaze || manhattan(game.player, enemy.at) == 1)) {
            game.damageEnemy(enemy, skill.damage);
            hit = true;
        }
        if (!hit) { notice = u8"没有可命中的敌人。"; return nullptr; }
        ++game.turn;
        game.actExpeditionGuards();
        if (game.health == 0) game.status = "lost";
        readyAt[selectedWeapon] = elapsed + std::chrono::seconds(30);
        rotationStart = (selectedWeapon + 1) % unlocked.size();
        notice = game.health ? skill.skill : u8"生命耗尽。";
        updateBossReward();
        updateEncounter();
        return &skill;
    }
    void setTarget(int cell) { target = game.world.walkable(cell) ? cell : -1; refreshRoutes(); }
    void setAlgorithm(int index) { if (index >= 0 && index < 3) algorithm = index; }
    const SearchResult& route() const { return searches[algorithm]; }
    const SearchResult& route(int index) const { return searches.at(index); }
    int timeLimit() const { return limits.seconds + bonusSeconds; }
    void updateTime(Time now) {
        if (game.status != "playing") return;
        elapsed = std::max(elapsed, now);
        if (elapsed >= std::chrono::seconds(timeLimit())) {
            game.status = "lost";
            notice = u8"时间用尽";
            return;
        }
        updateRain();
        updateEncounter();
    }
    void act(char command, bool jump = false) {
        teleported = false;
        if (game.status != "playing" || stunLeft() > Time::zero()) return;
        const int turn = game.turn, health = game.health, flowers = game.collected, kills = game.kills;
        const int previous = game.player;
        game.act(command, jump);
        if (game.collected > flowers) {
            const auto slot = std::find(flowerCells.begin(), flowerCells.end(), game.player);
            if (slot != flowerCells.end()) *slot = -1;
        }
        bool pickedClock = false;
        if (game.player != previous) {
            const auto clock = std::find(clocks.begin(), clocks.end(), game.player);
            if (clock != clocks.end()) {
                clocks.erase(clock);
                bonusSeconds += 30;
                pickedClock = true;
            }
        }
        if (game.status == "won") notice = u8"已找到出口！";
        else if (game.status == "lost") notice = game.limitFailure ? game.limitFailure : u8"生命耗尽。";
        else if (pickedClock) notice = u8"拾取时钟，延长 30 秒。";
        else if (game.collected > flowers) notice = u8"获得一朵花！" + std::to_string(game.collected) + " / 4";
        else if (game.health < health) notice = u8"受到守卫攻击，失去 " + std::to_string(health - game.health) + u8" 点生命。";
        else if (game.health > health) notice = u8"使用药剂，恢复生命。";
        else if (game.kills > kills) notice = u8"已击败守卫。";
        else if (command == 'F') notice = u8"挥动光刃，攻击相邻守卫。";
        else if (game.turn == turn) notice = jump ? u8"前方没有可安全跳过的碎石。" : u8"前方挡住了去路。";
        else if (game.player == game.world.exit)
            notice = bossFlowerPending() ? u8"击败首领并集齐四朵花，出口才会开启。" : u8"出口未开启，需集齐四朵花。";
        else notice = u8"在遗迹中寻找花朵。";
        checkEntry(previous);
        updateBossReward();
        updateEncounter();
        refreshRoutes();
    }
    bool moveTowardTarget() {
        teleported = false;
        if (stunLeft() > Time::zero()) return false;
        const auto path = route().path;
        if (path.size() < 2 || game.status != "playing") return false;
        for (const auto& enemy : game.enemies) if (enemy.active() && manhattan(enemy.at, path[1]) <= 1) {
            notice = u8"敌人在附近，已停止自动行走。"; return false;
        }
        const int before = game.player, health = game.health;
        const int delta = path[1] - game.player;
        act(delta == 1 ? 'D' : delta == -1 ? 'A' : delta == WIDTH ? 'S' : 'W');
        return !teleported && stunLeft() == Time::zero() && game.player != before &&
               game.health >= health && game.status == "playing" && route().path.size() >= 2;
    }
    void attack() { act('F'); }
    void waitTurn() { act('.'); }
};
}
