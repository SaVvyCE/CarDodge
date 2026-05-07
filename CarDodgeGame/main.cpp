#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────
const int WINDOW_W   = 500;
const int WINDOW_H   = 750;
const int ROAD_LEFT  = 70;
const int ROAD_RIGHT = 430;
const int NUM_LANES  = 3;
const float LANE_W   = (ROAD_RIGHT - ROAD_LEFT) / (float)NUM_LANES;
const float CAR_W    = 52.f;
const float CAR_H    = 88.f;

// ─────────────────────────────────────────────────────────────────────────────
// Utility
// ─────────────────────────────────────────────────────────────────────────────
std::string toStr(int v) {
    std::ostringstream o; o << v; return o.str();
}

float laneX(int lane) {
    return ROAD_LEFT + lane * LANE_W + (LANE_W - CAR_W) / 2.f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Base Car
// ─────────────────────────────────────────────────────────────────────────────
class Car {
public:
    sf::Sprite sprite;
    float speed;
    bool active;

    Car() : speed(0.f), active(true) {}

    virtual void update(float dt) = 0;

    sf::FloatRect getBounds() const {
        return sprite.getGlobalBounds();
    }

    void setPosition(float x, float y) {
        sprite.setPosition(x, y);
    }

    sf::Vector2f getPosition() const {
        return sprite.getPosition();
    }

    virtual void draw(sf::RenderWindow& w) {
        w.draw(sprite);
    }

    virtual ~Car() {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Player Car
// ─────────────────────────────────────────────────────────────────────────────
class PlayerCar : public Car {
public:
    int currentLane;
    float targetX;

    PlayerCar(sf::Texture& tex) {
        currentLane = 1;
        sprite.setTexture(tex);
        // Scale texture to desired car size
        sf::FloatRect tb = sprite.getLocalBounds();
        sprite.setScale(CAR_W / tb.width, CAR_H / tb.height);
        targetX = laneX(currentLane);
        sprite.setPosition(targetX, WINDOW_H - CAR_H - 50.f);
    }

    void moveLeft() {
        if (currentLane > 0) { currentLane--; targetX = laneX(currentLane); }
    }

    void moveRight() {
        if (currentLane < NUM_LANES - 1) { currentLane++; targetX = laneX(currentLane); }
    }

    void update(float dt) override {
        float curX = sprite.getPosition().x;
        float diff = targetX - curX;
        if (std::abs(diff) > 1.f)
            sprite.move(diff * 12.f * dt, 0.f);
        else
            sprite.setPosition(targetX, sprite.getPosition().y);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Enemy Car
// ─────────────────────────────────────────────────────────────────────────────
class EnemyCar : public Car {
public:
    EnemyCar(sf::Texture& tex, int lane, float spd) {
        speed = spd;
        sprite.setTexture(tex);
        sf::FloatRect tb = sprite.getLocalBounds();
        sprite.setScale(CAR_W / tb.width, CAR_H / tb.height);
        // Flip vertically so enemy cars face downward
        sprite.setScale(sprite.getScale().x, -sprite.getScale().y);
        sprite.setPosition(laneX(lane), -CAR_H);
        // After flip, origin shifts — adjust
        sprite.move(0.f, -CAR_H);
    }

    void update(float dt) override {
        sprite.move(0.f, speed * dt);
        if (sprite.getPosition().y > WINDOW_H + CAR_H + 10.f)
            active = false;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Game State & Difficulty
// ─────────────────────────────────────────────────────────────────────────────
enum class GameState  { MENU, PLAYING, PAUSED, GAMEOVER };
enum class Difficulty { EASY, MEDIUM, HARD };

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    srand((unsigned)time(nullptr));

    sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H), "Car Dodge", sf::Style::Close | sf::Style::Titlebar);
    window.setFramerateLimit(60);

    // ── Load Font ──
    sf::Font font;
    bool fontLoaded = font.loadFromFile("assets/font.ttf");
    if (!fontLoaded) {
        fontLoaded = font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    }
    if (!fontLoaded) {
        std::cerr << "ERROR: Could not load font!\n";
        return -1;
    }

    // ── Load Textures ──
    sf::Texture texPlayer, texRed1, texRed2, texYellow1, texYellow2, texYellow3;

    auto loadTex = [](sf::Texture& t, const std::string& path) {
        if (!t.loadFromFile(path)) {
            std::cerr << "WARNING: Could not load " << path << "\n";
            return false;
        }
        t.setSmooth(true);
        return true;
    };

    loadTex(texPlayer,  "assets/WhiteCar.png");
    loadTex(texRed1,    "assets/RedCar1.png");
    loadTex(texRed2,    "assets/RedCar2.png");
    loadTex(texYellow1, "assets/YellowCar1.png");
    loadTex(texYellow2, "assets/YellowCar2.png");
    loadTex(texYellow3, "assets/YellowCar3.png");

    // Pool of enemy textures to pick from randomly
    std::vector<sf::Texture*> enemyTextures = {
        &texRed1, &texRed2, &texYellow1, &texYellow2, &texYellow3
    };

    // ── Build Lane Dash Marks ──
    // We'll animate these by shifting their draw offset
    struct Dash { float x, baseY; };
    std::vector<Dash> dashes;
    for (int lane = 1; lane < NUM_LANES; lane++) {
        float lx = ROAD_LEFT + lane * LANE_W;
        for (int y = 0; y < WINDOW_H + 60; y += 60) {
            dashes.push_back({lx - 2.f, (float)y});
        }
    }

    // ── HUD Text helpers ──
    auto makeText = [&](const std::string& s, int sz, sf::Color col) {
        sf::Text t;
        t.setFont(font);
        t.setString(s);
        t.setCharacterSize(sz);
        t.setFillColor(col);
        return t;
    };

    // ── Game Variables ──
    GameState  state      = GameState::MENU;
    Difficulty difficulty = Difficulty::MEDIUM;

    PlayerCar* player = nullptr;
    std::vector<EnemyCar*> enemies;

    float spawnTimer    = 0.f;
    float spawnInterval = 1.4f;
    float baseSpeed     = 220.f;
    float gameSpeed     = baseSpeed;
    float elapsed       = 0.f;
    int   score         = 0;
    int   speedLevel    = 1;
    int   highScore     = 0;
    float markOffset    = 0.f;   // for animated lane marks

    // ── Apply Difficulty ──
    auto applyDifficulty = [&]() {
        switch (difficulty) {
            case Difficulty::EASY:   baseSpeed = 160.f; spawnInterval = 2.0f; break;
            case Difficulty::MEDIUM: baseSpeed = 220.f; spawnInterval = 1.4f; break;
            case Difficulty::HARD:   baseSpeed = 300.f; spawnInterval = 0.9f; break;
        }
        gameSpeed = baseSpeed;
    };

    // ── Reset / Start ──
    auto resetGame = [&]() {
        for (auto e : enemies) delete e;
        enemies.clear();
        delete player;
        player = new PlayerCar(texPlayer);
        spawnTimer = 0.f;
        elapsed    = 0.f;
        score      = 0;
        speedLevel = 1;
        applyDifficulty();
    };

    applyDifficulty();

    // ── Gradient sky background rectangles ──
    sf::RectangleShape bgTop({(float)WINDOW_W, (float)WINDOW_H / 2.f});
    bgTop.setFillColor(sf::Color(10, 10, 18));
    bgTop.setPosition(0, 0);
    sf::RectangleShape bgBot({(float)WINDOW_W, (float)WINDOW_H / 2.f});
    bgBot.setFillColor(sf::Color(18, 18, 30));
    bgBot.setPosition(0, WINDOW_H / 2.f);

    // ── Road shapes ──
    sf::RectangleShape road({(float)(ROAD_RIGHT - ROAD_LEFT), (float)WINDOW_H});
    road.setFillColor(sf::Color(38, 38, 38));
    road.setPosition(ROAD_LEFT, 0);

    sf::RectangleShape edgeL({5.f, (float)WINDOW_H}), edgeR({5.f, (float)WINDOW_H});
    edgeL.setFillColor(sf::Color(230, 230, 230)); edgeL.setPosition(ROAD_LEFT  - 5.f, 0);
    edgeR.setFillColor(sf::Color(230, 230, 230)); edgeR.setPosition(ROAD_RIGHT,       0);

    // ── Dash shape (reused each frame) ──
    sf::RectangleShape dashShape({4.f, 28.f});
    dashShape.setFillColor(sf::Color(200, 200, 200, 180));

    sf::Clock clock;

    // ═════════════════════════════════════════════════════════════════════════
    // Main Loop
    // ═════════════════════════════════════════════════════════════════════════
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f;

        // ── Events ──
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed) {
                auto k = event.key.code;

                // ── Menu ──
                if (state == GameState::MENU) {
                    if (k == sf::Keyboard::Num1) difficulty = Difficulty::EASY;
                    if (k == sf::Keyboard::Num2) difficulty = Difficulty::MEDIUM;
                    if (k == sf::Keyboard::Num3) difficulty = Difficulty::HARD;
                    if (k == sf::Keyboard::Enter) { resetGame(); state = GameState::PLAYING; }
                }

                // ── Pause ──
                if (state == GameState::PLAYING  && k == sf::Keyboard::P) state = GameState::PAUSED;
                if (state == GameState::PAUSED   && k == sf::Keyboard::P) state = GameState::PLAYING;

                // ── Game Over ──
                if (state == GameState::GAMEOVER) {
                    if (k == sf::Keyboard::R) { resetGame(); state = GameState::PLAYING; }
                    if (k == sf::Keyboard::M) state = GameState::MENU;
                }

                // ── Player movement ──
                if (state == GameState::PLAYING && player) {
                    if (k == sf::Keyboard::Left  || k == sf::Keyboard::A) player->moveLeft();
                    if (k == sf::Keyboard::Right || k == sf::Keyboard::D) player->moveRight();
                }

                if (k == sf::Keyboard::Escape) window.close();
            }
        }

        // ── Update ──
        if (state == GameState::PLAYING) {
            elapsed  += dt;
            score     = (int)(elapsed * 10);
            if (score > highScore) highScore = score;

            // Speed scaling — every 10s increase level
            speedLevel = 1 + (int)(elapsed / 10.f);
            gameSpeed  = baseSpeed + (speedLevel - 1) * 25.f;

            // Spawn interval shrinks over time (min 0.4s)
            float spawnNow = std::max(0.4f, spawnInterval - elapsed * 0.008f);

            // Animate lane marks
            markOffset += gameSpeed * dt;
            if (markOffset >= 60.f) markOffset -= 60.f;

            // Spawn enemies
            spawnTimer += dt;
            if (spawnTimer >= spawnNow) {
                spawnTimer = 0.f;
                int lane = rand() % NUM_LANES;
                sf::Texture* tex = enemyTextures[rand() % enemyTextures.size()];
                float spd = gameSpeed + (float)(rand() % 60 - 20);
                if (difficulty == Difficulty::HARD) spd += (float)(rand() % 80);
                enemies.push_back(new EnemyCar(*tex, lane, spd));

                // Hard: sometimes spawn in 2 lanes simultaneously
                if (difficulty == Difficulty::HARD && rand() % 3 == 0) {
                    int lane2 = (lane + 1 + rand() % 2) % NUM_LANES;
                    sf::Texture* tex2 = enemyTextures[rand() % enemyTextures.size()];
                    enemies.push_back(new EnemyCar(*tex2, lane2, spd + rand() % 50));
                }
                // Medium: occasionally 2 cars
                if (difficulty == Difficulty::MEDIUM && rand() % 5 == 0) {
                    int lane2 = (lane + 1 + rand() % 2) % NUM_LANES;
                    sf::Texture* tex2 = enemyTextures[rand() % enemyTextures.size()];
                    enemies.push_back(new EnemyCar(*tex2, lane2, spd));
                }
            }

            // Update player
            if (player) player->update(dt);

            // Update enemies & remove off-screen
            for (auto e : enemies) e->update(dt);
            enemies.erase(
                std::remove_if(enemies.begin(), enemies.end(),
                    [](EnemyCar* e) {
                        if (!e->active) { delete e; return true; }
                        return false;
                    }),
                enemies.end()
            );

            // Collision Detection — tighter hitbox for fairness
            if (player) {
                sf::FloatRect pb = player->getBounds();
                pb.left   += 8.f;  pb.width  -= 16.f;
                pb.top    += 8.f;  pb.height -= 16.f;
                for (auto e : enemies) {
                    sf::FloatRect eb = e->getBounds();
                    eb.left  += 6.f; eb.width  -= 12.f;
                    eb.top   += 6.f; eb.height -= 12.f;
                    if (pb.intersects(eb)) {
                        state = GameState::GAMEOVER;
                        break;
                    }
                }
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // Draw
        // ══════════════════════════════════════════════════════════════════
        window.clear(sf::Color(10, 10, 18));

        // Background
        window.draw(bgTop);
        window.draw(bgBot);

        // Road
        window.draw(road);
        window.draw(edgeL);
        window.draw(edgeR);

        // Animated lane dashes
        for (auto& d : dashes) {
            float dy = d.baseY + markOffset;
            // Wrap around
            if (dy > WINDOW_H) dy -= (WINDOW_H + 60.f);
            dashShape.setPosition(d.x, dy);
            window.draw(dashShape);
        }

        // ── MENU ──────────────────────────────────────────────────────────
        if (state == GameState::MENU) {
            // Title
            auto title = makeText("CAR  DODGE", 50, sf::Color::Yellow);
            title.setStyle(sf::Text::Bold);
            title.setPosition(WINDOW_W / 2.f - title.getLocalBounds().width / 2.f, 130);
            window.draw(title);

            // Subtitle
            auto sub = makeText("Survive the traffic!", 18, sf::Color(180,180,180));
            sub.setPosition(WINDOW_W / 2.f - sub.getLocalBounds().width / 2.f, 195);
            window.draw(sub);

            // Difficulty
            std::string dstr = "Difficulty: ";
            sf::Color dcol;
            if      (difficulty == Difficulty::EASY)   { dstr += "EASY";   dcol = sf::Color(80,220,80); }
            else if (difficulty == Difficulty::MEDIUM)  { dstr += "MEDIUM"; dcol = sf::Color(240,180,0); }
            else                                        { dstr += "HARD";   dcol = sf::Color(240,60,60); }
            auto dt2 = makeText(dstr, 22, dcol);
            dt2.setPosition(WINDOW_W / 2.f - dt2.getLocalBounds().width / 2.f, 260);
            window.draw(dt2);

            // Controls list
            std::vector<std::pair<std::string, sf::Color>> lines = {
                {"[1] Easy   [2] Medium   [3] Hard", sf::Color(160,160,160)},
                {"",                                 sf::Color::White},
                {"ENTER  :  Start",                  sf::Color::White},
                {"A / D or Arrow Keys  :  Move",     sf::Color(200,200,200)},
                {"P  :  Pause",                      sf::Color(200,200,200)},
                {"ESC  :  Quit",                     sf::Color(200,200,200)},
            };
            float lineY = 315.f;
            for (auto& [txt, col] : lines) {
                auto t = makeText(txt, 18, col);
                t.setPosition(WINDOW_W / 2.f - t.getLocalBounds().width / 2.f, lineY);
                window.draw(t);
                lineY += 32.f;
            }

            // High score
            if (highScore > 0) {
                auto hs = makeText("Best: " + toStr(highScore), 20, sf::Color(255,215,0));
                hs.setPosition(WINDOW_W / 2.f - hs.getLocalBounds().width / 2.f, lineY + 20);
                window.draw(hs);
            }
        }

        // ── PLAYING / PAUSED ──────────────────────────────────────────────
        if (state == GameState::PLAYING || state == GameState::PAUSED) {
            // Draw enemies then player (player on top)
            for (auto e : enemies) e->draw(window);
            if (player) player->draw(window);

            // Score bar background
            sf::RectangleShape hbar({(float)WINDOW_W, 40.f});
            hbar.setFillColor(sf::Color(0, 0, 0, 160));
            window.draw(hbar);

            auto sc = makeText("SCORE: " + toStr(score), 20, sf::Color::White);
            sc.setStyle(sf::Text::Bold);
            sc.setPosition(8, 8);
            window.draw(sc);

            auto sp = makeText("SPEED: " + toStr(speedLevel), 20, sf::Color(100, 220, 255));
            sp.setStyle(sf::Text::Bold);
            sp.setPosition(WINDOW_W - sp.getLocalBounds().width - 10, 8);
            window.draw(sp);

            std::string dLabel;
            sf::Color dCol;
            if      (difficulty == Difficulty::EASY)   { dLabel = "EASY";   dCol = sf::Color(80,220,80); }
            else if (difficulty == Difficulty::MEDIUM)  { dLabel = "MEDIUM"; dCol = sf::Color(240,180,0); }
            else                                        { dLabel = "HARD";   dCol = sf::Color(240,60,60); }
            auto dl = makeText(dLabel, 16, dCol);
            dl.setPosition(WINDOW_W / 2.f - dl.getLocalBounds().width / 2.f, 10);
            window.draw(dl);
        }

        // ── PAUSED overlay ────────────────────────────────────────────────
        if (state == GameState::PAUSED) {
            sf::RectangleShape ov({(float)WINDOW_W, (float)WINDOW_H});
            ov.setFillColor(sf::Color(0, 0, 0, 150));
            window.draw(ov);

            auto pm = makeText("PAUSED", 48, sf::Color::Cyan);
            pm.setStyle(sf::Text::Bold);
            pm.setPosition(WINDOW_W / 2.f - pm.getLocalBounds().width / 2.f, WINDOW_H / 2.f - 60);
            window.draw(pm);

            auto ps = makeText("Press P to Resume", 20, sf::Color(180,180,180));
            ps.setPosition(WINDOW_W / 2.f - ps.getLocalBounds().width / 2.f, WINDOW_H / 2.f + 10);
            window.draw(ps);
        }

        // ── GAME OVER ─────────────────────────────────────────────────────
        if (state == GameState::GAMEOVER) {
            // Still render the scene underneath
            for (auto e : enemies) e->draw(window);
            if (player) player->draw(window);

            // Dark overlay
            sf::RectangleShape ov({(float)WINDOW_W, (float)WINDOW_H});
            ov.setFillColor(sf::Color(0, 0, 0, 185));
            window.draw(ov);

            // Red flash border
            sf::RectangleShape border({(float)WINDOW_W - 8, (float)WINDOW_H - 8});
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineColor(sf::Color(220, 40, 40, 200));
            border.setOutlineThickness(4);
            border.setPosition(4, 4);
            window.draw(border);

            auto go = makeText("GAME OVER", 46, sf::Color(220, 40, 40));
            go.setStyle(sf::Text::Bold);
            go.setPosition(WINDOW_W / 2.f - go.getLocalBounds().width / 2.f, WINDOW_H / 2.f - 110);
            window.draw(go);

            auto sc = makeText("Score: " + toStr(score), 30, sf::Color::White);
            sc.setPosition(WINDOW_W / 2.f - sc.getLocalBounds().width / 2.f, WINDOW_H / 2.f - 40);
            window.draw(sc);

            auto hs = makeText("Best:  " + toStr(highScore), 22, sf::Color(255, 215, 0));
            hs.setPosition(WINDOW_W / 2.f - hs.getLocalBounds().width / 2.f, WINDOW_H / 2.f + 5);
            window.draw(hs);

            auto r1 = makeText("[R]  Restart", 20, sf::Color(100, 255, 100));
            r1.setPosition(WINDOW_W / 2.f - r1.getLocalBounds().width / 2.f, WINDOW_H / 2.f + 60);
            window.draw(r1);

            auto r2 = makeText("[M]  Main Menu", 20, sf::Color(180, 180, 255));
            r2.setPosition(WINDOW_W / 2.f - r2.getLocalBounds().width / 2.f, WINDOW_H / 2.f + 95);
            window.draw(r2);
        }

        window.display();
    }

    // Cleanup
    delete player;
    for (auto e : enemies) delete e;
    return 0;
}
