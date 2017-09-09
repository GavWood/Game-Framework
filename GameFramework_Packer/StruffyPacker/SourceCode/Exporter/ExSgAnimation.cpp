////////////////////////////////////////////////////////////////////////////////
// ExSgAnimation.cpp

// Includes
#include "StdAfx.h"
#include "ExSgAnimation.h"
#include "ExSgNode.h"

////////////////////////////////////////////////////////////////////////////////
// Constructor

ExSgAnimation::ExSgAnimation( ExSgNode *pNode, ExScene *pScene )
{
	m_pNode = pNode;
	m_pScene = pScene;
	m_animationKeys.clear();
}

////////////////////////////////////////////////////////////////////////////////
// Destructor

ExSgAnimation::~ExSgAnimation()
{
}

////////////////////////////////////////////////////////////////////////////////
// AddAnimation

void ExSgAnimation::AddAnimation( const ExSgAnimationKey& key )
{
	// Set the maximum duration
	m_maxDuration = key.m_time;

	// Add the key
	m_animationKeys.push_back( key );
}

////////////////////////////////////////////////////////////////////////////////
// GetNumKeys

BtU32 ExSgAnimation::GetNumKeys() const
{
	return m_animationKeys.size();
}

///////////////////////////////////////////////////////////////////////////////
// CopyAttributes

void ExSgAnimation::CopyAttributes()
{
}