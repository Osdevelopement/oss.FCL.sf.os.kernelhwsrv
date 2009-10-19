// Copyright (c) 1998-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the License "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// e32\include\win32crt.h
// 
//

/**
 @file
 @internalTechnology
*/

typedef void (__cdecl *_PVFV)(void);
typedef void* HANDLE;


//
// Static construction / destruction depends on compiler
//

// GCC creates two lists of functions to call, __CTOR_LIST__ and __DTOR_LIST__
#if defined(__GCC32__)
typedef void (*PFV)();
extern PFV __CTOR_LIST__[];
extern PFV __DTOR_LIST__[];
static inline void constructStatics() 
	{
	TUint _i=1;
	while (__CTOR_LIST__[_i])
		(*__CTOR_LIST__[_i++])();
	}
static inline void destroyStatics() 
	{
	TUint _i=1; 
	while (__DTOR_LIST__[_i])
		(*__DTOR_LIST__[_i++])();
	}

// VC puts constructor/destructor function pointers in specially-named sections
#else
#pragma data_seg(".CRT$XIA")
_PVFV __xi_a[] = { NULL };
#pragma data_seg(".CRT$XIZ")
_PVFV __xi_z[] = { NULL };
#pragma data_seg(".CRT$XCA")
_PVFV __xc_a[] = { NULL };
#pragma data_seg(".CRT$XCZ")
_PVFV __xc_z[] = { NULL };
#pragma data_seg(".CRT$XPA")
_PVFV __xp_a[] = { NULL };
#pragma data_seg(".CRT$XPZ")
_PVFV __xp_z[] = { NULL };
#pragma data_seg(".CRT$XTA")
_PVFV __xt_a[] = { NULL };
#pragma data_seg(".CRT$XTZ")
_PVFV __xt_z[] = { NULL };
#pragma data_seg()      /* reset */
LOCAL_C void invokeTable(_PVFV *aStart,_PVFV *aEnd)
	{
	while (aStart<aEnd)
		{
		if (*aStart!=NULL)
			(**aStart)();
		aStart++;
        }
	}
#define constructStatics() invokeTable(__xi_a,__xi_z), invokeTable(__xc_a,__xc_z)
#define destroyStatics() invokeTable(__xp_a,__xp_z), invokeTable(__xt_a,__xt_z)
#endif

#if defined(__VC32__)
//
// Some symbols generated by the VC++ compiler for floating point stuff.
//
extern "C" {
#ifndef __EPOC32__
#pragma data_seg(".data2")
#pragma bss_seg(".data2")
GLDEF_D TInt _adj_fdivr_m64;
GLDEF_D TInt _adjust_fdiv;
GLDEF_D TInt _adj_fdiv_r;
GLDEF_D TInt _adj_fdiv_m64;
GLDEF_D TInt _adj_fdiv_m32i;
GLDEF_D TInt _adj_fdivr_m32i;
#ifndef __FLTUSED
#define __FLTUSED
extern "C" __declspec(selectany) int _fltused=0;
#endif //__FLTUSED
#pragma data_seg()
#pragma bss_seg()
#endif // !__EPOC32__

__int64 _ftol(void)
    {
    __int64 retval;
    short tmpcw;
    short oldcw;

    _asm fstcw   oldcw 
    _asm wait

    _asm mov     ax, oldcw
    _asm or      ah, 0x0c
    _asm mov     tmpcw, ax

    _asm fldcw   tmpcw 
    _asm fistp   retval 
    _asm fldcw   oldcw 

    return retval;
    }

__declspec(naked) __int64 _ftol2(void)
	{
	_asm call _ftol;
	_asm ret;
	}

__declspec(naked) __int64 _ftol2_sse(void)
	{
	_asm call _ftol;
	_asm ret;
	}

__declspec(naked) __int64 _ftol2_sse_excpt(void)
	{
	_asm call _ftol;
	_asm ret;
	}

__declspec(naked) __int64 _ftol2_pentium4(void)
	{
	_asm call _ftol;
	_asm ret;
	}

}

extern "C"
void abort()
	{
	abort();
	}
#endif


extern "C"
int __cdecl _purecall()
/*
// Gets called for any unreplaced pure virtual methods.
*/
	{
	return 0;
	}
