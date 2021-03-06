// Copyright (c) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
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
// CMassStorageMountCB implementation.
//
//



/**
 @file
 @internalTechnology
*/

#include <f32fsys.h>

#include "mstypes.h"
#include "msctypes.h"

#include "cmassstoragefilesystem.h"
#include "drivemanager.h"


#include "cusbmassstoragecontroller.h"
#include "cmassstoragemountcb.h"
#include "debug.h"

CMassStorageMountCB::CMassStorageMountCB(const TLunToDriveMap& aDriveMapping)
:   iDriveMapping(aDriveMapping)
    {
    }

CMassStorageMountCB* CMassStorageMountCB::NewL(const TLunToDriveMap& aDriveMapping)
    {
    return new (ELeave) CMassStorageMountCB(aDriveMapping);
    }

/**
Checks that the drive number is supported.

@leave KErrNotReady The drive number is not supported.
*/
TInt CMassStorageMountCB::CheckDriveNumberL()
    {
    TInt driveNumber;
    driveNumber = Drive().DriveNumber();
    if (!IsValidLocalDriveMapping(driveNumber))
        {
        __PRINT1(_L("CMassStorageMountCB::CheckDriveNumberL: Drive number %d not supported"), driveNumber);
        User::Leave(KErrNotReady);
        }
    __PRINT1(_L("CMassStorageMountCB::CheckDriveNumberL: Drive number = %d"), driveNumber);
    return driveNumber;
    }

/**
Registers the drive with the Mass Storage drive manager.

@leave KErrNotSupported The drive is not compatible with Mass Storage.
*/
void CMassStorageMountCB::MountL(TBool /*aForceMount*/)
    {
    CheckDriveNumberL();
    CMassStorageFileSystem& msFsys = *reinterpret_cast<CMassStorageFileSystem*>(Drive().GetFSys());

    TInt lun = DriveNumberToLun(Drive().DriveNumber());

    if(lun < 0)
        {
        // This is not a supported Mass Storage drive
        User::Leave(KErrNotSupported);
        }

    TBusLocalDrive& localDrive = msFsys.iMediaChangedStatusList[lun].iLocalDrive;

    TInt err = CreateLocalDrive(localDrive);
    User::LeaveIfError(err);

    CProxyDrive* proxyDrive = LocalDrive();

    TLocalDriveCapsV2Buf caps;
    err = localDrive.Caps(caps);

    //Make sure file system is FAT and removable
    if (err == KErrNone)
        {
        err = KErrNotSupported;
        if ((caps().iDriveAtt & KDriveAttRemovable) == KDriveAttRemovable)
            {
            if (caps().iType != EMediaNotPresent)
                {
                err = KErrNone;
                }
            }
        }

    if (err != KErrNone && err != KErrNotReady)
        {
        __PRINT1(_L("CMassStorageMountCB::MountL: Drive is not compatible with Mass Storage, err=%d"), err);
        User::Leave(err);
        }

    __PRINT(_L("CMassStorageMountCB::MountL: Registering drive"));
    // Set media changed to true so that Win2K doesn't used cached drive data
    TBool& mediaChanged = msFsys.iMediaChangedStatusList[lun].iMediaChanged;
    mediaChanged = ETrue;
    msFsys.Controller().DriveManager().RegisterDriveL(*proxyDrive, mediaChanged, lun);
    SetVolumeName(_L("MassStorage").AllocL());
    }

/**
Returns the LUN that corresponds to the specified drive number.

@param aDriveNumber The drive number.
*/
TInt CMassStorageMountCB::DriveNumberToLun(TInt aDriveNumber)
    {
    TInt lun = -1;
    for (TInt i = 0; i < iDriveMapping.Count(); i++)
        {
        if (iDriveMapping[i] == aDriveNumber)
            {
            lun = i;
            break;
            }
        }
    __PRINT2(_L("CMassStorageMountCB::DriveNumberToLun: Drive %d maps to LUN %d"), aDriveNumber, lun);
    return lun;
    }

/**
Deregisters the drive from the Drive Manager.
*/
void CMassStorageMountCB::Dismounted()
    {
    TInt driveNumber = -1;
    TRAPD(err, driveNumber = CheckDriveNumberL());
    if (err != KErrNone)
        {
        return;
        }
    __PRINT(_L("CMassStorageMountCB::Dismounted: Deregistering drive"));
    CMassStorageFileSystem& msFsys = *reinterpret_cast<CMassStorageFileSystem*>(Drive().GetFSys());
    msFsys.Controller().DriveManager().DeregisterDrive(DriveNumberToLun(driveNumber));

    DismountedLocalDrive();
    }

/**
Unlocks the drive with the specified password, optionally storing the password for later use.

@param aPassword The password to use for unlocking the drive.
@param aStore True if the password is to be stored.
*/
TInt CMassStorageMountCB::Unlock(TMediaPassword& aPassword, TBool aStore)
    {
    TInt driveNumber = -1;
    TRAPD(err, driveNumber = CheckDriveNumberL());
    if (err != KErrNone)
        {
        return err;
        }
    TBusLocalDrive& localDrive=GetLocalDrive(driveNumber);
    if(localDrive.Status() == KErrLocked)
        {
        localDrive.Status() = KErrNotReady;
        }
    TInt r = localDrive.Unlock(aPassword, aStore);
    if(r == KErrNone && aStore)
        {
        WritePasswordData();
        }
    return(r);
    }

/**
Stores the password for the drive to the password file.
*/
void CMassStorageMountCB::WritePasswordData()
    {
    TBusLocalDrive& local=GetLocalDrive(Drive().DriveNumber());
    TInt length = local.PasswordStoreLengthInBytes();
    if(length==0)
        {
        TBuf<sizeof(KMediaPWrdFile)> mediaPWrdFile(KMediaPWrdFile);
        mediaPWrdFile[0] = (TUint8) RFs::GetSystemDriveChar();
        WriteToDisk(mediaPWrdFile,_L8(""));
        return;
        }
    HBufC8* hDes=HBufC8::New(length);
    if(hDes==NULL)
        {
        return;
        }
    TPtr8 pDes=hDes->Des();
    TInt r=local.ReadPasswordData(pDes);
    if(r==KErrNone)
        {
        TBuf<sizeof(KMediaPWrdFile)> mediaPWrdFile(KMediaPWrdFile);
        mediaPWrdFile[0] = (TUint8) RFs::GetSystemDriveChar();
        WriteToDisk(mediaPWrdFile,pDes);
        }
    delete hDes;
    }


TInt CMassStorageMountCB::ReMount()
    {
    RDebug::Printf("CMassStorageMountCB::ReMount()");
    return KErrNotReady;
    }

void CMassStorageMountCB::VolumeL(TVolumeInfo& /*aVolume*/) const
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::SetVolumeL(TDes& /*aName*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::MkDirL(const TDesC& /*aName*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::RmDirL(const TDesC& /*aName*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::DeleteL(const TDesC& /*aName*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::RenameL(const TDesC& /*anOldName*/,const TDesC& /*anNewName*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::ReplaceL(const TDesC& /*anOldName*/,const TDesC& /*anNewName*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::EntryL(const TDesC& /*aName*/,TEntry& /*anEntry*/) const
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::SetEntryL(const TDesC& /*aName*/,const TTime& /*aTime*/,TUint /*aSetAttMask*/,TUint /*aClearAttMask*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::FileOpenL(const TDesC& /*aName*/,TUint /*aMode*/,TFileOpen /*anOpen*/,CFileCB* /*aFile*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::DirOpenL(const TDesC& /*aName*/,CDirCB* /*aDir*/)
    {
    User::Leave(KErrNotReady);
    }


void CMassStorageMountCB::RawReadL(TInt64 /*aPos*/,TInt /*aLength*/,const TAny* /*aTrg*/,TInt /*anOffset*/,const RMessagePtr2& /*aMessage*/) const
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::RawWriteL(TInt64 /*aPos*/,TInt /*aLength*/,const TAny* /*aSrc*/,TInt /*anOffset*/,const RMessagePtr2& /*aMessage*/)
    {
    User::Leave(KErrNotReady);
    }


void CMassStorageMountCB::GetShortNameL(const TDesC& /*aLongName*/,TDes& /*aShortName*/)
    {
    User::Leave(KErrNotReady);
    }

void CMassStorageMountCB::GetLongNameL(const TDesC& /*aShorName*/,TDes& /*aLongName*/)
    {
    User::Leave(KErrNotReady);
    }

#if defined(_DEBUG)
TInt CMassStorageMountCB::ControlIO(const RMessagePtr2& aMessage,TInt aCommand,TAny* aParam1,TAny* aParam2)
//
// Debug function
//
    {
    if(aCommand>=(KMaxTInt/2))
        return LocalDrive()->ControlIO(aMessage,aCommand-(KMaxTInt/2),aParam1,aParam2);
    else
        return KErrNotSupported;
    }
#else
TInt CMassStorageMountCB::ControlIO(const RMessagePtr2& /*aMessage*/,TInt /*aCommand*/,TAny* /*aParam1*/,TAny* /*aParam2*/)
    {return(KErrNotSupported);}
#endif

void CMassStorageMountCB::ReadSectionL(const TDesC& /*aName*/,TInt /*aPos*/,TAny* /*aTrg*/,TInt /*aLength*/,const RMessagePtr2& /*aMessage*/)
    {
    User::Leave(KErrNotReady);
    }

/**
Returns ETrue if aNum is a power of two
*/
TBool CMassStorageMountCB::IsPowerOfTwo(TInt aNum)
    {

    if (aNum==0)
        return(EFalse);

    while(aNum)
        {
        if (aNum & 0x01)
            {
            if (aNum>>1)
                return EFalse;
            break;
            }
        aNum>>=1;
        }
    return ETrue;
    }

/**
Returns the position of the highest bit in aNum or -1 if aNum == 0
*/
TInt CMassStorageMountCB::Log2(TInt aNum)
    {

    TInt res=-1;
    while(aNum)
        {
        res++;
        aNum>>=1;
        }
    return(res);
    }
