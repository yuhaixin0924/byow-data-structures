#pragma once
#include "structures.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace relic {
constexpr int WIDTH = 57, HEIGHT = 35, CELLS = WIDTH * HEIGHT, INF = 1000000000;
struct Point { int x, y; int id() const { return y * WIDTH + x; } };
inline Point point(int id) { return {id % WIDTH, id / WIDTH}; }
inline int manhattan(int a, int b) {
    auto p = point(a), q = point(b); return std::abs(p.x-q.x) + std::abs(p.y-q.y);
}
// 固定的 xorshift32，避免不同 C++ 标准库的随机分布实现造成存档不兼容。
class Random {
    uint32_t value;
public:
    explicit Random(uint32_t seed) : value(seed ? seed : 0x9e3779b9u) {}
    uint32_t next() { value ^= value << 13; value ^= value >> 17; value ^= value << 5; return value; }
    int between(int lo, int hi) { return lo + int(next() % uint32_t(hi - lo + 1)); }
};
struct Room { int x, y, w, h; int center() const { return Point{x+w/2,y+h/2}.id(); } };
struct Edge { int a, b, weight; bool tree; };
struct SearchResult {
    std::string algorithm;
    std::vector<int> path, order;
    int cost = 0;
    double micros = 0;
};
enum class EnemyKind { Guard, Boss, Mummy, Ghost, Puppet };
struct Enemy {
    int at, home, target, health = 2;
    int tier = 0, damage = 8, warningCell = -1;
    EnemyKind kind = EnemyKind::Guard;
    bool awake = true;
    int appearance = -1;
    std::chrono::milliseconds nextMove{}, nextHit{};
    bool active() const { return awake && health > 0; }
};

enum Terrain { Rock, Floor, Sand, Bridge, Water, Rubble };

class World {
public:
    uint32_t seed;
    std::array<int,CELLS> tile{}; // 0 岩石、1 普通地面、2 沼泽（进入代价为 5）。
    std::array<std::vector<int>,CELLS> adjacency; // 四邻接有向图；边权为目标格进入代价。
    std::vector<Room> rooms;
    std::vector<Edge> edges;
    int start = 0, exit = 0, floorCount = 0, treeWeight = 0;
    explicit World(uint32_t s) : seed(s) { generate(); }
    bool walkable(int id) const { return id >= 0 && id < CELLS && tile[id] >= Floor && tile[id] <= Bridge; }
    int cost(int id) const { return tile[id] == Sand ? 5 : tile[id] == Bridge ? 3 : 1; }
    void addExpeditionTerrain(int level) {
        for (const auto index : {4, 7, 10}) {
            if (index > 4 && level < (index == 7 ? 3 : 6)) break;
            const auto& room = rooms[index];
            const auto center = point(room.center());
            for (int dy : {-1, 0, 1}) for (int x = room.x + 1; x < room.x + room.w - 1; ++x) {
                auto& cell = tile[Point{x, center.y + dy}.id()];
                if (cell == Sand) cell = (dy == 0 || x == center.x) ? Bridge : Water;
            }
        }
        const auto& room = rooms[2];
        tile[Point{room.x + 1, room.y + 1}.id()] = Rubble;
        for (int id = 0; id < CELLS; ++id) {
            auto& neighbors = adjacency[id];
            if (!walkable(id)) neighbors.clear();
            else neighbors.erase(std::remove_if(neighbors.begin(), neighbors.end(),
                [&](int cell) { return !walkable(cell); }), neighbors.end());
        }
        floorCount = static_cast<int>(std::count_if(tile.begin(), tile.end(),
            [](int cell) { return cell >= Floor && cell <= Bridge; }));
    }
    void generate() {
        Random rng(seed);
        // 分区内随机摆放房间，保留岩壁边界；每个种子均有 12 个房间。
        for (int row=0; row<3; ++row) for (int col=0; col<4; ++col) {
            Room r{2+col*14+rng.between(0,2), 2+row*11+rng.between(0,2),
                   rng.between(6,9),rng.between(5,7)};
            rooms.push_back(r);
            for (int y=r.y;y<r.y+r.h;++y) for(int x=r.x;x<r.x+r.w;++x) tile[y*WIDTH+x]=1;
        }
        std::vector<Edge> candidates;
        for (int a=0;a<int(rooms.size());++a) for(int b=a+1;b<int(rooms.size());++b)
            candidates.push_back({a,b,manhattan(rooms[a].center(),rooms[b].center()),false});
        std::sort(candidates.begin(),candidates.end(),[](const Edge&a,const Edge&b){
            if(a.weight!=b.weight)return a.weight<b.weight;
            if(a.a!=b.a)return a.a<b.a;
            return a.b<b.b;
        });
        DisjointSet sets(int(rooms.size()));
        std::vector<Edge> remaining;
        for(auto edge:candidates) {
            if(sets.unite(edge.a,edge.b)) {edge.tree=true;edges.push_back(edge);treeWeight+=edge.weight;}
            else remaining.push_back(edge);
        }
        // 额外连边创造环路，避免所有导航算法只能走唯一通道。
        for(int i=0;i<3;++i) {
            int j=rng.between(0,int(remaining.size())-1);
            edges.push_back(remaining[j]);remaining.erase(remaining.begin()+j);
        }
        for(auto e:edges) {
            Point p=point(rooms[e.a].center()),q=point(rooms[e.b].center());
            bool horizontal=rng.between(0,1)==0;
            while(p.id()!=q.id()) {
                tile[p.id()]=1;
                if(horizontal ? p.x!=q.x : p.y==q.y) p.x+=(q.x>p.x?1:-1);
                else p.y+=(q.y>p.y?1:-1);
            }
            tile[q.id()]=1;
        }
        // 连片高代价格：BFS 最少步数与 Dijkstra 最小代价可能不同。
        for(size_t i=1;i<rooms.size();++i) {
            auto r=rooms[i];
            if(i%3==1) for(int y=r.y+1;y<r.y+r.h-1;++y)
                for(int x=r.x+1;x<r.x+r.w-1;++x) tile[y*WIDTH+x]=2;
        }
        for(auto r:rooms)tile[r.center()]=1;
        for(int id=0;id<CELLS;++id) if(walkable(id)) {
            ++floorCount;Point p=point(id);
            const int dx[]={0,1,0,-1},dy[]={-1,0,1,0};
            for(int k=0;k<4;++k) {
                int x=p.x+dx[k],y=p.y+dy[k];
                if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT&&walkable(y*WIDTH+x))adjacency[id].push_back(y*WIDTH+x);
            }
        }
        start=rooms.front().center();exit=start;
        auto distances=distanceFrom(start);
        for(auto r:rooms)if(distances[r.center()]>distances[exit])exit=r.center();
    }
    std::array<int,CELLS> distanceFrom(int source) const {
        std::array<int,CELLS>d;d.fill(-1);RingQueue<int> queue;
        d[source]=0;queue.push(source);
        while(!queue.empty()) {int u=queue.pop();for(int v:adjacency[u])if(d[v]<0){d[v]=d[u]+1;queue.push(v);}}
        return d;
    }
    SearchResult search(int source,int target,const std::string& algorithm) const {
        SearchResult result;result.algorithm=algorithm;
        auto begin=std::chrono::steady_clock::now();
        if(!walkable(source)||!walkable(target))return result;
        std::array<int,CELLS> distance,parent;distance.fill(INF);parent.fill(-1);distance[source]=0;
        if(algorithm=="BFS") {
            RingQueue<int> queue;queue.push(source);
            while(!queue.empty()) {
                int u=queue.pop();result.order.push_back(u);if(u==target)break;
                for(int v:adjacency[u])if(distance[v]==INF){distance[v]=distance[u]+1;parent[v]=u;queue.push(v);}
            }
        } else {
            struct Node{int priority,distance,id;};
            struct Compare{bool operator()(const Node&a,const Node&b)const{
                if(a.priority!=b.priority)return a.priority<b.priority;
                if(a.distance!=b.distance)return a.distance>b.distance;
                return a.id<b.id;
            }};
            MinHeap<Node,Compare> heap;heap.push({algorithm=="ASTAR"?manhattan(source,target):0,0,source});
            while(!heap.empty()) {
                auto node=heap.pop();int u=node.id;
                if(node.distance!=distance[u])continue; // 忽略被后续松弛替代的堆条目。
                result.order.push_back(u);if(u==target)break;
                for(int v:adjacency[u]) {
                    int candidate=distance[u]+cost(v);
                    if(candidate<distance[v]) {distance[v]=candidate;parent[v]=u;
                        heap.push({candidate+(algorithm=="ASTAR"?manhattan(v,target):0),candidate,v});}
                }
            }
        }
        if(distance[target]!=INF) {
            for(int u=target;u!=-1;u=parent[u])result.path.push_back(u);
            std::reverse(result.path.begin(),result.path.end());
            for(size_t i=1;i<result.path.size();++i)result.cost+=cost(result.path[i]);
        }
        result.micros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
        return result;
    }
};

class Game {
public:
    static constexpr int expeditionAttackDamage = 10;
    World world;
    int player,health=100,turn=0,spent=0,collected=0,kills=0,steps=0;
    int stepLimit = INF, costLimit = INF, expeditionLevel = -1;
    const char* limitFailure = nullptr;
    std::array<bool,CELLS> visible{},explored{};
    std::vector<int> crystals,potions;
    std::vector<Enemy> enemies;
    std::string status="playing",message="收集 4 朵花，前往出口。";
    explicit Game(uint32_t seed) :world(seed),player(world.start) {
        std::vector<int> centers;
        for(auto r:world.rooms)if(r.center()!=player&&r.center()!=world.exit)centers.push_back(r.center());
        auto d=world.distanceFrom(player);
        std::sort(centers.begin(),centers.end(),[&](int a,int b){return d[a]==d[b]?a<b:d[a]>d[b];});
        for(int i=0;i<4;++i)crystals.push_back(centers[i*2]);
        for(int i:{1,3,5})potions.push_back(centers[i]);
        for(int i:{1,5,9})enemies.push_back({centers[i],centers[i],world.rooms[(i+3)%12].center(),2});
        updateSight();
    }
    // Bresenham 光线，遇到第一个不透明格停止；该墙本身仍可被看见。
    bool lineOfSight(int a,int b) const {
        Point p=point(a),q=point(b);int dx=std::abs(q.x-p.x),sx=p.x<q.x?1:-1;
        int dy=-std::abs(q.y-p.y),sy=p.y<q.y?1:-1,err=dx+dy;
        while(true){
            if(p.id()==b)return true;
            if(p.id()!=a&&!world.walkable(p.id()))return false;
            int e2=2*err;if(e2>=dy){err+=dy;p.x+=sx;}if(e2<=dx){err+=dx;p.y+=sy;}
        }
    }
    void updateSight(){
        visible.fill(false);Point p=point(player);
        for(int y=std::max(0,p.y-8);y<=std::min(HEIGHT-1,p.y+8);++y)
            for(int x=std::max(0,p.x-8);x<=std::min(WIDTH-1,p.x+8);++x){
                int id=y*WIDTH+x;
                if((x-p.x)*(x-p.x)+(y-p.y)*(y-p.y)<=64&&lineOfSight(player,id))visible[id]=explored[id]=true;
            }
    }
    bool occupied(int id,int except=-1)const{
        for(int i=0;i<int(enemies.size());++i)if(i!=except&&enemies[i].active()&&enemies[i].at==id)return true;
        return false;
    }
    void damageEnemy(Enemy& enemy, int damage) {
        if (enemy.health <= 0) return;
        enemy.health = std::max(0, enemy.health - damage);
        if (enemy.health == 0) {
            enemy.warningCell = -1;
            ++kills;
        }
    }
    SearchResult enemyPlan(const Enemy& enemy) const {
        auto chase = world.search(enemy.at, player, "BFS");
        if (!chase.path.empty() && chase.path.size() <= 14) return chase;
        int target = enemy.at == enemy.target ? enemy.home : enemy.target;
        return world.search(enemy.at, target, "BFS");
    }
    void act(char command, bool jump = false){
        if(status!="playing")return;
        int destination=player;Point p=point(player);
        if(command=='W')--p.y;else if(command=='S')++p.y;
        else if(command=='A')--p.x;else if(command=='D')++p.x;
        else if(command!='F'&&command!='.')return;
        if(command!='F'&&command!='.'){
            if (jump) {
                if (p.x < 0 || p.x >= WIDTH || p.y < 0 || p.y >= HEIGHT || world.tile[p.id()] != Rubble) return;
                const auto origin = point(player);
                p = {p.x * 2 - origin.x, p.y * 2 - origin.y};
            }
            if(p.x<0||p.x>=WIDTH||p.y<0||p.y>=HEIGHT||!world.walkable(p.id())){message="前方是岩壁。";return;}
            destination=p.id();
            if(occupied(destination)){message="守卫挡住了去路，按空格攻击。";return;}
        }
        if (destination != player && (steps >= stepLimit || spent + movementCost(destination, jump) > costLimit)) {
            limitFailure = steps >= stepLimit ? u8"超过步数上限" : u8"超过地形消耗上限";
            status = "lost";
            return;
        }
        ++turn;message="探索遗迹中……";
        if(command=='F'){
            bool hit=false;
            for(auto& e:enemies)if(e.active()&&manhattan(player,e.at)==1){
                hit=true;
                damageEnemy(e, expeditionLevel >= 0 ? expeditionAttackDamage : 1);
                message = e.health == 0 ? "守卫已被击败。" : "击中守卫！再攻击一次。";
            }
            if(!hit)message="挥动光刃。相邻的守卫会受到伤害。";
        }else if(destination!=player){player=destination;++steps;spent+=movementCost(player, jump);}
        auto crystal=std::find(crystals.begin(),crystals.end(),player);
        if(crystal!=crystals.end()){crystals.erase(crystal);++collected;message="获得一朵花！";}
        auto potion=std::find(potions.begin(),potions.end(),player);
        if(potion!=potions.end()&&health<100){potions.erase(potion);health=std::min(100,health+(expeditionLevel >= 0 ? 45 : 30));message="修复生命 +30。";}
        if(player==world.exit&&collected==4){status="won";message="出口已开启，探索完成！";updateSight();return;}
        if(player==world.exit)message="出口尚未开启，需要收集全部 4 朵花。";
        // 守卫每三回合行动一次。近处用 BFS 追踪，否则在家与巡逻点间往返。
        if (expeditionLevel >= 0) actExpeditionGuards();
        else if(turn%3==0)for(int i=0;i<int(enemies.size());++i){
            auto& e=enemies[i];if(e.health<=0)continue;
            if(e.at==e.target)std::swap(e.home,e.target);
            auto route=enemyPlan(e);
            if(manhattan(e.at,player)==1){health=std::max(0,health-8);message="受到守卫攻击 −8，按空格反击！";}
            else if(route.path.size()>1&&!occupied(route.path[1],i)&&route.path[1]!=player)e.at=route.path[1];
        }
        if(health==0){status="lost";message="探索中断。可以撤销行动，或开启新的旅程。";}
        updateSight();
    }
    int movementCost(int destination, bool jump) const {
        return jump ? 3 : world.cost(destination);
    }

    void actExpeditionGuards() {
        for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
            auto& guard = enemies[i];
            if (!guard.active() || guard.kind != EnemyKind::Guard) continue;
            if (guard.warningCell >= 0) {
                if (player == guard.warningCell) health = std::max(0, health - guard.damage);
                guard.warningCell = -1;
                continue;
            }
            const int period = expeditionLevel < 4 ? 4 : 3;
            if (turn % period != 0) continue;
            if (manhattan(guard.at, player) == 1) {
                guard.warningCell = player;
                continue;
            }
            if (guard.at == guard.target) std::swap(guard.home, guard.target);
            const auto route = enemyPlan(guard);
            if (route.path.size() > 1 && !occupied(route.path[1], i) && route.path[1] != player)
                guard.at = route.path[1];
        }
    }
    void replay(const std::string& history){for(char c:history)act(c);}
};

}
