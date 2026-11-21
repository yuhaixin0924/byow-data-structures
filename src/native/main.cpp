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
    {u8"长明殿", 4309, {1160, 285}}, {u8"霜冠之门", 15941, {1302, 498}}
}};
constexpr std::array<std::array<int, 2>, 11> ROADS{{
    {0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 4}, {2, 5}, {3, 6},
    {4, 6}, {4, 5}, {5, 7}, {6, 7}
}};
using Clock = std::chrono::steady_clock;
constexpr auto STEP_TIME = std::chrono::milliseconds(125);
const std::array<sf::Color, 4> BLOSSOM_COLORS{{{251, 111, 135}, {112, 205, 255}, {140, 233, 176}, {211, 154, 251}}};
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
    bool draw(sf::RenderWindow& window, sf::Vector2f pos, float size, bool flip = false, unsigned frame = 0) {
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
    std::array<int, 4> blossomCells{};
    std::optional<std::size_t> activeLevel;
    std::string seedText, notice;
    bool seedMode = false, mapMode = false, resultsMode = false;
    bool autoWalk = false, showPath = true, progressSaved = true;
    bool overview = true, briefing = false, helpMode = false, heroMode = false, briefingFromMap = false;
    bool heroDetail = false, paused = false;
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
                if (event->is<sf::Event::FocusLost>()) { autoWalk = false; held.clear(); }
                if (const auto* key = event->getIf<sf::Event::KeyReleased>())
                    held.erase(std::remove(held.begin(), held.end(), key->code), held.end());
                checkCompletion();
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
        if (timerStarted) session->updateTime(std::chrono::duration_cast<std::chrono::seconds>(playTime()));
        if (session->game.status == "playing") return;
        notice = session->notice;
        pauseTimer();
        autoWalk = false;
        seedMode = false;
        held.clear();
        resultsMode = true;
        if (session->game.status == "won" && activeLevel) recordClear(*activeLevel);
    }

    void finishResult() {
        const bool orchard = activeLevel==0 && session->game.status=="won";
        showMap();
        if (orchard) openPage(7);
    }

    void showMap() {
        pauseTimer();
        music.setExpedition(false);
        session.reset();
        activeLevel.reset();
        resultsMode = seedMode = autoWalk = briefing = paused = false;
        held.clear();
        mapMode = true;
    }

    Clock::duration timeBudget() const {
        return std::chrono::seconds(session->timeLimit());
    }

    void selectHero(int index) {
        hero = index;
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
        session = std::make_unique<relic::GameSession>(seed, static_cast<int>(level.value_or(0)));
        briefingFromMap = mapMode;
        briefing = true;
        music.setExpedition(false);
        strikeStarted = {};
        std::copy_n(session->game.crystals.begin(), blossomCells.size(), blossomCells.begin());
        activeLevel = level;
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
        if (Clock::now() - strikeStarted < strikeDuration) return;
        struckCount = 0;
        int toughest = 0;
        for (const auto& enemy : session->game.enemies) {
            if (enemy.health > 0 && relic::manhattan(session->game.player, enemy.at) == 1) {
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
        using Key = sf::Keyboard::Key;
        nextManual = Clock::now() + std::chrono::milliseconds(145);
        const bool jumping = sf::Keyboard::isKeyPressed(Key::LShift) || sf::Keyboard::isKeyPressed(Key::RShift);
        const int previous = session->game.player, previousHealth = session->game.health;
        session->act(direction(key), jumping);
        animateMove(previous, jumping);
        notice = session->notice;
        playDamage(previousHealth);
        checkCompletion();
    }

    void onKey(sf::Keyboard::Key key) {
        using Key = sf::Keyboard::Key;
        if (folioPage>=0) {
            if (key==Key::Escape) folioPage=-1;
            else if (key==Key::Right || key==Key::Enter) turnPage(1);
            else if (key==Key::Left) turnPage(-1);
            return;
        }
        if (key == Key::M) { music.toggle(); return; }
        if (heroDetail) {
            if (key == Key::Escape || key == Key::Enter) heroDetail = false;
            return;
        }
        if (helpMode || heroMode) {
            if (key == Key::Escape) helpMode = heroMode = false;
            else if (heroMode) {
                const int index = static_cast<int>(key) - static_cast<int>(Key::Num1);
                if (index >= 0 && index < static_cast<int>(heroes.size())) selectHero(index);
            }
            return;
        }
        if (briefing) {
            if (key == Key::Enter) startChallenge();
            else if (key == Key::Escape) cancelBriefing();
            else if (key == Key::C) heroMode = true;
            return;
        }
        if (paused) {
            if (key == Key::Escape || key == Key::Enter) resumeGame();
            else if (key == Key::C) heroMode = true;
            else if (key == Key::Tab) showMap();
            return;
        }
        if (resultsMode) {
            if (key == Key::Enter || key == Key::Escape) finishResult();
            return;
        }
        if (mapMode) {
            if (key == sf::Keyboard::Key::Escape) mapMode = false;
            else if (key == Key::C) heroMode = true;
            else if (key >= sf::Keyboard::Key::Num1 && key <= sf::Keyboard::Key::Num8) {
                const auto index = static_cast<std::size_t>(static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num1));
                beginGame(LEVELS[index].seed, index);
            }
            return;
        }
        if (key == sf::Keyboard::Key::Escape) {
            if (seedMode) closeSeedMenu();
            else if (session) pauseGame();
            autoWalk = false;
            held.clear();
            return;
        }
        if (seedMode) {
            if (key == sf::Keyboard::Key::Enter) beginCustomGame();
            else if (key == sf::Keyboard::Key::Backspace && seedCaret > 0) { seedText.erase(--seedCaret, 1); }
            else if (key == sf::Keyboard::Key::Left && seedCaret > 0) --seedCaret;
            else if (key == sf::Keyboard::Key::Right && seedCaret < seedText.size()) ++seedCaret;
            return;
        }
        if (key == sf::Keyboard::Key::N) { chooseSeed(); return; }
        if (!session) {
            if (key == sf::Keyboard::Key::Enter) showMap();
            else if (key == Key::Tab) showMap();
            else if (key == Key::F1) helpMode = true;
            else if (key == Key::C) heroMode = true;
            return;
        }
        if (key == Key::C) { pauseGame(); heroMode = true; return; }
        if (key != Key::E) autoWalk = false;
        if (direction(key)) {
            held.erase(std::remove(held.begin(), held.end(), key), held.end());
            held.push_back(key);
            moveManual(key);
            return;
        }
        const int previousHealth = session->game.health;
        if (key >= sf::Keyboard::Key::Num1 && key <= sf::Keyboard::Key::Num3)
            session->setAlgorithm(static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num1));
        else if (key == sf::Keyboard::Key::H) showPath = !showPath;
        else if (key == sf::Keyboard::Key::Tab) { overview = !overview; revealUntil = Clock::now(); }
        else if (key == Key::E) { held.clear(); autoWalk = !autoWalk; nextStep = Clock::now(); }
        else if (key == sf::Keyboard::Key::Space || key == sf::Keyboard::Key::F) strike();
        else if (key == sf::Keyboard::Key::Period) session->waitTurn();
        notice = session->notice;
        playDamage(previousHealth);
        checkCompletion();
    }

    void onMouse(sf::Vector2f point) {
        if (folioPage>=0) {
            if (BACK_BUTTON.contains(point)) folioPage=-1;
            else if (previousPage.contains(point)) turnPage(-1);
            else if (nextPage.contains(point)) turnPage(1);
            return;
        }
        if (MUSIC_BUTTON.contains(point) && music.available()) { music.toggle(); return; }
        if (heroDetail) {
            if (HERO_CLOSE.contains(point) || HERO_USE.contains(point)) heroDetail = false;
            return;
        }
        if (helpMode || heroMode) {
            if ((heroMode ? HERO_BACK : HELP_BACK).contains(point)) helpMode = heroMode = false;
            else if (heroMode) for (int i = 0; i < static_cast<int>(heroes.size()); ++i)
                if (heroCard(i).contains(point)) { selectHero(i); break; }
            return;
        }
        if (briefing) {
            if (BRIEF_GO.contains(point)) startChallenge();
            if (MODAL_BACK.contains(point)) cancelBriefing();
            if (BRIEF_HERO.contains(point)) heroMode = true;
            return;
        }
        if (paused) {
            if (RESUME.contains(point)) resumeGame();
            else if (RESELECT_LEVEL.contains(point)) showMap();
            else if (BRIEF_HERO.contains(point)) heroMode = true;
            return;
        }
        if (resultsMode) {
            if (CONTINUE_BUTTON.contains(point)) finishResult();
            return;
        }
        if (mapMode) {
            if (bookButton.contains(point)) { openPage(0); return; }
            if (cleared[0] && storyButton.contains(point)) { openPage(7); return; }
            if (MAP_HERO.contains(point)) { heroMode = true; return; }
            if (BACK_BUTTON.contains(point)) { mapMode = false; return; }
            for (std::size_t i = 0; i < LEVELS.size(); ++i) {
                const auto delta = point - LEVELS[i].position;
                if (flowerHit(delta)) {
                    beginGame(LEVELS[i].seed, i);
                    return;
                }
            }
            return;
        }
        if (seedMode) {
            if (PLAY_BUTTON.contains(point)) beginCustomGame();
            else if (SEED_BUTTON.contains(point)) closeSeedMenu();
            return;
        }
        if (!session) {
            if (PLAY_BUTTON.contains(point)) showMap();
            else if (SEED_BUTTON.contains(point)) chooseSeed();
            else if (MAP_BUTTON.contains(point)) showMap();
            else if (HELP_BUTTON.contains(point)) helpMode = true;
            else if (HERO_BUTTON.contains(point)) heroMode = true;
            return;
        }
        if (PAUSE_BUTTON.contains(point)) { pauseGame(); return; }
        if (point.y >= 414 && point.y < 452 && point.x >= SIDEBAR_CONTENT_X && point.x < SIDEBAR_CONTENT_X + 3 * ALGORITHM_BUTTON_STEP) {
            const int index = static_cast<int>((point.x - SIDEBAR_CONTENT_X) / ALGORITHM_BUTTON_STEP);
            session->setAlgorithm(index); autoWalk = false; return;
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
        }
    }

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
        if (folioPage>=0) { drawFolio(); window.display(); return; }
        if (mapMode) {
            drawMap();
        } else if (!session || seedMode) {
            drawMenu();
        } else {
            window.clear(sf::Color(12, 16, 21));
            drawSeasonLandscape({0,82},{1440,818});
            box(window, {0, 0}, {WINDOW_W, 82}, sf::Color(24, 31, 39));
            text(window, font, activeLevel ? LEVELS[*activeLevel].name : u8"自定义探险", 32, 25, 28);
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
        drawButton(bookButton,"七件武器");
        if (cleared[0]) drawButton(storyButton,"西圃秋记");
        drawButton(MAP_HERO,u8"换角色 [C]");
        const auto completed = std::count(cleared.begin(), cleared.end(), true);
        centeredLabel(window, mapFont, u8"已复苏 " + std::to_string(completed) + " / "
                      + std::to_string(LEVELS.size()) + u8" 朵花", {720, 837}, 26, ink);
        centeredLabel(window, mapFont, completed == static_cast<int>(LEVELS.size())
                      ? u8"八朵花已齐聚，花神的传说仍待揭晓。"
                      : u8"八朵花齐聚之时，拼合碎片，召唤花神，寻回昔日花都。",
                      {720, 872}, 17, ink);
    }

    void modal(const std::string& title) {
        const sf::Color gold(247, 205, 116);
        box(window, {0, 0}, {WINDOW_W, WINDOW_H}, sf::Color(7, 12, 19, 198));
        box(window, {388, 154}, {664, 620}, sf::Color(23, 32, 41), sf::Color(155, 120, 67));
        box(window, {410, 175}, {620, 2}, sf::Color(155, 120, 67));
        centeredLabel(window, font, title, {720, 231}, 38, gold, true);
    }

    void drawHeroChoice() {
        drawAntiqueButton(BRIEF_HERO, std::string(u8"换角色 [C]  ·  ") + HEROES[hero].name);
    }

    void drawBriefing() {
        modal(activeLevel ? std::to_string(*activeLevel + 1) + "  " + LEVELS[*activeLevel].name : u8"自定义探险");
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
        const auto& guards = session->game.enemies;
        std::string damage = std::to_string(guards.front().damage);
        if (guards.back().damage != guards.front().damage) damage += "–" + std::to_string(guards.back().damage);
        centeredLabel(window, font, u8"守卫单次伤害  " + damage, {720, 488}, 21, sf::Color(233, 168, 145));
        centeredLabel(window, font, u8"拾取时钟可增加 30 秒。确认后开始计时。",
                      {720, 546}, 19, sf::Color(210, 224, 224));
        centeredLabel(window, font, u8"集齐四朵花后前往出口。", {720, 582}, 19, sf::Color(210, 224, 224));
        drawButton(MODAL_BACK, u8"返回 [Esc]");
        drawHeroChoice();
        drawButton(BRIEF_GO, u8"出发 [Enter]", true);
    }

    void drawHelp() {
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
            {u8"战斗", u8"空格或 F 攻击相邻守卫。看到红圈，下一回合闪开。", u8"守卫需打 2–5 次。药剂恢复 45 点生命。"},
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
        constexpr std::array<const char*, 6> headers{u8"关卡", u8"时限", u8"步数", u8"消耗", u8"时钟", u8"守卫伤害"};
        for (std::size_t i = 0; i < headers.size(); ++i)
            centeredLabel(window, type, headers[i], {columns[i], 262}, 17, gold);
        for (std::size_t i = 0; i < LEVELS.size(); ++i) {
            const auto level = static_cast<int>(i);
            const auto& limits = GameSession::challenges[i];
            const auto damage = [level](int guard) { return std::to_string(GameSession::guardDamage(GameSession::guardTier(level, guard))); };
            const std::array<std::string, 6> cells{
                std::to_string(i + 1) + "  " + LEVELS[i].name, formatTime(std::chrono::seconds(limits.seconds)),
                std::to_string(limits.steps), std::to_string(limits.cost), std::to_string(GameSession::clockCount(level)),
                damage(0) + "/" + damage(1) + "/" + damage(2)
            };
            const float y = 294.f + i * 34.f;
            box(window, {744, y - 16}, {542, 32}, i % 2 ? sf::Color(25, 31, 44) : sf::Color(32, 40, 53));
            for (std::size_t column = 0; column < cells.size(); ++column)
                centeredLabel(window, type, cells[column], {columns[column], y}, 18, light);
        }
        text(window, type, u8"守卫伤害按三名守卫的单次命中列出。", 747, 558, 16, muted);
        cutPanel(window, {{149, 608}, {1142, 108}}, 10, {34, 32, 44}, {126, 106, 78});
        text(window, type, u8"八花齐聚", 169, 630, 25, gold);
        text(window, type, u8"每个预设关卡首次通关，会在花径复苏一朵遗迹之花。八关共八朵。", 321, 628, 20, light);
        text(window, type, u8"后续剧情中，八花齐聚便可拼合花神碎片，召唤花神，重返昔日花都，揭晓不为人知的历史。", 321, 665, 19, light);
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
        centeredLabel(window, font, activeLevel ? LEVELS[*activeLevel].name : u8"自定义探险",
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
            centeredLabel(window, font, notice, {720, 575}, 19, sf::Color(255, 156, 139));
        if (!progressSaved)
            centeredLabel(window, font, u8"通关进度未能保存", {720, 575}, 18, gold);
        drawButton(CONTINUE_BUTTON, activeLevel==0 && won ? u8"阅读西圃秋记 [Enter]" : u8"返回选关 [Enter]", true);
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
        } else {
            centeredLabel(window, menuFont, u8"循八朵花的踪迹，寻回失落的花都", {720, 439}, 23, light);
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
            centeredLabel(window, menuFont, u8"方向键 / WASD 移动     ·     空格 攻击", {720, 718}, 18, light);
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
        for (std::size_t i = 0; i < blossomCells.size(); ++i) {
            const int cell = blossomCells[i];
            if (std::find(game.crystals.begin(), game.crystals.end(), cell) == game.crystals.end()) continue;
            const float pulse = std::sin(std::chrono::duration<float>(Clock::now() - revealUntil).count() * 3.f + i);
            blossom(window, tilePosition(cell) + sf::Vector2f(9.75f, 8.2f), 6.5f + pulse * .55f, BLOSSOM_COLORS[i]);
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
                const auto p = relic::point(result.path[i]);
                box(window, {MAP_X + p.x * TILE + 7, MAP_Y + p.y * TILE + 7}, {6, 6}, sf::Color(66, 196, 221));
            }
            const auto p = relic::point(session->target);
            box(window, {MAP_X + p.x * TILE + 2, MAP_Y + p.y * TILE + 2}, {TILE - 5, TILE - 5}, sf::Color::Transparent, sf::Color(89, 219, 238));
        }
        const auto frame = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - cameraEpoch).count() / 125);
        for (const auto& enemy : game.enemies) if (enemy.health > 0) {
            const auto pos = tilePosition(enemy.at);
            if (!guardians[enemy.tier].draw(window, pos - sf::Vector2f(2, 4), TILE + 4, false, frame + enemy.tier))
                drawGuard(window, pos);
            if (enemy.warningCell >= 0) {
                const auto warning = tilePosition(enemy.warningCell);
                sf::CircleShape ring(9.f);
                ring.setPosition(warning);
                ring.setFillColor(sf::Color(255, 58, 43, 90));
                ring.setOutlineColor(sf::Color(255, 116, 82));
                ring.setOutlineThickness(1.5f);
                window.draw(ring);
            }
            for (int hp = 0; hp < enemy.health; ++hp)
                box(window, pos + sf::Vector2f(2.f + hp * 3.f, -4), {2, 2}, sf::Color(255, 147, 100));
        }
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
        window.setView(view);
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
        for (std::size_t i = 0; i < blossomCells.size(); ++i) {
            const bool collected = std::find(game.crystals.begin(), game.crystals.end(), blossomCells[i]) == game.crystals.end();
            blossom(window, {SIDEBAR_CONTENT_X + 129.f + i * 27.f, 255}, 7.f,
                    collected ? BLOSSOM_COLORS[i] : sf::Color(66, 78, 87), collected);
        }
        text(window, font, u8"步数   " + std::to_string(game.steps) + " / " + std::to_string(session->limits.steps), SIDEBAR_CONTENT_X, 282, 18);
        text(window, font, u8"地形消耗   " + std::to_string(game.spent) + " / " + std::to_string(session->limits.cost), SIDEBAR_CONTENT_X, 320, 18);
        text(window, font, u8"寻路算法", SIDEBAR_CONTENT_X, 378, 16, sf::Color(159, 199, 190));
        for (int i = 0; i < 3; ++i) {
            const auto color = i == session->algorithm ? sf::Color(38, 117, 113) : sf::Color(42, 52, 61);
            box(window, {SIDEBAR_CONTENT_X + i * ALGORITHM_BUTTON_STEP, 414}, {73, 38}, color);
            text(window, font, std::to_string(i + 1) + "  " + ALGORITHMS[i], SIDEBAR_CONTENT_X + 6.f + i * ALGORITHM_BUTTON_STEP, 423, 14);
        }
        text(window, font, u8"点击地面，规划路线", SIDEBAR_CONTENT_X, 486, 14, sf::Color(174, 188, 198));
        text(window, font, u8"空格 / F  攻击相邻守卫", SIDEBAR_CONTENT_X, 520, 14, sf::Color(174, 188, 198));
        text(window, font, u8"E 自动行走    H 切换路线显示", SIDEBAR_CONTENT_X, 552, 14, sf::Color(174, 188, 198));
        text(window, font, u8"Tab 切换全图与跟随视角", SIDEBAR_CONTENT_X, 580, 14, sf::Color(174, 188, 198));
        text(window, font, u8"方向键可长按    Shift + 方向 跳跃", SIDEBAR_CONTENT_X, 608, 14, sf::Color(174, 188, 198));
        if (game.status == "playing") text(window, font, notice, SIDEBAR_CONTENT_X, 648, 14, sf::Color(235, 196, 128));
        if (std::any_of(game.enemies.begin(), game.enemies.end(), [](const relic::Enemy& enemy) { return enemy.health > 0 && enemy.warningCell >= 0; }))
            text(window, font, u8"守卫即将攻击，离开标记格！", SIDEBAR_CONTENT_X, 681, 16, sf::Color(255, 150, 123));
        text(window, font, u8"N 新地图     Esc 停止自动行走", SIDEBAR_CONTENT_X, 742, 13, sf::Color(143, 157, 168));
    }
};
}

int main(int argc, char** argv) {
    try { executableDir = executableDirectory(argc ? argv[0] : "byow_native"); App app; app.run(); }
    catch (const std::exception& error) { return 1; }
}
