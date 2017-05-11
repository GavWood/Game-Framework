////////////////////////////////////////////////////////////////////////////////
// SbModel.cpp

// Includes
#include "BaArchive.h"
#include "SbModel.h"
#include "MtVector2.h"
#include "RsCamera.h"
#include "RsColour.h"
#include "RsFont.h"
#include "RsShader.h"
#include "RsUtil.h"
#include "RsRenderTarget.h"
#include "SgNode.h"
#include "MtAABB.h"
#include "SgMesh.h"
#include "SgAnimator.h"
#include "BtTime.h"
#include <stdio.h>
#include "UiKeyboard.h"
#include "HlDraw.h"
#include "ApConfig.h"
#include "SgBone.h"
#include "HlModel.h"
#include "SbWorld.h"

////////////////////////////////////////////////////////////////////////////////
// Setup

void SbModel::Setup( BaArchive *pGameArchive, BaArchive *pAnimArchive )
{
	// Cache our model
	m_pFish = pGameArchive->GetNode( "render_model" );
	//m_pCube = pGameArchive->GetNode("cube");
	//HlModel::SetSortOrders(m_pCube, ModelSortOrder);
	HlModel::SetSortOrders(m_pFish, ModelSortOrder);

	m_pAnimator = pAnimArchive->GetAnimator("swim");

	// Cache the main shader
	m_pShader = pGameArchive->GetShader( "shader" );

	m_time = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Update

void SbModel::Update()
{
	MtMatrix4 m4Transform;
	m4Transform.SetIdentity();

	if (m_pCube)
	{
		m4Transform.SetTranslation(MtVector3(0, 0, 2));
		m_pCube->SetLocalTransform(m4Transform);
		m_pCube->Update();
	}
	if (m_pFish)
	{
		BtFloat scale = 1.0f;
		MtMatrix4 m4Scale;
		m4Scale.SetScale( MtVector3(scale, scale, scale) );
		m_pFish->SetLocalTransform(m4Transform);
		MtMatrix4 m4Transform;
		m4Transform.SetTranslation(MtVector3(0, 0, 0 ));
		m_pFish->SetLocalTransform(m4Scale * m4Transform);
		m_pFish->Update();
	}
	if (m_pAnimator)
	{
		if( !ApConfig::IsPaused() )
		{
			m_time += BtTime::GetTick();

			BtFloat frameRate = m_pAnimator->GetFrameRate();
			(void)frameRate;

			BtU32 numFrames = m_pAnimator->GetNumFrames();
			(void)numFrames;

			BtFloat duration = m_pAnimator->GetDuration();

			BtU32 frameIndex = (BtU32)m_time;
			if (frameIndex >= duration)
			{
				m_time = 0;
			}
			m_pAnimator->SetTime(m_time);
			m_pAnimator->Update(m_pFish->pSkin());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Render

void SbModel::Render()
{
	// Set the light direction
	MtVector3 v3LightDirection( -1, -1, 0 );

	m_pShader->SetDirectionalLight( v3LightDirection );

	// Set the lights ambient level
	m_pShader->SetAmbient( RsColour( 0.5f, 0.4f, 0.3f, 0 ) );

	// Apply the shader
	m_pShader->Apply();

	if (m_pFish )
	{
		m_pFish->Render();
	}
	if (m_pCube)
	{
		m_pCube->Render();
	}
}