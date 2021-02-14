/**
 * FlappyBird++
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 **/
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "neat/neat_initialize.h"
#include "game.h"
#include "utility.h"

namespace flappybirdplusplus
{
namespace
{
    double distance(const sf::Vector2f& a, const sf::Vector2f& b)
    {
        auto nx = b.x - a.x;
        auto ny = b.y - a.y;

        return std::sqrt((nx * nx) + (ny * ny));
    }
}
    // TODO:
    // Rotate the bird's bounding box
    // Menu?
    // Highscore?
    static constexpr float FOREGROUND_Y = 100.f;
    static constexpr float BIRD_STARTING_X = 50.f;

    Game::Game(unsigned int windowWidth, unsigned int windowHeight) :
        m_debugMode(false)
#ifndef NDEBUG
      , m_godMode(false)
#endif // NDEBUG
    {
        // initialize renderer
        m_renderWindow.create(sf::VideoMode(windowWidth, windowHeight),
                              "FlappyBird++ AI : Generation " + std::to_string(m_generation));

        // initialize the background
        m_background.setSize(sf::Vector2f(windowWidth, windowHeight));
        m_background.setPosition(0, 0);

        // initialize the foreground
        m_foreground.setSize(sf::Vector2f(windowWidth, FOREGROUND_Y));
        m_foregroundPositions.push_back({ 0.f, 0.f });
        m_foregroundPositions.push_back({ windowWidth, windowWidth });
        m_foregroundOOBB.left = 0;
        m_foregroundOOBB.top = windowHeight - FOREGROUND_Y;
        m_foregroundOOBB.width = windowWidth;
        m_foregroundOOBB.height = FOREGROUND_Y;

        // initialize the obstacle ends
        m_obstacleUpperEnd.setSize(sf::Vector2f(52, 24));
        m_obstacleLowerEnd.setSize(sf::Vector2f(52, 24));

        m_gameOverFlash.setSize(sf::Vector2f(windowWidth, windowHeight));
        m_gameOverFlash.setPosition(0, 0);

        NEAT::initializeParameters();

        if(std::filesystem::exists(std::filesystem::path("population.txt"))) {
          m_population = std::make_unique<NEAT::Population>("population.txt");
        } else {
          m_startGenome = std::make_unique<NEAT::Genome>(4, 1, 1, 2);
          m_population = std::make_unique<NEAT::Population>(m_startGenome.get(), 100);
        }
    }

    Game::~Game()
    {
      std::ofstream out("population.txt");
      if(out.is_open()) {
        m_population->print_to_file_by_species(out);
      }
    }

    std::pair<bool, std::string> Game::loadResources()
    {
        static const std::vector<std::pair<std::string, std::string>> TEXTURE_RESOURCES =
        {
            // Bird textures
            { "TEXTURE_BLUE_BIRD_FLAP_DOWN", "assets/sprites/bluebird-downflap.png" },
            { "TEXTURE_BLUE_BIRD_FLAP_MID", "assets/sprites/bluebird-midflap.png" },
            { "TEXTURE_BLUE_BIRD_FLAP_UP", "assets/sprites/bluebird-upflap.png" },

            { "TEXTURE_RED_BIRD_FLAP_DOWN", "assets/sprites/redbird-downflap.png" },
            { "TEXTURE_RED_BIRD_FLAP_MID", "assets/sprites/redbird-midflap.png" },
            { "TEXTURE_RED_BIRD_FLAP_UP", "assets/sprites/redbird-upflap.png" },

            { "TEXTURE_YELLOW_BIRD_FLAP_DOWN", "assets/sprites/yellowbird-downflap.png" },
            { "TEXTURE_YELLOW_BIRD_FLAP_MID", "assets/sprites/yellowbird-midflap.png" },
            { "TEXTURE_YELLOW_BIRD_FLAP_UP", "assets/sprites/yellowbird-upflap.png" },

            // Background textures
            { "TEXTURE_BACKGROUND_DAY" , "assets/sprites/background-day.png" },
            { "TEXTURE_BACKGROUND_NIGHT" , "assets/sprites/background-night.png" },

            // foreground textures
            { "TEXTURE_FOREGROUND", "assets/sprites/base.png" },
            { "TEXTURE_PIPE_GREEN", "assets/sprites/pipe-green.png" },
            { "TEXTURE_PIPE_RED", "assets/sprites/pipe-red.png" },

            // score textures
            { "TEXTURE_0", "assets/sprites/0.png" },
            { "TEXTURE_1", "assets/sprites/1.png" },
            { "TEXTURE_2", "assets/sprites/2.png" },
            { "TEXTURE_3", "assets/sprites/3.png" },
            { "TEXTURE_4", "assets/sprites/4.png" },
            { "TEXTURE_5", "assets/sprites/5.png" },
            { "TEXTURE_6", "assets/sprites/6.png" },
            { "TEXTURE_7", "assets/sprites/7.png" },
            { "TEXTURE_8", "assets/sprites/8.png" },
            { "TEXTURE_9", "assets/sprites/9.png" },

            // message textures
            { "TEXTURE_GAMEOVER", "assets/sprites/gameover.png" },
            { "TEXTURE_GAMESTART_MESSAGE", "assets/sprites/message.png" }
        };

        static const std::vector<std::pair<std::string, std::string>> FONT_RESOURCES =
        {
            { "FONT_MAIN", "DejaVuSans.ttf" }
        };

        static const std::vector<std::pair<std::string, std::string>> SOUND_RESOURCES = {
            { "SOUND_DIE", "assets/audio/die.wav" },
            { "SOUND_HIT", "assets/audio/hit.wav" },
            { "SOUND_POINT", "assets/audio/point.wav" },
            { "SOUND_SWOOSH", "assets/audio/swoosh.wav" },
            { "SOUND_WING", "assets/audio/wing.wav" }
        };

        m_gameTexturesLookup.clear();
        for(const auto& [name, filename] : TEXTURE_RESOURCES) {
            if(!m_gameTexturesLookup.addResourceFromFile(name, filename))
                return { false, filename };
        }

        for(const auto& [name, filename] : FONT_RESOURCES) {
            if(!m_gameFontsLookup.addResourceFromFile(name, filename))
                return { false, filename };
        }

        for(const auto& [name, filename] : SOUND_RESOURCES) {
            if(!m_gameSoundLookup.addResourceFromFile(name, filename))
                return { false, filename };
        }

        m_foreground.setTexture(&m_gameTexturesLookup.getResource("TEXTURE_FOREGROUND"));

        std::vector<const sf::Texture*> numberTextures;
        for(auto i = 0; i < 10; ++i) {
            numberTextures.push_back(&m_gameTexturesLookup.getResource(std::string("TEXTURE_") + std::to_string(i)));
        }
        m_scoreRender = Score(std::move(numberTextures));
        m_scoreRender.setPosition(sf::Vector2f(m_renderWindow.getSize().x / 2.f, 50));

        auto [windowWidth, windowHeight] = m_renderWindow.getSize();
        m_gameOver.setTexture(m_gameTexturesLookup.getResource("TEXTURE_GAMEOVER"));
        m_gameOver.setPosition((windowWidth / 2.f) - (m_gameOver.getLocalBounds().width / 2.f),
                               (windowHeight / 2.f) - (m_gameOver.getLocalBounds().height / 2.f));

        m_gameStartMessage.setTexture(m_gameTexturesLookup.getResource("TEXTURE_GAMESTART_MESSAGE"));
        m_gameStartMessage.setPosition((windowWidth / 2.f) - (m_gameStartMessage.getLocalBounds().width / 2.f),
                                       (windowHeight / 2.f) - (m_gameStartMessage.getLocalBounds().height / 2.f));

        m_dieSound.setBuffer(m_gameSoundLookup.getResource("SOUND_DIE"));
        m_hitSound.setBuffer(m_gameSoundLookup.getResource("SOUND_HIT"));
        m_pointSound.setBuffer(m_gameSoundLookup.getResource("SOUND_POINT"));
        m_swooshSound.setBuffer(m_gameSoundLookup.getResource("SOUND_SWOOSH"));
        m_wingSound.setBuffer(m_gameSoundLookup.getResource("SOUND_WING"));

        return { true, "" };
    }

    void Game::reset()
    {
        // randomize which bird texture to use
        auto birdTextures = [this]() -> std::tuple<const sf::Texture&,
                                                         const sf::Texture&,
                                                         const sf::Texture&> {
            auto p = std::rand() % 100;
            if(p <= 33)
                return { m_gameTexturesLookup.getResource("TEXTURE_BLUE_BIRD_FLAP_DOWN"),
                         m_gameTexturesLookup.getResource("TEXTURE_BLUE_BIRD_FLAP_MID"),
                         m_gameTexturesLookup.getResource("TEXTURE_BLUE_BIRD_FLAP_UP") };
            else if(p <= 66)
                return { m_gameTexturesLookup.getResource("TEXTURE_RED_BIRD_FLAP_DOWN"),
                         m_gameTexturesLookup.getResource("TEXTURE_RED_BIRD_FLAP_MID"),
                         m_gameTexturesLookup.getResource("TEXTURE_RED_BIRD_FLAP_UP") };

            return { m_gameTexturesLookup.getResource("TEXTURE_YELLOW_BIRD_FLAP_DOWN"),
                     m_gameTexturesLookup.getResource("TEXTURE_YELLOW_BIRD_FLAP_MID"),
                     m_gameTexturesLookup.getResource("TEXTURE_YELLOW_BIRD_FLAP_UP") };
        } ();
        m_bird = Bird(std::get<0>(birdTextures), std::get<1>(birdTextures), std::get<2>(birdTextures));
        m_bird.reset(BIRD_STARTING_X, m_renderWindow.getSize().y / 2);

        // randomize the obstacles
        const auto& obstacleTexture = [this]() -> const sf::Texture& {
            auto p = std::rand() % 100;
            if(p <= 50)
                return m_gameTexturesLookup.getResource("TEXTURE_PIPE_RED");

            return m_gameTexturesLookup.getResource("TEXTURE_PIPE_GREEN");
        } ();
        m_obstacleUpperEnd.setTexture(&obstacleTexture);
        m_obstacleUpperEnd.setTextureRect(sf::IntRect(0, 24, 52, -24));
        m_obstacleLowerEnd.setTexture(&obstacleTexture);
        m_obstacleLowerEnd.setTextureRect(sf::IntRect(0, 0, 52, 24));
        m_obstacle.setTexture(&obstacleTexture);
        m_obstacle.setTextureRect(sf::IntRect(0, 10, 52, 10));

        // randomize the background
        const auto& backgroundTexture = [this]() -> const sf::Texture& {
             auto p = std::rand() % 100;
            if(p <= 50)
                return m_gameTexturesLookup.getResource("TEXTURE_BACKGROUND_DAY");

            return m_gameTexturesLookup.getResource("TEXTURE_BACKGROUND_NIGHT");
        } ();
        m_background.setTexture(&backgroundTexture);

        // reset the score and flags
        m_gameOverFlashAlpha = 255;
        m_score = 0;
        m_scoreRender.setScore(0);
        m_currentObstacleIndex = 0;
        m_gameStart = false;
        m_dead = false;

        // create randomized obstacles
        auto startingX = 500.f;
        m_obstacles.clear();
        for(auto i = 0; i < 10; ++i) {
            m_obstacles.push_back(createRandomObstaclePair(startingX));
            startingX += 250.f;
        }
    }

    void Game::run()
    {
        FPS fps;

        reset();

        static constexpr float timeStep = 1.0f / 128.f;
        sf::Clock clock;

        // Fixed time step from Glen Fiedler's "Fix Your TimeStep"
        // on https://gafferongames.com/
        auto accumulator = 0.f;
        while(m_renderWindow.isOpen()) {
            accumulator += clock.restart().asSeconds();
            while(accumulator >= timeStep) {
                handleInput();
                update(timeStep);
                accumulator -= timeStep;
            }

            fps.update();

            auto alpha = accumulator / timeStep;
            m_renderWindow.clear();
            draw(alpha, fps);
            m_renderWindow.display();
        }
    }

    void Game::draw(float alpha, FPS& fps)
    {
        auto [windowWidth, windowHeight] = m_renderWindow.getSize();

        m_renderWindow.draw(m_background);
        drawForeground(alpha, windowHeight);
        drawObstacle(alpha);
        m_bird.draw(m_renderWindow, alpha);

        if(m_debugMode)
            drawDebug(fps);
        if(m_dead) {
            if(m_gameOverFlashAlpha == 0)
                m_renderWindow.draw(m_gameOver);
            else {
                m_gameOverFlash.setFillColor(sf::Color(255, 255, 255, m_gameOverFlashAlpha));
                m_renderWindow.draw(m_gameOverFlash);
            }
        } else if(!m_gameStart)
            m_renderWindow.draw(m_gameStartMessage);
        m_scoreRender.draw(m_renderWindow);
    }

    void Game::drawForeground(float alpha, unsigned int windowHeight)
    {
        auto foregroundHeight = windowHeight - FOREGROUND_Y;
        for(auto& [newPos, oldPos] : m_foregroundPositions) {
            auto renderPosition = linearInterpolation(sf::Vector2f(oldPos, foregroundHeight), sf::Vector2f(newPos, foregroundHeight), alpha);
            renderPosition.x = std::roundf(renderPosition.x);
            m_foreground.setPosition(renderPosition);
            m_renderWindow.draw(m_foreground);
        }
    }

    void Game::drawObstacle(float alpha)
    {
        for(const auto& [upperObstacle, lowerObstacle] : m_obstacles) {
            auto upperRenderPosition = linearInterpolation(upperObstacle.oldPosition, upperObstacle.position, alpha);
            auto lowerRenderPosition = linearInterpolation(lowerObstacle.oldPosition, lowerObstacle.position, alpha);

            upperRenderPosition.x = std::roundf(upperRenderPosition.x + 2.f);
            lowerRenderPosition.x = std::roundf(lowerRenderPosition.x + 2.f);

            m_obstacle.setSize(sf::Vector2f(upperObstacle.dimension.x - 4, upperObstacle.dimension.y));
            m_obstacle.setPosition(upperRenderPosition);

            m_renderWindow.draw(m_obstacle);

            m_obstacle.setSize(sf::Vector2f(lowerObstacle.dimension.x - 4, lowerObstacle.dimension.y));
            m_obstacle.setPosition(lowerRenderPosition);
            m_renderWindow.draw(m_obstacle);

            auto upperEndRenderPosition = upperRenderPosition;
            upperEndRenderPosition.x -= 2.f;
            upperEndRenderPosition.x = std::roundf(upperEndRenderPosition.x);

            m_obstacleUpperEnd.setPosition(upperEndRenderPosition);
            m_obstacleUpperEnd.move(0, upperObstacle.dimension.y - 24);
            m_renderWindow.draw(m_obstacleUpperEnd);

            auto lowerEndRenderPosition = lowerRenderPosition;
            lowerEndRenderPosition.x -= 2.f;
            lowerEndRenderPosition.x = std::roundf(lowerEndRenderPosition.x);
            m_obstacleLowerEnd.setPosition(lowerEndRenderPosition);
            m_renderWindow.draw(m_obstacleLowerEnd);
        }
    }

    void Game::drawDebug(FPS& fps)
    {
        static sf::Text debugText = [this] {
            sf::Text t;
            t.setFont(m_gameFontsLookup.getResource("FONT_MAIN"));
            t.setCharacterSize(12);
            return t;
        } ();

        sf::RectangleShape collider;

        // draw the bird collider
        {
            const auto& birdOOBB = m_bird.getOOBB();
            collider.setSize(sf::Vector2f(birdOOBB.width, birdOOBB.height));
            collider.setPosition(birdOOBB.left, birdOOBB.top);
            collider.setFillColor(sf::Color(255, 0, 0, 128));
            m_renderWindow.draw(collider);
        }

        // draw the foreground collider
        {
            collider.setSize(sf::Vector2f(m_foregroundOOBB.width, m_foregroundOOBB.height));
            collider.setPosition(m_foregroundOOBB.left, m_foregroundOOBB.top);
            m_renderWindow.draw(collider);
        }

        // draw the bird position
        {
            auto birdPosition = m_bird.getPosition();
            std::string birdPositionStr = "[";
            birdPositionStr += std::to_string(static_cast<int>(birdPosition.x));
            birdPositionStr += ", ";
            birdPositionStr += std::to_string(static_cast<int>(birdPosition.y));
            birdPositionStr += "]";
            debugText.setString(birdPositionStr);
            debugText.setPosition(birdPosition - sf::Vector2f(debugText.getLocalBounds().width / 2.f, -15));
            m_renderWindow.draw(debugText);
        }

        // draw the obstacle positions
        for(const auto& [upperObstacle, lowerObstacle] : m_obstacles) {
            auto upperObstaclePosition = upperObstacle.position;
            upperObstaclePosition.y += upperObstacle.dimension.y;
            auto lowerObstaclePosition = lowerObstacle.position;

            // draw colliders
            collider.setSize(upperObstacle.dimension);
            collider.setPosition(upperObstacle.position);
            m_renderWindow.draw(collider);

            collider.setSize(lowerObstacle.dimension);
            collider.setPosition(lowerObstacle.position);
            m_renderWindow.draw(collider);

            std::string obstaclePositionStr = "[";
            obstaclePositionStr += std::to_string(static_cast<int>(upperObstaclePosition.x));
            obstaclePositionStr += ", ";
            obstaclePositionStr += std::to_string(static_cast<int>(upperObstaclePosition.y));
            obstaclePositionStr += "]";
            debugText.setString(obstaclePositionStr);
            debugText.setPosition(upperObstaclePosition - sf::Vector2f((debugText.getLocalBounds().width / 2.f) - 26, -10));
            m_renderWindow.draw(debugText);

            obstaclePositionStr = "[";
            obstaclePositionStr += std::to_string(static_cast<int>(lowerObstaclePosition.x));
            obstaclePositionStr += ", ";
            obstaclePositionStr += std::to_string(static_cast<int>(lowerObstaclePosition.y));
            obstaclePositionStr += "]";
            debugText.setString(obstaclePositionStr);
            debugText.setPosition(lowerObstaclePosition - sf::Vector2f((debugText.getLocalBounds().width / 2.f) - 26, 20));
            m_renderWindow.draw(debugText);
        }

        auto tempSize = debugText.getCharacterSize();
        auto tempColor = debugText.getFillColor();
        auto tempOutlineColor = debugText.getOutlineColor();
        auto tempOutlineThicknes = debugText.getOutlineThickness();

        debugText.setFillColor(sf::Color::Yellow);
        debugText.setOutlineColor(sf::Color::Black);
        debugText.setOutlineThickness(1.0f);
        debugText.setCharacterSize(20);
        debugText.setString("FPS:" + std::to_string(fps.getFPs()));
        debugText.setPosition(5, 5);
        m_renderWindow.draw(debugText);

        debugText.setFillColor(tempColor);
        debugText.setCharacterSize(tempSize);
        debugText.setOutlineColor(tempOutlineColor);
        debugText.setOutlineThickness(tempOutlineThicknes);
    }

    void Game::update(float dt)
    {
        if(!m_dead) {
            m_bird.updateAnimation(dt);
            if(m_gameStart) {
                m_bird.update(dt);

                const auto& birdOOBB = m_bird.getOOBB();
                if((birdOOBB.intersects(m_foregroundOOBB) /*||
                   birdOOBB.top <= birdOOBB.height / 2.f*/)
#ifndef NDEBUG
                        && !m_godMode
#endif // NDEBUG
                   ) {
                    m_dead = true;
                    m_gameStart = false;
                    m_bird.preReset();
                    m_bird.resetUpForce();

                    for(auto& [ob1, ob2] : m_obstacles) {
                        ob1.preReset();
                        ob2.preReset();
                    }
                    for(auto& [newPos, oldPos] : m_foregroundPositions) {
                        oldPos = newPos;
                    }

                    m_hitSound.play();
                    m_dieSound.play();

                } else {
                    for(std::size_t i = 0, isize = m_obstacles.size(); i < isize;) {
                        auto& [upperObstacle, lowerObstacle] = m_obstacles[i];

                        upperObstacle.oldPosition = upperObstacle.position;
                        upperObstacle.position.x -= 100.f * dt;
                        lowerObstacle.oldPosition = lowerObstacle.position;
                        lowerObstacle.position.x -= 100.f * dt;

                        if(upperObstacle.position.x <= -52.f) {
                            const auto& lastObstacle = m_obstacles.back();
                            auto difficultyDistance = getDifficultyDistance();
                            auto newObstacle = createRandomObstaclePair(lastObstacle.first.position.x + difficultyDistance);

                            m_obstacles.erase(m_obstacles.begin() + i);
                            m_obstacles.push_back(newObstacle);
                            --m_currentObstacleIndex;
                        } else {
                                sf::FloatRect upperObstacleOOBB(upperObstacle.position, upperObstacle.dimension);
                                sf::FloatRect lowerObstacleOOBB(lowerObstacle.position, lowerObstacle.dimension);
                                if((birdOOBB.intersects(upperObstacleOOBB) || birdOOBB.intersects(lowerObstacleOOBB))
#ifndef NDEBUG
                                       // godmode available on debug mode
                                       && !m_godMode
#endif // NDEBUG
                                   ) {
                                    m_dead = true;
                                    m_gameStart = false;
                                    m_bird.preReset();
                                    m_bird.resetUpForce();

                                    for(auto& [ob1, ob2] : m_obstacles) {
                                        ob1.preReset();
                                        ob2.preReset();
                                    }
                                    for(auto& [newPos, oldPos] : m_foregroundPositions) {
                                        oldPos = newPos;
                                    }

                                    m_hitSound.play();
                                    m_dieSound.play();
                                    break;
                                }
                            ++i;
                        }
                    }

                    const auto& currentObstacle = m_obstacles[m_currentObstacleIndex];
                    if(currentObstacle.first.position.x <= BIRD_STARTING_X - 17 - 52) {
                        ++m_currentObstacleIndex;
                        ++m_score;
                        m_scoreRender.setScore(m_score);
                        m_pointSound.play();
                    }
                }
            }

            if(!m_dead) {
                for(auto& [newPos, oldPos] : m_foregroundPositions) {
                    oldPos = newPos;
                    newPos -= 100.f * dt;
                    if(float windowWidth = m_renderWindow.getSize().x; newPos <= -windowWidth) {
                        auto diff = -windowWidth - newPos;
                        auto interDiff = newPos - oldPos;

                        // copy the difference between the old position and the new position
                        // when resetting the position of the foreground, which should fix
                        // the render problem where there is space between when the foreground resets
                        newPos = (windowWidth - diff);
                        oldPos = newPos - interDiff;
                    }
                }
            }
        } else {
            if(m_gameOverFlashAlpha > 0) {
                auto a = 500.f * dt;
                a = clamp(a, 0.f, 255.f);
                auto au = static_cast<unsigned char>(a);
                if(m_gameOverFlashAlpha < au)
                    m_gameOverFlashAlpha = 0;
                else
                    m_gameOverFlashAlpha -= au;
            }

            const auto& birdOOBB = m_bird.getOOBB();
            if(!birdOOBB.intersects(m_foregroundOOBB))
                m_bird.update(dt);
            else
                m_bird.preReset();
        }
    }

    void Game::handleInput()
    {
        if(!m_dead) {
            if(!m_gameStart) {
              m_gameStart = true;
            } else {
              const auto& bird_pos = m_bird.getPosition();

              auto obstacle_index = 0;
              for(size_t i = 0; i < m_obstacles.size(); ++i) {
                if(bird_pos.x < m_obstacles[i].first.position.x + m_obstacles[i].first.dimension.x + 25) {
                  obstacle_index = i;
                  break;
                }
              }

              const auto& organism = m_population->organisms[m_currentOrganismIndex];
              const auto& upper_ob_pos = m_obstacles[obstacle_index].first.position;
              const auto& upper_ob_dim = m_obstacles[obstacle_index].first.dimension;
              const auto& lower_ob_pos = m_obstacles[obstacle_index].second.position;

              double input[4];
              input[0] = 1.0;
              input[1] = (lower_ob_pos.x - bird_pos.x) / 1000.0;
              input[2] = ((upper_ob_pos.y + upper_ob_dim.y)  - bird_pos.y) / 1000.0;
              input[3] = (lower_ob_pos.y - bird_pos.y) / 1000.0;

              std::cout << input[1] << " " << input[2] << " " << input[3] << std::endl;

              auto* net = organism->net;
              net->load_sensors(&input[0]);
              net->activate();

              double res = (*net->outputs.begin())->activation;
              std::cout << res << std::endl;
              if(res > 0.5) {
                m_bird.applyUpForce();
                m_wingSound.play();
                m_upClicked = true;
              } else {
                if(m_upClicked) {
                  m_upClicked = false;
                  m_bird.resetUpForce();
                }
              }
            }
        } else {
          m_population->organisms[m_currentOrganismIndex]->fitness = m_score * 10;
          for(auto& specie : m_population->species) {
            specie->compute_average_fitness();
            specie->compute_max_fitness();
          }

          m_renderWindow.setTitle("FlappyBird++ AI : Generation " +
                                  std::to_string(m_generation) +
                                  " Organism " +
                                  std::to_string(m_currentOrganismIndex));
            ++m_currentOrganismIndex;
            if(m_currentOrganismIndex > m_population->organisms.size()) {
                m_currentOrganismIndex = 0;
                m_population->epoch(++m_generation);
            }

            reset();
            m_swooshSound.play();
        }

        sf::Event event;
        while(m_renderWindow.pollEvent(event)) {
            switch(event.type) {
            case sf::Event::Closed:
                m_renderWindow.close();
                break;
            case sf::Event::KeyPressed:
                if(event.key.code == sf::Keyboard::Space) {
                    if(m_gameStart && !m_dead) {
                        m_bird.applyUpForce();
                        m_wingSound.play();
                    }
                } else if(event.key.code == sf::Keyboard::Enter) {
                    if(m_dead && m_gameOverFlashAlpha == 0) {
                        reset();
                        m_swooshSound.play();
                    } else if(!m_gameStart) {
                        m_gameStart = true;
                        m_swooshSound.play();
                    }
                }

                break;
            case sf::Event::KeyReleased:
                if(event.key.code == sf::Keyboard::Space) {
                    if(!m_dead)
                        m_bird.resetUpForce();
                } else if(event.key.code == sf::Keyboard::BackSpace)
                    m_enteredText.clear();
                break;
            case sf::Event::TextEntered:
                m_enteredText.push_back(static_cast<char>(tolower(event.text.unicode)));
                if(m_enteredText == "gotodebug")
                    m_debugMode = !m_debugMode;
#ifndef NDEBUG
                else if(m_enteredText == "godmode") {
                    m_godMode = !m_godMode;
                }
#endif // NDEBUG
                if(m_enteredText.size() >= 9)
                    m_enteredText.clear();
                break;
            default:
                break;
            }
        }
    }

    std::pair<Obstacle, Obstacle> Game::createRandomObstaclePair(float startingPositionX)
    {
        auto windowHeight = m_renderWindow.getSize().y;
        auto maxHeight = static_cast<int>(windowHeight * 0.60f) - static_cast<int>(FOREGROUND_Y);
        auto minHeight = 50;

        auto upperObstacleHeight = minHeight + (std::rand() % maxHeight);
        auto lowerObstacleHeight = windowHeight - upperObstacleHeight - 120 - FOREGROUND_Y;

        Obstacle upperObstacle(startingPositionX, 0, 52, upperObstacleHeight);
        Obstacle lowerObstacle(startingPositionX, windowHeight - lowerObstacleHeight - FOREGROUND_Y, 52, lowerObstacleHeight);

        return { upperObstacle, lowerObstacle };
    }

    float Game::getDifficultyDistance() const
    {
        return std::max(250.f - (m_score * 2), 150.f);
    }
}
