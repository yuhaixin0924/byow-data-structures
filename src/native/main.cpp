#include "session.hpp"
#include "music.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace {
constexpr unsigned WINDOW_W = 1440, WINDOW_H = 900;
constexpr float TILE = 19.5f, MAP_X = 24.f, MAP_Y = 138.f;
constexpr float PLAYER_SIZE = 29.f;
constexpr float SIDEBAR_X = MAP_X + relic::WIDTH * TILE + 22.f;
constexpr float SIDEBAR_CONTENT_X = SIDEBAR_X + 14.f;
constexpr float ALGORITHM_BUTTON_STEP = 78.f;
constexpr unsigned HERO_IDLE_FRAMES = 24;
constexpr auto HERO_IDLE_FRAME_TIME = std::chrono::milliseconds(90);
const sf::FloatRect MAZE_AREA({MAP_X, MAP_Y}, {relic::WIDTH * TILE, relic::HEIGHT * TILE});
const sf::FloatRect MAP_HERO({1185,762},{188,54});
const sf::FloatRect BRIEF_HERO({563,610},{314,48});
const sf::FloatRect PAUSE_BUTTON({985,19},{210,46});
const sf::FloatRect RESELECT_LEVEL({563,510},{314,54});
const sf::FloatRect RESUME({563,422},{314,54});
const sf::FloatRect HERO_USE({810,603},{260,53});
const sf::FloatRect PLAY_BUTTON({534, 474}, {372, 74});
const sf::FloatRect SEED_BUTTON({534, 566}, {177, 45});
const sf::FloatRect MAP_BUTTON({729, 566}, {177, 45});
const sf::FloatRect HELP_BUTTON({534, 625}, {177, 45});
const sf::FloatRect HERO_BUTTON({729, 625}, {177, 45});
const sf::FloatRect MODAL_BACK({417, 690}, {183, 56});
const sf::FloatRect HELP_BACK({148, 736}, {166, 45});

const sf::FloatRect HELP_PAGE({1026, 736}, {260, 45});
const sf::FloatRect BRIEF_GO({801, 690}, {222, 56});
struct Hero {
    const char* name;
    const char* asset;
    sf::Color accent;
    const char* intro;
};
const std::array<Hero, 8> HEROES{{
    {u8"方糖", "twig", {164, 229, 197},
     u8"白西装配着橄榄绿领结。\n方糖从口袋里抽出一张牌，\n抬眼望向下一处岔路。"},
    {u8"焰尾", "ember", {255, 197, 117},
     u8"披着红斗篷的探险家。\n爱用指尖的小火星打招呼，\n看见陌生的门总想推开看看。"},
    {u8"晚铃", "bell", {241, 151, 198},
     u8"喜欢在日暮时出发。\n发辫上的铃铛轻轻响，\n让幽静的石廊有了回声。"},
    {u8"霜绒", "fleece", {170, 222, 240},
     u8"总会带上那只小羊团。\n冰蓝的眼睛藏在银发下，\n脚步轻得像踩在新雪上。"},
    {u8"柚星", "yuzu", {250, 208, 115},
     u8"戴着旧帽子的小小术士。\n与不爱落地的小使魔结伴，\n走到哪都要数一数星星。"},
    {u8"重音テト", "teto", {255, 151, 174},
     u8"双卷发会随着脚步摇摆。\n她擅长把长廊走成舞台，\n偶尔眨一只眼和你打招呼。"},
    {u8"凜凜蝶", "rin", {210, 200, 244},
     u8"循着月色寻花的旅人。\n齐刘海下是一双紫色眼睛，\n深紫长发拂过白色衣领。"},
    {u8"鸬宫天马", "tenma", {255, 205, 149},
     u8"黑袍的金色衣领上方，\n一只眼映暮紫，一只眼映新绿。\n几绺翘发随步子轻晃。"}
}};
sf::FloatRect heroCard(int index) {
    return {{115.f + (index % 4) * 307.f, 228.f + (index / 4) * 246.f}, {289.f, 232.f}};
}
const sf::FloatRect HERO_BACK({115, 735}, {183, 50});
const sf::FloatRect HERO_CLOSE({1178, 141}, {54, 52});
const sf::FloatRect BACK_BUTTON({64, 762}, {160, 54});
const sf::FloatRect CONTINUE_BUTTON({520, 606}, {400, 64});
const sf::FloatRect MUSIC_BUTTON({1224, 19}, {184, 46});
constexpr const char* GAME_TITLE = u8"花神遗迹";
struct Level {
    const char* name;
    uint32_t seed;
    sf::Vector2f position;
};
const std::array<Level, 8> LEVELS{{
    {u8"初入遗迹", 2026, {156, 350}}, {u8"苔庭", 3142, {330, 479}},
    {u8"沉水小径", 836, {254, 659}}, {u8"琥珀石窟", 5017, {588, 289}},
    {u8"幽泉", 7129, {739, 493}}, {u8"古根密室", 9953, {1026, 658}},
    {u8"长阳殿", 4309, {1160, 285}}, {u8"霜冠之门", 15941, {1302, 498}}
}};
constexpr std::array<std::array<int, 2>, 11> ROADS{{
    {0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 4}, {2, 5}, {3, 6},
    {4, 6}, {4, 5}, {5, 7}, {6, 7}
}};
using Clock = std::chrono::steady_clock;
constexpr auto STEP_TIME = std::chrono::milliseconds(125);
struct Season {
    const char* name;
    sf::Color sky, distant, ground, foliage, accent, floor, sand;
};
const std::array<Season, 4> SEASONS{{
    {u8"春", {35, 63, 68}, {48, 84, 83}, {35, 69, 61}, {70, 127, 93}, {244, 169, 189}, {63, 100, 81}, {139, 101, 69}},
    {u8"夏", {33, 67, 77}, {39, 107, 105}, {25, 76, 62}, {53, 133, 81}, {240, 208, 113}, {49, 95, 83}, {150, 116, 64}},
    {u8"秋", {70, 53, 75}, {101, 68, 83}, {76, 54, 51}, {157, 88, 66}, {245, 170, 91}, {99, 80, 70}, {153, 104, 59}},
    {u8"冬", {40, 61, 89}, {64, 90, 113}, {35, 58, 76}, {68, 107, 126}, {205, 233, 249}, {64, 90, 111}, {116, 115, 136}}
}};
constexpr std::array<int, 8> LEVEL_SEASONS{0, 0, 1, 2, 1, 2, 3, 3};
const std::array<std::string, 3> ALGORITHMS{"BFS", "Dijkstra", "A*"};
std::filesystem::path executableDir;

sf::String fromUtf8(const std::string& value) {
    return sf::String::fromUtf8(value.begin(), value.end());
}

std::filesystem::path progressFile() {
#ifdef _WIN32
    if (const auto* base = _wgetenv(L"LOCALAPPDATA"))
        return std::filesystem::path(base) / "BYOW" / "cleared-levels.txt";
#elif defined(__APPLE__)
    if (const auto* base = std::getenv("HOME"))
        return std::filesystem::path(base) / "Library/Application Support/BYOW/cleared-levels.txt";
#else
    if (const auto* base = std::getenv("XDG_DATA_HOME"))
        return std::filesystem::path(base) / "byow" / "cleared-levels.txt";
    if (const auto* base = std::getenv("HOME"))
        return std::filesystem::path(base) / ".local/share/byow/cleared-levels.txt";
#endif
    return executableDir / "cleared-levels.txt";
}

std::string formatTime(Clock::duration elapsed) {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    const auto minutes = seconds / 60;
    return (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
           (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
}
std::filesystem::path executableDirectory(const char* argv0) {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) throw std::runtime_error("Cannot locate executable");
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) throw std::runtime_error("Cannot locate executable");
    return std::filesystem::weakly_canonical(buffer.c_str()).parent_path();
#else
    std::error_code error;
    auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    return (error ? std::filesystem::absolute(argv0) : path).parent_path();
#endif
}

std::filesystem::path assetPath(const char* name) {
    const auto besideExe = executableDir / "assets" / name;
    if (std::filesystem::exists(besideExe)) return besideExe;
    const auto fromWorkingDirectory = std::filesystem::current_path() / "assets" / name;
    if (std::filesystem::exists(fromWorkingDirectory)) return fromWorkingDirectory;
#ifdef BYOW_SOURCE_ASSETS
    const auto source = std::filesystem::path(BYOW_SOURCE_ASSETS) / name;
    if (std::filesystem::exists(source)) return source;
#endif
    return besideExe;
}

void text(sf::RenderWindow& window, const sf::Font& font, const std::string& value,
          float x, float y, unsigned size, sf::Color color = sf::Color(225, 232, 238)) {
    sf::Text label(font, fromUtf8(value), size);
    label.setPosition({x, y});
    label.setFillColor(color);
    window.draw(label);
}

void box(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size,
         sf::Color fill, sf::Color outline = sf::Color::Transparent) {
    sf::RectangleShape shape(size);
    shape.setPosition(pos);
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(outline == sf::Color::Transparent ? 0.f : 1.f);
    window.draw(shape);
}

void centeredLabel(sf::RenderWindow& window, const sf::Font& font, const std::string& value,
                   sf::Vector2f center, unsigned size, sf::Color color, bool bold = false, float tracking = 1.f) {
    sf::Text label(font, fromUtf8(value), size);
    label.setLetterSpacing(tracking);
    if (bold) label.setStyle(sf::Text::Bold);
    const auto bounds = label.getLocalBounds();
    label.setOrigin(bounds.position + bounds.size / 2.f);
    label.setPosition(center);
    label.setFillColor(color);
    if (bold) {
        label.setOutlineColor(sf::Color(17, 19, 25));
        label.setOutlineThickness(2.f);
    }
    window.draw(label);
}

void polygon(sf::RenderWindow& window, std::initializer_list<sf::Vector2f> points, sf::Color color) {
    sf::ConvexShape shape(points.size());
    std::size_t index = 0;
    for (const auto point : points) shape.setPoint(index++, point);
    shape.setFillColor(color);
    window.draw(shape);
}

constexpr std::array<sf::Vector2f, 4> SEAL_PETAL_DIRECTIONS{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
constexpr float SEAL_PETAL_SHOULDER = .56f, SEAL_PETAL_HALF_WIDTH = .29f;

void relicSeal(sf::RenderWindow& window, sf::Vector2f center, float size, sf::Color color) {
    sf::CircleShape rim(size * 1.4f, 8);
    rim.setOrigin({rim.getRadius(), rim.getRadius()});
    rim.setPosition(center);
    rim.setFillColor(sf::Color(20, 27, 33));
    rim.setOutlineColor(sf::Color(color.r, color.g, color.b, 100));
    rim.setOutlineThickness(2.f);
    window.draw(rim);
    for (const auto direction : SEAL_PETAL_DIRECTIONS) {
        const sf::Vector2f perpendicular{-direction.y, direction.x};
        const auto shoulder = center + direction * (size * SEAL_PETAL_SHOULDER);
        const auto width = perpendicular * (size * SEAL_PETAL_HALF_WIDTH);
        polygon(window, {center, shoulder + width, center + direction * size, shoulder - width}, color);
    }
    sf::CircleShape centerStone(size * .18f, 8);
    centerStone.setOrigin({centerStone.getRadius(), centerStone.getRadius()});
    centerStone.setPosition(center);
    centerStone.setFillColor(sf::Color(250, 228, 173));
    window.draw(centerStone);
}

sf::Color alpha(sf::Color color, std::uint8_t opacity) {
    color.a = opacity;
    return color;
}

void disc(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color) {
    sf::CircleShape shape(radius, 20);
    shape.setOrigin({radius, radius});
    shape.setPosition(center);
    shape.setFillColor(color);
    window.draw(shape);
}

void softLight(sf::RenderWindow& window, sf::Vector2f center, sf::Vector2f radius,
               sf::Color color, sf::BlendMode blend = sf::BlendAlpha) {
    constexpr unsigned segments = 40;
    for (const float scale : {1.f, .62f, .33f}) {
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, segments + 2);
        fan[0] = {center, alpha(color, color.a / 3)};
        for (unsigned i = 0; i <= segments; ++i) {
            const float angle = i * 6.2831853f / segments;
            fan[i + 1] = {center + sf::Vector2f(std::cos(angle) * radius.x, std::sin(angle) * radius.y) * scale,
                          alpha(color, 0)};
        }
        window.draw(fan, blend);
    }
}

void cutPanel(sf::RenderWindow& window, sf::FloatRect bounds, float bevel,
              sf::Color fill, sf::Color edge) {
    const auto a = bounds.position;
    const auto b = a + bounds.size;
    sf::ConvexShape panel(8);
    const std::array<sf::Vector2f, 8> points{{
        {a.x + bevel, a.y}, {b.x - bevel, a.y}, {b.x, a.y + bevel}, {b.x, b.y - bevel},
        {b.x - bevel, b.y}, {a.x + bevel, b.y}, {a.x, b.y - bevel}, {a.x, a.y + bevel}}};
    for (std::size_t i = 0; i < points.size(); ++i) panel.setPoint(i, points[i]);
    panel.setFillColor(fill);
    panel.setOutlineThickness(1.f);
    panel.setOutlineColor(edge);
    window.draw(panel);
}

void filigreeFrame(sf::RenderWindow& window, sf::FloatRect frame, sf::Color fill, sf::Color metal, float scale = 1.f) {
    const auto inset = sf::Vector2f(5, 5) * scale;
    cutPanel(window, frame, 12 * scale, fill, metal);
    cutPanel(window, {frame.position + inset, frame.size - inset * 2.f},
             10 * scale, sf::Color::Transparent, alpha(metal, metal.a / 2));
    for (int corner = 0; corner < 4; ++corner) {
        const float dx = corner % 2 ? -1.f : 1.f;
        const float dy = corner / 2 ? -1.f : 1.f;
        const auto origin = frame.position + sf::Vector2f(dx < 0 ? frame.size.x : 0.f,
                                                          dy < 0 ? frame.size.y : 0.f);
        const auto at = [&](float x, float y) { return origin + sf::Vector2f(dx*x, dy*y) * scale; };
        for (bool vertical : {false, true}) {
            const auto turn = [&](float x, float y) { return vertical ? at(y, x) : at(x, y); };
            sf::VertexArray curl(sf::PrimitiveType::LineStrip, 31);
            for (std::size_t i = 0; i < curl.getVertexCount(); ++i) {
                const float t = i / 30.f;
                const float radius = 10.f * (1.f - .92f*t);
                const float angle = 2.8f + 7.2f*t;
                curl[i] = {turn(31.f + std::cos(angle)*radius + 6.f*t,
                                18.f + std::sin(angle)*radius), metal};
            }
            window.draw(curl);
            polygon(window, {turn(17, 11), turn(21, 12), turn(29, 9), turn(21, 9)}, alpha(metal, metal.a * 3 / 4));
            polygon(window, {turn(37, 27), turn(44, 27), turn(52, 22), turn(45, 23)}, alpha(metal, metal.a * 3 / 4));
        }
        polygon(window, {at(12, 18), at(14, 13), at(19, 12), at(17, 17)}, metal);
        disc(window, at(13.5f, 32), .95f * scale, metal);
        disc(window, at(32, 13.5f), .95f * scale, metal);
    }
}

void frameTracery(sf::RenderWindow& window, sf::FloatRect frame, sf::Color fill, sf::Color metal) {
    cutPanel(window, frame, 19, fill, metal);
    cutPanel(window, {frame.position + sf::Vector2f(7, 7), frame.size - sf::Vector2f(14, 14)},
             15, sf::Color::Transparent, alpha(metal, 92));
    for (int corner = 0; corner < 4; ++corner) {
        const float dx = corner % 2 ? -1.f : 1.f;
        const float dy = corner / 2 ? -1.f : 1.f;
        const auto origin = frame.position + sf::Vector2f(dx < 0 ? frame.size.x : 0.f,
                                                          dy < 0 ? frame.size.y : 0.f);
        const auto at = [&](float x, float y) { return origin + sf::Vector2f(x*dx, y*dy); };
        const auto drawLine = [&](std::initializer_list<sf::Vector2f> points, sf::Color color) {
            sf::VertexArray line(sf::PrimitiveType::LineStrip);
            for (const auto point : points) line.append({at(point.x, point.y), color});
            window.draw(line);
        };
        drawLine({{11, 65}, {11, 28}, {28, 11}, {65, 11}}, metal);
        drawLine({{16, 57}, {16, 31}, {31, 16}, {57, 16}}, alpha(metal, 125));
        drawLine({{22, 43}, {22, 30}, {30, 22}, {43, 22}, {37, 37}, {22, 43}}, metal);
        drawLine({{27, 36}, {36, 27}, {33, 33}, {27, 36}}, alpha(metal, 150));
        polygon(window, {at(8, 67), at(11, 62), at(14, 67), at(11, 72)}, metal);
        polygon(window, {at(62, 11), at(67, 8), at(72, 11), at(67, 14)}, metal);
    }
}

void pointedMedallion(sf::RenderWindow& window, sf::Vector2f center, float half, float height, sf::Color tint) {
    for (float inset : {0.f, 5.f}) {
        const float width = half - inset;
        const float crown = center.y - height / 2.f + inset * 1.7f;
        const float spring = crown + width * 1.73205f;
        const float foot = center.y + height / 2.f - inset;
        sf::VertexArray arch(sf::PrimitiveType::LineStrip, 51);
        arch[0] = {{center.x-width, foot}, tint};
        for (unsigned i = 0; i <= 24; ++i) {
            const float dx = width * (1.f-i/24.f);
            const float rise = std::sqrt(std::max(0.f, 4*width*width - (width+dx)*(width+dx)));
            arch[i+1] = {{center.x-dx, spring-rise}, tint};
            arch[49-i] = {{center.x+dx, spring-rise}, tint};
        }
        arch[25] = {{center.x, crown}, tint};
        arch[50] = {{center.x+width, foot}, tint};
        window.draw(arch);
        box(window, {center.x-width, foot}, {width*2, 1}, tint);
    }
}

void blossom(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color, bool lit = true) {
    if (lit) disc(window, center, radius * 1.75f, alpha(color, 40));
    const auto stem = lit ? sf::Color(133, 175, 130) : sf::Color(67, 80, 84);
    polygon(window, {center, center + sf::Vector2f(radius*.20f, radius*1.38f),
                     center + sf::Vector2f(-radius*.07f, radius*1.38f)}, stem);
    polygon(window, {center + sf::Vector2f(radius*.1f, radius*1.1f),
                     center + sf::Vector2f(-radius*.94f, radius*.69f),
                     center + sf::Vector2f(-radius*.55f, radius*1.19f)}, stem);
    sf::CircleShape petal(1.f, 16);
    petal.setOrigin({1, 1});
    petal.setScale({radius * .58f, radius * .33f});
    petal.setFillColor(color);
    petal.setOutlineThickness(.10f);
    petal.setOutlineColor({29, 30, 41});
    for (int i = 0; i < 5; ++i) {
        const float angle = -1.5708f + i * 1.25664f;
        petal.setPosition(center + sf::Vector2f(std::cos(angle), std::sin(angle)) * radius * .53f);
        petal.setRotation(sf::radians(angle));
        window.draw(petal);
    }
    disc(window, center, radius * .28f, lit ? sf::Color(255, 232, 159) : sf::Color(104, 112, 116));
}

void crystalFlower(sf::RenderWindow& window, sf::Vector2f center, sf::Color color, float time) {
    constexpr float scale = .70f;
    constexpr float radius = 43.f * scale;
    const float breath = .73f + .23f * std::sin(time * 1.43f);
    center.y += 2.f * scale * std::sin(time * 1.2f);
    softLight(window, center, sf::Vector2f(84, 59) * scale,
              alpha(color, static_cast<std::uint8_t>(102 * breath)), sf::BlendAdd);
    const sf::Color shade(color.r / 2, color.g / 2, color.b / 2);
    const sf::Color pale((color.r + 255) / 2, (color.g + 255) / 2, (color.b + 255) / 2);
    for (std::size_t i = 0; i < SEAL_PETAL_DIRECTIONS.size(); ++i) {
        const auto outward = SEAL_PETAL_DIRECTIONS[i];
        const sf::Vector2f cross(-outward.y, outward.x);
        const auto at = [&](float x, float y) { return center + (cross*x + outward*y) * radius; };
        const auto tip = at(0, 1.f), facet = at(0, SEAL_PETAL_SHOULDER);
        const auto left = at(-SEAL_PETAL_HALF_WIDTH, SEAL_PETAL_SHOULDER);
        const auto right = at(SEAL_PETAL_HALF_WIDTH, SEAL_PETAL_SHOULDER);
        const float shine = std::max(0.f, std::cos(time * 2.3f - i * 1.5707963f));
        const sf::Color glint(255, 249, 234, static_cast<std::uint8_t>(90 + 125 * shine));
        const sf::Vertex facets[] = {
            {center, shade}, {left, color}, {facet, pale},
            {left, pale}, {tip, {255, 249, 234}}, {facet, color},
            {tip, pale}, {right, color}, {facet, pale},
            {right, shade}, {center, color}, {facet, color}
        };
        window.draw(facets, std::size(facets), sf::PrimitiveType::Triangles);
        const sf::Vertex bevel[] = {{left, alpha(pale, 120)}, {tip, glint}, {right, alpha(color, 130)}};
        window.draw(bevel, std::size(bevel), sf::PrimitiveType::LineStrip);
        const float along = .23f + .70f * std::fmod(time * .58f + i * .18f, 1.f);
        polygon(window, {at(0, along + .04f), at(-.095f, along), at(0, along - .035f), at(.08f, along)},
                alpha(glint, static_cast<std::uint8_t>(210 * shine * shine)));
    }
    disc(window, center, 8.f * scale, shade);
    disc(window, center, 6.5f * scale, pale);
    disc(window, center - sf::Vector2f(1, 1) * scale, 3.5f * scale, {255, 245, 213});
    for (int i = 0; i < 5; ++i) {
        const float t = time * (.49f + .07f * i) + 2.4f * i;
        const float sparkle = .5f + .5f * std::sin(2.1f*t);
        const auto p = center + sf::Vector2f(std::cos(t) * (44.f + i*5.f),
                                           std::sin(t) * (30.f + i*3.f)) * scale;
        disc(window, p, (1.f + sparkle) * scale, alpha(pale, static_cast<std::uint8_t>(55 + 151*sparkle)));
    }
}

constexpr std::array<const char*, 8> FLOWER_NAMES{
    u8"樱花", u8"紫花地丁", u8"荷花", u8"桂花", u8"栀子", u8"秋菊", u8"水仙", u8"红梅"
};

bool flowerHit(sf::Vector2f delta) {
    return std::abs(delta.x) < 83 && delta.y >= -34 && delta.y <= 76;
}

void drawStoneHall(sf::RenderWindow& window) {
    window.clear(sf::Color(14, 19, 24));
    for (int row = 0; row < 10; ++row) {
        for (int column = -1; column < 9; ++column) {
            const float x = column * 180.f + (row % 2) * 90.f;
            const float y = row * 72.f;
            const auto shade = static_cast<std::uint8_t>(34 + ((column + 1) * 17 + row * 7) % 9);
            box(window, {x + 2, y + 2}, {176, 68}, sf::Color(shade, shade + 8, shade + 14));
            box(window, {x + 3, y + 3}, {174, 2}, sf::Color(shade + 10, shade + 18, shade + 24));
        }
    }

    const auto portal = [&](float x, sf::Color color) {
        polygon(window, {{x, 704}, {x, 318}, {x + 42, 264}, {x + 198, 264}, {x + 240, 318}, {x + 240, 704}},
                sf::Color(76, 88, 96));
        polygon(window, {{x + 13, 704}, {x + 13, 322}, {x + 49, 279}, {x + 191, 279}, {x + 227, 322}, {x + 227, 704}},
                sf::Color(23, 31, 39));
        box(window, {x + 32, 358}, {176, 346}, sf::Color(18, 22, 29));
        box(window, {x + 32, 527}, {176, 2}, sf::Color(color.r, color.g, color.b, 90));
        relicSeal(window, {x + 120, 451}, 34, color);
    };
    portal(126, sf::Color(231, 122, 76));
    portal(1074, sf::Color(95, 205, 240));

    box(window, {0, 704}, {1440, 196}, sf::Color(25, 33, 41));
    box(window, {0, 707}, {1440, 4}, sf::Color(75, 89, 98));
    for (const float y : {754.f, 821.f}) box(window, {0, y}, {1440, 2}, sf::Color(39, 51, 61));
    polygon(window, {{553, 711}, {887, 711}, {1020, 900}, {420, 900}}, sf::Color(33, 44, 53));
    for (const float x : {192.f, 1248.f}) {
        relicSeal(window, {x, 777}, 14, x < 720 ? sf::Color(231, 122, 76) : sf::Color(95, 205, 240));
    }
}

void drawGothicHall(sf::RenderWindow& window, const sf::Texture& texture, float time) {
    window.clear(sf::Color(8, 12, 19));
    sf::Sprite hall(texture);
    const auto size = texture.getSize();
    hall.setScale({WINDOW_W / static_cast<float>(size.x), WINDOW_H / static_cast<float>(size.y)});
    window.draw(hall);
    crystalFlower(window, {259, 210}, {237, 82, 112}, time);
    crystalFlower(window, {1181, 210}, {92, 172, 247}, time + 1.2f);

    const std::array<sf::Vector2f, 6> candles{{{291,486}, {309,471}, {326,490},
                                              {1112,484}, {1131,470}, {1148,491}}};
    for (std::size_t i = 0; i < candles.size(); ++i) {
        const auto base = candles[i] + sf::Vector2f(0, 6);
        const float phase = time + i * .51f;
        const float flicker = .70f + .20f * std::sin(phase * 7.f) + .09f * std::sin(phase * 13.3f);
        const float sway = std::sin(phase * 4.9f) * 3.9f + std::sin(phase * 9.8f) * 1.35f;
        if (i == 1 || i == 4) {
            const float outward = i == 1 ? -1.f : 1.f;
            sf::VertexArray shadow(sf::PrimitiveType::Triangles, 6);
            const std::array<sf::Vector2f, 3> shadowOffsets{{{outward*16, 26}, {outward*49+sway*2, 35},
                                                  {outward*76+sway*3, 56}}};
            const auto dark = sf::Color(21, 14, 21, static_cast<std::uint8_t>(42*flicker));
            shadow[0] = {base+shadowOffsets[0], dark};
            shadow[1] = {base+shadowOffsets[1], dark};
            shadow[2] = {base+sf::Vector2f(outward*167+sway*5, 6), alpha(dark, 0)};
            shadow[3] = {base+shadowOffsets[0], dark};
            shadow[4] = {base+shadowOffsets[2], alpha(dark, 0)};
            shadow[5] = {shadow[2].position, alpha(dark, 0)};
            window.draw(shadow);
            softLight(window, base, {154, 186},
                      {255, 137, 48, static_cast<std::uint8_t>(60*flicker)}, sf::BlendAdd);
        }
        softLight(window, base-sf::Vector2f(0, 8), {28, 36}, {255, 149, 54, 105}, sf::BlendAdd);
        polygon(window, {base + sf::Vector2f(-4, -4), base + sf::Vector2f(-3, -12),
                         base + sf::Vector2f(sway, -13 - 8*flicker), base + sf::Vector2f(4, -9),
                         base + sf::Vector2f(4, -4), base + sf::Vector2f(0, -1)}, {250, 160, 72});
        polygon(window, {base + sf::Vector2f(-2, -4), base + sf::Vector2f(sway*.65f, -11 - 4*flicker),
                         base + sf::Vector2f(2.6f, -5), base + sf::Vector2f(0, -2)}, {255, 238, 177});
    }
    for (int i = 0; i < 5; ++i) {
        const float phase = time * (.11f + .009f*i) + 1.7f*i;
        const float x = 100.f + i*310.f + 145.f*std::sin(phase);
        softLight(window, {x, 762.f + i%3*40.f + 14.f*std::cos(phase)}, {370, 58},
                  {126, 164, 199, 31}, sf::BlendAdd);
    }
    const std::array<sf::Vector2f, 6> flowers{{{87,228}, {246,388}, {157,536},
                                               {1348,255}, {1175,390}, {1264,534}}};
    for (int i = 0; i < 15; ++i) {
        const float period = 7.8f + (i % 4) * 1.25f;
        const float progress = std::fmod(time + i * 2.71f, period) / period;
        const float spin = progress * (17.f + i % 3) + i;
        const auto origin = flowers[i % flowers.size()];
        const float wind = (origin.x < 720 ? 1.f : -1.f) * (72.f + i%4*21.f);
        const auto p = origin + sf::Vector2f(wind*progress + 17*std::sin(spin)*progress,
                                             (355.f + i%3*30.f)*progress);
        const float fade = std::min({1.f, progress*15.f, (1.f-progress)*6.f});
        const float width = (1.4f + std::abs(std::sin(spin))*3.9f) * (.82f+i%3*.17f);
        const sf::Vector2f axis{std::cos(spin)*width, std::sin(spin)*width};
        const sf::Vector2f rib{-std::sin(spin)*5.5f, std::cos(spin)*5.5f};
        const auto tint = i%3 == 0 ? sf::Color(121, 183, 244) : sf::Color(75, 117, 191);
        polygon(window, {p-rib*.82f, p-axis+rib*.16f, p+rib, p+axis+rib*.35f},
                alpha(tint, static_cast<std::uint8_t>(202*fade)));
        polygon(window, {p-rib*.5f, p+rib*.82f, p+axis*.66f+rib*.26f},
                alpha(sf::Color(185, 217, 248), static_cast<std::uint8_t>(111*fade)));
    }
}

sf::Vector2f tilePosition(int cell) {
    const auto point = relic::point(cell);
    return {MAP_X + point.x * TILE, MAP_Y + point.y * TILE};
}

void drawExplorer(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f facing, float progress, int style = 0, bool jumping = false) {
    constexpr float pi = 3.14159265f;
    const float stride = std::sin(2.f * pi * progress) * 2.6f;
    const float bob = std::abs(std::sin(pi * progress)) * (jumping ? 10.f : 2.f);
    const auto body = pos + sf::Vector2f(0, -bob);
    const auto ink = sf::Color(13, 23, 30);
    const auto hood = HEROES[style].accent;
    const auto coat = sf::Color(hood.r * 2 / 3, hood.g * 2 / 3, hood.b * 2 / 3);
    box(window, pos + sf::Vector2f(2, 15), {15, 3}, sf::Color(12, 22, 29, 160));
    box(window, body + sf::Vector2f(4.f + facing.x * stride, 11.f + facing.y * stride), {4, 6}, ink);
    box(window, body + sf::Vector2f(10.f - facing.x * stride, 11.f - facing.y * stride), {4, 6}, ink);
    box(window, body + sf::Vector2f(4.f + facing.x * stride, 12.f + facing.y * stride), {3, 3}, hood);
    box(window, body + sf::Vector2f(10.f - facing.x * stride, 12.f - facing.y * stride), {3, 3}, hood);
    polygon(window, {body + sf::Vector2f(3, 13), body + sf::Vector2f(6, 6),
                     body + sf::Vector2f(12, 6), body + sf::Vector2f(15, 13)}, ink);
    polygon(window, {body + sf::Vector2f(5, 12), body + sf::Vector2f(7, 7),
                     body + sf::Vector2f(11, 7), body + sf::Vector2f(13, 12)}, coat);
    polygon(window, {body + sf::Vector2f(3, 8), body + sf::Vector2f(4, 3),
                     body + sf::Vector2f(9, -1), body + sf::Vector2f(14, 3),
                     body + sf::Vector2f(15, 8), body + sf::Vector2f(9, 11)}, ink);
    polygon(window, {body + sf::Vector2f(5, 7), body + sf::Vector2f(6, 3),
                     body + sf::Vector2f(9, 1), body + sf::Vector2f(12, 3),
                     body + sf::Vector2f(13, 7), body + sf::Vector2f(9, 9)}, hood);
    if (facing.y >= 0) {
        const float faceX = 7 + facing.x * 2;
        box(window, body + sf::Vector2f(faceX, 4), {4, 3}, ink);
        box(window, body + sf::Vector2f(faceX + 1, 5), {2, 1}, sf::Color(244, 223, 171));
    }
    const float lanternX = facing.x < 0 ? 1.f : 14.f;
    box(window, body + sf::Vector2f(lanternX, 9.f + stride * .55f), {4, 5},
        sf::Color(245, 179, 71), ink);
    box(window, body + sf::Vector2f(lanternX + 1, 10.f + stride * .55f), {2, 2},
        sf::Color(255, 240, 176));
}

void drawGuard(sf::RenderWindow& window, sf::Vector2f pos) {
    const auto ink = sf::Color(22, 18, 29);
    polygon(window, {pos + sf::Vector2f(1, 15), pos + sf::Vector2f(3, 5), pos + sf::Vector2f(7, 1),
                     pos + sf::Vector2f(11, 1), pos + sf::Vector2f(15, 5), pos + sf::Vector2f(17, 15)}, ink);
    box(window, pos + sf::Vector2f(4, 5), {10, 9}, sf::Color(143, 66, 72));
    box(window, pos + sf::Vector2f(5, 7), {8, 3}, ink);
    box(window, pos + sf::Vector2f(6, 8), {2, 1}, sf::Color(249, 174, 87));
    box(window, pos + sf::Vector2f(10, 8), {2, 1}, sf::Color(249, 174, 87));
}

// A continuous density field gives moving wisps soft edges at every camera zoom.
float fogNoise(float x, float y) {
    const auto hash = [](int gx, int gy) {
        uint32_t bits = static_cast<uint32_t>(gx) * 0x8da6b343u ^
                        static_cast<uint32_t>(gy) * 0xd8163841u;
        bits ^= bits >> 16;
        bits *= 0x7feb352du;
        bits ^= bits >> 15;
        return (bits & 0xffffu) / 65535.f;
    };
    const auto blend = [](float a, float b, float t) { return a + (b - a) * t; };
    const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    const auto ease = [](float t) { return t * t * (3.f - 2.f * t); };
    const float u = ease(x - ix), v = ease(y - iy);
    return blend(blend(hash(ix, iy), hash(ix + 1, iy), u),
                 blend(hash(ix, iy + 1), hash(ix + 1, iy + 1), u), v);
}

sf::Image makeFogImage() {
    constexpr unsigned size = 192;
    sf::Image image({size, size}, sf::Color::Transparent);
    for (unsigned y = 0; y < size; ++y) for (unsigned x = 0; x < size; ++x) {
        const float u = 2.f * x / (size - 1) - 1.f, v = 2.f * y / (size - 1) - 1.f;
        const float warp = fogNoise(u * 2.3f + 11.f, v * 3.9f + 8.f);
        const float coarse = fogNoise(u * 2.8f + 3.f, v * 5.2f + warp * 1.4f + 5.f);
        const float fine = fogNoise(u * 6.f + 7.f, v * 12.f + warp * 2.f + 3.f);
        const float body = std::clamp((.93f + warp * .17f - std::sqrt(u * u + v * v)) / .48f, 0.f, 1.f);
        const float edge = std::clamp((1.f - std::max(std::abs(u), std::abs(v))) / .14f, 0.f, 1.f);
        const float density = body * body * (3.f - 2.f * body) * edge *
                              std::clamp(.44f + coarse * .62f + fine * .26f, 0.f, 1.f);
        const auto shade = static_cast<uint8_t>(154.f + coarse * 56.f + fine * 29.f);
        image.setPixel({x, y}, {shade, static_cast<uint8_t>(shade + 5),
                               static_cast<uint8_t>(shade + 9), static_cast<uint8_t>(density * 255.f)});
    }
    return image;
}

struct SpriteAsset {
    unsigned frames = 1, columns = 1, rows = 1;
    std::optional<sf::Texture> texture;
    std::optional<sf::Sprite> sprite;
    explicit SpriteAsset(const std::string& name, unsigned count = 1, unsigned across = 0)
        : frames(count), columns(across ? across : count), rows((count + columns - 1) / columns) {
        const auto path = assetPath(name.c_str());
        if (!std::filesystem::exists(path)) return;
        texture.emplace();
        if (texture->loadFromFile(path) && texture->getSize().x % columns == 0 &&
            texture->getSize().y % rows == 0) {
            texture->setSmooth(true);
            sprite.emplace(*texture);
        } else {
            std::cerr << "Ignoring invalid optional texture: " << path << '\n';
            texture.reset();
        }
    }
    // sf::Sprite refers to its texture. Rebinding here keeps moved sprite assets valid.
    SpriteAsset(SpriteAsset&& other) : frames(other.frames), columns(other.columns), rows(other.rows),
        texture(std::move(other.texture)) {
        if (other.sprite) sprite.emplace(*texture);
    }
    bool draw(sf::RenderWindow& window, sf::Vector2f pos, float size, bool flip = false, unsigned frame = 0,
              sf::Color tint = sf::Color::White) {
        if (!sprite) return false;
        const auto dimensions = texture->getSize();
        const int width = static_cast<int>(dimensions.x / columns);
        const int height = static_cast<int>(dimensions.y / rows);
        frame %= frames;
        sprite->setTextureRect({{static_cast<int>(frame % columns) * width,
                                 static_cast<int>(frame / columns) * height}, {width, height}});
        const auto bounds = sprite->getLocalBounds();
        const auto side = std::max(bounds.size.x, bounds.size.y);
        const float scale = size / side;
        sprite->setScale({flip ? -scale : scale, scale});
        sprite->setColor(tint);
        sprite->setPosition(pos + sf::Vector2f((size - bounds.size.x * scale) / 2 +
                                                  (flip ? bounds.size.x * scale : 0.f),
                                              (size - bounds.size.y * scale) / 2));
        window.draw(*sprite);
        return true;
    }
};

template<std::size_t... Slots>
auto loadHeroSprites(std::index_sequence<Slots...>) {
    return std::array<SpriteAsset, sizeof...(Slots)>{
        SpriteAsset(std::string("heroes/portraits/") + HEROES[Slots].asset + ".png")...};
}

class App {
    sf::RenderWindow window{sf::VideoMode({WINDOW_W, WINDOW_H}), fromUtf8(GAME_TITLE)};
    sf::Font font;
    sf::Font calligraphy;
    sf::Font titleCalligraphy, blackletter;
    sf::Texture gothicHall, valley;
    std::optional<sf::Texture> fogTexture;
    SpriteAsset flowers{"garden/flowers.png", 8, 4};
    SpriteAsset dormantFlowers{"garden/flowers-dormant.png", 8, 4};
    bool hasCalligraphy = false, hasGothicHall = false;
    bool hasTitleCalligraphy = false, hasBlackletter = false;
    std::array<SpriteAsset, HEROES.size()> heroes = loadHeroSprites(std::make_index_sequence<HEROES.size()>{});
    std::array<SpriteAsset, 4> guardians{SpriteAsset("guards/moss.png", 4), SpriteAsset("guards/ember.png", 4),
                                        SpriteAsset("guards/tide.png", 4), SpriteAsset("guards/frost.png", 4)};
    std::unique_ptr<relic::GameSession> session;
    Music music{assetPath("ruins-theme.wav"), assetPath("menu-theme.flac")};
    std::filesystem::path progressPath = progressFile();
    std::array<bool, LEVELS.size()> cleared{};
    std::optional<std::size_t> newWeapon;
    std::optional<std::size_t> activeLevel;
    std::string seedText, notice;
    bool seedMode = false, mapMode = false, resultsMode = false;
    bool autoWalk = false, showPath = true, progressSaved = true;
    bool overview = true, briefing = false, helpMode = false, heroMode = false, briefingFromMap = false;
    bool heroDetail = false, paused = false;
    bool armory = false, weaponHelp = false;
    unsigned shiftKeys = 0;
    bool shiftCombo = false;
    std::string ultimateTitle;
    relic::GameSession::Time ultimateUntil{};
    bool jumpPose = false;
    int hero = 0;
    std::vector<sf::Keyboard::Key> held;
    Clock::time_point nextManual{};
    unsigned seedCaret = 0;
    Clock::time_point nextStep{}, stepStarted{};
    Clock::time_point strikeStarted{};
    std::chrono::milliseconds strikeDuration{};
    std::array<int, 4> struckCells{};
    std::size_t struckCount = 0;
    const Clock::time_point cameraEpoch = Clock::now();
    Clock::time_point cameraUpdated{}, revealUntil{};
    sf::Vector2f cameraCenter{};
    float cameraZoom = 1.f;
    sf::View mazeView;
    std::optional<Clock::time_point> timerStarted;
    Clock::duration elapsed{};
    sf::Vector2f stepOrigin{}, facing{0, 1};
    sf::View view{sf::FloatRect({0.f, 0.f}, {static_cast<float>(WINDOW_W), static_cast<float>(WINDOW_H)})};

public:
    App() {
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);
        const auto path = assetPath("ruins-zh.ttf");
        if (!font.openFromFile(path)) throw std::runtime_error("Missing assets/ruins-zh.ttf. Keep assets beside the executable.");
        hasCalligraphy = calligraphy.openFromFile(assetPath("LXGWWenKaiLite-Medium.ttf"));
        hasTitleCalligraphy = titleCalligraphy.openFromFile(assetPath("LongCang-Regular.ttf"));
        hasBlackletter = blackletter.openFromFile(assetPath("UnifrakturMaguntia-Book.ttf"));
        hasGothicHall = gothicHall.loadFromFile(assetPath("gothic-hall.jpg"));
        gothicHall.setSmooth(true);
        if (!valley.loadFromFile(assetPath("garden/valley.jpg")))
            throw std::runtime_error("Missing garden/valley.jpg");
        valley.setSmooth(true);
        resize(window.getSize());
        loadProgress();
        std::ifstream choice(progressPath.parent_path() / "hero.txt");
        if (!(choice >> hero) || hero < 0 || hero >= static_cast<int>(heroes.size())) hero = 0;
    }

    void run() {
        while (window.isOpen()) {
            while (const auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) window.close();
                if (const auto* size = event->getIf<sf::Event::Resized>()) resize(size->size);
                if (event->is<sf::Event::FocusLost>()) { autoWalk = false; held.clear(); shiftKeys = 0; }
                checkCompletion();
                if (const auto* key = event->getIf<sf::Event::KeyReleased>()) onKeyReleased(key->code);
                if (const auto* key = event->getIf<sf::Event::KeyPressed>()) onKey(key->code);
                if (const auto* entered = event->getIf<sf::Event::TextEntered>()) onText(entered->unicode);
                if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
                    if (mouse->button == sf::Mouse::Button::Left) onMouse(window.mapPixelToCoords(mouse->position));
            }
            checkCompletion();
            if (session && !briefing && !seedMode && !resultsMode && !held.empty() && Clock::now() >= nextManual)
                moveManual(held.back());
            if (autoWalk && session && Clock::now() >= nextStep) {
                nextStep = Clock::now() + std::chrono::milliseconds(140);
                const int previous = session->game.player;
                const int previousHealth = session->game.health;
                autoWalk = session->moveTowardTarget();
                animateMove(previous, false);
                playDamage(previousHealth);
                notice = session->notice;
                checkCompletion();
                if (session->game.status != "playing" || session->route().path.size() < 2) autoWalk = false;
            }
            if (window.isOpen()) draw();
        }
    }

private:
    const sf::Font& displayFont() const { return hasCalligraphy ? calligraphy : font; }
    const sf::Font& titleFont() const { return hasTitleCalligraphy ? titleCalligraphy : displayFont(); }

    void loadProgress() {
        std::ifstream input(progressPath);
        uint32_t seed;
        while (input >> seed) {
            for (std::size_t i = 0; i < LEVELS.size(); ++i)
                if (LEVELS[i].seed == seed) cleared[i] = true;
        }
    }

    void recordClear(std::size_t index) {
        if (cleared[index]) return;
        loadProgress();
        if (cleared[index]) return;
        std::error_code error;
        std::filesystem::create_directories(progressPath.parent_path(), error);
        std::ofstream output(progressPath, std::ios::app);
        output << LEVELS[index].seed << '\n';
        output.close();
        progressSaved = static_cast<bool>(output);
        cleared[index] = true;
    }

    Clock::duration playTime() const {
        return elapsed + (timerStarted ? Clock::now() - *timerStarted : Clock::duration::zero());
    }

    void pauseTimer() {
        elapsed = playTime();
        timerStarted.reset();
    }

    void checkCompletion() {
        if (!session || resultsMode || briefing) return;
        if (timerStarted) {
            const int previousHealth = session->game.health;
            const auto previousNotice = session->notice;
            session->updateTime(gameTime());
            if (session->notice != previousNotice) notice = session->notice;
            if (session->game.health < previousHealth) {
                notice = session->notice;
                autoWalk = false;
                playDamage(previousHealth);
            }
        }
        if (session->game.status == "playing") return;
        notice = session->notice;
        pauseTimer();
        autoWalk = false;
        seedMode = false;
        held.clear();
        shiftKeys = 0;
        resultsMode = true;
        if (session->game.status == "won" && activeLevel) {
            if (*activeLevel < relic::weapons.size() && !cleared[*activeLevel]) newWeapon = *activeLevel;
            endingPending = !cleared[*activeLevel] && std::count(cleared.begin(),cleared.end(),true)==7;
            recordClear(*activeLevel);
            readingStory = true;
        }
    }

    bool readingStory = false;

    void finishResult() {

        showMap();
        if (endingPending) { archive=8; endingPending=false; }
    }

    void showMap() {
        pauseTimer();
        music.setExpedition(false);
        session.reset();
        activeLevel.reset();
        readingStory = false;
        resultsMode = seedMode = autoWalk = briefing = paused = false;
        held.clear();
        mapMode = true;
    }

    Clock::duration timeBudget() const {
        return std::chrono::seconds(session->timeLimit());
    }

    void selectHero(int index) {
        hero = index;
        if (session) session->setPlayerHero(hero, static_cast<int>(heroes.size()));
        std::error_code error;
        std::filesystem::create_directories(progressPath.parent_path(), error);
        std::ofstream choice(progressPath.parent_path() / "hero.txt");
        choice << hero << '\n';
        heroMode = false;
        heroDetail = true;
    }

    void pauseGame() {
        pauseTimer();
        held.clear();
        shiftKeys = 0;
        autoWalk = false;
        paused = true;
    }

    void resumeGame() {
        paused = false;
        timerStarted = Clock::now();
    }

    void startChallenge() {
        briefing = false;
        music.setExpedition(true);
        timerStarted = Clock::now();
        revealUntil = *timerStarted + std::chrono::milliseconds(1600);
    }

    void cancelBriefing() {
        showMap();
        mapMode = briefingFromMap;
    }

    void resize(sf::Vector2u size) {
        if (!size.x || !size.y) return;
        const float ratio = static_cast<float>(size.x) / size.y;
        const float wanted = static_cast<float>(WINDOW_W) / WINDOW_H;
        view.setViewport(ratio > wanted ? sf::FloatRect({(1.f - wanted / ratio) / 2.f, 0.f}, {wanted / ratio, 1.f})
                                        : sf::FloatRect({0.f, (1.f - ratio / wanted) / 2.f}, {1.f, ratio / wanted}));
        window.setView(view);
    }
    void chooseSeed() {
        seedMode = true;
        held.clear();
        autoWalk = false;
        seedText.clear();
        seedCaret = 0;
        notice.clear();
    }

    void closeSeedMenu() {
        seedMode = false;
        if (session && session->game.status == "playing" && !timerStarted)
            timerStarted = Clock::now();
        notice = session ? session->notice : "";
    }

    void beginGame(uint32_t seed, std::optional<std::size_t> level = std::nullopt) {
        if (!level) {
            for (std::size_t i = 0; i < LEVELS.size(); ++i)
                if (LEVELS[i].seed == seed) { level = i; break; }
        }
        relic::GameSession::Arsenal arsenal{};
        std::copy_n(cleared.begin(), arsenal.size(), arsenal.begin());
        const auto hazardSeed = static_cast<uint32_t>(Clock::now().time_since_epoch().count());
        session = std::make_unique<relic::GameSession>(seed, static_cast<int>(level.value_or(0)), arsenal, hazardSeed,
                                                       hero, static_cast<int>(heroes.size()));
        briefingFromMap = mapMode;
        briefing = true;
        music.setExpedition(false);
        strikeStarted = {};
        newWeapon.reset();
        ultimateUntil = {};
        shiftKeys = 0;
        activeLevel = level;
        readingStory = level.has_value();
        cameraCenter = MAZE_AREA.getCenter();
        cameraZoom = 1.f;
        cameraUpdated = Clock::now();
        revealUntil = cameraUpdated + std::chrono::milliseconds(1600);
        overview = level && *level < 4;
        mapMode = resultsMode = false;
        progressSaved = true;
        elapsed = Clock::duration::zero();
        timerStarted.reset();
        held.clear();
        jumpPose = false;
        stepOrigin = tilePosition(session->game.player);
        stepStarted = Clock::now() - STEP_TIME;
        facing = {0, 1};
        updateCamera();
        autoWalk = false;
        seedMode = false;
        notice = session->notice;
    }

    void beginCustomGame() {
        uint32_t seed{};
        const auto result = std::from_chars(seedText.data(), seedText.data() + seedText.size(), seed);
        if (result.ec != std::errc{} || result.ptr != seedText.data() + seedText.size()) {
            notice = u8"种子必须是 0–4294967295 之间的整数。";
            return;
        }
        beginGame(seed);
    }

    void onText(char32_t code) {
        if (!seedMode || code < U'0' || code > U'9' || seedText.size() >= 10) return;
        seedText.insert(seedCaret, 1, static_cast<char>(code));
        ++seedCaret;
        music.play(Music::Cue::Tap);
    }

    float stepProgress(Clock::time_point now) const {
        return std::clamp(std::chrono::duration<float>(now - stepStarted).count() /
                          std::chrono::duration<float>(STEP_TIME).count(), 0.f, 1.f);
    }

    sf::Vector2f animatedPlayer(Clock::time_point now) const {
        return stepOrigin + (tilePosition(session->game.player) - stepOrigin) * stepProgress(now);
    }

    std::size_t seasonIndex() const {
        return activeLevel ? LEVEL_SEASONS[*activeLevel] : session->game.world.seed % SEASONS.size();
    }
    const Season& currentSeason() const { return SEASONS[seasonIndex()]; }

    void drawSeasonLandscape(sf::Vector2f position, sf::Vector2f size) {
        // Windows onto the four regions of the same painted valley.
        const std::array<sf::IntRect,4> regions{{
            {{0,85},{660,420}}, {{180,552},{660,420}},
            {{860,440},{660,420}}, {{918,0},{660,420}}
        }};
        sf::Sprite landscape(valley, regions[seasonIndex()]);
        landscape.setPosition(position);
        landscape.setScale({size.x/660.f,size.y/420.f});
        landscape.setColor({105,112,121});
        window.draw(landscape);
    }

    void updateCamera() {
        const auto now = Clock::now();
        const float dt = std::clamp(std::chrono::duration<float>(now - cameraUpdated).count(), 0.f, .1f);
        cameraUpdated = now;
        const float amount = 1.f - std::exp(-dt * 5.f);
        const bool full = overview || now < revealUntil || resultsMode || briefing;
        cameraZoom += ((full ? 1.f : 2.05f) - cameraZoom) * amount;
        const auto half = MAZE_AREA.size / (cameraZoom * 2.f);
        const auto bottomRight = MAZE_AREA.position + MAZE_AREA.size;
        const auto player = animatedPlayer(now) + sf::Vector2f(TILE / 2, TILE / 2);
        const auto target = full ? MAZE_AREA.getCenter() : player;
        cameraCenter += (target - cameraCenter) * amount;
        cameraCenter.x = std::clamp(cameraCenter.x, MAZE_AREA.position.x + half.x, bottomRight.x - half.x);
        cameraCenter.y = std::clamp(cameraCenter.y, MAZE_AREA.position.y + half.y, bottomRight.y - half.y);
        mazeView.setCenter(cameraCenter);
        mazeView.setSize(half * 2.f);
        const auto viewport = view.getViewport();
        mazeView.setViewport({
            viewport.position + sf::Vector2f(viewport.size.x * MAP_X / WINDOW_W,
                                             viewport.size.y * MAP_Y / WINDOW_H),
            {viewport.size.x * MAZE_AREA.size.x / WINDOW_W,
             viewport.size.y * MAZE_AREA.size.y / WINDOW_H}
        });
    }

    void animateMove(int previous, bool jumping) {
        const int current = session->game.player;
        if (session->didTeleport()) {
            stepOrigin = tilePosition(current);
            stepStarted = Clock::now() - STEP_TIME;
            jumpPose = autoWalk = false;
            return;
        }
        if (current == previous) return;
        music.play(session->game.world.tile[current] == 2 ? Music::Cue::Sand : Music::Cue::Stone);
        const auto now = Clock::now();
        const auto previousPosition = tilePosition(previous);
        const auto progress = stepProgress(now);
        // A quick second key press starts from the currently drawn position.
        stepOrigin += (previousPosition - stepOrigin) * progress;
        jumpPose = jumping;
        facing = (tilePosition(current) - previousPosition) / (jumping ? TILE * 2.f : TILE);
        stepStarted = now;
    }

    void playDamage(int previousHealth) {
        if (session->game.health < previousHealth) music.play(Music::Cue::Hurt);
    }

    void strike() {
        if (session->stunLeft() > relic::GameSession::Time::zero()) return;
        if (Clock::now() - strikeStarted < strikeDuration) return;
        struckCount = 0;
        int toughest = 0;
        for (const auto& enemy : session->game.enemies) {
            if (enemy.active() && relic::manhattan(session->game.player, enemy.at) == 1 && struckCount < struckCells.size()) {
                struckCells[struckCount++] = enemy.at;
                toughest = std::max(toughest, enemy.tier);
            }
        }
        strikeDuration = std::chrono::milliseconds(360 + 110 * toughest);
        session->attack();
        strikeStarted = Clock::now();
        music.play(struckCount ? Music::Cue::Hit : Music::Cue::Miss);
    }

    void drawStrike(sf::Vector2f playerPos, Clock::time_point now) {
        const float elapsed = std::chrono::duration<float>(now - strikeStarted).count();
        const float duration = std::chrono::duration<float>(strikeDuration).count();
        if (elapsed >= duration) return;
        const auto center = playerPos + sf::Vector2f(TILE / 2, TILE / 2);
        const float t = elapsed / duration;
        const bool hit = struckCount > 0;
        const auto color = hit ? sf::Color(255, 199, 97) : sf::Color(120, 220, 235);
        const float radius = (hit ? 10.f : 8.f) + t * (hit ? 23.f : 12.f);
        sf::CircleShape ring(radius, 40);
        ring.setOrigin({radius, radius});
        ring.setPosition(center);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(hit ? 2.2f : 1.f);
        ring.setOutlineColor(alpha(color, static_cast<std::uint8_t>(210 * (1.f - t))));
        window.draw(ring);
        for (std::size_t i = 0; i < struckCount; ++i) {
            const auto impact = tilePosition(struckCells[i]) + sf::Vector2f(TILE / 2, TILE / 2);
            for (int ray = 0; ray < 7; ++ray) {
                const float angle = ray * .8976f + .2f;
                const auto direction = sf::Vector2f(std::cos(angle), std::sin(angle));
                disc(window, impact + direction * (3.f + t * 14.f), 2.1f * (1.f - t),
                     alpha(color, static_cast<std::uint8_t>(255 * (1.f - t))));
            }
        }
    }

    static char direction(sf::Keyboard::Key key) {
        using Key = sf::Keyboard::Key;
        switch (key) {
            case Key::Up: case Key::W: return 'W';
            case Key::Left: case Key::A: return 'A';
            case Key::Down: case Key::S: return 'S';
            case Key::Right: case Key::D: return 'D';
            default: return 0;
        }
    }

    void moveManual(sf::Keyboard::Key key) {
        nextManual = Clock::now() + std::chrono::milliseconds(145);
        const bool jumping = shiftKeys != 0;
        if (jumping) shiftCombo = true;
        const int previous = session->game.player, previousHealth = session->game.health;
        session->act(direction(key), jumping);
        animateMove(previous, jumping);
        notice = session->notice;
        playDamage(previousHealth);
        checkCompletion();
    }

    relic::GameSession::Time gameTime() const {
        return std::chrono::duration_cast<relic::GameSession::Time>(playTime());
    }
    static unsigned shiftBit(sf::Keyboard::Key key) {
        using Key = sf::Keyboard::Key;
        return key == Key::LShift ? 1u : key == Key::RShift ? 2u : 0u;
    }
    void onKeyReleased(sf::Keyboard::Key key) {
        held.erase(std::remove(held.begin(), held.end(), key), held.end());
        const unsigned bit = shiftBit(key);
        if (!(shiftKeys & bit)) return;
        shiftKeys &= ~bit;
        if (shiftKeys || shiftCombo || !session || !timerStarted || resultsMode || paused ||
            briefing || seedMode || heroMode || heroDetail) return;
        const int previousHealth = session->game.health;
        if (const auto* skill = session->releaseUltimate()) {
            ultimateTitle = skill->skill;
            ultimateUntil = gameTime() + std::chrono::milliseconds(1800);
            music.playUltimate(static_cast<std::size_t>(skill - relic::weapons.data()));
        }
        notice = session->notice;
        playDamage(previousHealth);
        checkCompletion();
    }

    void click() { music.play(Music::Cue::Click); }

    void onKey(sf::Keyboard::Key key) {
        using Key = sf::Keyboard::Key;
        if (archive>=0) {
            if (key==Key::Escape) archive=-1;
            else if (key==Key::Right || key==Key::Enter) browse(1);
            else if (key==Key::Left) browse(-1);
            return;
        }
        if (readingStory) {
            if (key == Key::Enter || key == Key::Space) readingStory = false;
            else if (key == Key::Escape) {
                readingStory = false;
                if (briefing) cancelBriefing();
            }
            return;
        }
        if (folioPage>=0) {
            if (key==Key::Escape) folioPage=-1;
            else if (key==Key::Right || key==Key::Enter) turnPage(1);
            else if (key==Key::Left) turnPage(-1);
            return;
        }
        if (key == Key::M) { music.toggle(); return click(); }
        if (heroDetail) {
            if (key == Key::Escape || key == Key::Enter) { heroDetail = false; click(); }
            return;
        }
        if (helpMode || heroMode || armory) {
            if (key == Key::Escape) { helpMode = heroMode = armory = false; click(); }
            else if (helpMode && key == Key::Tab) { weaponHelp = !weaponHelp; click(); }
            else if (heroMode) {
                const int index = static_cast<int>(key) - static_cast<int>(Key::Num1);
                if (index >= 0 && index < static_cast<int>(heroes.size())) { selectHero(index); click(); }
            }
            return;
        }
        if (briefing) {
            if (key == Key::Enter) startChallenge();
            else if (key == Key::Escape) cancelBriefing();
            else if (key == Key::C) heroMode = true;
            else return;
            return click();
        }
        if (paused) {
            if (key == Key::Escape || key == Key::Enter) resumeGame();
            else if (key == Key::C) heroMode = true;
            else if (key == Key::Tab) showMap();
            else return;
            return click();
        }
        if (resultsMode) {
            if (key == Key::Enter || key == Key::Escape) finishResult();
            return;
        }
        if (mapMode) {
            if (key==Key::V) { armory=true; return click(); }
            if (key == sf::Keyboard::Key::Escape) mapMode = false;
            else if (key == Key::C) heroMode = true;
            else if (key >= sf::Keyboard::Key::Num1 && key <= sf::Keyboard::Key::Num8) {
                const auto index = static_cast<std::size_t>(static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num1));
                beginGame(LEVELS[index].seed, index);
            } else return;
            return click();
        }
        if (key == sf::Keyboard::Key::Escape) {
            if (seedMode) { closeSeedMenu(); click(); }
            else if (session) { pauseGame(); click(); }
            autoWalk = false;
            held.clear();
            return;
        }
        if (seedMode) {
            if (key == sf::Keyboard::Key::Enter) beginCustomGame();
            else if (key == sf::Keyboard::Key::Backspace && seedCaret > 0) { seedText.erase(--seedCaret, 1); }
            else if (key == sf::Keyboard::Key::Left && seedCaret > 0) --seedCaret;
            else if (key == sf::Keyboard::Key::Right && seedCaret < seedText.size()) ++seedCaret;
            else return;
            return click();
        }
        if (key == Key::N) { chooseSeed(); return click(); }
        if (!session) {
            if (key == sf::Keyboard::Key::Enter) showMap();
            else if (key == Key::Tab) showMap();
            else if (key == Key::F1) { helpMode = true; weaponHelp = false; }
            else if (key == Key::C) heroMode = true;
            else return;
            return click();
        }
        if (key == Key::C) { pauseGame(); heroMode = true; return click(); }
        if (key != Key::E) autoWalk = false;
        if (const unsigned bit = shiftBit(key)) {
            if (!shiftKeys) shiftCombo = !held.empty() || session->stunLeft() > relic::GameSession::Time::zero();
            shiftKeys |= bit;
            return;
        }
        if (direction(key)) {
            held.erase(std::remove(held.begin(), held.end(), key), held.end());
            held.push_back(key);
            music.play(Music::Cue::Tap);
            moveManual(key);
            return;
        }
        const int previousHealth = session->game.health;
        if (key >= sf::Keyboard::Key::Num1 && key <= sf::Keyboard::Key::Num3)
            session->setAlgorithm(static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num1));
        else if (key == sf::Keyboard::Key::H) showPath = !showPath;
        else if (key == sf::Keyboard::Key::Tab) { overview = !overview; revealUntil = Clock::now(); }
        else if (key == Key::E) { held.clear(); autoWalk = !autoWalk; nextStep = Clock::now(); }
        else if (key == Key::Space || key == Key::F) strike();
        else if (key == sf::Keyboard::Key::Period) session->waitTurn();
        else return;
        if (key != Key::Space && key != Key::F) click();
        notice = session->notice;
        playDamage(previousHealth);
        checkCompletion();
    }

    void onMouse(sf::Vector2f point) {
        if (archive>=0) {
            if (BACK_BUTTON.contains(point)) archive=-1;
            else if (previousPage.contains(point)) browse(-1);
            else if (nextPage.contains(point)) browse(1);
            return;
        }
        if (readingStory) {
            if (BRIEF_GO.contains(point)) readingStory = false;
            else if (briefing && MODAL_BACK.contains(point)) cancelBriefing();
            return;
        }
        if (folioPage>=0) {
            if (BACK_BUTTON.contains(point)) folioPage=-1;
            else if (previousPage.contains(point)) turnPage(-1);
            else if (nextPage.contains(point)) turnPage(1);
            return;
        }
        if (MUSIC_BUTTON.contains(point) && music.available()) { music.toggle(); return click(); }
        if (heroDetail) {
            if (HERO_CLOSE.contains(point) || HERO_USE.contains(point)) { heroDetail = false; click(); }
            return;
        }
        if (armory) {
            if (point.x>=150 && point.x<=1290 && point.y>=268 && point.y<527) {
                openPage(static_cast<int>((point.y-268)/37));
                return click();
            }
            if (HELP_BACK.contains(point)) { armory = false; click(); }
            return;
        }
        if (helpMode || heroMode) {
            if ((heroMode ? HERO_BACK : HELP_BACK).contains(point)) { helpMode = heroMode = false; click(); }
            else if (helpMode && HELP_PAGE.contains(point)) { weaponHelp = !weaponHelp; click(); }
            else if (heroMode) for (int i = 0; i < static_cast<int>(heroes.size()); ++i)
                if (heroCard(i).contains(point)) { selectHero(i); return click(); }
            return;
        }
        if (briefing) {
            if (BRIEF_GO.contains(point)) startChallenge();
            else if (MODAL_BACK.contains(point)) cancelBriefing();
            else if (BRIEF_HERO.contains(point)) heroMode = true;
            else return;
            return click();
        }
        if (paused) {
            if (RESUME.contains(point)) resumeGame();
            else if (RESELECT_LEVEL.contains(point)) showMap();
            else if (BRIEF_HERO.contains(point)) heroMode = true;
            else return;
            return click();
        }
        if (resultsMode) {
            if (CONTINUE_BUTTON.contains(point)) finishResult();
            return;
        }
        if (mapMode) {
            for (std::size_t i=0;i<weaponIcons.size();++i)
                if (boardSlot(i).contains(point)) { openPage(i); return click(); }
            if (bookButton.contains(point)) { armory=true; return click(); }
            if (storyButton.contains(point)) { browse(1); return; }
            if (MAP_HERO.contains(point)) { heroMode = true; return; }
            if (BACK_BUTTON.contains(point)) { mapMode = false; return; }
            for (std::size_t i = 0; i < LEVELS.size(); ++i) {
                const auto delta = point - LEVELS[i].position;
                if (flowerHit(delta)) {
                    beginGame(LEVELS[i].seed, i);
                    return click();
                }
            }
            return;
        }
        if (seedMode) {
            if (PLAY_BUTTON.contains(point)) beginCustomGame();
            else if (SEED_BUTTON.contains(point)) closeSeedMenu();
            else return;
            return click();
        }
        if (!session) {
            if (PLAY_BUTTON.contains(point)) showMap();
            else if (SEED_BUTTON.contains(point)) chooseSeed();
            else if (MAP_BUTTON.contains(point)) showMap();
            else if (HELP_BUTTON.contains(point)) { helpMode = true; weaponHelp = false; }
            else if (HERO_BUTTON.contains(point)) heroMode = true;
            else return;
            return click();
        }
        if (PAUSE_BUTTON.contains(point)) { pauseGame(); return click(); }
        if (!session->weaponRotationEnabled() && weaponRotationButton().contains(point)) {
            session->resumeWeaponRotation();
            return click();
        }
        for (std::size_t i = 0; i < relic::weapons.size(); ++i)
            if (session->hasWeapon(i) && weaponRow(i).contains(point)) {
                session->selectWeapon(static_cast<int>(i));
                return click();
            }
        if (point.y >= 365 && point.y < 398 && point.x >= SIDEBAR_CONTENT_X && point.x < SIDEBAR_CONTENT_X + 3 * ALGORITHM_BUTTON_STEP) {
            const int index = static_cast<int>((point.x - SIDEBAR_CONTENT_X) / ALGORITHM_BUTTON_STEP);
            session->setAlgorithm(index); autoWalk = false; return click();
        }
        if (!MAZE_AREA.contains(point)) return;
        const auto mapPoint = mazeView.getCenter() - mazeView.getSize() / 2.f +
                              (point - MAZE_AREA.position) / cameraZoom;
        const int x = static_cast<int>((mapPoint.x - MAP_X) / TILE);
        const int y = static_cast<int>((mapPoint.y - MAP_Y) / TILE);
        if (x < 0 || x >= relic::WIDTH || y < 0 || y >= relic::HEIGHT) return;
        const int cell = y * relic::WIDTH + x;
        if (session->game.world.walkable(cell)) {
            session->setTarget(cell);
            autoWalk = false;
            const auto result = session->route();
            notice = result.path.empty() ? u8"无法到达此处。" : u8"已选定目标，按 E 自动行走。";
            click();
        }
    }

    static sf::FloatRect boardSlot(std::size_t i) { return {{272.f+128.f*i,776.f},{124,52}}; }
    std::array<SpriteAsset,7> weaponIcons{SpriteAsset("folio/original-0.jpg"),SpriteAsset("folio/original-1.jpg"),SpriteAsset("folio/new-0.jpg"),SpriteAsset("folio/original-3.jpg"),SpriteAsset("folio/new-1.jpg"),SpriteAsset("folio/new-2.jpg"),SpriteAsset("folio/new-3.jpg")};
    int folioPage = -1;
    sf::Texture folioArt;
    const sf::FloatRect bookButton{{65,23},{210,44}}, storyButton{{300,23},{210,44}};
    const sf::FloatRect previousPage{{420,785},{190,52}}, nextPage{{830,785},{190,52}};
    const std::array<const char*,9> plates{"original-0","original-1","new-0","original-3","new-1","new-2","new-3","orchard-0","orchard-1"};
    const std::array<const char*,9> titles{"长风鞭","幻影之泪","圆月弯刀","魔杖","梅花亮银枪","生死牌","虚空之剑","西圃秋记 · 三颗斑","西圃秋记 · 留一个梨"};
    const std::array<const char*,9> captions{
        "长鞭盘落，梢头还绕着半个圈。","金线拢住一滴蓝光，水滴里藏着另一重影子。",
        "月纹沿着深蓝刀背，隐入弯曲的银光里。","杖顶的蓝晶像夜空里一颗碎星。",
        "梅纹刻在银色枪头上，红穗从枪缨处垂下。","两面牌一明一暗。把它们并在一起，两个人像才会相望。",
        "维蕾的剑。旧日的侧门边，绞盘上还留着剑尖的缺口。",
        "妮娜把梨转了半圈：“你看，这面像你。鼻子旁边也有三颗。”",
        "父亲摸了摸鼻子。妮娜把梨收进包里：“这个留给 Flora。花期我替她记好了。”"};
    void openPage(int page) {
        if (!folioArt.loadFromFile(assetPath((std::string("folio/")+plates[page]+".jpg").c_str()))) return;
        folioArt.setSmooth(true);
        folioPage=page;
    }
    void turnPage(int step) {
        const int first=folioPage<7 ? 0 : 7, count=folioPage<7 ? 7 : 2;
        openPage(first+(folioPage-first+step+count)%count);
    }
    void drawFolio() {
        window.clear({29,31,33});
        const bool story=folioPage>=7;
        centeredLabel(window,titleFont(),titles[folioPage],{720,60},42,{247,220,169});
        sf::Sprite art(folioArt);
        const auto size=folioArt.getSize();
        const float scale=std::min(1220.f/size.x,545.f/size.y);
        art.setScale({scale,scale});
        art.setPosition({(1440-size.x*scale)/2,110+(545-size.y*scale)/2});
        window.draw(art);
        centeredLabel(window,displayFont(),captions[folioPage],{720,710},24,{242,233,211});
        centeredLabel(window,displayFont(),std::to_string(story?folioPage-6:folioPage+1)+(story?" / 2":" / 7"),{720,810},21,{211,196,164});
        drawAntiqueButton(previousPage,"上一页");
        drawAntiqueButton(nextPage,"下一页");
        drawAntiqueButton(BACK_BUTTON,"返回 [Esc]");
    }
    void draw() {
        if (readingStory || archive>=0) { drawStory(); window.display(); return; }
        if (folioPage>=0) { drawFolio(); window.display(); return; }
        if (mapMode) {
            drawMap();
        } else if (!session || seedMode) {
            drawMenu();
        } else {
            window.clear(sf::Color(12, 16, 21));
            drawSeasonLandscape({0,82},{1440,818});
            box(window, {0, 0}, {WINDOW_W, 82}, sf::Color(24, 31, 39));
            text(window, displayFont(), activeLevel ? LEVELS[*activeLevel].name : u8"自定义探险", 32, 25, 28);
            text(window, font, currentSeason().name, 357, 32, 19, currentSeason().accent);
            polygon(window, {{566, 0}, {874, 0}, {845, 82}, {595, 82}}, sf::Color(111, 85, 50));
            polygon(window, {{573, 0}, {867, 0}, {840, 76}, {600, 76}}, sf::Color(15, 22, 29));
            const auto remaining = std::max(Clock::duration::zero(), timeBudget() - playTime());
            const auto clockColor = remaining <= std::chrono::seconds(30) ? sf::Color(255, 118, 108) : sf::Color(247, 205, 116);
            centeredLabel(window, font, formatTime(std::chrono::ceil<std::chrono::seconds>(remaining)), {720, 32}, 40, clockColor, true);
            centeredLabel(window, font, u8"剩余时间", {720, 65}, 13, clockColor);
            updateCamera();
            drawGame();
            if (!resultsMode && !briefing) drawButton(PAUSE_BUTTON,u8"暂停 / 换关 [Esc]");
            if (paused) {
                modal(u8"暂停");
                drawButton(RESUME,u8"继续游戏 [Enter]",true);
                drawButton(RESELECT_LEVEL,u8"放弃本局并选关 [Tab]");
                drawHeroChoice();
            }
            if (resultsMode) drawResults();
            if (briefing) drawBriefing();
        }
        if (helpMode) drawHelp();
        if (armory) drawWeapons(false);
        if (heroMode) drawHeroPicker();
        if (heroDetail) drawHeroDetails();
        if (music.available()) {
            const auto* label = music.muted() ? u8"声音关 [M]" : u8"声音开 [M]";
            if (!mapMode && (!session || seedMode)) drawAntiqueButton(MUSIC_BUTTON, label);
            else drawButton(MUSIC_BUTTON, label);
        }
        window.display();
    }

    void drawButton(const sf::FloatRect& bounds, const std::string& title, bool primary = false) {
        const auto pointer = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        const bool hover = bounds.contains(pointer);
        if (mapMode) {
            cutPanel(window,bounds,5,hover ? sf::Color(255,243,214) : sf::Color(246,232,202,237),
                     {125,98,71});
            centeredLabel(window,titleFont(),title,bounds.getCenter(),21,{65,48,37});
            return;
        }
        const sf::Color gold(247, 205, 116), light(230, 237, 240);
        box(window, bounds.position, bounds.size,
            primary ? (hover ? gold : sf::Color(224, 171, 77))
                    : (hover ? sf::Color(65, 83, 96) : sf::Color(37, 49, 61)),
            primary ? gold : sf::Color(97, 114, 123));
        centeredLabel(window, mapMode ? titleFont() : font, title, bounds.getCenter(), primary ? 26 : 19,
                      primary ? sf::Color(33, 22, 16) : light);
    }

    void drawAntiqueButton(const sf::FloatRect& bounds, const std::string& title) {
        const bool hover = bounds.contains(window.mapPixelToCoords(sf::Mouse::getPosition(window)));
        const auto metal = hover ? sf::Color(225, 192, 135) : sf::Color(134, 115, 89);
        cutPanel(window, bounds, 7, hover ? sf::Color(66, 53, 48) : sf::Color(25, 28, 37), metal);
        centeredLabel(window, displayFont(), title, bounds.getCenter(), 19, {236, 219, 188});
    }

    void drawMap() {
        const auto& mapFont = titleFont();
        window.clear({37,40,37});
        sf::Sprite painting(valley);
        const auto size = valley.getSize();
        painting.setScale({float(WINDOW_W)/size.x, float(WINDOW_H)/size.y});
        window.draw(painting);
        const sf::Color ink(57,49,40);
        centeredLabel(window, mapFont, u8"四季花径", {720,65}, 62, ink, false, 1.12f);
        centeredLabel(window, displayFont(), u8"任选一朵花进入，通关后恢复花色", {720,113}, 19, ink);
        for (std::size_t road = 0; road < ROADS.size(); ++road) {
            const auto& edge = ROADS[road];
            const auto from = LEVELS[edge[0]].position;
            const auto to = LEVELS[edge[1]].position;
            const auto direction = to - from;
            const auto normal = sf::Vector2f(-direction.y, direction.x) /
                                std::hypot(direction.x, direction.y);
            const float bend = (road % 2 == 0 ? 1.f : -1.f) * (33.f + (road % 3) * 14.f);
            const auto control = (from + to) / 2.f + normal * bend;
            const bool visited = cleared[edge[0]] && cleared[edge[1]];
            for (int pebble = 0; pebble <= 24; ++pebble) {
                const float t = pebble / 24.f;
                const auto p = from * ((1.f-t)*(1.f-t)) + control * (2.f*t*(1.f-t)) + to * (t*t);
                disc(window, p + normal * std::sin(pebble * 1.7f) * 2.f,
                     1.7f + (pebble % 3) * .35f, visited ? sf::Color(130,60,39,205) : sf::Color(87,74,54,155));
            }
        }
        const auto pointer = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        for (std::size_t i = 0; i < LEVELS.size(); ++i) {
            const auto position = LEVELS[i].position;
            const auto delta = pointer - position;
            const bool hover = flowerHit(delta);
            const auto& season = SEASONS[LEVEL_SEASONS[i]];
            const float side = hover ? 75.f : 66.f;
            // A warm mounting behind the specimen keeps white petals readable on snow.
            disc(window,position,side*.48f,{246,232,202,225});
            auto& atlas = cleared[i] || hover ? flowers : dormantFlowers;
            atlas.draw(window,position-sf::Vector2f(side/2,side/2),side,false,i);
            const sf::FloatRect label(position+sf::Vector2f(-81,30),{162,46});
            cutPanel(window,label,5,{247,234,205,242},{123,97,65,186});
            centeredLabel(window,mapFont,std::to_string(i+1)+"  "+LEVELS[i].name,
                          position+sf::Vector2f(0,43),22,ink);
            centeredLabel(window,displayFont(),std::string(season.name)+u8" · "+FLOWER_NAMES[i],
                          position+sf::Vector2f(0,65),15,{100,70,50});
        }
        drawButton(BACK_BUTTON, u8"返回 [Esc]");
        drawButton(bookButton,"武器看板");
        if (std::count(cleared.begin(),cleared.end(),true)) drawButton(storyButton,"花名册 · 重读");
        drawButton(MAP_HERO,u8"换角色 [C]");
        const auto completed = std::count(cleared.begin(), cleared.end(), true);
        for (std::size_t i=0;i<weaponIcons.size();++i) {
            const auto bounds=boardSlot(i);
            cutPanel(window,bounds,4,{245,232,204,245},{128,99,67});
            weaponIcons[i].draw(window,bounds.position+sf::Vector2f(4,6),38,false,0,
                                cleared[i]?sf::Color::White:sf::Color(161,157,150));
            centeredLabel(window,displayFont(),relic::weapons[i].name,bounds.position+sf::Vector2f(81,15),15,ink);
            centeredLabel(window,displayFont(),cleared[i]?"已获得":"第 "+std::to_string(i+1)+" 关",
                          bounds.position+sf::Vector2f(81,36),14,ink);
        }
        centeredLabel(window,mapFont,"已复苏 "+std::to_string(completed)+" / 8 朵花",{720,863},23,ink);
    }

    void modal(const std::string& title) {
        const sf::Color gold(247, 205, 116);
        box(window, {0, 0}, {WINDOW_W, WINDOW_H}, sf::Color(7, 12, 19, 198));
        box(window, {388, 154}, {664, 620}, sf::Color(23, 32, 41), sf::Color(155, 120, 67));
        box(window, {410, 175}, {620, 2}, sf::Color(155, 120, 67));
        centeredLabel(window, displayFont(), title, {720, 231}, 38, gold, true);
    }

    void drawHeroChoice() {
        drawAntiqueButton(BRIEF_HERO, std::string(u8"换角色 [C]  ·  ") + HEROES[hero].name);
    }

    const std::array<const char*,8> beforeStory{
        "西圃梨园的矮凳旁，压着一张折过的纸。是谁留下的\n？","苔庭的旧令上，为什么挤着几十个不同的年份？",
        "雨水漫进石阶，有人曾抱着孩子沿这条路去敲医师的\n门。","石窟里立着一块灾碑。把拓片和花名册放在一起看看\n。",
        "岔口开着两朵红花。弥娅举起扇子，却指向另一边。","一张模糊的底片上，四个人挤在同一辆板车里。",
        "侧门的绞盘停在半空，齿间闪着一点碎银。","霜冠之门后，秋梨一遍遍落下。树旁的矮凳为谁留着\n？"};
    struct Leaf { const char* title; const char* body; };
    const std::array<Leaf,15> leaves{{
        {"西圃秋记", "“东边那棵十二号开的，井边那棵十四号，最小的十\n九号。你每次来都要数，今年我替你看过了。”纸在\nFlora 的册子里夹了半个年头。\n\n秋天她又来吃梨。妮娜把一只梨转向父亲：“你看，\n这面像你。鼻子旁边也有三颗。”这回 Flora\n坐着吃完了。"},
        {"照旧", "“已去看过，西圃无人。今夜可引霜。”那年秋天，\n账房拨钱修了空棚，给看园人住。\n\n第四十七年，梨花又赶上晚霜。神殿取出旧令，在今\n年的日期旁批了两个字。“照旧。”"},
        {"天亮以前", "“西圃。夜诊。母亲抱来时，孩子裹着两件大人的外\n套。”医师在名字旁边写下了时辰。\n\n父亲把凳子搬到 Flora\n身旁：“我们半夜就去敲门了。她没熬到天亮。”"},
        {"在籍者全", "“是夕开门，在籍者全。”书记官照着撤离后那一日\n的花名册，从头点到末页。\n\nFlora 曾亲笔补过“妮娜”。第二天，夹页上\n三个日期还在，那两个字又淡了。"},
        {"红扇指路", "雪里分不清水面和石路。弥娅把两朵绢花系在最薄的\n冰旁，自己站到风口。\n\n“花好看吧？回来再摘。现在往照相馆走。”"},
        {"留影", "奥伦撕下遮光布，把四名伤员裹严，挨个扶上板车。\n有人求他留张相，等上了山好找家人。\n\n“好了，看这里。”快门响时，最小的那个偏偏抬起\n手。"},
        {"门楔", "剑尖楔进齿轮时，侧门停在半空。四个伤员都低下头\n，板车从门底擦了过去。\n\n维蕾在桥头数到第四个人，才冲岸上的花灯举起手。\n她脚下的石板还在抖。花根一直托到车轮上了岸。"},
        {"霜冠之门", "心兽伏下，冰霜消退。它身后竟是一座秋日梨园。\n\n空凳边，一只梨落下来，滚了半圈，又回到了枝头。"},
        {"灾夜至今", "八朵花从桥下亮起。最后一个车轮上岸，远处的西门\n沉了下去。\n\n三年过去，旧城留在山下。西山上，升起了人家的炊\n烟。"},
        {"树下", "八花复苏，Flora 在旧梨园里恢复了人形。幻\n景散去，她仍坐在矮凳边。\n\n你递过折纸。她用拇指抚平了“十九号”，把纸收好\n，站了起来。"},
        {"篱笆外", "三年后的秋天。父亲抬头，看见篱笆外那朵五瓣蓝花\n。\n\n“那年我总问收了多少梨。你家里冷不冷，我一次也\n没问过。”父亲放下手上的活。"},
        {"吃梨", "父亲把矮凳从桌下勾出来：“坐吧。今年这棵结得有\n点酸。”\n\nFlora 坐下，把梨转了半圈。三颗斑还在梨皮\n上。父亲摸了摸鼻子。"},
        {"花都往事", "Flora 第一次给妮娜送花，是照着册上“西圃\n梨园”四个字找去的。后来不用翻册，她也记得那道\n篱笆。梨花开时，孩子替她记日子。秋天梨熟，两个\n人就坐下来，听父亲说今年哪一棵甜。"},
        {"花都往事", "那一年，神殿又把晚霜引去了西圃。妮娜没能活到天\n亮。Flora 顺着霜痕找到当年的旧令，也看见\n了城下的裂缝。灾夜，弥娅站在岔口挥扇，奥伦和维\n蕾送伤员过桥。Flora 用尽神力，让脚下的路\n多撑了一程。半座城塌进地下，人们带着还能用的东\n西去了西山。看园人带走了几根梨枝。"},
        {"花都往事", "第三个秋天，山上的梨熟了。八朵花重新开放，Fl\nora 收回妮娜留下的折纸。那孩子费劲记下三个\n日子，是想让她坐下来，好好吃一个梨。她从旧梨园\n起身，去了西山。"}
    }};
    SpriteAsset storyArt{"folio/chronicle.jpg",12,4};
    SpriteAsset autumnMemory{"folio/orchard-0.jpg"};
    int archive = -1;
    bool endingPending = false;
    bool availableLeaf(int page) const {
        return page < 8 ? cleared[page] : std::all_of(cleared.begin(),cleared.end(),[](bool v){return v;});
    }
    void browse(int step) {
        for (int n=archive+step;n>=0 && n<static_cast<int>(leaves.size());n+=step)
            if (availableLeaf(n)) { archive=n; return; }
        if (step>0) archive=-1;
    }
    void drawStory() {
        const int i=archive>=0 ? archive : static_cast<int>(*activeLevel);
        const bool opening=archive<0 && briefing;
        window.clear({33,40,39});
        box(window,{85,100},{1270,565},{240,228,198},{158,127,88});
        if (i==0 && !opening) autumnMemory.draw(window,{105,114},540);
        else storyArt.draw(window,{105,114},540,false,std::min(i,11));
        const std::string heading=i<8 ? std::string(LEVELS[i].name)+(opening?" · 入境":" · 回忆") : "终章 · 今年的梨";
        text(window,displayFont(),heading,693,155,23,{110,83,58});
        text(window,titleFont(),leaves[i].title,690,200,52,{69,47,38});
        text(window,displayFont(),opening?beforeStory[i]:leaves[i].body,692,316,22,{55,47,42});
        if (archive>=0) {
            drawAntiqueButton(BACK_BUTTON,"返回花径 [Esc]");
            drawAntiqueButton(previousPage,"上一页");
            drawAntiqueButton(nextPage,"继续 [Enter]");
        } else {
            if (opening) drawButton(MODAL_BACK,"返回选关 [Esc]");
            drawButton(BRIEF_GO,opening?"关卡准备 [Enter]":"查看结算 [Enter]",true);
        }
    }

    void drawBriefing() {
        modal(activeLevel ? std::to_string(*activeLevel + 1) + "  " + LEVELS[*activeLevel].name : u8"自定义探险");
        if (activeLevel) {
            const auto i=*activeLevel;
            const std::string reward=i<relic::weapons.size() ?
                std::string(cleared[i]?"已拥有 · ":"首次通关获得 · ")+relic::weapons[i].name : "终关奖励 · 霜冠之花";
            centeredLabel(window,displayFont(),reward,{720,307},18,{247,205,116});
        }
        const auto& limits = session->limits;
        const std::array<std::string, 3> labels{u8"时间上限", u8"步数上限", u8"地形消耗上限"};
        const std::array<std::string, 3> values{formatTime(std::chrono::seconds(limits.seconds)),
                                               std::to_string(limits.steps), std::to_string(limits.cost)};
        const sf::Color gold(247, 205, 116);
        for (int i = 0; i < 3; ++i) {
            const float x = 421.f + 201.f * i;
            box(window, {x, 327}, {195, 114}, sf::Color(33, 46, 54), sf::Color(70, 88, 96));
            centeredLabel(window, font, labels[i], {x + 97.5f, 353}, 17, sf::Color(191, 217, 213));
            centeredLabel(window, font, values[i], {x + 97.5f, 400}, 34, gold, true);
        }
        const int level = session->game.expeditionLevel;
        const int guardCount = relic::GameSession::guardCount(level);
        const auto low = relic::GameSession::guardDamage(relic::GameSession::guardTier(level, 0));
        const auto high = relic::GameSession::guardDamage(relic::GameSession::guardTier(level, guardCount - 1));
        const auto damage = low == high ? std::to_string(low) : std::to_string(low) + "–" + std::to_string(high);
        centeredLabel(window, displayFont(), u8"守卫 " + std::to_string(guardCount) + u8" 名   单次伤害 " + damage,
                      {720, 488}, 21, sf::Color(233, 168, 145));
        centeredLabel(window, font, u8"拾取时钟可增加 30 秒。确认后开始计时。",
                      {720, 546}, 19, sf::Color(210, 224, 224));
        const char* rule = u8"集齐四朵花后前往出口。";
        using Hazard = relic::GameSession::Hazard;
        switch (session->hazard) {
        case Hazard::Fog: rule = u8"三处灰白迷雾，踏入眩晕 5 秒，12 秒后重聚。"; break;
        case Hazard::Rain: rule = u8"乌云会追赶你。进入雨区扣 10 点生命，停留每 2 秒再扣一次。"; break;
        case Hazard::Mirror: rule = u8"约 8% 的地砖会触发传送，未收集的花也会移位。"; break;
        case Hazard::None: break;
        }
        if (level >= 4) {
            constexpr std::array<const char*, 4> encounters{{
                u8"小心两朵假花，误采会眩晕 5 秒。击败首领可拾取第四朵花。",
                u8"四只木乃伊缓慢追踪，靠近会叠加伤害。第四朵花由首领掉落。",
                u8"五只幽魂中三只藏在雕像里。击败首领后拾取第四朵花。",
                u8"终极首领会唤醒三个棺材里的傀儡。击败它后拾取第四朵花。"
            }};
            rule = encounters[level - 4];
        }
        centeredLabel(window, displayFont(), rule, {720, 582}, 19, sf::Color(210, 224, 224));
        drawButton(MODAL_BACK, u8"返回 [Esc]");
        drawHeroChoice();
        drawButton(BRIEF_GO, u8"出发 [Enter]", true);
    }

    void drawHelp() {
        if (weaponHelp) { drawWeapons(true); return; }
        using relic::GameSession;
        const auto& type = displayFont();
        const sf::Color gold(244, 212, 153), light(228, 227, 219), muted(186, 197, 201);
        box(window, {0, 0}, {WINDOW_W, WINDOW_H}, {7, 12, 19, 213});
        frameTracery(window, {{112, 106}, {1216, 688}}, {18, 23, 34}, {176, 146, 107});
        centeredLabel(window, titleFont(), u8"探险指南", {720, 156}, 53, gold);
        text(window, type, u8"游玩规则", 153, 203, 25, gold);
        text(window, type, u8"八关限制", 746, 203, 25, gold);
        box(window, {152, 240}, {543, 1}, {101, 88, 75});
        box(window, {744, 240}, {542, 1}, {101, 88, 75});
        const std::array<std::array<const char*, 3>, 6> rules{{
            {u8"通关", u8"每关集齐四朵花并走到出口。出发后开始计时。", u8"拾取一个时钟增加 30 秒。"},
            {u8"移动", u8"方向键或 WASD 移动，成功移动一次计 1 步。", u8"地面耗 1，沙地耗 5，木桥耗 3。池水不可走。"},
            {u8"跳跃", u8"按住 Shift 加方向，跳过一格碎石，耗 3、计 1 步。", u8"落点须能站立，且没有守卫。"},
            {u8"战斗", u8"空格或 F 攻击相邻守卫，伤害 10。看到红圈就闪开。", u8"守卫需打 2–5 次。药剂恢复 45 点生命。"},
            {u8"寻路", u8"点地图设目标，按 1–3 切换算法，E 启停自动走。", u8"自动走会绕过碎石和池水，靠近守卫时停止。"},
            {u8"失败", u8"生命或时间归零，或尝试超出步数或消耗上限。", u8"步数和消耗正好等于上限时仍可通关。"}
        }};
        for (std::size_t i = 0; i < rules.size(); ++i) {
            const float y = 263.f + i * 55.f;
            text(window, type, rules[i][0], 153, y, 19, gold);
            text(window, type, rules[i][1], 221, y, 17, light);
            text(window, type, rules[i][2], 221, y + 23, 16, muted);
        }
        constexpr std::array<float, 6> columns{817, 924, 993, 1068, 1139, 1233};
        constexpr std::array<const char*, 6> headers{u8"关卡", u8"时限", u8"步数", u8"消耗", u8"时钟", u8"守卫人数"};
        for (std::size_t i = 0; i < headers.size(); ++i)
            centeredLabel(window, type, headers[i], {columns[i], 262}, 17, gold);
        for (std::size_t i = 0; i < LEVELS.size(); ++i) {
            const auto level = static_cast<int>(i);
            const auto& limits = GameSession::challenges[i];
            const std::array<std::string, 6> cells{
                std::to_string(i + 1) + "  " + LEVELS[i].name, formatTime(std::chrono::seconds(limits.seconds)),
                std::to_string(limits.steps), std::to_string(limits.cost), std::to_string(GameSession::clockCount(level)),
                std::to_string(GameSession::guardCount(level))
            };
            const float y = 294.f + i * 34.f;
            box(window, {744, y - 16}, {542, 32}, i % 2 ? sf::Color(25, 31, 44) : sf::Color(32, 40, 53));
            for (std::size_t column = 0; column < cells.size(); ++column)
                centeredLabel(window, type, cells[column], {columns[column], y}, 18, light);
        }
        text(window, type, u8"第 5–8 关的第四朵花由首领掉落。", 747, 558, 16, muted);
        text(window, type, u8"假花、木乃伊、雕像和棺材规则见各关出发说明。", 747, 581, 16, muted);
        cutPanel(window, {{149, 608}, {1142, 108}}, 10, {34, 32, 44}, {126, 106, 78});
        text(window, type, u8"八花齐聚", 169, 630, 25, gold);
        text(window, type, u8"每个预设关卡首次通关，会在花径复苏一朵遗迹之花。八关共八朵。", 321, 628, 20, light);
        text(window, type, u8"后续剧情中，八花齐聚便可拼合花神碎片，召唤花神，重返昔日花都，揭晓不为人知的历史。", 321, 665, 19, light);
        drawAntiqueButton(HELP_BACK, u8"返回 [Esc]");
        drawAntiqueButton(HELP_PAGE, u8"机关与武器 [Tab]");
    }

    void drawWeapons(bool guide) {
        const auto& type = displayFont();
        const sf::Color gold(244, 212, 153), light(228, 227, 219), muted(165, 178, 188);
        box(window, {0, 0}, {WINDOW_W, WINDOW_H}, {7, 12, 19, 213});
        frameTracery(window, {{112, 106}, {1216, 688}}, {18, 23, 34}, {176, 146, 107});
        centeredLabel(window, titleFont(), guide ? u8"机关与武器" : u8"武器库", {720, 156}, 49, gold);
        centeredLabel(window, type, u8"首次通关前七个预设关卡，对应武器就会进入武器库", {720, 207}, 22, light);
        constexpr std::array<float, 5> columns{225, 424, 685, 895, 1115};
        constexpr std::array<const char*, 5> headers{u8"通关奖励", u8"武器", u8"大招", u8"伤害", u8"状态"};
        for (std::size_t i = 0; i < columns.size(); ++i)
            centeredLabel(window, type, headers[i], {columns[i], 250}, 18, gold);
        for (std::size_t i = 0; i < relic::weapons.size(); ++i) {
            const auto& weapon = relic::weapons[i];
            const float y = 284.f + 37.f * i;
            box(window, {150, y - 16}, {1140, 34}, i % 2 ? sf::Color(24, 30, 42) : sf::Color(31, 39, 52));
            const std::array<std::string, 5> cells{
                u8"第 " + std::to_string(i + 1) + u8" 关", weapon.name,
                std::string(weapon.skill) + (weapon.wholeMaze ? u8" · 全图" : ""),
                std::to_string(weapon.damage), cleared[i] ? u8"已解锁" : u8"未解锁"
            };
            for (std::size_t j = 0; j < cells.size(); ++j)
                centeredLabel(window, type, cells[j], {columns[j], y}, 20, j == 4 && !cleared[i] ? muted : light);
        }
        text(window, type, u8"单按 Shift 松开施放，武器从低到高自动轮换。右侧点选可锁定武器，再点「恢复轮换」切回。", 174, 539, 20, light);
        text(window, type, u8"武器各自冷却 30 秒。幻泪之域命中全图，其他大招命中相邻敌人。挥空不消耗冷却。", 174, 571, 19, muted);
        if (!guide) centeredLabel(window,type,"点选一行查看武器插画",{720,635},20,gold);
        if (guide) {
            text(window, type, u8"第 2 关  灰白迷雾遮路，踏入眩晕 5 秒，雾散后 12 秒重聚。", 174, 617, 19, light);
            text(window, type, u8"第 3 关  乌云追踪玩家，淋雨扣 10 点生命，持续淋雨每 2 秒再受伤。", 174, 652, 19, light);
            text(window, type, u8"第 4 关  约 8% 的地砖各传送一次，未收集的花会移到别处。", 174, 687, 19, light);
            drawAntiqueButton(HELP_PAGE, u8"游玩规则 [Tab]");
        }
        drawAntiqueButton(HELP_BACK, u8"返回 [Esc]");
    }

    unsigned idleFrame(Clock::time_point now, int slot) const {
        return static_cast<unsigned>((now - cameraEpoch) / HERO_IDLE_FRAME_TIME + slot * 4) % HERO_IDLE_FRAMES;
    }

    void drawHeroPicker() {
        const auto now = Clock::now();
        const auto pointer = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        const sf::Color antiquedGold(171, 141, 101);
        box(window, {0, 0}, {WINDOW_W, WINDOW_H}, sf::Color(7, 12, 19, 213));
        frameTracery(window, {{88, 120}, {1264, 690}}, {16, 19, 29, 248}, antiquedGold);
        centeredLabel(window, displayFont(), u8"选择角色", {720, 173}, 42, {246, 221, 173}, false, 1.19f);
        box(window, {540, 174}, {63, 1}, alpha(antiquedGold, 130));
        box(window, {837, 174}, {63, 1}, alpha(antiquedGold, 130));
        blossom(window, {607, 174}, 4, antiquedGold);
        blossom(window, {833, 174}, 4, antiquedGold);
        for (int i = 0; i < static_cast<int>(heroes.size()); ++i) {
            const auto card = heroCard(i);
            const auto color = HEROES[i].accent;
            const bool hovered = card.contains(pointer);
            const bool selected = i == hero;
            const auto edge = selected ? sf::Color(244, 205, 134)
                                       : alpha(antiquedGold, hovered ? 255 : 177);
            filigreeFrame(window, card, hovered ? sf::Color(34, 35, 49) : sf::Color(23, 25, 36), edge);
            const float center = card.getCenter().x;
            const float top = card.position.y;
            const sf::Vector2f medallion{center, top + 89};
            softLight(window, medallion, {106, 82}, alpha(color, hovered || selected ? 110 : 78));
            pointedMedallion(window, medallion, 59, 151, alpha(color, hovered || selected ? 112 : 58));
            blossom(window, {center, top + 10}, 4.f, selected ? edge : color);
            constexpr float portrait = 176.f;
            const auto frame = idleFrame(now, i);
            if (!heroes[i].draw(window, {center - portrait / 2, top - 2 + 1.3f*std::sin(frame*.262f)}, portrait))
                drawExplorer(window, {center - TILE / 2, top + 78}, {0, 1}, frame / float(HERO_IDLE_FRAMES), i);
            box(window, {center - 65, top + 173}, {130, 1}, alpha(edge, 97));
            centeredLabel(window, displayFont(), HEROES[i].name, {center, top + 190}, 25, color);
            cutPanel(window, {{card.position.x + 21, top + 79}, {33, 27}}, 5,
                     {18, 21, 32}, alpha(edge, 160));
            centeredLabel(window, displayFont(), std::to_string(i + 1), {card.position.x + 37.5f, top + 92}, 16, edge);
            if (selected) {
                cutPanel(window, {{center - 47, top + 204}, {94, 19}}, 5, {63, 49, 37}, alpha(edge, 146));
                centeredLabel(window, displayFont(), u8"当前角色", {center, top + 213.5f}, 14, {247, 224, 180});
            }
        }
        drawAntiqueButton(HERO_BACK, u8"返回 [Esc]");
    }

    void drawHeroDetails() {
        const auto& profile = HEROES[hero];
        const auto color = profile.accent;
        const auto now = Clock::now();
        box(window, {0, 0}, {WINDOW_W, WINDOW_H}, sf::Color(7, 12, 19, 213));
        frameTracery(window, {{185, 121}, {1070, 659}}, {19, 23, 34}, {184, 151, 108});
        const sf::FloatRect gallery({217, 155}, {537, 590});
        filigreeFrame(window, gallery, {24, 29, 43}, alpha(color, 132), 1.24f);
        const auto center = gallery.getCenter() + sf::Vector2f(0, -9);
        for (int halo = 0; halo < 7; ++halo)
            disc(window, center, 239.f - halo * 20.f, alpha(color, 10 + halo));
        pointedMedallion(window, center, 171, 476, alpha(color, 84));
        for (int i = 0; i < 21; ++i) {
            const float angle = 2.39996f * i;
            const float radius = 88.f + 6.8f * i;
            const auto position = center + sf::Vector2f(std::cos(angle), std::sin(angle)) * radius;
            disc(window, position, i % 4 == 0 ? 2.7f : 1.4f, alpha(color, 120));
        }
        if (!heroes[hero].draw(window, {226, 169 + 2.f*std::sin(idleFrame(now,hero)*.262f)}, 518.f))
            drawExplorer(window, center, {0, 1}, 0.f, hero);
        text(window, displayFont(), profile.name, 810, 297, 52, color);
        box(window, {812, 389}, {275, 2}, alpha(color, 146));
        text(window, displayFont(), profile.intro, 812, 429, 22, sf::Color(226, 231, 240));
        drawAntiqueButton(HERO_USE,u8"使用此角色 [Enter]");
        drawAntiqueButton(HERO_CLOSE, u8"×");
    }

    void drawResults() {
        const auto& game = session->game;
        const bool won = game.status == "won";
        const sf::Color gold(247, 205, 116), light(230, 237, 240);
        box(window, {0, 0}, {WINDOW_W, WINDOW_H}, sf::Color(7, 12, 19, 190));
        box(window, {340, 185}, {760, 520}, sf::Color(23, 32, 41), sf::Color(156, 124, 73));
        box(window, {358, 203}, {724, 2}, sf::Color(156, 124, 73));
        centeredLabel(window, font, won ? u8"探险完成" : u8"探险结束", {720, 254}, 43, gold, true);
        centeredLabel(window, displayFont(), activeLevel ? LEVELS[*activeLevel].name : u8"自定义探险",
                      {720, 301}, 22, light);
        const std::array<std::string, 4> labels{u8"用时", u8"花朵", u8"步数", u8"地形消耗"};
        const std::array<std::string, 4> values{formatTime(playTime()), std::to_string(game.collected) + " / 4",
                                                std::to_string(game.steps) + " / " + std::to_string(session->limits.steps),
                                                std::to_string(game.spent) + " / " + std::to_string(session->limits.cost)};
        for (std::size_t i = 0; i < labels.size(); ++i) {
            const float y = 368.f + i * 48.f;
            text(window, font, labels[i], 422, y, 23, light);
            text(window, font, values[i], 627, y, 23, gold);
        }
        if (activeLevel) {
            (cleared[*activeLevel] ? flowers : dormantFlowers).draw(window,{840,383},110,false,*activeLevel);
            centeredLabel(window, displayFont(), won ? u8"遗迹之花已复苏" : cleared[*activeLevel] ? u8"此前已复苏" : u8"花朵尚未绽放",
                          {895, 521}, 18, light);
        }
        if (!won)
            centeredLabel(window, displayFont(), notice, {720, 575}, 19, sf::Color(255, 156, 139));
        if (newWeapon)
            centeredLabel(window, displayFont(), u8"新武器   " + std::string(relic::weapons[*newWeapon].name),
                          {720, 575}, 25, gold);
        if (!progressSaved)
            centeredLabel(window, font, u8"通关进度未能保存", {720, 682}, 18, gold);
        drawButton(CONTINUE_BUTTON, endingPending ? u8"阅读终章 [Enter]" : u8"返回选关 [Enter]", true);
    }

    void drawMenu() {
        const float time = std::chrono::duration<float>(Clock::now() - cameraEpoch).count();
        if (hasGothicHall) drawGothicHall(window, gothicHall, time);
        else drawStoneHall(window);
        const auto& menuFont = displayFont();
        const sf::Color gold(246, 219, 168), light(224, 219, 206);
        softLight(window, {720, 370}, {386, 368}, {7, 12, 22, 168});
        relicSeal(window, {720, 201}, 23, alpha(gold, 198));
        box(window, {590, 201}, {88, 1}, {171, 145, 105, 125});
        box(window, {762, 201}, {88, 1}, {171, 145, 105, 125});
        centeredLabel(window, titleFont(), GAME_TITLE,
                      {720, 290}, 108, gold, false, 1.10f);
        centeredLabel(window, hasBlackletter ? blackletter : menuFont, "Flower Sanctuary",
                      {720, 369}, 39, gold);
        box(window, {604, 406}, {232, 1}, {171, 145, 105, 153});
        blossom(window, {720, 404}, 5, gold);
        if (seedMode) {
            cutPanel(window, {{534, 420}, {372, 42}}, 8, {12, 18, 30}, {164, 139, 106});
            centeredLabel(window, menuFont, u8"地图种子  " + std::string(seedText).insert(seedCaret, "|"), {720, 441}, 23, light);
        }

        const bool playHovered = PLAY_BUTTON.contains(window.mapPixelToCoords(sf::Mouse::getPosition(window)));
        filigreeFrame(window, PLAY_BUTTON, playHovered ? sf::Color(108, 77, 49) : sf::Color(66, 48, 39),
                       playHovered ? gold : sf::Color(195, 155, 100), .8f);
        centeredLabel(window, menuFont, seedMode ? u8"进入遗迹" : u8"开始游戏", {720, 500}, 30, gold);
        centeredLabel(window, menuFont, u8"回车键", {720, 529}, 15, light);
        drawAntiqueButton(SEED_BUTTON, seedMode ? u8"返回 [Esc]" : u8"自定义种子 [N]");
        if (seedMode) {
            centeredLabel(window, menuFont, notice.empty() ? u8"输入 0–4294967295 之间的整数" : notice,
                          {720, 649}, 19, notice.empty() ? light : gold);
        } else {
            drawAntiqueButton(MAP_BUTTON, u8"选择关卡 [Tab]");
            drawAntiqueButton(HELP_BUTTON, u8"玩法帮助 [F1]");
            drawAntiqueButton(HERO_BUTTON, u8"角色 [C]");
            centeredLabel(window, menuFont, u8"方向键 / WASD 移动     ·     空格 攻击", {720, 760}, 18, light);
        }
    }

    static sf::FloatRect weaponRow(std::size_t index) {
        return {{SIDEBAR_CONTENT_X, 455.f + 32.f * index}, {231.f, 29.f}};
    }
    static sf::FloatRect weaponRotationButton() {
        return {{SIDEBAR_CONTENT_X + 142.f, 419.f}, {89.f, 29.f}};
    }

    void drawWeather() {
        using Hazard = relic::GameSession::Hazard;
        constexpr int radius = relic::GameSession::hazardRadius;
        const float time = std::chrono::duration<float>(playTime()).count();
        if (session->hazard == Hazard::None || session->hazard == Hazard::Mirror) return;
        if (!fogTexture) {
            fogTexture.emplace(makeFogImage());
            fogTexture->setSmooth(true);
        }
        if (session->hazard == Hazard::Fog) {
            for (const auto& patch : session->fogPatches()) {
                float strength = 1.f, spread = 1.f;
                if (patch.reforms != relic::GameSession::Time::zero()) {
                    const auto sinceReform = gameTime() - patch.reforms;
                    if (sinceReform < relic::GameSession::Time::zero()) {
                        const float dispersal = std::clamp(std::chrono::duration<float>(
                            sinceReform + relic::GameSession::fogReformDelay).count() / .9f, 0.f, 1.f);
                        strength = (1.f - dispersal) * (1.f - dispersal);
                        spread += dispersal * .42f;
                    } else {
                        strength = std::clamp(std::chrono::duration<float>(sinceReform).count() / .85f, 0.f, 1.f);
                    }
                }
                if (strength <= 0.f) continue;
                const auto center = tilePosition(patch.center) + sf::Vector2f(TILE * .5f, TILE * .5f);
                const float phase = time + patch.center * .131f;
                for (int layer = 0; layer < 3; ++layer) {
                    const float drift = phase * (.33f + layer * .13f) + layer * 2.39f;
                    const float reach = TILE * (radius + 1.8f) * spread;
                    sf::Sprite mist(*fogTexture);
                    const auto size = sf::Vector2f(fogTexture->getSize());
                    mist.setOrigin(size / 2.f);
                    const sf::Vector2f scale{reach * (2.f + layer * .19f) / size.x,
                                             reach * (1.91f - layer * .19f) / size.y};
                    mist.setScale({layer == 1 ? -scale.x : scale.x, scale.y});
                    mist.setPosition(center + sf::Vector2f(std::sin(drift) * (4.f + layer * 4.f),
                                     std::cos(drift * .73f) * 6.f + (layer - 1) * 7.f));
                    mist.setRotation(sf::degrees(std::sin(drift * .55f) * 9.f + (layer - 1) * 13.f));
                    const std::array<uint8_t, 3> opacity{253, 219, 177};
                    mist.setColor(alpha(sf::Color::White, static_cast<uint8_t>(opacity[layer] * strength)));
                    window.draw(mist);
                }
            }
        }
        if (session->hazard == Hazard::Rain) {
            const auto center = session->rainCenter();
            const auto p = sf::Vector2f(MAP_X + (center[0] + .5f) * TILE, MAP_Y + (center[1] + .5f) * TILE);
            const float reach = TILE * radius;
            softLight(window, p, {reach * 1.18f, reach}, {38, 62, 83, 112});
            sf::VertexArray drops(sf::PrimitiveType::Lines);
            const float wind = 3.4f + std::sin(time * 1.3f) * .9f;
            for (int i = 0; i < 76; ++i) {
                // Keep landing points inside the rain region while streaks fall from above.
                const float x = ((i * 43) % 79 - 39) / 39.f * reach;
                const float halfHeight = reach - std::abs(x);
                const float across = std::fmod(i * .381966f, 1.f);
                const sf::Vector2f landing = p + sf::Vector2f(x, (2.f * across - 1.f) * halfHeight);
                const float cycle = std::fmod(time * (2.0f + (i % 7) * .16f) + i * .618034f, 1.f);
                const float length = 5.f + i % 8;
                if (cycle < .88f) {
                    const float height = 1.f - cycle / .88f;
                    const sf::Vector2f tip = landing + sf::Vector2f(height * wind * 3.f, -height * 40.f);
                    drops.append({tip + sf::Vector2f(wind, -length), {153, 194, 218, 17}});
                    drops.append({tip, {192, 226, 245, static_cast<uint8_t>(108 + i % 61)}});
                } else if (i % 3 == 0) {
                    const float splash = (cycle - .88f) / .12f;
                    sf::CircleShape ripple(1.f + splash * 3.f, 14);
                    ripple.setOrigin({ripple.getRadius(), ripple.getRadius()});
                    ripple.setPosition(landing);
                    ripple.setScale({1.f, .43f});
                    ripple.setFillColor(sf::Color::Transparent);
                    ripple.setOutlineThickness(.65f);
                    ripple.setOutlineColor({174, 218, 242, static_cast<uint8_t>(146.f * (1.f - splash))});
                    window.draw(ripple);
                }
            }
            window.draw(drops);

            const auto textureSize = sf::Vector2f(fogTexture->getSize());
            for (int cloud = 0; cloud < 3; ++cloud) {
                const float phase = time * .62f + cloud * 2.4f;
                const auto cloudCenter = p + sf::Vector2f((cloud - 1) * 23.f + 4.f * std::sin(phase),
                    -36.f - (cloud == 1 ? 8.f : 0.f) + 3.f * std::cos(phase));
                const float width = cloud == 1 ? 64.f : 51.f;
                sf::Sprite vapor(*fogTexture);
                vapor.setOrigin(textureSize / 2.f);
                vapor.setScale({width / textureSize.x, 32.f / textureSize.y});
                vapor.setPosition(cloudCenter + sf::Vector2f(0, 4.f));
                vapor.setColor({51, 63, 83, 239});
                window.draw(vapor);
                vapor.setRotation(sf::degrees((cloud - 1) * 12.f));
                vapor.setPosition(cloudCenter - sf::Vector2f(2.f, 2.f));
                vapor.setColor({126, 143, 162, 214});
                window.draw(vapor);
            }
        }
    }

    void drawGame() {
        const auto& game = session->game;
        const auto& season = currentSeason();
        box(window, MAZE_AREA.position - sf::Vector2f(6, 6), MAZE_AREA.size + sf::Vector2f(12, 12),
            sf::Color(20, 25, 30), sf::Color(82, 79, 65));
        window.setView(mazeView);
        drawSeasonLandscape(MAZE_AREA.position,MAZE_AREA.size);
        for (int id = 0; id < relic::CELLS; ++id) {
            const auto pos = tilePosition(id);
            const auto variant = static_cast<std::uint8_t>((id * 13 + id / relic::WIDTH * 7) % 7);
            if (game.world.tile[id] == 0) {
                if ((id % relic::WIDTH > 0 && game.world.walkable(id - 1)) ||
                    (id % relic::WIDTH < relic::WIDTH - 1 && game.world.walkable(id + 1)) ||
                    game.world.walkable(id - relic::WIDTH) || game.world.walkable(id + relic::WIDTH)) {
                    box(window, pos, {TILE - 1, TILE - 1}, sf::Color(29 + variant, 35 + variant, 39 + variant));
                    if (id % 11 == 0) disc(window, pos + sf::Vector2f(6, 8), 2.8f, season.accent);
                }
                continue;
            }
            if (game.world.tile[id] == relic::Water || game.world.tile[id] == relic::Bridge) {
                box(window, pos, {TILE, TILE}, sf::Color(32, 102, 136));
                for (int line = 0; line < 2; ++line)
                    box(window, pos + sf::Vector2f(2 + (variant + line * 3) % 8, 4 + line * 9), {6, 1}, sf::Color(101, 188, 199));
                if (game.world.tile[id] == relic::Bridge)
                    for (int plank = 0; plank < 3; ++plank)
                        box(window, pos + sf::Vector2f(1, 1 + plank * 6), {TILE - 2, 5}, sf::Color(178, 124 + plank * 8, 70));
                continue;
            }
            if (game.world.tile[id] == relic::Rubble) {
                box(window, pos, {TILE - 1, TILE - 1}, season.floor);
                polygon(window, {pos + sf::Vector2f(1, 15), pos + sf::Vector2f(6, 5), pos + sf::Vector2f(13, 3),
                                 pos + sf::Vector2f(17, 13), pos + sf::Vector2f(14, 16)}, sf::Color(82, 80, 96));
                box(window, pos + sf::Vector2f(6, 7), {8, 2}, sf::Color(174, 169, 172));
                continue;
            }
            const bool sand = game.world.tile[id] == relic::Sand;
            const auto color = sand ? season.sand : season.floor;
            box(window, pos, {TILE - 1, TILE - 1}, sf::Color(color.r + variant, color.g + variant, color.b + variant));
            box(window, pos + sf::Vector2f(1, 1), {TILE - 3, 1},
                alpha(season.accent, 55));
        }
        if (session->hazard == relic::GameSession::Hazard::Mirror) {
            for (int cell = 0; cell < relic::CELLS; ++cell) if (session->mirrorAt(cell)) {
                const auto p = tilePosition(cell) + sf::Vector2f(9.5f, 9.5f);
                polygon(window, {p + sf::Vector2f(-5, 0), p + sf::Vector2f(0, -5),
                                 p + sf::Vector2f(5, 0), p + sf::Vector2f(0, 5)}, {174, 157, 223, 100});
                box(window, p - sf::Vector2f(1, 2), {2, 4}, {232, 215, 249, 170});
            }
        }
        // Paint behind every interactive object and route marker, inside the maze view.
        const float seconds = std::chrono::duration<float>(Clock::now()-cameraEpoch).count();
        for (int i=0; i<64; ++i) {
            const float phase = seconds*(.5f+.07f*(i%5)) + i*1.7f;
            const bool firefly = &season == &SEASONS[1];
            const float fall = firefly ? 0.f : seconds*(8.f+i%11);
            const float x = std::fmod(i*117.f + 16.f + 15.f*std::sin(phase), MAZE_AREA.size.x);
            const float y = std::fmod(i*173.f + 7.f + fall + 6.f*std::cos(phase), MAZE_AREA.size.y);
            const auto p = MAZE_AREA.position + sf::Vector2f(x,y);
            if (firefly) {
                const auto glow = static_cast<std::uint8_t>(80 + 45*std::sin(phase));
                disc(window,p,2.8f,{225,230,134,static_cast<std::uint8_t>(glow/3)});
                disc(window,p,1.f,{245,247,161,glow});
            } else if (&season == &SEASONS[3]) {
                disc(window,p,1.f+.35f*(i%3),{231,243,250,156});
            } else {
                const float tilt = 2.2f*std::sin(phase);
                polygon(window,{p+sf::Vector2f(-3,tilt),p+sf::Vector2f(-1,-2),
                                p+sf::Vector2f(3,-tilt),p+sf::Vector2f(0,1.7f)},alpha(season.accent,146));
            }
        }
        const auto exit = tilePosition(game.world.exit);
        const auto exitColor = game.collected == 4 ? sf::Color(249, 181, 73) : sf::Color(174, 118, 74);
        box(window, exit + sf::Vector2f(3, 1), {11, 15}, exitColor);
        box(window, exit + sf::Vector2f(5, 4), {7, 12}, sf::Color(38, 29, 27));
        box(window, exit + sf::Vector2f(10, 10), {2, 2}, exitColor);
        const std::size_t flowerType = activeLevel.value_or(game.world.seed % LEVELS.size());
        const auto paintFlower = [&](int cell) {
            const auto center = tilePosition(cell) + sf::Vector2f(TILE * .5f, TILE * .47f);
            const float size = 23.f + .8f * std::sin(seconds * 3.f + cell * .17f);
            disc(window, center, 6.f, {251, 225, 158, 60});
            if (!flowers.draw(window, center - sf::Vector2f(size, size) / 2.f, size, false, flowerType))
                blossom(window, center, 7.f, season.accent);
        };
        for (int cell : session->flowerCells) if (cell >= 0) paintFlower(cell);
        for (int cell : session->fakeFlowers()) paintFlower(cell);
        for (int cell : session->statueCells()) {
            const auto p = tilePosition(cell);
            box(window, p + sf::Vector2f(3, 15), {13, 4}, {90, 91, 104}, {39, 44, 58});
            polygon(window, {p + sf::Vector2f(5, 14), p + sf::Vector2f(7, 5),
                             p + sf::Vector2f(12, 5), p + sf::Vector2f(15, 14)}, {118, 114, 133});
            disc(window, p + sf::Vector2f(9.5f, 4), 3.1f, {158, 153, 172});
        }
        for (int cell : session->coffinCells()) {
            const auto p = tilePosition(cell);
            polygon(window, {p + sf::Vector2f(5, 1), p + sf::Vector2f(14, 1), p + sf::Vector2f(17, 6),
                             p + sf::Vector2f(14, 18), p + sf::Vector2f(5, 18), p + sf::Vector2f(2, 6)}, {126, 94, 79});
            box(window, p + sf::Vector2f(5, 4), {9, 12}, session->coffinsOpened() ? sf::Color(19, 16, 30) : sf::Color(63, 42, 58));
            if (!session->coffinsOpened())
                box(window, p + sf::Vector2f(8, 8), {3, 2}, {213, 169, 103});
        }
        for (const int potion : game.potions) {
            const auto pos = tilePosition(potion);
            box(window, pos + sf::Vector2f(5, 7), {8, 7}, sf::Color(212, 86, 76), sf::Color(31, 25, 30));
            box(window, pos + sf::Vector2f(7, 4), {4, 3}, sf::Color(244, 203, 131));
        }
        for (const int clock : session->clocks) {
            const auto center = tilePosition(clock) + sf::Vector2f(9, 9);
            disc(window, center, 8, sf::Color(247, 205, 116, 85));
            disc(window, center, 6, sf::Color(249, 213, 121));
            disc(window, center, 4, sf::Color(45, 48, 61));
            box(window, center + sf::Vector2f(-.6f, -3), {1.2f, 3.8f}, sf::Color(255, 239, 190));
            box(window, center, {3, 1.2f}, sf::Color(255, 239, 190));
        }
        if (showPath && session->target >= 0) {
            const auto result = session->route();
            for (int i = 1; i < static_cast<int>(result.path.size()); ++i) {
                if (session->fogAt(result.path[i])) continue;
                const auto p = relic::point(result.path[i]);
                box(window, {MAP_X + p.x * TILE + 7, MAP_Y + p.y * TILE + 7}, {6, 6}, sf::Color(66, 196, 221));
            }
            const auto p = relic::point(session->target);
            if (!session->fogAt(session->target))
                box(window, {MAP_X + p.x * TILE + 2, MAP_Y + p.y * TILE + 2}, {TILE - 5, TILE - 5}, sf::Color::Transparent, sf::Color(89, 219, 238));
        }
        const auto frame = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - cameraEpoch).count() / 125);
        std::vector<const relic::Enemy*> visibleEnemies;
        for (const auto& enemy : game.enemies) if (enemy.active()) visibleEnemies.push_back(&enemy);
        std::sort(visibleEnemies.begin(), visibleEnemies.end(), [](const auto* a, const auto* b) { return a->at < b->at; });
        for (const auto* item : visibleEnemies) {
            const auto& enemy = *item;
            using Kind = relic::EnemyKind;
            const auto pos = tilePosition(enemy.at);
            if (enemy.kind == Kind::Guard) {
                if (!guardians[enemy.tier].draw(window, pos - sf::Vector2f(2, 4), TILE + 4, false, frame + enemy.tier))
                    drawGuard(window, pos);
            } else if (enemy.kind == Kind::Boss) {
                const auto feet = pos + sf::Vector2f(TILE / 2, 17);
                const float breathing = 1.f + .07f * std::sin(seconds * 3.f);
                softLight(window, feet, {22, 10}, {174, 64, 123, 135});
                polygon(window, {feet + sf::Vector2f(-18, 1), feet + sf::Vector2f(-9, -21),
                                 feet + sf::Vector2f(9, -21), feet + sf::Vector2f(18, 1),
                                 feet + sf::Vector2f(0, -3)}, {44, 24, 51});
                disc(window, feet + sf::Vector2f(0, -23), 8.f * breathing, {37, 21, 45});
                disc(window, feet + sf::Vector2f(-3, -23), 1.2f, {252, 131, 154});
                disc(window, feet + sf::Vector2f(3, -23), 1.2f, {252, 131, 154});
            } else if (enemy.kind == Kind::Mummy) {
                const auto p = pos + sf::Vector2f(0, .7f * std::sin(seconds * 5.f + enemy.home));
                box(window, p + sf::Vector2f(4, 5), {11, 12}, {47, 39, 47});
                disc(window, p + sf::Vector2f(9, 4), 5.f, {196, 176, 137});
                for (int band = 0; band < 4; ++band)
                    box(window, p + sf::Vector2f(4.f + band % 2, 8.f + band * 2.f), {10, 1.5f}, {207, 187, 148});
                box(window, p + sf::Vector2f(7, 3), {5, 2}, {86, 58, 47});
            } else if (enemy.kind == Kind::Ghost) {
                const auto p = pos + sf::Vector2f(9.5f, 5.f + 1.8f * std::sin(seconds * 3.f + enemy.home));
                softLight(window, p, {13, 15}, {143, 200, 239, 131});
                polygon(window, {p + sf::Vector2f(-6, 0), p + sf::Vector2f(6, 0), p + sf::Vector2f(7, 10),
                                 p + sf::Vector2f(2, 7), p + sf::Vector2f(-2, 12), p + sf::Vector2f(-7, 9)}, {167, 216, 239, 182});
                disc(window, p, 6.2f, {194, 226, 242, 225});
                disc(window, p + sf::Vector2f(-2.5f, 0), 1.1f, {45, 61, 88});
                disc(window, p + sf::Vector2f(2.5f, 0), 1.1f, {45, 61, 88});
            } else if (enemy.kind == Kind::Puppet) {
                softLight(window, pos + sf::Vector2f(9, 15), {14, 8}, {194, 87, 213, 150});
                if (enemy.appearance >= 0 && static_cast<std::size_t>(enemy.appearance) < heroes.size())
                    heroes[enemy.appearance].draw(window, pos + sf::Vector2f(-5, -10), 29.f,
                        relic::point(game.player).x < relic::point(enemy.at).x, 0);
            }
            if (enemy.warningCell >= 0) {
                const auto warning = tilePosition(enemy.warningCell);
                sf::CircleShape ring(9.f);
                ring.setPosition(warning);
                ring.setFillColor(sf::Color(255, 58, 43, 90));
                ring.setOutlineColor(sf::Color(255, 116, 82));
                ring.setOutlineThickness(1.5f);
                window.draw(ring);
            }
            if (enemy.kind == Kind::Guard) {
                constexpr int pip = relic::Game::expeditionAttackDamage;
                for (int hp = 0; hp * pip < enemy.health; ++hp)
                    box(window, pos + sf::Vector2f(2.f + hp * 3.f, -4),
                        {2.f * std::min(pip, enemy.health - hp * pip) / pip, 2}, {255, 147, 100});
            } else {
                const int maximum = enemy.kind == Kind::Boss ? relic::GameSession::bossHealth(game.expeditionLevel) :
                    enemy.kind == Kind::Puppet ? 100 : enemy.kind == Kind::Ghost ? 60 : 40;
                const float width = enemy.kind == Kind::Boss ? 37.f : 21.f;
                const float offset = enemy.kind == Kind::Boss ? 26.f : enemy.kind == Kind::Puppet ? 13.f : 6.f;
                const auto corner = pos + sf::Vector2f((TILE - width) / 2.f, -offset);
                box(window, corner, {width, 3}, {48, 34, 55});
                box(window, corner, {width * enemy.health / maximum, 3}, {238, 107, 155});
            }
        }
        drawWeather();
        const auto now = Clock::now();
        const float progress = stepProgress(now);
        const bool walking = progress < 1.f;
        const auto playerPos = animatedPlayer(now);
        const float lift = walking && jumpPose ? std::sin(progress * 3.14159265f) * 10.f : 0.f;
        // The bottom padding in each frame leaves the shoes aligned with the floor.
        const auto spriteOrigin = playerPos + sf::Vector2f((TILE - PLAYER_SIZE) / 2.f, TILE - PLAYER_SIZE + 1.f - lift);
        auto& character = heroes[hero];
        const float bounce = walking ? 2.f*std::sin(progress*3.14159f) : 0.f;
        const bool drawn = character.draw(window,spriteOrigin-sf::Vector2f(0,bounce),PLAYER_SIZE,facing.x<0);
        if (!drawn) drawExplorer(window, playerPos, facing, walking ? progress : 0.f, hero, jumpPose);
        drawStrike(playerPos, now);
        if (session->stunLeft() > relic::GameSession::Time::zero()) {
            const float spin = std::chrono::duration<float>(playTime()).count() * 5.f;
            for (int i = 0; i < 3; ++i) {
                const float angle = spin + i * 2.0944f;
                disc(window, playerPos + sf::Vector2f(9.75f + 11.f * std::cos(angle), -10.f + 3.f * std::sin(angle)),
                     2.f, {249, 224, 149});
            }
        }
        window.setView(view);
        if (gameTime() < ultimateUntil) {
            cutPanel(window, {{402, 154}, {365, 63}}, 12, {24, 22, 37, 241}, {211, 172, 102});
            centeredLabel(window, displayFont(), ultimateTitle, {584, 185}, 31, {255, 224, 157});
        }
        if (showPath && session->target >= 0) {
            const auto& result = session->route();
            text(window, font, ALGORITHMS[session->algorithm] + u8"    步数 " + std::to_string(result.path.empty() ? 0 : result.path.size()-1) +
                 u8"    消耗 " + std::to_string(result.cost) + u8"    已搜索 " + std::to_string(result.order.size()), 28, 830, 15, sf::Color(173, 235, 244));
        }
        box(window, {SIDEBAR_X, 104}, {WINDOW_W - SIDEBAR_X - 24.f, 714}, sf::Color(24, 31, 39), sf::Color(58, 72, 84));
        text(window, font, u8"探险记录", SIDEBAR_CONTENT_X, 126, 20, sf::Color(159, 199, 190));
        text(window, font, u8"生命   " + std::to_string(game.health) + " / 100", SIDEBAR_CONTENT_X, 174, 18);
        box(window, {SIDEBAR_CONTENT_X, 208}, {224, 12}, sf::Color(60, 70, 78));
        box(window, {SIDEBAR_CONTENT_X, 208}, {224.f * std::max(0, game.health) / 100.f, 12}, sf::Color(79, 190, 133));
        text(window, font, u8"花朵   " + std::to_string(game.collected) + " / 4", SIDEBAR_CONTENT_X, 244, 18);
        for (std::size_t i = 0; i < session->flowerCells.size(); ++i) {
            const bool collected = session->flowerCells[i] == -1;
            const auto tint = collected ? sf::Color::White : sf::Color(255, 255, 255, 116);
            (collected ? flowers : dormantFlowers).draw(window,
                {SIDEBAR_CONTENT_X + 118.f + i * 27.f, 244}, 22.f, false, flowerType, tint);
        }
        text(window, font, u8"步数   " + std::to_string(game.steps) + " / " + std::to_string(session->limits.steps), SIDEBAR_CONTENT_X, 282, 18);
        text(window, font, u8"地形消耗   " + std::to_string(game.spent) + " / " + std::to_string(session->limits.cost), SIDEBAR_CONTENT_X, 320, 18);
        text(window, font, u8"寻路算法", SIDEBAR_CONTENT_X, 343, 16, sf::Color(159, 199, 190));
        for (int i = 0; i < 3; ++i) {
            const auto color = i == session->algorithm ? sf::Color(38, 117, 113) : sf::Color(42, 52, 61);
            box(window, {SIDEBAR_CONTENT_X + i * ALGORITHM_BUTTON_STEP, 365}, {73, 33}, color);
            text(window, font, std::to_string(i + 1) + "  " + ALGORITHMS[i], SIDEBAR_CONTENT_X + 6.f + i * ALGORITHM_BUTTON_STEP, 371, 14);
        }
        const auto& type = displayFont();
        const sf::Color muted(174, 188, 198), gold(235, 196, 128);
        if (const auto* boss = session->boss(); boss && boss->active())
            text(window, type, u8"首领 " + std::to_string(boss->health) + " / " +
                 std::to_string(relic::GameSession::bossHealth(game.expeditionLevel)),
                 SIDEBAR_CONTENT_X + 108, 131, 15, {245, 167, 191});
        const int nextWeapon = session->weapon();
        const bool automatic = session->weaponRotationEnabled();
        text(window, type, automatic ? u8"武器 · 自动轮换" : u8"武器 · 手动锁定", SIDEBAR_CONTENT_X, 422, 19, gold);
        if (!automatic) {
            const auto bounds = weaponRotationButton();
            const bool hover = bounds.contains(window.mapPixelToCoords(sf::Mouse::getPosition(window)));
            box(window, bounds.position, bounds.size, hover ? sf::Color(73, 59, 43) : sf::Color(36, 44, 54), gold);
            centeredLabel(window, type, u8"恢复轮换", bounds.getCenter(), 15, gold);
        }
        if (nextWeapon < 0)
            text(window, type, u8"通关后解锁武器", SIDEBAR_CONTENT_X, 460, 17, muted);
        for (std::size_t i = 0; i < relic::weapons.size(); ++i) {
            if (!session->hasWeapon(i)) continue;
            const auto bounds = weaponRow(i);
            const bool selected = nextWeapon == static_cast<int>(i);
            box(window, bounds.position, bounds.size, selected ? sf::Color(73, 59, 43) : sf::Color(36, 44, 54),
                selected ? gold : sf::Color(64, 73, 82));
            weaponIcons[i].draw(window,bounds.position+sf::Vector2f(3,2),25);
            text(window, type, relic::weapons[i].name, bounds.position.x + 34, bounds.position.y + 3, 17, selected ? gold : muted);
            const auto left = std::chrono::ceil<std::chrono::seconds>(session->cooldown(i)).count();
            centeredLabel(window, type, left ? std::to_string(left) + " s" : u8"就绪",
                          {bounds.position.x + 196, bounds.position.y + 14}, 16, left ? muted : gold);
        }
        text(window, type, u8"空格 普攻   单按 Shift 大招", SIDEBAR_CONTENT_X, 694, 15, muted);
        text(window, type, u8"Shift + 方向 跳跃   E 自动走", SIDEBAR_CONTENT_X, 718, 15, muted);
        text(window, type, u8"Tab 视角   H 路线   点地面设目标", SIDEBAR_CONTENT_X, 742, 15, muted);
        if (game.status == "playing") text(window, type, notice, 28, 855, 18, gold);
        const auto stun = std::chrono::ceil<std::chrono::seconds>(session->stunLeft()).count();
        if (stun > 0)
            text(window, type, u8"眩晕   " + std::to_string(stun) + " s", SIDEBAR_CONTENT_X, 772, 18, gold);
        if (std::any_of(game.enemies.begin(), game.enemies.end(), [](const relic::Enemy& enemy) { return enemy.health > 0 && enemy.warningCell >= 0; }))
            text(window, type, u8"守卫即将攻击", SIDEBAR_CONTENT_X + 108, 773, 16, sf::Color(255, 150, 123));
    }
};
}

int main(int argc, char** argv) {
    try { executableDir = executableDirectory(argc ? argv[0] : "byow_native"); App app; app.run(); }
    catch (const std::exception& error) { return 1; }
}
