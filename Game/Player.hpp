#pragma once
#include "Game/Block.hpp"
#include "Game/Entity.hpp"
#include "Game/GameCamera.hpp"
#include "Engine/Renderer/Camera.h"
#include "Engine/Core/Timer.hpp"
// -----------------------------------------------------------------------------
class Game;
// -----------------------------------------------------------------------------
class Player : public Entity
{
public:
	Player(Game* owner, Vec3 const& position);
	~Player();

	void Update(float deltaSeconds) override;
	void Render() const override;
	void DrawPlayer() const;

	Vec3 GetForwardNormal() const;
	Vec3 GetPlayerPosition() const;
	Vec3 GetEyePosition() const;
	EulerAngles GetPlayerOrientation() const;
	PhysicsMode GetPlayerPhysicsMode() const;
	uint8_t GetSelectedBlockInSlot() const;

public:
	Timer   m_physicsTimer;

	// Raycast locking variables
	bool m_isRaycastLocked = false;
	Vec3 m_lockedStartPos  = Vec3::ZERO;
	Vec3 m_lockedDirection = Vec3::ZERO;

	// Inventory
	uint8_t m_currentSelectedBlock;
	uint8_t m_inventory[INVENTORY_SIZE];
	uint8_t m_inventoryCounts[INVENTORY_SIZE];
	int     m_selectedInventorySlot = 0;

private:
	void CyclePhysicsMode();
	void CameraKeyPresses(float deltaSeconds);
	void CameraControllerPresses(float deltaSeconds);
	void HandlePlayerMovement(float deltaSeconds);
	bool PlaceBlockBelowPosition(uint8_t blockType);
	IntVec3 GetFirstSolidBlockBelow() const;
	bool AddBlockToInventory(uint8_t blockType);
	bool RemoveBlockFromSelectedSlot();

	float m_walkTimer = 0.f;
	float m_walkSpeed = 4.f;
	float m_walkBlend = 0.f;
};