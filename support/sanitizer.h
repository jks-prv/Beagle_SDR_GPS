/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2018 John Seamons, ZL4VO/KF6VO

#pragma once

#if defined(HOST) && defined(__clang__)
	// only clang >= 3.9 has support for the coroutines used by the KiwiSDR software
	// (__sanitizer_start_switch_fiber, __sanitizer_finish_switch_fiber)
	#if __has_feature(address_sanitizer) && ((__clang_major__ == 3 && __clang_minor__ >= 9) || __clang_major__ >= 4)
		#include <sanitizer/asan_interface.h>
		#define USE_ASAN
	    #if (__clang_major__ >= 6)
		    #define USE_ASAN2
        #endif	        
	#endif
#endif
