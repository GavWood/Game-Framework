////////////////////////////////////////////////////////////////////////////////
// MuMumuration.cpp

// Includes
#include "RsVertex.h"
#include "RsTexture.h"
#include "BaArchive.h"
#include "RsMaterial.h"
#include "SgNode.h"
#include "UiKeyboard.h"
#include "Ui360.h"
#include "RsShader.h"
#include "MuMurmuration.h"
#include "BtTime.h"
#include "HlFont.h"
#include "RsRenderTarget.h"
#include "HlModel.h"
#include "RsUtil.h"
#include "HlDraw.h"
#include "BtArray.h"
#include "ApConfig.h"
#include "HlDebug.h"
#include "MtMath.h"
#include "MtAABB.h"
#include "ErrorLog.h"
#include <stdio.h>
#include <vector>

#include "nanoflann.hpp"

// starling speed murmuration 
// traveling speed 50-65
// foraging speed 30-40

// Pre-requisits
const BtFloat WorldSize = 128;

const BtU32 MaxBoids = 256.0f;// 2048.0f;
BtS32 NumBoids = MaxBoids;

const BtU32 MaxPredators = 4;
BtS32 NumPredators = MaxPredators;

MtAABB aabb;
const BtU32 MaxVerts = MaxBoids + MaxPredators;
RsVertex3 myVertex[MaxVerts * 3];
MtVector3 v3Centre( 0, 0, 0 );
const BtU32 MaxNeighbours = 7;

// ------------------------------ Variables --------------------------------
BtFloat SeparationFactor = 0;
BtFloat NeighbourAlignFactor = 0;
BtFloat CohesionFactor = 0;

BtFloat StarlingWingSpan = 0.37f * 4;					// 37 to 42cm
BtFloat KestrelWingSpan = 0.82f * 4;					// 65 to 82cm

BtFloat dt = 1.0f / 60.0f;
BtFloat LocalTargetFactor = 0;						// unused for now
BtFloat GlobalTargetFactor = 0;

BtFloat MinSpeed = 0.0f;
BtFloat MaxSpeed = 18.0f;

BtFloat PredatorAvoidedFactor = 0;
BtFloat PredatorAvoidDistance = 0;
BtFloat PredatorAttractedFactor = 0;

//-------------------------------------------------------------

// Global variables to place the birds
SbKestrel g_predators[MaxPredators];
SbStarling g_flock[MaxBoids];

//-------------------------------------------------------------
// KD Tree

extern void doKDTree(SbStarling *pFlock, int numBoids);

////////////////////////////////////////////////////////////////////////////////
// Setup

void SbMurmuration::Setup( BaArchive *pArchive )
{
	m_pShader = pArchive->GetShader( "shader" );
	m_pWhite3 = pArchive->GetMaterial( "white3" );
	m_pBird3  = pArchive->GetMaterial("bird3");				// The texture for the bird

	// Some good stable low factors
	SeparationFactor = 0.05f;
	NeighbourAlignFactor = 0.1f;
	CohesionFactor = 0.2f;
	GlobalTargetFactor = 0.015f;
	MinSpeed = 15.0f;

	PredatorAvoidedFactor = 10.0f;
	PredatorAvoidDistance = 20.0f;
	PredatorAttractedFactor = 2.0f;
}

////////////////////////////////////////////////////////////////////////////////
// Reset

void SbMurmuration::Reset()
{
	RdRandom::SetSeed(0);

	m_isPaused = BtFalse;
	m_v3Target = MtVector3(0, 0, 0);

	for(BtU32 i = 0; i < MaxPredators; i++)
	{
		SbKestrel &kestrel = g_predators[i];

		BtFloat x = RdRandom::GetFloat(-WorldSize, WorldSize);
		BtFloat y = RdRandom::GetFloat(-WorldSize, WorldSize);
		BtFloat z = RdRandom::GetFloat(-WorldSize, WorldSize);
		
		MtVector3 v3Position(x, y, z);
		kestrel.v3Pos = v3Position;
		kestrel.v3Vel = MtVector3(0, 0, 0);

		// Give the kestrels a dark red colour
//		kestrel.colour = RsColour(0.3f, 0.1f, 0.05f, 0.8f ).asWord();
		kestrel.colour = RsColour::RedColour().asWord();
	}

	for (BtU32 i = 0; i < MaxBoids; i++)
	{
		SbStarling &starling = g_flock[i];

		BtFloat r = RdRandom::GetFloat(0.01f, 0.10f);
		BtFloat g = RdRandom::GetFloat(0.01f, 0.10f);
		BtFloat b = RdRandom::GetFloat(0.01f, 0.10f);

		// Give the starlings a random dark colour
		starling.colour = RsColour(r, g, b, 1.0f).asWord();
		starling.v3Target = MtVector3(0, 0, 0);
		starling.v3Vel = MtVector3(0, 0, 0);
	}

	if (1)
	{
		for (BtU32 i = 0; i < MaxBoids; i++)
		{
			SbStarling &starling = g_flock[i];
			BtFloat x = RdRandom::GetFloat(-WorldSize, WorldSize);
			BtFloat y = RdRandom::GetFloat(-WorldSize, WorldSize);
			BtFloat z = RdRandom::GetFloat(-WorldSize, WorldSize);
			starling.v3Pos = MtVector3(x, y, z);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// UpdateFactors

void SbMurmuration::UpdateFactors()
{
	for (BtS32 i = 0; i < NumBoids; i++)
	{
		SbStarling &Starling = g_flock[i];
		Starling.cohesion = CohesionFactor;//  *RdRandom::GetFloat(0.8f, 1.0f);
		Starling.separation = SeparationFactor;//  *RdRandom::GetFloat(0.8f, 1.0f);
		Starling.alignment = NeighbourAlignFactor;//  *RdRandom::GetFloat(0.8f, 1.0f);
		Starling.predatorAvoidFactor = PredatorAvoidedFactor;//  *RdRandom::GetFloat(0.8f, 1.0f);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Update

void SbMurmuration::Update()
{
	BtFloat small = 0;

	BtFloat maxFactor = 1.0f;
	BtFloat maxBigFactor = 2.0f;
	BtFloat maxSpeed = MtKnotsToMetersPerSecond(60.0f);
	BtFloat maxDist = WorldSize;

	BtBool readOnly = BtFalse;
	HlDebug::Reset();
	HlDebug::AddInteger(0, "Num boids", &NumBoids, readOnly, 1, MaxBoids, 1);
	HlDebug::AddInteger(0, "Num predators", &NumPredators, readOnly, 10, 4, 1);
	
	HlDebug::AddFloat(0, "Min speed", &MinSpeed, readOnly, HLUnits_Knots, small, maxSpeed);
	HlDebug::AddFloat(0, "Max speed", &MaxSpeed, readOnly, HLUnits_Knots, small, maxSpeed);

	HlDebug::AddFloat(0, "Predator avoid dist", &PredatorAvoidDistance, readOnly, HLUnits_StandardIndex, small, maxSpeed);

	HlDebug::AddFloat(0, "Predator avoided factor", &PredatorAvoidedFactor, readOnly, HLUnits_StandardIndex, small, maxBigFactor);
	HlDebug::AddFloat(0, "Predator attracted factor", &PredatorAttractedFactor, readOnly, HLUnits_StandardIndex, small, maxBigFactor);
	HlDebug::AddFloat(0, "Local target factor", &LocalTargetFactor, readOnly, HLUnits_StandardIndex, small, maxFactor);
	HlDebug::AddFloat(0, "Starling global target factor", &GlobalTargetFactor, readOnly, HLUnits_StandardIndex,	 small, maxFactor);
	HlDebug::AddFloat(0, "Cohesion", &CohesionFactor, readOnly, HLUnits_StandardIndex,					 small, maxFactor);
	HlDebug::AddFloat(0, "Separation factor", &SeparationFactor, readOnly, HLUnits_StandardIndex,		 small, maxFactor);
	HlDebug::AddFloat(0, "Alignment factor", &NeighbourAlignFactor, readOnly, HLUnits_StandardIndex,	 small, maxFactor);

	UpdateFactors();

	if( UiKeyboard::pInstance()->IsPressed( UiKeyCode_P ) )
	{
		m_isPaused = !m_isPaused;
	}

	if( UiKeyboard::pInstance()->IsHeld( UiKeyCode_R ) )
	{
		Reset();
	}

	// Calculate the ABB for our flock
	MtAABB aabb = MtAABB( g_flock[0].v3Pos );
	for( BtS32 i=1; i<NumBoids; i++ )
	{
		// Expand the AABB
		aabb.ExpandBy( g_flock[i].v3Pos );
	}

	// Set the centre
	v3Centre = aabb.Center();

	// If we are paused exit
	if( m_isPaused == BtTrue )
	{
		return;
	}

	// Bring the global target gradually to the centre of the world
	m_v3Target = MtVector3(0, 0, 0);

	// Make a KD tree
	doKDTree( g_flock, NumBoids );

	// Make the starlings avoid the predators
	for(BtS32 i = 0; i < NumPredators; i++)
	{
		SbKestrel &kestrel = g_predators[i];

		kestrel.shortestDistance = (kestrel.v3Pos - g_flock[0].v3Pos).GetLengthSquared();
		kestrel.pStarling = &g_flock[0];

		for(BtS32 j = 0; j < NumBoids; j++)
		{
			SbStarling &starling = g_flock[j];

			MtVector3 v3Distance = kestrel.v3Pos - starling.v3Pos;

			BtFloat distance = v3Distance.GetLengthSquared();

			if( distance < kestrel.shortestDistance )
			{
				kestrel.shortestDistance = distance;
				kestrel.pStarling = &starling;
			}
			if( distance < PredatorAvoidDistance * PredatorAvoidDistance )
			{
				MtVector3 v3Delta = v3Distance.GetNormalise();
				starling.v3Vel -= v3Delta * starling.predatorAvoidFactor;					// run away
			}
		}
	}

	// Predator attract
	for(BtS32 i = 0; i < NumPredators; i++)
	{
		SbKestrel &kestrel = g_predators[i];

		if( kestrel.shortestDistance < PredatorAvoidDistance * PredatorAvoidDistance)
		{
			MtVector3 v3Distance = kestrel.pStarling->v3Pos - kestrel.v3Pos;
			kestrel.v3Vel += v3Distance.GetNormalise() * PredatorAttractedFactor;
		}
		else
		{
			MtVector3 v3Distance = m_v3Target - kestrel.v3Pos;
			kestrel.v3Vel += v3Distance.GetNormalise() * PredatorAttractedFactor;
		}

		// Integrate the position
		kestrel.v3Pos += kestrel.v3Vel * dt;

		// Cap the speed
		BtFloat mag = kestrel.v3Vel.GetLength();
		if (mag < MinSpeed)
		{
			kestrel.v3Vel = kestrel.v3Vel.Normalise() * MinSpeed;
		}
		if(mag > MaxSpeed)
		{
			kestrel.v3Vel = kestrel.v3Vel.Normalise() * MaxSpeed;
		}
	}

	// Main flocking calculations
	for( BtS32 i=0; i<NumBoids; i++ )
	{
		SbStarling &bird = g_flock[i];

		BtU32 numNeighbours = bird.neighbours.GetNumItems();
		if( numNeighbours )
		{
			// Reynolds rules from wikipedia
			//	separation: steer to avoid crowding local flockmates
			//	cohesion : steer to move toward the average position(center of mass) of local flockmates
			//	alignment : steer towards the average heading of local flockmates

			MtVector3 v3AverageVelocity(0, 0, 0);
			MtVector3 v3AveragePosition(0, 0, 0);

			SbStarling *closestNeighbour = bird.neighbours[0];
			BtFloat closestDistance = (bird.neighbours[0]->v3Pos - bird.v3Pos).GetLengthSquared();

			for (BtU32 j = 0; j < numNeighbours; j++)
			{
				SbStarling *pNeighbour = bird.neighbours[j];

				v3AveragePosition = pNeighbour->v3Pos;
				v3AverageVelocity = pNeighbour->v3Vel;
			}
			v3AveragePosition /= (BtFloat)numNeighbours;
			v3AverageVelocity /= (BtFloat)numNeighbours;

			// Separate the birds
			{
				MtVector3 v3Delta = closestNeighbour->v3Pos - bird.v3Pos;
				bird.v3Vel -= v3Delta.GetNormalise() * bird.separation;
			}

			// Cohese the birds
			{
				MtVector3 v3Delta = v3AveragePosition - bird.v3Pos;
				bird.v3Vel += v3Delta.GetNormalise() * bird.cohesion;
			}

			// Align the birds
			{
				MtVector3 v3Delta = v3AverageVelocity - bird.v3Vel;
				bird.v3Vel += v3Delta.GetNormalise() * bird.alignment;
			}
		}
		
	}

	// Update the birds velocity and position
	for( BtS32 i=0; i<NumBoids; i++ )
	{
		SbStarling &starling = g_flock[i];

		{
			// Calculate the distance to the origin
			MtVector3 v3Delta = starling.v3Target - starling.v3Pos;

			// Update the velocity to the target
			starling.v3Vel += v3Delta.GetNormalise() * LocalTargetFactor;
		}

		{
			// Calculate the distance to the origin
			MtVector3 v3Delta = m_v3Target - starling.v3Pos;

			if (v3Delta.GetLength() > 0)
			{
				// Update the velocity to the target
				starling.v3Vel += v3Delta.GetNormalise() * GlobalTargetFactor;
			}
		}

		// Integrate the position
		starling.v3Pos += starling.v3Vel * dt;

		// Cap the speed
		BtFloat mag = starling.v3Vel.GetLength();

		if (mag < MinSpeed)
		{
			starling.v3Vel = starling.v3Vel.Normalise() * MinSpeed;
		}

		if( mag > MaxSpeed )
		{
			starling.v3Vel = starling.v3Vel.Normalise() * MaxSpeed;
		}
	}

	m_v3Target = v3Centre;
}

////////////////////////////////////////////////////////////////////////////////
// RenderToTexture

void SbMurmuration::RenderToTexture()
{
	// Render the shadow to the texture
	//m_shadow.RenderToTexture();
}

////////////////////////////////////////////////////////////////////////////////
// Render

void SbMurmuration::Render( RsCamera *pCamera )
{
	// Setup the shader
	MtVector3 v3LightDirection(0, 1, 0);
	MtVector3 ambient(0.1f, 0.1f, 0.1f);
	m_pShader->SetFloats(RsHandles_Light0Direction, &v3LightDirection.x, 3);
	m_pShader->SetFloats(RsHandles_LightAmbient, &ambient.x, 3);

	// Cache the camera because we are going to use billboards to render the birds flat toward the camera
	MtMatrix4 m4View = pCamera->GetRotation();
	MtMatrix4 m4Projection = pCamera->GetProjection();
	MtMatrix4 m4ViewProjection = m4View * m4Projection;
	BtU32 width = (BtU32)pCamera->GetWidth();
	BtU32 height = (BtU32)pCamera->GetHeight();

	// Calculate what this flat vector would be
	MtVector3 v3Up(0, 1, 0);
	MtVector3 v3Side(1, 0, 0);
	v3Up *= m4View;
	v3Side *= m4View;

	// Setup the width of the birds
	const BtFloat StarlingHalfWingSpan = StarlingWingSpan * 0.5f;

	MtMatrix3 m3Orientation = pCamera->GetRotation();

	// Draw the birds
	BtU32 tri = 0;
	if( NumBoids )
	{
		for( BtU32 i=0; i<NumBoids; i++ )
		{
			SbStarling &Starling = g_flock[i];

			MtVector3 &v3Position = Starling.v3Pos;

			// Render the bird
			myVertex[tri + 0].m_v3Position = v3Position;
			myVertex[tri + 1].m_v3Position = v3Position + (m3Orientation.Col0() * StarlingWingSpan);
			myVertex[tri + 2].m_v3Position = v3Position + (m3Orientation.Col1() * StarlingWingSpan);
			myVertex[tri + 0].m_colour = Starling.colour;
			myVertex[tri + 1].m_colour = Starling.colour;
			myVertex[tri + 2].m_colour = Starling.colour;

			myVertex[tri + 0].m_v2UV = MtVector2(0.0f, 0.0f);
			myVertex[tri + 1].m_v2UV = MtVector2(0.0f, 1.0f);
			myVertex[tri + 2].m_v2UV = MtVector2(1.0f, 1.0f);
			tri += 3;
		}

		for(BtU32 i = 0; i < tri; i += 3)
		{
			myVertex[i + 0].m_v2UV = MtVector2(0, 0);
			myVertex[i + 1].m_v2UV = MtVector2(0, 1);
			myVertex[i + 2].m_v2UV = MtVector2(1, 0);
		}
		m_pBird3->Render(RsPT_TriangleList, myVertex, tri, MaxSortOrders - 1, BtFalse);
	}

	// Draw the predators
	RsVertex3 *pVertex = &myVertex[tri];

	if(NumPredators)
	{
		const BtFloat KestrelHalfWingSpan = KestrelWingSpan;

		for(BtS32 i = 0; i < NumPredators; i++)
		{
			SbKestrel &kestrel = g_predators[i];

			MtVector3 &v3Position = kestrel.v3Pos;

			// Render the kestrel
			myVertex[tri + 0].m_v3Position = v3Position;
			myVertex[tri + 1].m_v3Position = v3Position + (m3Orientation.Col0() * KestrelWingSpan);
			myVertex[tri + 2].m_v3Position = v3Position + (m3Orientation.Col1() * KestrelWingSpan);
			myVertex[tri + 0].m_colour = kestrel.colour;
			myVertex[tri + 1].m_colour = kestrel.colour;
			myVertex[tri + 2].m_colour = kestrel.colour;
			tri += 3;
		}
		for(BtU32 i = 0; i < tri; i += 3)
		{
			myVertex[i + 0].m_v2UV = MtVector2(0, 0);
			myVertex[i + 1].m_v2UV = MtVector2(0, 1);
			myVertex[i + 2].m_v2UV = MtVector2(1, 0);
		}
		m_pBird3->Render(RsPT_TriangleList, pVertex, NumPredators * 3, MaxSortOrders - 1, BtFalse);
	}

	if( ApConfig::IsDebug() )
	{
		HlDebug::Render();

		MtMatrix4 m4Transform;
		m4Transform.SetTranslation(m_v3Target);
		HlDraw::RenderCross(m4Transform, KestrelWingSpan * 2.0f, MaxSortOrders - 1);

		//sprintf(text, "FPS %.0f", RsUtil::GetFPS());
		//HlFont::RenderHeavy(MtVector2(100, 15), text, MaxSortOrders - 1);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Destroy

void SbMurmuration::Destroy()
{
}