#include "Game/Game.h"
#include "Game/GameCommon.h"
#include "Game/App.h"
#include "Game/Player.hpp"
#include "Game/World.hpp"
#include "Game/BlockDefinition.hpp"

#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Window/Window.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Rgba8.h"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/VertexUtils.h"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Math/MathUtils.h"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/Curve.hpp"
#include "Engine/Math/Splines.hpp"
#include "Engine/Math/SmoothNoise.hpp"

Game::Game(App* owner)
	: m_app(owner)
{
}

Game::~Game()
{
}

void Game::StartUp()
{
	// Write control interface into devconsole
	DevConsoleControls();

	// Get squirrel font
	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	m_spriteImage = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/SpriteSheet_Classic_Faithful_32x.png");
	m_spriteSheet = new SpriteSheet(*m_spriteImage, IntVec2::GRID8X8);

	// Get world shader
	m_worldShader = g_theRenderer->CreateOrGetShader("Data/Shaders/WorldShader", VertexType::VERTEX_PCUTBN);
	m_world_CBO = g_theRenderer->CreateConstantBuffer(sizeof(WorldConstants));

	// Initialize Block Definitions (may change to World)
	BlockDefinition::InitializeBlockDefinitions();
	InitializeContinentCurves();
	BuildTreeStamps();
	InitializeInventoryBar();

	// Create and push back the entities
	m_player = new Player(this, Vec3(-48.f, -50.f, 71.f));
	m_gameCamera = new GameCamera(this, Vec3(-50.f, -48.f, 71.f));
	m_gameCamera->AttachCameraTo(m_player);
	m_currentWorld = new World(this);
}

void Game::DevConsoleControls()
{
	g_theDevConsole->AddLine(Rgba8::CYAN, "Welcome to SimpleMiner!");
	g_theDevConsole->AddLine(Rgba8::SEAWEED, "----------------------------------------------------------------------");
	g_theDevConsole->AddLine(Rgba8::CYAN, "KEYBOARD CONTROLS:");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "ESC   - Quits the game");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "SPACE - Start game");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "SHIFT - Increase speed by factor of 20.");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Q/E   - Roll negative/positive");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "A/D   - Move up/down");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "W/S   - Move forward/backward");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "F2    - Toggle debug draw");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "F3    - Toggle job debug text");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "F4    - Toggle collision debug raycasts");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "F8    - Reload game");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "C     - Switch camera mode");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "LMB   - Dig block");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "RMB   - Place block");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "1     - Select glowstone");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "2     - Select cobblestone");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "3     - Select chiseledBrick");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Y     - Accelerate world time by 50");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "R     - Lock/Unlock Raycast");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "L     - Toggle lightning effects");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "0-9   - Select respective inventory slot");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Mouse scroll - Scrolls through inventory slots");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "X     - Empty selected inventory slot");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Z     - Empty all inventory slots");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "K     - Toggle lighting color, for cave viewing");
	g_theDevConsole->AddLine(Rgba8::SEAWEED, "----------------------------------------------------------------------");
}

void Game::InitializeContinentCurves()
{
	std::vector<Vec2> heightOffsetPoints = {Vec2(-1.0f, -0.3f), Vec2(-0.4f, -0.2f), Vec2(0.4f, 0.4f), Vec2(1.0f, 0.6f)};
	m_continentHeightOffsetCurve = new Spline(heightOffsetPoints);

	std::vector<Vec2> squashPoints = { Vec2(-1.0f, 0.0f), Vec2(-0.25f, 0.2f), Vec2(0.25f, 0.8f), Vec2(1.0f, 0.0f) };
	m_continentSquashCurve = new Spline(squashPoints);
}

void Game::Update()
{
	double deltaSeconds = m_gameClock.GetDeltaSeconds();

	if (m_isAttractMode == false)
	{
		m_player->Update(static_cast<float>(deltaSeconds));
		m_gameCamera->Update(static_cast<float>(deltaSeconds));
		m_currentWorld->Update(static_cast<float>(deltaSeconds));
		UpdateWorldTime(static_cast<float>(deltaSeconds));
		ToggleCameraMode();
	}

	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));
	KeyInputPresses();
	UpdateCameras();
}

void Game::Render() const
{
	if (m_isAttractMode == true)
	{
		g_theRenderer->BeginCamera(m_screenCamera);
		RenderAttractMode();
		g_theRenderer->EndCamera(m_screenCamera);
	}
	if (m_isAttractMode == false)
	{
		g_theRenderer->BeginCamera(m_gameCamera->GetRenderCamera());
		g_theGame->SetWorldConstants();
		g_theRenderer->ClearScreen(m_skyColor);
		m_player->Render();
		m_currentWorld->Render();
		g_theRenderer->EndCamera(m_gameCamera->GetRenderCamera());

		g_theRenderer->BeginCamera(m_screenCamera);
		DrawScreenText();
		if (m_gameCamera->m_cameraMode == CameraMode::FIRST_PERSON || m_gameCamera->m_cameraMode == CameraMode::OVER_SHOULDER
			|| m_gameCamera->m_cameraMode == CameraMode::FIXED_ANGLE_TRACKING || m_gameCamera->m_cameraMode == CameraMode::INDEPENDENT)
		{
			RenderInventoryBar();
			RenderHighlightedInventorySlot();
			RenderInventoryIcons();
		}
		g_theRenderer->EndCamera(m_screenCamera);

		DebugRenderWorld(m_gameCamera->GetRenderCamera());
		DebugRenderScreen(m_screenCamera);
	}
}

void Game::Shutdown()
{
	DestroyWorldAndPlayer();
	BlockDefinition::ClearBlockDefinitions();

	delete m_spriteSheet;
	m_spriteSheet = nullptr;

	delete m_continentHeightOffsetCurve;
	m_continentHeightOffsetCurve = nullptr;
	delete m_continentSquashCurve;
	m_continentSquashCurve = nullptr;

	// Clear any remaining completed jobs
	while (Job* completedJob = g_theJobSystem->RetreiveCompletedJob())
	{
		delete completedJob;
	}

	delete m_world_CBO;
	m_world_CBO = nullptr;
}

void Game::DestroyWorldAndPlayer()
{
	delete m_player;
	m_player = nullptr;

	delete m_gameCamera;
	m_gameCamera = nullptr;

	delete m_currentWorld;
	m_currentWorld = nullptr;
}

void Game::KeyInputPresses()
{
	// Attract Mode
	if (g_theInput->WasKeyJustPressed(' '))
	{
		m_isAttractMode = false;
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		m_isAttractMode = true;
	}

	if (g_theInput->WasKeyJustPressed('L'))
	{
		m_canLightningAppear = !m_canLightningAppear;
	}
}

void Game::AdjustForPauseAndTimeDistortion(float deltaSeconds) {

	UNUSED(deltaSeconds);

	if (g_theInput->IsKeyDown('T'))
	{
		m_gameClock.SetTimeScale(0.1);
	}
	else
	{
		m_gameClock.SetTimeScale(1.0);
	}

	if (g_theInput->WasKeyJustPressed('P'))
	{
		m_gameClock.TogglePause();
	}

	if (g_theInput->WasKeyJustPressed('O'))
	{
		m_gameClock.StepSingleFrame();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_ESC) && m_isAttractMode)
	{
		g_theEventSystem->FireEvent("Quit");
	}
}

float Game::GetTimeOfDay() const
{
	return fmodf(m_worldTimeDays, 1.f);
}

void Game::UpdateCameras()
{
	m_screenCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
}

void Game::UpdateWorldTime(float deltaSeconds)
{
	float timeScale = m_worldTimeScale;

	if (g_theInput->IsKeyDown('Y'))
	{
		timeScale *= 100.f;
	}

	m_worldTimeDays += deltaSeconds * timeScale / (60.f * 60.f * 24.f);

	if (m_worldTimeDays >= 1.f)
	{
		m_worldTimeDays -= floorf(m_worldTimeDays);
	}
}

void Game::ToggleCameraMode()
{
	if (g_theInput->WasKeyJustPressed('C'))
	{
		m_gameCamera->CycleCameraMode();
	}
}

void Game::RenderAttractMode() const
{
	std::vector<Vertex_PCU> textVerts;

	m_font->AddVertsForTextInBox2D(textVerts, "Press SPACE to Start", AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y)), 25.f);
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->DrawVertexArray(textVerts);
}

void Game::RenderHighlightedInventorySlot() const
{
	static const float slotX[11] = {400.f, 470.f, 550.f, 630.f, 710.f, 790.f, 870.f, 950.f, 1030.f, 1110.f, 1190.f};
	int selectedSlot = m_player->m_selectedInventorySlot;

	float x0 = slotX[selectedSlot];
	float x1 = slotX[selectedSlot + 1];

	float y0 = 10.f;
	float y1 = 60.f;

	Rgba8 highlightColor = Rgba8::WHITE;
	std::vector<Vertex_PCU> highlightVerts;

	float thickness = 3.f;
	float half = thickness * 0.5f;

	// Top edge
	AddVertsForLineSegment2D(highlightVerts, Vec2(x0 - half, y1), Vec2(x1 + half, y1), thickness, highlightColor);

	// Bottom edge
	AddVertsForLineSegment2D(highlightVerts, Vec2(x0 - half, y0), Vec2(x1 + half, y0), thickness, highlightColor);

	// Left edge
	AddVertsForLineSegment2D(highlightVerts, Vec2(x0, y0 - half), Vec2(x0, y1 + half), thickness, highlightColor);

	// Right edge
	AddVertsForLineSegment2D(highlightVerts, Vec2(x1, y0 - half), Vec2(x1, y1 + half), thickness,highlightColor);

	g_theRenderer->DrawVertexArray(highlightVerts);
}

void Game::RenderInventoryIcons() const
{
	static const float slotX[11] = {400.f, 470.f, 550.f, 630.f, 710.f, 790.f, 870.f, 950.f, 1030.f, 1110.f, 1190.f};

	for (int slotIndex = 0; slotIndex < 10; ++slotIndex)
	{
		uint8_t blockType = m_player->m_inventory[slotIndex];

		if (blockType == BLOCKTYPE_AIR)
		{
			continue;
		}

		BlockDefinition* blockDef = BlockDefinition::s_blockDefs[blockType];
		IntVec2 spriteCoords = blockDef->m_topSpriteCoords;
		Vec2 uvMins = m_spriteSheet->GetSpriteUVCoords(spriteCoords).m_mins;
		Vec2 uvMaxs = m_spriteSheet->GetSpriteUVCoords(spriteCoords).m_maxs;

		float x0 = slotX[slotIndex];
		float x1 = slotX[slotIndex + 1];
		float y0 = 10.f;
		float y1 = 60.f;

		float padding = 8.f;
		AABB2 iconBounds(Vec2(x0 + padding, y0 + padding), Vec2(x1 - padding, y1 - padding));

		std::vector<Vertex_PCU> iconVerts;
		AddVertsForAABB2D(iconVerts, iconBounds, Rgba8::WHITE, uvMins, uvMaxs);

		g_theRenderer->BindTexture(&m_spriteSheet->GetTexture());
		g_theRenderer->DrawVertexArray(iconVerts);

		// Drawing stack text
		int count = m_player->m_inventoryCounts[slotIndex];
		if (count > 1)
		{
			std::vector<Vertex_PCU> textVerts;
			std::string countText = Stringf("x%d", count);

			float textHeight = 10.f;

			m_font->AddVertsForTextInBox2D(textVerts, countText, iconBounds, textHeight, Rgba8::WHITE, 1.f, Vec2(1.f, 0.f));

			g_theRenderer->BindTexture(&m_font->GetTexture());
			g_theRenderer->DrawVertexArray(textVerts);
		}
	}
}

void Game::DrawBasis() const
{
	std::vector<Vertex_PCU> basisVerts;
	Vec3 arrowPosition = m_gameCamera->GetRenderCamera().GetPosition() + Vec3::MakeFromPolarDegrees(
		m_gameCamera->GetRenderCamera().GetOrientation().m_pitchDegrees,
		m_gameCamera->GetRenderCamera().GetOrientation().m_yawDegrees) * 0.5f;

	float axisLength = 0.01f;
	float shaftFactor = 0.6f;
	float shaftRadius = 0.0008f;
	float coneRadius = 0.0015f;

	Vec3 xConeEnd = arrowPosition + Vec3::XAXE * axisLength;
	Vec3 yConeEnd = arrowPosition + Vec3::YAXE * axisLength;
	Vec3 zConeEnd = arrowPosition + Vec3::ZAXE * axisLength;

	Vec3 xCylinderEnd = arrowPosition + Vec3::XAXE * (axisLength * shaftFactor);
	Vec3 yCylinderEnd = arrowPosition + Vec3::YAXE * (axisLength * shaftFactor);
	Vec3 zCylinderEnd = arrowPosition + Vec3::ZAXE * (axisLength * shaftFactor);

	AddVertsForCylinder3D(basisVerts, arrowPosition, xCylinderEnd, shaftRadius, Rgba8::RED, AABB2::ZERO_TO_ONE, 32);
	AddVertsForCylinder3D(basisVerts, arrowPosition, yCylinderEnd, shaftRadius, Rgba8::GREEN, AABB2::ZERO_TO_ONE, 32);
	AddVertsForCylinder3D(basisVerts, arrowPosition, zCylinderEnd, shaftRadius, Rgba8::BLUE, AABB2::ZERO_TO_ONE, 32);

	AddVertsForCone3D(basisVerts, xCylinderEnd, xConeEnd, coneRadius, Rgba8::RED, AABB2::ZERO_TO_ONE, 32);
	AddVertsForCone3D(basisVerts, yCylinderEnd, yConeEnd, coneRadius, Rgba8::GREEN, AABB2::ZERO_TO_ONE, 32);
	AddVertsForCone3D(basisVerts, zCylinderEnd, zConeEnd, coneRadius, Rgba8::BLUE, AABB2::ZERO_TO_ONE, 32);
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(basisVerts);
}

void Game::DrawScreenText() const
{
	std::vector<Vertex_PCU> textVerts;
	double totalTime = Clock::GetSystemClock().GetTotalSeconds();
	double frameRate = Clock::GetSystemClock().GetFrameRate();

	// Set text for time, FPS, and scale
	std::string timeScaleText = Stringf("Time: %0.2fs FPS: %0.2f", totalTime, frameRate);
	m_font->AddVertsForTextInBox2D(textVerts, timeScaleText, m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.98f, 0.97f));

	if (m_gameCamera->m_cameraMode == CameraMode::SPECTATOR_FULL)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[C] Camera: Spectator", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.97f));
	}
	else if (m_gameCamera->m_cameraMode == CameraMode::SPECTATOR_XY)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[C] Camera: SpectatorXY", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.97f));
	}
	else if (m_gameCamera->m_cameraMode == CameraMode::FIRST_PERSON)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[C] Camera: First Person", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.97f));
	}
	else if (m_gameCamera->m_cameraMode == CameraMode::FIXED_ANGLE_TRACKING)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[C] Camera: Fixed Angle Tracking", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.97f));
	}
	else if (m_gameCamera->m_cameraMode == CameraMode::OVER_SHOULDER)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[C] Camera: Over Shoulder", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.97f));
	}
	else if (m_gameCamera->m_cameraMode == CameraMode::INDEPENDENT)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[C] Camera: Independent", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.97f));
	}

	if (m_player->GetPlayerPhysicsMode() == PhysicsMode::WALKING)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[V] PhysicsMode: Walking", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.95f));
	}
	else if (m_player->GetPlayerPhysicsMode() == PhysicsMode::FLYING)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[V] PhysicsMode: Flying", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.95f));
	}
	else if (m_player->GetPlayerPhysicsMode() == PhysicsMode::NOCLIP)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[V] PhysicsMode: No clip", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.65f, 0.95f));
	}

	if (m_player->m_currentSelectedBlock == BLOCKTYPE_GLOWSTONE)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[LMB] Dig [RMB] Add Glowstone", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.97f));
	}
	else if (m_player->m_currentSelectedBlock == BLOCKTYPE_COBBLESTONE)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[LMB] Dig [RMB] Add Cobblestone", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.97f));
	}
	else if (m_player->m_currentSelectedBlock == BLOCKTYPE_CHISELEDBRICK)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "[LMB] Dig [RMB] Add ChiseledBrick", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.97f));
	}

	m_font->AddVertsForTextInBox2D(textVerts, "[1] Glowstone [2] Cobblestone [3] ChiseledBrick", m_gameScreenBounds, 10.f, Rgba8::GOLD, 1.f, Vec2(0.3f, 0.97f));

	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(textVerts);
}

void Game::RenderInventoryBar() const
{
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(m_inventoryQuadVerts);
}

void Game::InitializeInventoryBar()
{
	AddVertsForAABB2D(m_inventoryQuadVerts, AABB2(Vec2(400.f, 10.f), Vec2(1190.f, 60.f)), Rgba8(0, 0, 0, 125));
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(400.f, 10.f), Vec2(1190.f, 10.f), 3.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(400.f, 60.f), Vec2(1190.f, 60.f), 3.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(400.f, 8.5f), Vec2(400.f, 61.5f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(470.f, 10.f), Vec2(470.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(550.f, 10.f), Vec2(550.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(630.f, 10.f), Vec2(630.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(710.f, 10.f), Vec2(710.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(790.f, 10.f), Vec2(790.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(870.f, 10.f), Vec2(870.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(950.f, 10.f), Vec2(950.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(1030.f, 10.f), Vec2(1030.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(1110.f, 10.f), Vec2(1110.f, 60.f), 4.f, Rgba8::SLATEGRAY);
	AddVertsForLineSegment2D(m_inventoryQuadVerts, Vec2(1190.f, 8.5f), Vec2(1190.f, 61.5f), 4.f, Rgba8::SLATEGRAY);
}

void Game::SetWorldConstants()
{
	WorldConstants worldConstants;
	worldConstants.CameraPosition = Vec4(m_player->GetPlayerPosition(), 1.f);
	worldConstants.FogFarDistance = CHUNK_ACTIVATION_RANGE;
	worldConstants.FogNearDistance = worldConstants.FogFarDistance * 0.5f;

	float timeofDay = GetTimeOfDay();

	// Sky color
	Rgba8 nightSky(20, 20, 40, 255);
	Rgba8 daySky(200, 230, 255, 255);

	// Map daytime
	float dayFactor = 0.f;
	if (timeofDay >= 0.25f && timeofDay <= 0.75f)
	{
		dayFactor = RangeMapClamped(timeofDay, 0.25f, 0.75f, 0.f, 1.f);
		dayFactor = 1.f - fabsf((timeofDay - 0.5f) / 0.25f);
	}

	Vec4 nightSkyAsVec4 = nightSky.GetAsVec4();
	Vec4 daySkyAsVec4 = daySky.GetAsVec4();
	Vec4 skyColor = Interpolate(nightSkyAsVec4, daySkyAsVec4, dayFactor);
	Rgba8 skyColorRGBA = Rgba8(
		static_cast<unsigned char>((skyColor.x * 255.f)),
		static_cast<unsigned char>((skyColor.y * 255.f)),
		static_cast<unsigned char>((skyColor.z * 255.f)),
		static_cast<unsigned char>((skyColor.w * 255.f))
	);
	m_skyColor = skyColorRGBA;

	if (m_canLightningAppear)
	{
		Vec4 whiteLightning = Vec4(80.f, 80.f, 100.f, 255.f);
		float lightningNoise = Compute1dPerlinNoise(m_worldTimeDays, 0.0002f, LIGHTNING_OCTAVES);
		lightningNoise = RangeMapClamped(lightningNoise, 0.85f, 0.95f, 0.f, 1.f);
		skyColor = Interpolate(skyColor, whiteLightning, lightningNoise);
	}

	// Outdoor light color
	Rgba8 nightLight(40, 40, 60, 255);
	Rgba8 dayLight(255, 255, 255, 255);
	Vec4  nightLightAsVec4 = nightLight.GetAsVec4();
	Vec4  dayLightAsVec4 = dayLight.GetAsVec4();
	Vec4  outdoorLightColor = Interpolate(nightLightAsVec4, dayLightAsVec4, dayFactor);

	// Indoor light color
	Rgba8 indoorLightColor = Rgba8(255, 230, 204, 255);

	// Glowstone flicker
	float glowstoneNoise = Compute1dPerlinNoise(m_worldTimeDays, 0.001f, GLOWSTONE_OCTAVES);
	glowstoneNoise = RangeMapClamped(glowstoneNoise, -1.f, 1.f, 0.8f, 1.f);
	Rgba8 indoorFlicker = Rgba8(static_cast<unsigned char>(RoundDownToInt(indoorLightColor.r * glowstoneNoise)),
								static_cast<unsigned char>(RoundDownToInt(indoorLightColor.g * glowstoneNoise)),
								static_cast<unsigned char>(RoundDownToInt(indoorLightColor.b * glowstoneNoise)), 255);
	Vec4 indoorLightColorAsVec4 = indoorFlicker.GetAsVec4();

	worldConstants.IndoorLightColor = indoorLightColorAsVec4;
	worldConstants.OutdoorLightColor = outdoorLightColor;
	worldConstants.SkyColor = skyColor;

	g_theRenderer->CopyCPUToGPU((void*)&worldConstants, sizeof(worldConstants), m_world_CBO);
	g_theRenderer->BindConstantBuffer(4, m_world_CBO);
}
