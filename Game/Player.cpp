#include "Game/Player.hpp"
#include "Game/World.hpp"
#include "Game/Game.h"
#include "Game/Chunk.hpp"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Math/MathUtils.h"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/DebugRender.hpp"

Player::Player(Game* owner, Vec3 const& position)
	:Entity(owner, position), m_physicsTimer(1.0 / 60.0, &m_theGame->m_gameClock)
{
	// Defaulting our current selected block to glowstone
	m_currentSelectedBlock = BLOCKTYPE_GLOWSTONE;

	// Player entity is 1.80m tall, 0.60m wide
	m_physicsBody = AABB3(Vec3(-0.30f, -0.30f, 0.0f), Vec3(0.30f, 0.30f, 1.60f));

	// Physics timer
	m_physicsTimer.Start();

	// Fill inventory with block types
	m_inventory[0] = BLOCKTYPE_GLOWSTONE;
	m_inventoryCounts[0] = 64;
	m_inventory[1] = BLOCKTYPE_COBBLESTONE;
	m_inventoryCounts[1] = 12;
	m_inventory[2] = BLOCKTYPE_CHISELEDBRICK;
	m_inventoryCounts[2] = 64;

	for (int inventoryIndex = 3; inventoryIndex < INVENTORY_SIZE; ++inventoryIndex)
	{
		m_inventory[inventoryIndex] = BLOCKTYPE_AIR;
		m_inventoryCounts[inventoryIndex] = 0;
	}

	m_selectedInventorySlot = 0;
}

Player::~Player()
{
}

void Player::Update(float deltaSeconds)
{
	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
	m_orientation.m_rollDegrees = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);

	CameraMode cam = m_theGame->m_gameCamera->m_cameraMode;

	if (g_theInput->WasKeyJustPressed('V'))
	{
		CyclePhysicsMode();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F4))
	{
		m_isCollisionDebugOn = !m_isCollisionDebugOn;
	}

	if (cam == CameraMode::FIRST_PERSON || cam == CameraMode::OVER_SHOULDER || cam == CameraMode::INDEPENDENT || cam == CameraMode::FIXED_ANGLE_TRACKING)
	{
		CameraKeyPresses(deltaSeconds);
		HandlePlayerMovement(deltaSeconds);
		while (m_physicsTimer.DecrementPeriodIfElapsed()) 
		{
			UpdatePhysics(FIXED_DELTASECONDS);
			if (m_physicsMode == PhysicsMode::WALKING)
			{
				UpdateIsGrounded();
			}
		}
	}
	
	// Move check for animation if in walking mode
	if (m_physicsMode == PhysicsMode::WALKING)
	{
		bool isMoving = m_velocity.GetXY().GetLengthSquared() > 0.001f;

		if (isMoving)
		{
			m_walkTimer += deltaSeconds * m_walkSpeed;
		}

		float targetBlend = isMoving ? 1.f : 0.f;
		float blendSpeed = 15.f;
		m_walkBlend = Interpolate(m_walkBlend, targetBlend, deltaSeconds * blendSpeed);
	}
	else
	{
		m_walkTimer = 0.f;
	}

	if (g_theInput->WasKeyJustPressed('R'))
	{
		m_isRaycastLocked = !m_isRaycastLocked;

		if (m_isRaycastLocked)
		{
			m_lockedStartPos = m_position;
			m_lockedDirection = GetForwardNormal();
		}
		else
		{
			// Blank continue
		}
	}

	// Debug Raycast Visualization
	{
		Vec3 rayStart;
		Vec3 rayDir;
		float maxDist = 100.f;

		if (m_isRaycastLocked)
		{
			rayStart = m_lockedStartPos;
			rayDir = m_lockedDirection;
		}
		else
		{
			rayStart = GetEyePosition();
			rayDir = GetForwardNormal();
		}

		GameRaycastResult3D hitResult = m_theGame->m_currentWorld->RaycastVsBlocks(rayStart, rayDir, maxDist);
		Vec3 rayEnd = rayStart + rayDir * maxDist;

		if (hitResult.m_didImpact)
		{
			rayEnd = hitResult.m_impactPos;

			Vec3 center = hitResult.m_impactedBlockIterator.GetBlockCenter();
			Vec3 normal = hitResult.m_impactNormal.GetNormalized();
			float size = 1.01f;
			float faceOffset = 0.51f;

			Vec3 right, up;
			if (fabsf(normal.x) > 0.5f)
			{
				right = Vec3::YAXE * (normal.x > 0.f ? 1.f : -1.f);
				up = Vec3::ZAXE;
			}
			else if (fabsf(normal.y) > 0.5f)
			{
				right = Vec3::XAXE * (normal.y < 0.f ? 1.f : -1.f);
				up = Vec3::ZAXE;
			}
			else
			{
				right = Vec3::XAXE;
				up = Vec3::YAXE * (normal.z < 0.f ? -1.f : 1.f);
			}

			Vec3 faceCenter = center + normal * faceOffset;
			Vec3 halfRight = right * (0.5f * size);
			Vec3 halfUp = up * (0.5f * size);

			Vec3 p0 = faceCenter - halfRight - halfUp;
			Vec3 p1 = faceCenter + halfRight - halfUp;
			Vec3 p2 = faceCenter + halfRight + halfUp;
			Vec3 p3 = faceCenter - halfRight + halfUp;
			DebugAddWorldQuad(p0, p1, p2, p3, 0.f, Rgba8::RED, Rgba8::RED);
			DebugAddWorldArrow(hitResult.m_impactPos, hitResult.m_impactPos + hitResult.m_impactNormal * 0.5f, 0.05f, 0.f);
		}

		// Drawing the ray
		if (cam == CameraMode::INDEPENDENT || cam == CameraMode::FIXED_ANGLE_TRACKING)
		{
			DebugAddWorldCylinder(rayStart, rayEnd, 0.02f, 0.f, Rgba8::BLUE, Rgba8::BLUE);
		}
	}
}

void Player::Render() const
{
	CameraMode cam = m_theGame->m_gameCamera->m_cameraMode;

	// Not rendering player when in first person mode
	if (cam == CameraMode::FIRST_PERSON)
	{
		return;
	}

	DrawPlayer();
}

void Player::DrawPlayer() const
{
	std::vector<Vertex_PCU> playerVerts;
	float leftLeghalfWidth = 0.175f;
	float bodyHalfWidth = 0.2f;
	float legHeight = 0.6f;
	float bodyHeight = 0.6f;
	float headHeight = 0.4f;
	float shoeHeight = 0.15f;
	float shoulderZ = legHeight + bodyHeight - 0.3f;

	float legSwing = SinDegrees(m_walkTimer * 60.f) * 0.15f * m_walkBlend;
	float armSwing = SinDegrees(m_walkTimer * 60.f + 180.f) * 0.1f * m_walkBlend;
	float walkAngleLegs = SinDegrees(m_walkTimer * 60.f) * -10.f * m_walkBlend; 
	float walkAngleArms = SinDegrees(m_walkTimer * 60.f + 180.f) * -30.f * m_walkBlend;

	Vec3 leftLegOffset(0.f, 0.f, 0.f);
	Vec3 rightLegOffset(0.f, 0.f, 0.f);
	Vec3 leftArmOffset(0.f, 0.f, 0.f);
	Vec3 rightArmOffset(0.f, 0.f, 0.f);
	Vec3 leftShoulderPivot = Vec3(0.f, bodyHalfWidth, shoulderZ);
	Vec3 rightShoulderPivot = Vec3(0.f, -bodyHalfWidth, shoulderZ);
	Vec3 leftHipPivot = Vec3(0.f, -0.175f, legHeight);
	Vec3 rightHipPivot = Vec3(0.f, 0.05f, legHeight);

	leftLegOffset.x = legSwing;
	rightLegOffset.x = -legSwing;
	leftArmOffset.x = -armSwing;
	rightArmOffset.x = armSwing;

	// Legs
	Vec3 leftLegCenter = Vec3(0.f, -0.175f, 0.f) + leftLegOffset;
	Vec3 rightLegCenter = Vec3(0.f, 0.05f, 0.f) + rightLegOffset;
	AABB3 leftLeg = AABB3(leftLegCenter + Vec3(-leftLeghalfWidth, -0.05f, shoeHeight), leftLegCenter + Vec3(leftLeghalfWidth, leftLeghalfWidth, legHeight));
	AABB3 rightLeg = AABB3(rightLegCenter + Vec3(-leftLeghalfWidth, -0.05f, shoeHeight), rightLegCenter + Vec3(leftLeghalfWidth, leftLeghalfWidth, legHeight));

	// Body
	AABB3 body = AABB3(Vec3(-bodyHalfWidth, -bodyHalfWidth, legHeight), Vec3(bodyHalfWidth, bodyHalfWidth, legHeight + bodyHeight));

	// Head
	AABB3 head = AABB3(Vec3(-bodyHalfWidth, -bodyHalfWidth, legHeight + bodyHeight), Vec3(bodyHalfWidth, bodyHalfWidth, legHeight + bodyHeight + headHeight));

	// Hair
	float hairThickness = 0.05f;
	AABB3 hairTop(Vec3(-bodyHalfWidth - hairThickness, -bodyHalfWidth - hairThickness, legHeight + bodyHeight + headHeight),
		Vec3(bodyHalfWidth + hairThickness, bodyHalfWidth + hairThickness, legHeight + bodyHeight + headHeight + hairThickness));
	AABB3 hairLeft(Vec3(-bodyHalfWidth - hairThickness, bodyHalfWidth, legHeight + bodyHeight + 0.05f),
		Vec3(bodyHalfWidth + hairThickness, bodyHalfWidth + hairThickness, legHeight + bodyHeight + headHeight));
	AABB3 hairRight(Vec3(-bodyHalfWidth - hairThickness, -bodyHalfWidth - hairThickness, legHeight + bodyHeight + 0.05f),
		Vec3(bodyHalfWidth + hairThickness, -bodyHalfWidth, legHeight + bodyHeight + headHeight));
	AABB3 hairBack(Vec3(-bodyHalfWidth - hairThickness, -bodyHalfWidth - hairThickness, legHeight + bodyHeight + 0.05f),
		Vec3(-bodyHalfWidth, bodyHalfWidth + hairThickness, legHeight + bodyHeight + headHeight));

	// Eyes
	float eyeSize = 0.05f;
	float eyeDepth = 0.01f;
	float eyeOffsetY = 0.06f;
	float eyeHeightZ = legHeight + bodyHeight + 0.25f;

	// Left Eye
	AABB3 leftEyeWhite(Vec3(bodyHalfWidth + 0.001f, -eyeOffsetY - 0.08f, eyeHeightZ), Vec3(bodyHalfWidth + 0.001f + eyeDepth, -eyeOffsetY + eyeSize - 0.08f, eyeHeightZ + eyeSize));
	AABB3 leftEyeBlue(Vec3(bodyHalfWidth + 0.001f, -eyeOffsetY + eyeSize - 0.08f, eyeHeightZ), Vec3(bodyHalfWidth + 0.001f + eyeDepth, -eyeOffsetY + 2 * eyeSize - 0.08f, eyeHeightZ + eyeSize));

	// Right Eye
	AABB3 rightEyeBlue(Vec3(bodyHalfWidth + 0.001f, eyeOffsetY - 0.02f, eyeHeightZ), Vec3(bodyHalfWidth + 0.001f + eyeDepth, eyeOffsetY + eyeSize - 0.02f, eyeHeightZ + eyeSize));
	AABB3 rightEyeWhite(Vec3(bodyHalfWidth + 0.001f, eyeOffsetY + eyeSize - 0.02f, eyeHeightZ), Vec3(bodyHalfWidth + 0.001f + eyeDepth, eyeOffsetY + 2 * eyeSize - 0.02f, eyeHeightZ + eyeSize));

	// Nose and Mouth
	AABB3 nose(Vec3(bodyHalfWidth + 0.001f, -0.015f, legHeight + bodyHeight + 0.18f), Vec3(bodyHalfWidth + 0.015f, 0.015f, legHeight + bodyHeight + 0.24f));
	AABB3 mouth(Vec3(bodyHalfWidth + 0.001f, -0.04f, legHeight + bodyHeight + 0.12f), Vec3(bodyHalfWidth + 0.001f, 0.04f, legHeight + bodyHeight + 0.16f));

	// Arms
	float armWidth = 0.12f;
	float armHeight = 0.45f;
	AABB3 leftArm = AABB3(Vec3(-armWidth, bodyHalfWidth, shoulderZ - armHeight * 0.5f) + leftArmOffset, Vec3(armWidth, bodyHalfWidth + armWidth, shoulderZ + armHeight * 0.5f) + leftArmOffset);
	AABB3 rightArm = AABB3(Vec3(-armWidth, -bodyHalfWidth - armWidth, shoulderZ - armHeight * 0.5f) + rightArmOffset, Vec3(armWidth, -bodyHalfWidth, shoulderZ + armHeight * 0.5f) + rightArmOffset);

	// Shoulders
	float shoulderX = 0.135f;
	float shoulderThickness = 0.15f;
	float shoulderZStart = legHeight + bodyHeight - 0.1f;
	float shoulderZEnd = shoulderZStart + 0.1f;
	AABB3 leftShoulder = AABB3(Vec3(-shoulderX, bodyHalfWidth, shoulderZStart) + leftArmOffset, Vec3(shoulderX, bodyHalfWidth + shoulderThickness, shoulderZEnd) + leftArmOffset);
	AABB3 rightShoulder = AABB3(Vec3(-shoulderX, -bodyHalfWidth - shoulderThickness, shoulderZStart) + rightArmOffset, Vec3(shoulderX, -bodyHalfWidth, shoulderZEnd) + rightArmOffset);

	// Shoes
	AABB3 leftShoe = AABB3(leftLegCenter + Vec3(-leftLeghalfWidth, -0.05f, 0.f), leftLegCenter + Vec3(leftLeghalfWidth, leftLeghalfWidth, shoeHeight));
	AABB3 rightShoe = AABB3(rightLegCenter + Vec3(-leftLeghalfWidth, -0.05f, 0.f), rightLegCenter + Vec3(leftLeghalfWidth, leftLeghalfWidth, shoeHeight));

	AddVertsForAABB3D(playerVerts, body, Rgba8::CYAN);
	AddVertsForAABB3D(playerVerts, head, Rgba8::SKIN);
	AddVertsForAABB3D(playerVerts, leftEyeWhite, Rgba8::WHITE);
	AddVertsForAABB3D(playerVerts, leftEyeBlue, Rgba8::BLUE);
	AddVertsForAABB3D(playerVerts, rightEyeWhite, Rgba8::WHITE);
	AddVertsForAABB3D(playerVerts, rightEyeBlue, Rgba8::BLUE);
	AddVertsForAABB3D(playerVerts, nose, Rgba8(200, 160, 120));
	AddVertsForAABB3D(playerVerts, mouth, Rgba8(255, 180, 180));
	AddVertsForAABB3D(playerVerts, hairTop, Rgba8::DARKBROWN);
	AddVertsForAABB3D(playerVerts, hairLeft, Rgba8::DARKBROWN);
	AddVertsForAABB3D(playerVerts, hairRight, Rgba8::DARKBROWN);
	AddVertsForAABB3D(playerVerts, hairBack, Rgba8::DARKBROWN);
	AddVertsForRotatedAABB3D(playerVerts, leftShoulder, -walkAngleArms, leftShoulderPivot, Rgba8::CYAN);
	AddVertsForRotatedAABB3D(playerVerts, rightShoulder, walkAngleArms, rightShoulderPivot, Rgba8::CYAN);
	AddVertsForRotatedAABB3D(playerVerts, leftArm, -walkAngleArms, leftShoulderPivot, Rgba8::SKIN);
	AddVertsForRotatedAABB3D(playerVerts, rightArm, walkAngleArms, rightShoulderPivot, Rgba8::SKIN);
	AddVertsForRotatedAABB3D(playerVerts, leftLeg, walkAngleLegs, leftHipPivot, Rgba8::BLUE);
	AddVertsForRotatedAABB3D(playerVerts, rightLeg, -walkAngleLegs, rightHipPivot, Rgba8::BLUE);
	AddVertsForRotatedAABB3D(playerVerts, leftShoe, walkAngleLegs, leftHipPivot, Rgba8::SLATEGRAY);
	AddVertsForRotatedAABB3D(playerVerts, rightShoe, -walkAngleLegs, rightHipPivot, Rgba8::SLATEGRAY);

	g_theRenderer->SetModelConstants(GetModelToWorldTransform());
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(playerVerts);
}

Vec3 Player::GetForwardNormal() const
{
	return Vec3::MakeFromPolarDegrees(m_orientation.m_pitchDegrees, m_orientation.m_yawDegrees, 1.f);
}

Vec3 Player::GetPlayerPosition() const
{
	return m_position;
}

Vec3 Player::GetEyePosition() const
{
	return m_position + Vec3(0.f, 0.f, 1.65f);
}

EulerAngles Player::GetPlayerOrientation() const
{
	return m_orientation;
}

PhysicsMode Player::GetPlayerPhysicsMode() const
{
	return m_physicsMode;
}

uint8_t Player::GetSelectedBlockInSlot() const
{
	return m_inventory[m_selectedInventorySlot];
}

void Player::CyclePhysicsMode()
{
	int physicsMode = static_cast<int>(m_physicsMode);
	physicsMode = (physicsMode + 1) % static_cast<int>(PhysicsMode::NUM_PHYSICS_MODES);
	m_physicsMode = static_cast<PhysicsMode>(physicsMode);
}

void Player::CameraKeyPresses(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	// Block digging and placing
	if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
	{
		Vec3 rayStart = GetEyePosition();
		Vec3 rayDir = GetForwardNormal();
		float maxDist = 100.f;

		GameRaycastResult3D hit = m_theGame->m_currentWorld->RaycastVsBlocks(rayStart, rayDir, maxDist);
		if (hit.m_didImpact)
		{
			IntVec3 coords = hit.m_impactedBlockIterator.GetBlockCoords();
			uint8_t blockType = m_theGame->m_currentWorld->GetBlockTypeAtCoords(coords);

			if (blockType != BLOCKTYPE_AIR)
			{
				AddBlockToInventory(blockType);
				m_theGame->m_currentWorld->SetBlockTypeAtCoords(coords, BLOCKTYPE_AIR);
			}
		}
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE))
	{
		Vec3 rayStart = GetEyePosition();
		Vec3 rayDir = GetForwardNormal();
		float maxDist = 100.f;

		GameRaycastResult3D hit = m_theGame->m_currentWorld->RaycastVsBlocks(rayStart, rayDir, maxDist);

		if (hit.m_didImpact)
		{
			IntVec3 hitCoords = hit.m_impactedBlockIterator.GetBlockCoords();
			IntVec3 placeBlockCoords = hitCoords + IntVec3(RoundDownToInt(hit.m_impactNormal.x), RoundDownToInt(hit.m_impactNormal.y), RoundDownToInt(hit.m_impactNormal.z));

			uint8_t existingBlock = m_theGame->m_currentWorld->GetBlockTypeAtCoords(placeBlockCoords);
			if (existingBlock == BLOCKTYPE_AIR)
			{
				uint8_t blockToPlace = GetSelectedBlockInSlot();

				if (blockToPlace != BLOCKTYPE_AIR && RemoveBlockFromSelectedSlot())
				{
					m_theGame->m_currentWorld->SetBlockTypeAtCoords(placeBlockCoords, blockToPlace);
				}
			}
		}
	}

	// Number keys 1–9 select slots 0–8
	for (int slotIndex = 0; slotIndex < 9; ++slotIndex)
	{
		if (g_theInput->WasKeyJustPressed('1' + static_cast<unsigned char>(slotIndex)))
		{
			m_selectedInventorySlot = slotIndex;
		}
	}

	// Zero key selects slot 9
	if (g_theInput->WasKeyJustPressed('0'))
	{
		m_selectedInventorySlot = 9;
	}

	// Inventory mouse scrolling
	if (g_theInput->WasMouseWheelScrolledUp())
	{
		m_selectedInventorySlot--;
		if (m_selectedInventorySlot < 0)
		{
			m_selectedInventorySlot = 9;
		}
	}
	else if (g_theInput->WasMouseWheelScrolledDown())
	{
		m_selectedInventorySlot++;
		if (m_selectedInventorySlot > 9)
		{
			m_selectedInventorySlot = 0;
		}
	}

	// Empty current inventory slot
	if (g_theInput->WasKeyJustPressed('X'))
	{
		int slot = m_selectedInventorySlot;
		m_inventory[slot] = BLOCKTYPE_AIR;
		m_inventoryCounts[slot] = 0;
	}

	// Empty full inventory
	if (g_theInput->WasKeyJustPressed('Z'))
	{
		for (int inventoryIndex = 0; inventoryIndex < INVENTORY_SIZE; ++inventoryIndex)
		{
			m_inventory[inventoryIndex] = BLOCKTYPE_AIR;
			m_inventoryCounts[inventoryIndex] = 0;
		}
	}
}

void Player::CameraControllerPresses(float deltaSeconds)
{
	XboxController const& controller = g_theInput->GetController(0);
	float movementSpeed = 4.f;

	// Increase speed by a factor of 10
	if (controller.GetRightTrigger())
	{
		movementSpeed *= 20.f;
	}

	// Move left, right, forward, and backward
	if (controller.GetLeftStick().GetMagnitude() > 0.f)
	{
		m_position += (-movementSpeed * controller.GetLeftStick().GetPosition().x * m_orientation.GetAsMatrix_IFwd_JLeft_KUp().GetJBasis3D() * deltaSeconds);
		m_position += (movementSpeed * controller.GetLeftStick().GetPosition().y * m_orientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D() * deltaSeconds);
	}

	// Move Up and Down
	if (controller.IsButtonDown(XBOX_BUTTON_LSHOULDER))
	{
		m_position += -movementSpeed * Vec3::ZAXE * deltaSeconds;
	}
	if (controller.IsButtonDown(XBOX_BUTTON_RSHOULDER))
	{
		m_position += movementSpeed * Vec3::ZAXE * deltaSeconds;
	}
}

void Player::HandlePlayerMovement(float deltaSeconds)
{
	// Yaw and Pitch with mouse
	m_orientation.m_yawDegrees += 0.075f * g_theInput->GetCursorClientDelta().x;
	m_orientation.m_pitchDegrees -= 0.075f * g_theInput->GetCursorClientDelta().y;

	float movementSpeed = 4.f;

	if (g_theInput->IsKeyDown(KEYCODE_SHIFT))
	{
		movementSpeed *= 20.f;
	}

	Mat44 matrix = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
	Vec3 forward = matrix.GetIBasis3D();
	Vec3 left = matrix.GetJBasis3D();

	Vec3 desiredDirection = Vec3::ZERO;
	if (g_theInput->IsKeyDown('W')) desiredDirection += forward;
	if (g_theInput->IsKeyDown('S')) desiredDirection -= forward;
	if (g_theInput->IsKeyDown('A')) desiredDirection += left;
	if (g_theInput->IsKeyDown('D')) desiredDirection -= left;

	if (desiredDirection.GetLengthSquared() > 0.f)
	{
		desiredDirection.Normalize();
	}

	float acceleration = 0.f;
	if (m_isGrounded)
	{
		acceleration = m_groundAcceleration;
	}
	else
	{
		acceleration = m_airAcceleration;
	}

	// Apply XY acceleration
	m_velocity.x += desiredDirection.x * acceleration * deltaSeconds;
	m_velocity.y += desiredDirection.y * acceleration * deltaSeconds;

	if (m_physicsMode == PhysicsMode::FLYING || m_physicsMode == PhysicsMode::NOCLIP)
	{
		m_velocity.z += desiredDirection.z * acceleration * deltaSeconds;
	}

	// Clamp XY speed
	Vec2 velocityXY = m_velocity.GetXY();
	if (velocityXY.GetLength() > m_maxSpeedXY)
	{
		velocityXY.SetLength(m_maxSpeedXY);
		m_velocity.x = velocityXY.x;
		m_velocity.y = velocityXY.y;
	}

	// Jumping
	if (m_physicsMode == PhysicsMode::WALKING && m_isGrounded && g_theInput->WasKeyJustPressed(KEYCODE_SPACE))
	{
		m_velocity.z = m_jumpImpulse;
		m_isGrounded = false;
	}
}

bool Player::PlaceBlockBelowPosition(uint8_t newBlockType)
{
	IntVec3 solidBlock = GetFirstSolidBlockBelow();
	IntVec3 blockPosition = IntVec3(solidBlock.x, solidBlock.y, solidBlock.z + BLOCK_SIZE);

	if (blockPosition.z >= CHUNK_SIZE_Z)
	{
		return false;
	}

	uint8_t oldBlockType = m_theGame->m_currentWorld->GetBlockTypeAtCoords(blockPosition);
	if (oldBlockType != BLOCKTYPE_AIR)
	{
		return false;
	}

	bool success = m_theGame->m_currentWorld->SetBlockTypeAtCoords(blockPosition, newBlockType);
	return success;
}

IntVec3 Player::GetFirstSolidBlockBelow() const
{
	IntVec3 playerPosition = m_theGame->m_currentWorld->GetChunkCoordsFromPosition(m_position);

	for (int blockStepSize = playerPosition.z; blockStepSize >= 0; blockStepSize -= 1)
	{
		IntVec3 solidBlockPosition(playerPosition.x, playerPosition.y, blockStepSize);
		uint8_t blockType = m_theGame->m_currentWorld->GetBlockTypeAtCoords(solidBlockPosition);

		if (blockType != BLOCKTYPE_AIR)
		{
			return solidBlockPosition;
		}
	}
	return IntVec3::INVALID;
}

bool Player::AddBlockToInventory(uint8_t blockType)
{
	// Try to add to existing stack
	for (int inventoryIndex = 0; inventoryIndex < INVENTORY_SIZE; ++inventoryIndex)
	{
		if (m_inventory[inventoryIndex] == blockType && m_inventoryCounts[inventoryIndex] < MAX_STACK_IN_SLOT)
		{
			m_inventoryCounts[inventoryIndex] += 1;
			return true;
		}
	}

	// Try to place in existing slot
	for (int inventoryIndex = 0; inventoryIndex < INVENTORY_SIZE; ++inventoryIndex)
	{
		if (m_inventory[inventoryIndex] == BLOCKTYPE_AIR)
		{
			m_inventory[inventoryIndex] = blockType;
			m_inventoryCounts[inventoryIndex] = 1;
			return true;
		}
	}

	// Inventory is full!
	return false;
}

bool Player::RemoveBlockFromSelectedSlot()
{
	int slot = m_selectedInventorySlot;

	// Check if there is nothing to remove
	if (m_inventory[slot] == BLOCKTYPE_AIR || m_inventoryCounts[slot] == 0)
	{
		return false;
	}

	m_inventoryCounts[slot] -= 1;

	if (m_inventoryCounts[slot] <= 0)
	{
		m_inventory[slot] = BLOCKTYPE_AIR;
		m_inventoryCounts[slot] = 0;
	}

	return true;
}
