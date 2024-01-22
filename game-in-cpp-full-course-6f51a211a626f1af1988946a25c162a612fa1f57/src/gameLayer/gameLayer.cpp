#define GLM_ENABLE_EXPERIMENTAL
#include "gameLayer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "platformInput.h"
#include "imgui.h"
#include <iostream>
#include <sstream>
#include "imfilebrowser.h"
#include <gl2d/gl2d.h>
#include <platformTools.h>
#include <tileRenderer.h>
#include <bullet.h>
#include <vector>
#include <enemy.h>
#include <cstdio>
#include <future>
#include <raudio.h>
#include <glui/glui.h>
#include <timer.h>
//#include "mathematics.h"

using namespace std::literals::chrono_literals;

struct GameplayData
{
	glm::vec2 PlayerPos = { 100,100 };

	std::vector<Bullet> bullets;
	std::vector<Enemy> enemies;
	float health = 1.f;
	float spawnEnemyTimerSecconds = 3;
};

GameplayData data;
gl2d::Renderer2D renderer;

constexpr int BACKGROUNDS = 3;
constexpr float shipSize = 250.f;

gl2d::Texture spaceShipsTexture;
gl2d::TextureAtlasPadding spaceShipsAtlas;

gl2d::Texture bulletsTexture;
gl2d::TextureAtlasPadding bulletsAtlas;

gl2d::Texture explosionTexture;
gl2d::TextureAtlasPadding explosionAtlas;

gl2d::Texture backgroundTexture[BACKGROUNDS];

TileRenderer tileRenderer[BACKGROUNDS];

gl2d::Texture healthBar;
gl2d::Texture health;
gl2d::Font font;

Sound shootSound;
Sound explosionSound;
Music backgroundMusic;

glui::RendererUi uirenderer;
bool isInGame = 0;

int score = 0;
std::vector < std::future<void>> m_futures;
bool explosionActive = 0;
constexpr float DASH_CD = 3.f;
float dashTimer = DASH_CD;
constexpr float EFFECT_DURATION = .5f;
float effectTimer = EFFECT_DURATION;
bool canDash = 1;
glm::vec2 playerPosOnHit;
//Mathematics math;

void resetGame()
{
	data = {};
	renderer.currentCamera.follow(data.PlayerPos, 550, 0, 0, renderer.windowW, renderer.windowH);
	healthBar.loadFromFile(RESOURCES_PATH "healthBar.png", true);
	health.loadFromFile(RESOURCES_PATH "health.png", true);
	backgroundTexture[0].loadFromFile(RESOURCES_PATH "background1.png", true);
	score = 0;
	StopMusicStream(backgroundMusic);
	PlayMusicStream(backgroundMusic);
}

bool initGame()
{
	//initializing stuff for the renderer
	std::srand(std::time(0));
	gl2d::init();
	renderer.create();

	spaceShipsTexture.loadFromFileWithPixelPadding(RESOURCES_PATH "spaceShip/stitchedFiles/spaceships.png", 128, true);
	spaceShipsAtlas = gl2d::TextureAtlasPadding(5, 2, spaceShipsTexture.GetSize().x, spaceShipsTexture.GetSize().y);

	bulletsTexture.loadFromFileWithPixelPadding(RESOURCES_PATH "spaceShip/stitchedFiles/projectiles.png", 500, true);
	bulletsAtlas = gl2d::TextureAtlasPadding(3, 2, bulletsTexture.GetSize().x, bulletsTexture.GetSize().y);

	explosionTexture.loadFromFile(RESOURCES_PATH "spaceShip/stitchedFiles/explosions.png", true);
	explosionAtlas = gl2d::TextureAtlasPadding(5, 2, explosionTexture.GetSize().x, explosionTexture.GetSize().y);

	backgroundTexture[0].loadFromFile(RESOURCES_PATH "background1.png", true);
	backgroundTexture[1].loadFromFile(RESOURCES_PATH "background2.png", true);
	backgroundTexture[2].loadFromFile(RESOURCES_PATH "background3.png", true);

	font.createFromFile(RESOURCES_PATH "CommodorePixeled.ttf");

	tileRenderer[0].texture = backgroundTexture[0];
	tileRenderer[1].texture = backgroundTexture[1];
	tileRenderer[2].texture = backgroundTexture[2];
	tileRenderer[0].parallaxStrength = 0;
	tileRenderer[1].parallaxStrength = 0.4f;
	tileRenderer[2].parallaxStrength = 0.7f;

	shootSound = LoadSound(RESOURCES_PATH "shoot.flac");
	SetSoundVolume(shootSound, 0.1f);
	explosionSound = LoadSound(RESOURCES_PATH "explosion.flac");
	SetSoundVolume(explosionSound, 0.8f);
	backgroundMusic = LoadMusicStream(RESOURCES_PATH "bgMusic.mp3");
	SetMusicVolume(backgroundMusic, 0.5f);

	resetGame();

	return true;
}

void spawnEnemy()
{
	glm::uvec2 shipTypes[] = { {0,0}, {0,1}, {2,0}, {3, 1} };
	
	Enemy e;
	e.position = data.PlayerPos;

	glm::vec2 offset(2000, 0);
	offset = glm::vec2(glm::vec4(offset, 0, 1) * glm::rotate(glm::mat4(1.f), glm::radians((float)(rand() % 360)), glm::vec3(0, 0, 1)));

	e.position += offset;
	e.speed = 800 + rand() % 1000;
	e.turnSpeed = 2.2f + (rand() & 1000) / 500.f;

	int enemyType = rand() % 3;
	if (enemyType == 1)
	{
		e.type = shipTypes[rand() % 3];
		e.enemyType = Enemy::shooter;
		e.fireRange = 1.5 + (rand() % 1000) / 2000.f;
		e.fireTimeReset = 0.1 + (rand() % 1000) / 500;
		e.bulletSpeed = rand() % 3000 + 1000;
	}
	else //crasher ship
	{
		e.type = shipTypes[3];
		e.enemyType = Enemy::crasher;
		e.canShoot = false;
		e.speed += 150;
		e.turnSpeed += 200;
		e.life = 0.1f;
	}

	data.enemies.push_back(e);
}

bool intersectBullet(glm::vec2 bulletPos, glm::vec2 shipPos, float shipSize)
{
	return glm::distance(bulletPos, shipPos) <= shipSize;
}

bool intersectEnemyCrasher(glm::vec2 enemyPos)
{
	return glm::distance(enemyPos, data.PlayerPos) <= shipSize;
}

int explosionXIdx = 0, explosionYIdx = 0;

void renderExplosion(float deltaTime, glm::vec2 playerPos)
{
	effectTimer -= deltaTime;
	//explosionXIdx += deltaTime;
	renderer.renderRectangle({ playerPos - glm::vec2(shipSize/2, shipSize/2), 250, 250 }, explosionTexture,
		Colors_White, {}, 0, explosionAtlas.get(0, 1));
	if (effectTimer <= 0)
	{
		effectTimer = EFFECT_DURATION;
		explosionActive = 0;
	}
}

void gameplay(float deltaTime, int w, int h)
{

#pragma region movement

	glm::vec2 move = {};

	if (platform::isButtonHeld(platform::Button::W) || platform::isButtonHeld(platform::Button::Up))
	{
		move.y = -1;
	}
	if (platform::isButtonHeld(platform::Button::A) || platform::isButtonHeld(platform::Button::Left))
	{
		move.x = -1;
	}
	if (platform::isButtonHeld(platform::Button::S) || platform::isButtonHeld(platform::Button::Down))
	{
		move.y = 1;
	}
	if (platform::isButtonHeld(platform::Button::D) || platform::isButtonHeld(platform::Button::Right))
	{
		move.x = 1;
	}
	if (platform::isButtonPressedOn(platform::Button::LeftShift) && canDash)
	{
		canDash = 0;
		glm::vec2 dash = glm::vec2(move.x * 250, move.y * 250);
		data.PlayerPos += dash;
		//todo cam dash effect and dash meter
	}
	if (move.x != 0 || move.y != 0) //cant divide by 0
	{
		move = glm::normalize(move);
		move *= deltaTime * 1000;
		data.PlayerPos += move;
	}
	if(!canDash) //dashing 
	{
		dashTimer -= deltaTime;
	    if (dashTimer <= 0)
		{
			dashTimer = DASH_CD;
			canDash = 1;
		}
	}
#pragma endregion

#pragma region camera follow
    renderer.currentCamera.follow(data.PlayerPos, deltaTime * 550, 1, 700, w, h);


#pragma endregion

#pragma region background rendering

	renderer.currentCamera.zoom = 0.5f;
	for (int i = 0; i < BACKGROUNDS; i++)
	{
		tileRenderer[i].render(renderer);
	}

#pragma endregion

#pragma region mouse pos

	glm::vec2 mousePos = platform::getRelMousePosition();
	glm::vec2 screenCentre(w / 2.f, h / 2.f);

	glm::vec2 mouseDirection = mousePos - screenCentre;
	if (glm::length(mouseDirection) == 0.0f)
	{
		mouseDirection = { 1,0 };
	}
	else
	{
		mouseDirection = glm::normalize(mouseDirection);
	}
	float spaceShipAngle = atan2(mouseDirection.y, -mouseDirection.x);


#pragma endregion

#pragma region bullets

	if (platform::isLMousePressed())
	{
		Bullet b;
		b.position = data.PlayerPos;
		b.fireDirection = mouseDirection;

		data.bullets.push_back(b);

		PlaySound(shootSound);
	}

	for (auto& b : data.bullets)
	{
		b.render(renderer, bulletsTexture, bulletsAtlas);
	}

	for (int i = 0; i < data.bullets.size(); i++)
	{

		if (glm::distance(data.bullets[i].position, data.PlayerPos) > 5000)
		{
			data.bullets.erase(data.bullets.begin() + i);
			i--;
			continue;
		}

		if (!data.bullets[i].isEnemy)
		{
			bool breakBothLoops = false;
			for (int e = 0; e < data.enemies.size(); e++)
			{
				if (intersectBullet(data.bullets[i].position, data.enemies[e].position, enemyShipSize))
				{
					data.enemies[e].life -= 0.1f;

					if (data.enemies[e].life <= 0)
					{
						//kill enemy
						score++;
						data.enemies.erase(data.enemies.begin() + e);
					}

					data.bullets.erase(data.bullets.begin() + i);
					i--;
					breakBothLoops = true;
					continue;
				}

			}
			if (breakBothLoops)
				continue;
		}
		else
		{
			if (intersectBullet(data.bullets[i].position, data.PlayerPos, shipSize))
			{
				data.health -= 0.05f;

				data.bullets.erase(data.bullets.begin() + i);
				i--;
				continue;
			}
		}
		data.bullets[i].update(deltaTime);
	}

	if (data.health <= 0)
	{
		//kill player

		//resetGame();
		isInGame = false;
	}
	else
	{
		data.health += deltaTime * 0.01;
		data.health = glm::clamp(data.health, 0.f, 1.f);
	}
#pragma endregion

#pragma region handle enemies

	if (data.enemies.size() < 15)
	{
		data.spawnEnemyTimerSecconds -= deltaTime;

		if (data.spawnEnemyTimerSecconds < 0)
		{
			data.spawnEnemyTimerSecconds = rand() % 6 + 1;

			spawnEnemy();
			if (rand() % 3 == 0)
			{
				spawnEnemy();
			}
		}
	}

	for (int i = 0; i < data.enemies.size(); i++)
	{
		if (glm::distance(data.PlayerPos, data.enemies[i].position) > 4000.f)
		{
			data.enemies.erase(data.enemies.begin() + i);
			i--;
			continue;
		}
		if (data.enemies[i].update(deltaTime, data.PlayerPos))
		{
			Bullet b;
			b.position = data.enemies[i].position;
			b.fireDirection = data.enemies[i].viewDirection;
			b.isEnemy = true;
			b.speed = data.enemies[i].bulletSpeed;
			data.bullets.push_back(b);
			if (!IsSoundPlaying(shootSound)) PlaySound(shootSound);
		}
		if(data.enemies[i].enemyType == Enemy::crasher) //crasher hit player //TODO
		{
		    if(intersectEnemyCrasher(data.enemies[i].position))
		    {
				PlaySound(explosionSound);
				if(!explosionActive)
				{
					playerPosOnHit = data.PlayerPos;
					explosionActive = 1;
				}

				data.enemies.erase(data.enemies.begin() + i);
				data.health -= 0.1f;
				i--;
		    }
		}
	}
	
#pragma endregion

#pragma region render enemies

	for (auto& e : data.enemies)
	{
		e.render(renderer, spaceShipsTexture, spaceShipsAtlas);
	}

#pragma endregion

#pragma region render ship/player

	renderSpaceShip(renderer, data.PlayerPos, shipSize, spaceShipsTexture, spaceShipsAtlas.get(3, 0), mouseDirection);

	if(explosionActive) //to render over player ship
	{
		renderExplosion(deltaTime, playerPosOnHit);
		//m_futures.push_back(std::async(std::launch::async, renderExplosion, deltaTime));
	}
#pragma endregion

#pragma region ui

	renderer.pushCamera();
	{
		glui::Frame f({ 0,0, w, h });

		glui::Box healthBox = glui::Box().xLeftPerc(0.65f).yTopPerc(0.1f).xDimensionPercentage(0.3f).yAspectRatio(1.f / 8.f);

		renderer.renderRectangle(healthBox, healthBar);

		glm::vec4 newRect = healthBox();
		newRect.z *= data.health;

		glm::vec4 textCoords = { 0,1,1,0 };
		textCoords.z *= data.health;

		renderer.renderRectangle(newRect, health, Colors_White, {}, {}, textCoords);

		std::string s = "Score : " + (std::to_string)(score);
		renderer.renderText(glm::vec2(w/2, 100), s.data(), font, Colors_White, 1.f);

	}
	renderer.popCamera();

#pragma endregion

	UpdateMusicStream(backgroundMusic);
}

bool gameLogic(float deltaTime)
{
#pragma region init stuff

	int w = 0; int h = 0;
	w = platform::getFrameBufferSizeX(); //window w
	h = platform::getFrameBufferSizeY(); //window h

	glViewport(0, 0, w, h);
	glClear(GL_COLOR_BUFFER_BIT); //clear screen

	renderer.updateWindowMetrics(w, h);
#pragma endregion

#pragma region menu logic
	if(isInGame)
	    gameplay(deltaTime, w, h);
	else
	{
		uirenderer.Begin(1);

		if(uirenderer.Button("Play", Colors_White))
		{
			isInGame = true;
			resetGame();
		}

		uirenderer.End();

		uirenderer.renderFrame(renderer, font, platform::getRelMousePosition(), platform::isLMousePressed(), 
			platform::isLMouseHeld(), platform::isLMouseReleased(), 
			platform::isButtonReleased(platform::Button::Escape), platform::getTypedInput(), deltaTime);
	}

#pragma endregion

#pragma region imgui
	ImGui::Begin("debug");

	ImGui::Text("Bullets count: %d", (int)data.bullets.size());

	ImGui::Text("Enemies count: %d", (int)data.enemies.size());

	if (ImGui::Button("Spawn enemy"))
	{
		spawnEnemy();
	}

	if (ImGui::Button("Reset game"))
	{
		resetGame();
	}

	ImGui::SliderFloat("Player Health", &data.health, 0, 1);

	ImGui::End();


#pragma endregion
	renderer.flush();

	//ImGui::ShowDemoWindow();


	return true;
#pragma endregion

}

//This function might not be be called if the program is forced closed
void closeGame()
{



}
