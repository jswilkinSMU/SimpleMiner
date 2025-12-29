#pragma once
#include "Game/GameCommon.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Rgba8.h"
#include "Engine/Core/Vertex_PCU.h"
// -----------------------------------------------------------------------------
class Player;
class GameCamera;
class World;
class BitmapFont;
class Spline;
class Shader;
class ConstantBuffer;
class Texture;
class SpriteSheet;
// -----------------------------------------------------------------------------
class Game
{
public:
	App* m_app;
	Game(App* owner);
	~Game();
	void StartUp();
	void DevConsoleControls();
	void InitializeContinentCurves();

	void Update();
	void UpdateCameras();
	void UpdateWorldTime(float deltaSeconds);
	void ToggleCameraMode();

	void Render() const;
	void RenderAttractMode() const;
	void RenderHighlightedInventorySlot() const;
	void RenderInventoryIcons() const;
	void DrawBasis() const;
	void DrawScreenText() const;

	void RenderInventoryBar() const;

	void InitializeInventoryBar();
	void SetWorldConstants();

	void Shutdown();
	void DestroyWorldAndPlayer();

	void KeyInputPresses();
	void AdjustForPauseAndTimeDistortion(float deltaSeconds);
	bool		m_isAttractMode = true;

	Player* m_player = nullptr;
	World* m_currentWorld = nullptr;
	Clock		m_gameClock;

private:
	Camera		m_screenCamera;
	BitmapFont* m_font = nullptr;
	AABB2		m_gameScreenBounds = AABB2(0.f, 0.f, SCREEN_SIZE_X, SCREEN_SIZE_Y);
	std::vector<Vertex_PCU> m_inventoryQuadVerts;

	// World time
	float m_worldTimeDays = 0.5f;
	float m_worldTimeScale = 200.f;
	float GetTimeOfDay() const;

	// Effects
	bool m_canLightningAppear = false;

public:
	// Continent curves
	Spline* m_continentHeightOffsetCurve = nullptr;
	Spline* m_continentSquashCurve = nullptr;

	Shader* m_worldShader = nullptr;
	ConstantBuffer* m_world_CBO = nullptr;

	GameCamera* m_gameCamera = nullptr;
	Rgba8 m_skyColor = Rgba8::WHITE;

	Texture* m_spriteImage = nullptr;
	SpriteSheet* m_spriteSheet = nullptr;
};