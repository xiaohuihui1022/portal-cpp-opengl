﻿#include "Portal.h"
#include <iostream>

#define _USE_MATH_DEFINES
#include <math.h>

#include <glm/gtc/matrix_transform.hpp>

#include "LevelConstants.h"

using namespace portal;
using namespace portal::physics;
using namespace portal::level;

namespace
{
	const float PORTAL_FRAME_WIDTH = 19.f;
	const float PORTAL_FRAME_HEIGHT = 16.f;
	const float PORTAL_GUT_WIDTH = 4.5f;
	const float PORTAL_GUT_HEIGHT = 6.8f;

	std::vector<Vertex>
	generate_portal_frame()
	{
		return {
			{ { -PORTAL_FRAME_WIDTH / 2.f, -PORTAL_FRAME_HEIGHT / 2.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 0.f }, { 0.f, 0.f, 1.f } },
			{ { -PORTAL_FRAME_WIDTH / 2.f,  PORTAL_FRAME_HEIGHT / 2.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f }, { 0.f, 0.f, 1.f } },
			{ { PORTAL_FRAME_WIDTH / 2.f,  -PORTAL_FRAME_HEIGHT / 2.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f, 1.f } },
			{ { PORTAL_FRAME_WIDTH / 2.f,  -PORTAL_FRAME_HEIGHT / 2.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f, 1.f } },
			{ { -PORTAL_FRAME_WIDTH / 2.f,  PORTAL_FRAME_HEIGHT / 2.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 0.f, 1.f }, { 0.f, 0.f, 1.f } },
			{ { PORTAL_FRAME_WIDTH / 2.f,   PORTAL_FRAME_HEIGHT / 2.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, { 1.f, 1.f }, { 0.f, 0.f, 1.f } },
		};
	}

	const int ELLIPSE_NUM_SIDES = 20;
	std::vector<Vertex>
	generate_portal_ellipse_hole( float radius_x, float radius_y )
	{
		const glm::vec4 black{ 0.f, 0.f, 0.f, 1.f };
		const glm::vec2 fake_uv( 0.f, 0.f );
		const glm::vec3 normal{ 0.f, 0.f, 1.f };

		constexpr int num_vertices = ELLIPSE_NUM_SIDES + 2;
		const float two_pi = 2.f * static_cast<float>( M_PI );

		std::vector<Vertex> vertices;
		vertices.reserve( num_vertices );
		vertices.emplace_back( Vertex{ { 0.f, 0.f, 0.f }, black, fake_uv, normal } );
		
		for( int i = 0; i < num_vertices; i++ )
		{
			float rad = (num_vertices - i) * two_pi / ELLIPSE_NUM_SIDES;
			vertices.emplace_back( Vertex{
				{ cos( rad ) * radius_x, sin( rad ) * radius_y, 0.f },
				black, fake_uv, normal
			});
		}
		return vertices;
	}

	const float PORTAL_FRAME_UP_OFFSET = 2 * PORTAL_GUT_HEIGHT;
	const float PORTAL_FRAM_RIGHT_OFFSET = 2 * PORTAL_GUT_WIDTH;
}

Portal::Portal( TextureInfo* texture, physics::Physics& physics )
	: mFaceDir( 0.f, 0.f, 1.f )
	, mPosition( 10.f, 10.f, 10.f )
	, mOriginFaceDir( 0.f, 0.f, 1.f )
	, mUpDir( 0.f, 1.f, 0.f )
	, mRightDir( -1.f, 0.f, 0.f )
	, mFrameRenderable( generate_portal_frame(), Renderer::PORTAL_FRAME_SHADER, texture )
	, mHoleRenderable( generate_portal_ellipse_hole( PORTAL_GUT_WIDTH, PORTAL_GUT_HEIGHT ), Renderer::PORTAL_HOLE_SHADER, nullptr, Renderer::Renderable::DrawType::TRIANGLE_FANS )
	, mHasBeenPlaced( false )
	, mPairedPortal( nullptr )
{
	// 生成门框
	mFrameBoxes.emplace_back(
		physics.CreateBox( 
			mPosition + mUpDir * PORTAL_FRAME_UP_OFFSET,
			{ 6 * PORTAL_GUT_WIDTH, 2 * PORTAL_GUT_HEIGHT, 0.1f }, 
			Physics::PhysicsObject::Type::STATIC, 
			static_cast<int>( PhysicsGroup::PORTAL_FRAME ),
			static_cast<int>( PhysicsGroup::PLAYER ) ) );
	mFrameBoxes.emplace_back(
		physics.CreateBox( 
			mPosition +  mUpDir * -PORTAL_FRAME_UP_OFFSET,
			{ 6 * PORTAL_GUT_WIDTH, 2 * PORTAL_GUT_HEIGHT, 0.1f }, 
			Physics::PhysicsObject::Type::STATIC, 
			static_cast<int>( PhysicsGroup::PORTAL_FRAME ),
			static_cast<int>( PhysicsGroup::PLAYER ) ) );
	mFrameBoxes.emplace_back(
		physics.CreateBox( 
			mPosition + mRightDir * -PORTAL_FRAM_RIGHT_OFFSET,
			{ 2 * PORTAL_GUT_WIDTH, 2 * PORTAL_GUT_HEIGHT, 0.1f }, 
			Physics::PhysicsObject::Type::STATIC, 
			static_cast<int>( PhysicsGroup::PORTAL_FRAME ),
			static_cast<int>( PhysicsGroup::PLAYER ) ) );
	mFrameBoxes.emplace_back(
		physics.CreateBox( 
			mPosition + mRightDir * PORTAL_FRAM_RIGHT_OFFSET,
			{ 2 * PORTAL_GUT_WIDTH, 2 * PORTAL_GUT_HEIGHT, 0.1f }, 
			Physics::PhysicsObject::Type::STATIC, 
			static_cast<int>( PhysicsGroup::PORTAL_FRAME ),
			static_cast<int>( PhysicsGroup::PLAYER ) ) );
}

Portal::~Portal()
{}

void 
Portal::SetPair( Portal* paired_portal )
{
	mPairedPortal = paired_portal;
}

bool 
Portal::PlaceAt( glm::vec3 pos, glm::vec3 dir )
{
	if( pos == mPosition && dir == mFaceDir )
	{
		return false;
	}

	// 求它们之间的夹角
	float theta = std::acos( glm::dot( mOriginFaceDir, dir ) );
	// 求垂直于它们的向量用作转轴
	glm::vec3 up_axis = glm::cross( mOriginFaceDir, dir );
	up_axis = glm::normalize( up_axis );

	mPosition = pos;
	mFaceDir = dir;
	mUpDir = up_axis;
	mRightDir = glm::normalize( glm::cross( mFaceDir, mUpDir ) );

	std::cout << "Hit nor (" << dir.x << ", " << dir.y << ", " << dir.z << ")\n";
	std::cout << "mPosition (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
	std::cout << "mFaceDir (" << mFaceDir.x << ", " << mFaceDir.y << ", " << mFaceDir.z << ")\n";
	std::cout << "mUpDir (" << mUpDir.x << ", " << mUpDir.y << ", " << mUpDir.z << ")\n";
	std::cout << "mRightDir (" << mRightDir.x << ", " << mRightDir.y << ", " << mRightDir.z << ")\n";

	// 根据上面求得的位置和旋转变量来更新门口和门面的模型矩阵
	mFrameRenderable.Translate( pos + mFaceDir * 0.2f );
	mFrameRenderable.Rotate( theta, up_axis );
	mHoleRenderable.Translate( pos + mFaceDir * 0.1f );
	mHoleRenderable.Rotate( theta, up_axis );

	mHasBeenPlaced = true;
	
	// 确保门框也做同样的位移和旋转
	for( size_t i = 0; i < mFrameBoxes.size(); i ++)
	{
		glm::mat4 trans( 1.f );
		switch(i)
		{
		case 0:
			trans = glm::translate( trans, pos + mUpDir * PORTAL_FRAME_UP_OFFSET );
			break;
		case 1:
			trans = glm::translate( trans, pos + mUpDir * -PORTAL_FRAME_UP_OFFSET );
			break;
		case 2:
			trans = glm::translate( trans, pos + mRightDir * -PORTAL_FRAM_RIGHT_OFFSET );
			break;
		case 3:
			trans = glm::translate( trans, pos + mRightDir * PORTAL_FRAM_RIGHT_OFFSET );
			break;
		default:
			// You are fucked if it hits here.
			break;
		}
		trans = glm::rotate( trans, theta, up_axis );

		mFrameBoxes[i]->SetTransform( std::move( trans ) );
	}

	return true;
}

Renderer::Renderable*
Portal::GetFrameRenderable()
{
	return &mFrameRenderable;
}

Renderer::Renderable* 
Portal::GetHoleRenderable()
{
	return &mHoleRenderable;
}

bool 
Portal::HasBeenPlaced()
{
	return mHasBeenPlaced;
}

bool
Portal::IsLinkActive()
{
	if( mPairedPortal )
	{
		return mHasBeenPlaced && mPairedPortal->HasBeenPlaced();
	}
	return false;
}

Portal*
Portal::GetPairedPortal()
{
	return mPairedPortal;
}

glm::vec3 
Portal::GetPosition()
{
	return mPosition;
}

glm::mat4 
Portal::ConvertView( const glm::mat4& view_matrix )
{
	if( !mPairedPortal )
	{
		std::cerr << "ERROR: Trying to convert portal view while the paired portal is invalid." << std::endl;
		return view_matrix;
	}
	// 先将视图矩阵转换到本传送门的本地空间
	glm::mat4 model_view = view_matrix * mHoleRenderable.GetTransform();
	glm::mat4 final_view = model_view
						   * glm::rotate( glm::mat4( 1.f ), glm::radians( 180.f ), glm::vec3( 0.f, 1.f, 0.f ) )
						   * glm::inverse( mPairedPortal->GetHoleRenderable()->GetTransform() );
	return final_view;
}
