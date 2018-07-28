// St4285.cpp : Defines the entry point for the DLL application.
//

// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include "kiwi.h"

#include "stdafx.h"
#include "st4285.h"

// This is the constructor of a class that has been exported.
// see St4285.h for the class definition

CSt4285 m_CSt4285[MAX_RX_CHANS];

CSt4285::CSt4285()
{ 
	return; 
}

void CSt4285::reset()
{ 
	allocate_context();
	m_constellation_offset = 0;
	m_sample_counter = 0;
	return; 
}

CSt4285::~CSt4285()
{ 
	return; 
}

#endif
