////////////////////////////////////////////////////////////////////////////////
// SbModel.h

// Include guard
#pragma once
#include "BtTypes.h"
#include "RsCamera.h"
#include "MtMatrix4.h"

class RsModel;
class BaArchive;
class SgAnimator;

// Class definition
class SbModel
{
public:

	// Public functions
	void							Init();
	void							Setup( BaArchive *pArchive, BaArchive *pAnimArchive  );
	void							Update();
	void							Render();

	// Accessors

private:

	// Private functions
	
	// Private members
	SgAnimator					   *m_pAnimator;
	SgNode						   *m_pFish;
	SgNode						   *m_pCube;
	BtFloat							m_time;
	RsShader					   *m_pShader;
};
