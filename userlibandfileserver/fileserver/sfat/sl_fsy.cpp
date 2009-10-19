// Copyright (c) 1996-2009 Nokia Corporation and/or its subsidiary(-ies).
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
// f32\sfat\sl_fsy.cpp
// 
//

#include "sl_std.h"
#include <e32hal.h>

CFatFileSystem* CFatFileSystem::New()
//
// Create a FatFileSystem 
//
	{
	CFatFileSystem* fatfsys=new CFatFileSystem();
	if (fatfsys==NULL)
		return(NULL);

	return fatfsys;
	}


CFatFileSystem::CFatFileSystem() : iUseLocalTimeIfRemovable(EFalse)
//
// Construct the file system
//
	{
	}	

CFatFileSystem::~CFatFileSystem()
//
// Destructor
//
	{
	}

TInt CFatFileSystem::Install()
//
// Install the file system
//
	{
	iVersion=TVersion(KF32MajorVersionNumber,KF32MinorVersionNumber,KF32BuildVersionNumber);

	// Read in setting from the config file to possibly make file server 
 	// use local time.
 	_LIT8(KFatConfigSection, "FatConfig");
 	_LIT8(KLocalTimeIfRemovable, "LocalTimeIfRemovable");
 	F32Properties::GetBool(KFatConfigSection, KLocalTimeIfRemovable, iUseLocalTimeIfRemovable);

	return(SetName(&KFileSystemName_FAT));
	}

CMountCB* CFatFileSystem::NewMountL() const
//
// Create a new mount control block.
//
	{

	return(CFatMountCB::NewL());
	}

CFileCB* CFatFileSystem::NewFileL() const
//
// Create a new file.
//
	{

	return(new(ELeave) CFatFileCB());
	}

CDirCB* CFatFileSystem::NewDirL() const
//
// Create a new directory lister.
//
	{

	return(CFatDirCB::NewL());
	}

CFormatCB* CFatFileSystem::NewFormatL() const
//
// Create a new media formatter.
//
	{

	return (new(ELeave) CFatFormatCB());
	}

TInt CFatFileSystem::DefaultPath(TDes& aPath) const
//
// Return the initial default path.
//
	{

	aPath=_L("?:\\");
	aPath[0] = (TUint8) RFs::GetSystemDriveChar();
	return(KErrNone);
	}


void CFatFileSystem::DriveInfo(TDriveInfo& anInfo,TInt aDriveNumber) const
//
// Return the drive info. iBatteryState are already set.
//
	{

	if(!IsValidLocalDriveMapping(aDriveNumber))
		return;

    TLocalDriveCapsV2Buf localDriveCaps;
	
	TInt r = KErrNone;

	// is the drive local?
	if (!IsProxyDrive(aDriveNumber))
		{
		// if not valid local drive, use default values in localDriveCaps
		// if valid local drive and not locked, use TBusLocalDrive::Caps() values
		// if valid drive and locked, hard-code attributes
		r = GetLocalDrive(aDriveNumber).Caps(localDriveCaps);
		}
	else  // this need to be made a bit nicer
		{   
		CExtProxyDrive* pD = GetProxyDrive(aDriveNumber);
		if(pD)
			r = pD->Caps(localDriveCaps);
		else
			r = KErrNotReady;	// What should the behaviour really be here?
		}

	if (r != KErrLocked )
		{
		anInfo.iMediaAtt=localDriveCaps().iMediaAtt;
		}
	else
		{
		anInfo.iMediaAtt = KMediaAttLocked | KMediaAttLockable | KMediaAttHasPassword;
		}

	anInfo.iType=localDriveCaps().iType;
	anInfo.iDriveAtt=localDriveCaps().iDriveAtt;
	}


TBool CFatFileSystem::IsExtensionSupported() const
//
//
//
	{
	return(ETrue);
	}

TBool CFatFileSystem::GetUseLocalTime() const
	{
	return iUseLocalTimeIfRemovable;
	}

void CFatFileSystem::SetUseLocalTime(TBool aFlag)
	{
	iUseLocalTimeIfRemovable = aFlag;
	}

/**
Reports whether the specified interface is supported - if it is,
the supplied interface object is modified to it

@param aInterfaceId     The interface of interest
@param aInterface       The interface object
@return                 KErrNone if the interface is supported, otherwise KErrNotFound 

@see CFileSystem::GetInterface()
*/
TInt CFatFileSystem::GetInterface(TInt aInterfaceId, TAny*& aInterface,TAny* aInput)
    {
    switch(aInterfaceId)
        {
        case CFileSystem::EProxyDriveSupport: // The FAT Filesystem supports proxy drives
			return KErrNone;

        default:
            return(CFileSystem::GetInterface(aInterfaceId, aInterface, aInput));
        }
    }
