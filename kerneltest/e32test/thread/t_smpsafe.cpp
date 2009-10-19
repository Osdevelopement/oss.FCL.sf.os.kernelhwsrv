// Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
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
// e32test\thread\t_smpsafe.cpp
// 
//

#define __E32TEST_EXTENSION__
#include <e32test.h>
#include <e32ldr.h>
#include <e32svr.h>
#include "u32std.h"
#include <u32hal.h>
#include <d_ldrtst.h>

RTest test(_L("T_SMPSAFE"));
RLdrTest ldd;

RProcess pLoaded;
TBool SMPPlatform;
TBool CompatMode;

extern TInt CheckAffinity();

void DoStartExe(RProcess& p, const TDesC &aFilename, TInt aExpectedUnsafe)
	{
	test_KErrNone(p.Create(aFilename, KNullDesC));
	test_Equal(aExpectedUnsafe, ldd.ProcessSMPUnsafeCount(p.Handle()));
	}

void DoStopExe(RProcess& p, TInt aExpectedUnsafe)
	{
	TRequestStatus s;
	p.Logon(s);
	p.Resume();
	User::WaitForRequest(s);
	if (CompatMode)
		test_Equal(aExpectedUnsafe ? 1 : 0, s.Int());
	test_Equal(EExitKill, p.ExitType());
	p.NotifyDestruction(s);
	p.Close();
	User::WaitForRequest(s);
	}

void StartExe(const TDesC &aFilename, TInt aExpectedUnsafe)
	{
	DoStartExe(pLoaded, aFilename, aExpectedUnsafe);
	}

void StopExe(TInt aExpectedUnsafe)
	{
	DoStopExe(pLoaded, aExpectedUnsafe);
	}

void TryExe(const TDesC &aFilename, TInt aExpectedUnsafe)
	{
	RProcess p;
	DoStartExe(p, aFilename, aExpectedUnsafe);
	DoStopExe(p, aExpectedUnsafe);
	}

void CheckSelf(TInt aExpectedUnsafe)
	{
	test_Equal(aExpectedUnsafe, ldd.ProcessSMPUnsafeCount(RProcess().Handle()));
	if (CompatMode)
		test_Equal(aExpectedUnsafe ? 1 : 0, CheckAffinity());
	}

GLDEF_C TInt E32Main()
	{
	RLibrary l, l2;

	// Turn off evil lazy dll unloading
	RLoader ldr;
	test(ldr.Connect()==KErrNone);
	test(ldr.CancelLazyDllUnload()==KErrNone);
	ldr.Close();

	test.Title();
	test.Start(_L("Test SMP safe binary flag"));

	test.Next(_L("Get number of CPUs"));
	TInt cpus = UserSvr::HalFunction(EHalGroupKernel, EKernelHalNumLogicalCpus, 0, 0);
	test_Compare(cpus, >, 0);
	SMPPlatform = cpus > 1;

	test.Next(_L("Get compatibility mode setting"));
	TInt flags = UserSvr::HalFunction(EHalGroupKernel, EKernelHalConfigFlags, 0, 0);
	test_Compare(flags, >=, 0);
	CompatMode = flags & (EKernelConfigSMPUnsafeCompat | EKernelConfigSMPUnsafeCPU0);
	if (SMPPlatform && !CompatMode)
		{
		test.Printf(_L("*************************************************\n"));
		test.Printf(_L("Compatibility mode is not enabled, not testing it\n"));
		test.Printf(_L("*************************************************\n"));
		}

	test.Next(_L("Load test LDD"));
	TInt r = User::LoadLogicalDevice(_L("d_ldrtst.ldd"));
	test(r==KErrNone || r==KErrAlreadyExists);
	test_KErrNone(ldd.Open());

	test.Next(_L("Check we are safe ourselves"));
	CheckSelf(0);

	test.Next(_L("Check safe exe"));
	StartExe(_L("smpsafe0.exe"), 0);
	test.Next(_L("Load already loaded safe exe (self)"));
	TryExe(_L("smpsafe0.exe"), 0);
	StopExe(0);

	test.Next(_L("Check safe XIP exe"));
	StartExe(_L("smpsafex0.exe"), 0);
	test.Next(_L("Load already loaded safe XIP exe (self)"));
	TryExe(_L("smpsafex0.exe"), 0);
	StopExe(0);

	test.Next(_L("Load unsafe exe"));
	StartExe(_L("smpsafe1.exe"), 1);
	test.Next(_L("Load already loaded unsafe exe"));
	TryExe(_L("smpsafe1.exe"), 1);
	StopExe(1);

	test.Next(_L("Load safe exe directly linked to unsafe dll"));
	TryExe(_L("smpsafe2.exe"), 1);

	test.Next(_L("Dynamically load unsafe dll"));
	test_KErrNone(l.Load(_L("smpsafea.dll")));
	CheckSelf(1);
	test.Next(_L("Load safe exe directly linked to loaded unsafe dll"));
	TryExe(_L("smpsafe2.exe"), 1);
	test.Next(_L("Dynamically unload unsafe dll"));
	l.Close();
	CheckSelf(0);

	test.Next(_L("Load safe XIP exe directly linked to unsafe XIP dll"));
	TryExe(_L("smpsafex2.exe"), 1);

	test.Next(_L("Dynamically load unsafe XIP dll"));
	test_KErrNone(l.Load(_L("smpsafexa.dll")));
	CheckSelf(1);
	test.Next(_L("Load safe XIP exe directly linked to loaded unsafe XIP dll"));
	TryExe(_L("smpsafex2.exe"), 1);
	test.Next(_L("Dynamically unload unsafe XIP dll"));
	l.Close();
	CheckSelf(0);

	test.Next(_L("Load safe exe indirectly linked to unsafe dll"));
	TryExe(_L("smpsafe3.exe"), 1);

	test.Next(_L("Dynamically load unsafe dll"));
	test_KErrNone(l.Load(_L("smpsafea.dll")));
	CheckSelf(1);
	test.Next(_L("Load safe exe indirectly linked to loaded unsafe dll"));
	TryExe(_L("smpsafe3.exe"), 1);
	test.Next(_L("Dynamically unload unsafe dll"));
	l.Close();
	CheckSelf(0);

	test.Next(_L("Dynamically load safe dll linked to unsafe dll"));
	test_KErrNone(l.Load(_L("smpsafeb.dll")));
	CheckSelf(1);
	test.Next(_L("Load safe exe indirectly linked to unsafe dll, inbetween loaded"));
	TryExe(_L("smpsafe3.exe"), 1);
	test.Next(_L("Dynamically load unsafe dll as well"));
	test_KErrNone(l2.Load(_L("smpsafea.dll")));
	CheckSelf(2);
	test.Next(_L("Dynamically unload safe dll linked to unsafe dll"));
	l.Close();
	CheckSelf(1);
	test.Next(_L("Dynamically unload unsafe dll as well"));
	l2.Close();
	CheckSelf(0);

	test.Next(_L("Load safe exe directly linked to unsafe XIP dll"));
	TryExe(_L("smpsafe4.exe"), 1);

	test.Next(_L("Dynamically load unsafe XIP dll"));
	test_KErrNone(l.Load(_L("smpsafexa.dll")));
	CheckSelf(1);
	test.Next(_L("Load safe exe directly linked to loaded unsafe XIP dll"));
	TryExe(_L("smpsafe4.exe"), 1);
	test.Next(_L("Dynamically unload unsafe XIP dll"));
	l.Close();
	CheckSelf(0);

	test.Next(_L("Dynamically load figure-eight dll cycle"));
	test_KErrNone(l.Load(_L("smpsafec.dll")));
	CheckSelf(1);
	test.Next(_L("Load figure-eight from a different point"));
	test_KErrNone(l2.Load(_L("smpsafed.dll")));
	CheckSelf(2);
	test.Next(_L("Unload original point"));
	l.Close();
	CheckSelf(1);
	test.Next(_L("Unload second point"));
	l2.Close();
	CheckSelf(0);

	test.Next(_L("Close test LDD"));
	ldd.Close();
	test_KErrNone(User::FreeLogicalDevice(KLdrTestLddName));

	test.End();
	return KErrNone;
	}
