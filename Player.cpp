﻿#include "Player.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "Camera.h"
#include "LevelConstants.h"
#include "Portal.h"
#include "Utility.h"

using namespace portal;
using namespace portal::physics;
using namespace portal::level;

namespace
{
	const float PLAYER_CAPSULE_RAIDUS = 1.8f;
	const float PLAYER_CAPSULE_HEIGHT = 5.f;
	const float PLAYER_GROUND_DAMPING = 0.9f;
	const float PLAYER_AIR_DAMPING = 0.1f;
	const float PLAYER_MAX_VELOCITY = 20.f;
	const float PLAYER_JUMP_FORCE = 2.f;
	const float PLAYER_CAMERA_OFFSET = 4.f;

	const size_t PORTAL_1 = 0;
	const size_t PORTAL_2 = 1;

	float clip( float n, float lower, float upper )
	{
		return std::max( lower, std::min( n, upper ) );
	}
}

Player::Player( Physics& physics )
	: mPhysics( physics )
	, mIsActive( false )
	, mIsGrounded( false )
	, mPreviousUpdateTime( std::chrono::steady_clock::now() )
	, mFallingAccumulatedSec( 0.f )
	, mDownCastHitNumber( 0 )
	, mMoveVelocity( 0.f )
	, mMoveDirection( 0.f )
	, mIsRunning( false )
{}

Player::~Player()
{}

void 
Player::Spawn( glm::vec3 position, std::shared_ptr<Camera> camera )
{
	mMainCamera = std::move( camera );
	mMainCamera->SetPosition( { position.x, position.y + 4.f, position.z } );
	mCollisionCapsule = mPhysics.CreateCapsule( 
		position, 
		PLAYER_CAPSULE_RAIDUS, PLAYER_CAPSULE_HEIGHT, 
		Physics::PhysicsObject::Type::DYNAMIC,
		static_cast<int>( PhysicsGroup::PLAYER ),
		static_cast<int>( PhysicsGroup::WALL ) | static_cast<int>( PhysicsGroup::PORTAL_FRAME )
	);
	mCollisionCapsule->SetAngularFactor( { 0.f, 0.f, 0.f } );
	mCollisionCapsule->SetDamping( PLAYER_AIR_DAMPING, 0.f );
	mIsActive = true;
	mPreviousUpdateTime = std::chrono::steady_clock::now();
	mPortalInfo.emplace_back( PortalInfo{} );
	mPortalInfo.emplace_back( PortalInfo{} );

	mCollisionCapsule->GetCollisionObject()->setUserPointer( static_cast<void*>( this ) );
}

void
Player::Update()
{
	if( !mIsActive )
	{
		return;
	}

	auto current_time = std::chrono::steady_clock::now();
	float delta_seconds = std::chrono::duration<float, std::milli>( current_time - mPreviousUpdateTime ).count() / 1000.f;
	mPreviousUpdateTime = current_time;

	auto pos = mCollisionCapsule->GetPosition();
	mMainCamera->SetPosition( { pos.x, pos.y + PLAYER_CAMERA_OFFSET, pos.z } );

	//CastGroundCheckRay( pos );
	/*if( !mIsGrounded && !mIsRunning )
	{
		mCollisionCapsule->SetDamping( PLAYER_AIR_DAMPING, 0.f );
	}*/
}

void 
Player::HandleKeys( std::unordered_map<unsigned int, bool>& key_map )
{
	if( !mIsActive )
	{
		return;
	}

	if( mMainCamera /*&& mIsGrounded*/ )
	{
		glm::vec3 forward_dir = mMainCamera->GetFrontDirection();
		glm::vec3 right_dir = mMainCamera->GetRightDirection();
		if( key_map[ 'w' ] )
		{
			mMoveDirection += forward_dir;
		}
		if( key_map[ 's' ] )
		{
			mMoveDirection -= forward_dir;
		}
		if( key_map[ 'a' ] )
		{
			mMoveDirection -= right_dir;
		}
		if( key_map[ 'd' ] )
		{
			mMoveDirection += right_dir;
		}
		if( key_map[ ' ' ] )
		{
			mCollisionCapsule->SetImpluse( { 0.f, PLAYER_JUMP_FORCE, 0.f }, mCollisionCapsule->GetPosition() );
		}

		mIsRunning = key_map[ 'w' ] || key_map[ 's' ] || key_map[ 'a' ] || key_map[ 'd' ];

		if( mIsRunning )
		{
			mCollisionCapsule->Activate();
			mCollisionCapsule->SetDamping( 0.f, 0.f );
			mMoveDirection = mMoveDirection == glm::vec3{ 0.f } ? mMoveDirection :glm::normalize( mMoveDirection );
			mMoveVelocity = mMoveDirection * PLAYER_MAX_VELOCITY;
			mCollisionCapsule->SetLinearVelocity(
				{
					mMoveVelocity.x,
					mCollisionCapsule->GetLinearVelocity().y,
					mMoveVelocity.z
				}
			);
		}
		else
		{
			mCollisionCapsule->SetDamping( PLAYER_GROUND_DAMPING, 0.9f );
		}
	}
}

void 
Player::HandleMouse( std::unordered_map<int, bool>& button_map, Portal& portal_left, Portal& portal_right )
{
	if( mMouseLeftPressed != button_map[ 1 ] )
	{
		mMouseLeftPressed = button_map[ 1 ];
		if( mMouseLeftPressed )
		{
			auto look_dir = mMainCamera->GetLookDirection();
			look_dir *= 1000.f;
			mPhysics.CastRay( 
				mMainCamera->GetPosition(), look_dir, 
				static_cast<int>( PhysicsGroup::RAY ),
				[&, this]( bool is_hit, glm::vec3 hit_point, glm::vec3 hit_normal, const btCollisionObject* obj )
				{
					if( is_hit )
					{
						portal_left.PlaceAt( hit_point, hit_normal, mCollisionCapsule.get(), obj );
					}
				}
			);
		}
	}

	if( mMouseRightPressed != button_map[ 2 ] )
	{
		mMouseRightPressed = button_map[ 2 ];
		if( mMouseRightPressed )
		{
			auto look_dir = mMainCamera->GetLookDirection();
			look_dir *= 1000.f;
			mPhysics.CastRay( 
				mMainCamera->GetPosition(), look_dir, 
				static_cast<int>( PhysicsGroup::RAY ),
				[&, this]( bool is_hit, glm::vec3 hit_point, glm::vec3 hit_normal, const btCollisionObject* obj )
				{
					if( is_hit )
					{
						portal_right.PlaceAt( hit_point, hit_normal, mCollisionCapsule.get(), obj );
					}
				}
			);
		}
	}
}

void
Player::Look( float yaw_angle, float pitch_angle )
{
	if( !mIsActive )
	{
		return;
	}

	mMainCamera->Look( yaw_angle, pitch_angle );
}

const std::vector<Player::PortalInfo>& 
Player::GetPortalInfo() const
{
	return mPortalInfo;
}

void 
Player::Teleport( glm::vec3 new_pos, glm::vec3 face_dir, glm::mat4 src_trans, glm::mat4 dst_trans )
{
	//glm::mat4 warpped_view = Portal::ConvertView( mMainCamera->GetViewMatrix(), src_trans, dst_trans );
	//glm::vec3 pos = utility::extract_view_postion_from_matrix( warpped_view );
	//glm::vec3 cam_look_at = { warpped_view[0].y, warpped_view[1].y, warpped_view[2].y };
	//glm::vec3 cam_look_dir = mMainCamera->GetLookDirection();
	//std::cout << "Prev look (" << cam_look_dir.x << ", " << cam_look_dir.y << ", " << cam_look_dir.z << ")" << std::endl;
	//std::cout << "Now Look to (" << cam_look_at.x << ", " << cam_look_at.y << ", " << cam_look_at.z << ")" << std::endl;
	//float theta = std::acos( glm::dot( cam_look_dir, cam_look_at ) );
	//theta *= 180.f/3.1415926f;
	///*glm::vec3 prev_v = glm::transpose( glm::inverse( dst_trans ) ) * glm::vec4( mCollisionCapsule->GetLinearVelocity(), 1.f );
	//float theta = std::acos( glm::dot( prev_v, face_dir ) );
	//glm::vec3 p_axis = glm::normalize( glm::cross( prev_v, face_dir ) );
	//glm::vec3 plane = glm::cross( p_axis, prev_v );
	//glm::vec3 new_v = cos( theta ) * prev_v + sin( theta ) * plane;
	//mCollisionCapsule->SetLinearVelocity( std::move( new_v ) );*/

	////glm::vec3 cam_front = mMainCamera->GetFrontDirection();
	////cam_front = glm::transpose( glm::inverse( dst_trans ) ) * glm::vec4( cam_front, 1.0 );
	//////cam_front = glm::rotate( glm::mat4( 1.f ), glm::radians( 180.f ), glm::vec3( 0.f, 1.f, 0.f ) ) * glm::inverse( dst_trans ) * glm::vec4( cam_front, 1.0 );
	////cam_front = glm::normalize( cam_front );
	////std::cout << "Look at (" << cam_front.x << ", " << cam_front.y << ", " << cam_front.z << ")" << std::endl;
	//
	//std::cout << "Cam turned: " << theta << " degree.\n";
	//Look( theta, 0 );
	////mCollisionCapsule->SetPosition( std::move( new_pos ) );
	//std::cout << "Warp to (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
	//pos. y -= PLAYER_CAMERA_OFFSET;
	//pos += face_dir * 0.1f;
	//mCollisionCapsule->SetPosition( std::move( pos ) );
}

void 
Player::CastGroundCheckRay( glm::vec3 pos )
{
	// 当玩家胶囊发生碰撞时，我们在玩家头顶高一点点的位置往正下方发射一根射线。
	// 射线会先击中玩家本身的胶囊，然后继续前进9个单位。
	// 如果它继续击中另一个物体，表示玩家站在一个东西上，看下面OnGroundRayHit
	glm::vec3 from{ 
		pos.x, 
		pos.y,
		pos.z
	};
	glm::vec3 to = from;
	to.y -= PLAYER_CAPSULE_HEIGHT / 2.f + PLAYER_CAPSULE_RAIDUS + 1.f;
	mPhysics.CastRay( 
		std::move( from ), std::move( to ), 
		static_cast<int>( PhysicsGroup::RAY ),
		[this]( bool is_hit, glm::vec3, glm::vec3, const btCollisionObject* )
		{
			mIsGrounded = is_hit;
		}
	);
}
