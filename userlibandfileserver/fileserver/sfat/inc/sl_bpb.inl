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
// f32\sfat\inc\sl_bpb.inl
// 
//

#ifndef SL_BPB_INL
#define SL_BPB_INL


#define pDir ((SFatDirEntry*)&iData[0])

// class TFatBootSector
inline const TPtrC8 TFatBootSector::VendorId() const
	{return TPtrC8(iVendorId,8);}
inline TInt TFatBootSector::BytesPerSector() const
	{return iBytesPerSector;}
inline TInt TFatBootSector::SectorsPerCluster() const
	{return iSectorsPerCluster;}
inline TInt TFatBootSector::ReservedSectors() const
	{return iReservedSectors;}
inline TInt TFatBootSector::NumberOfFats() const
	{return iNumberOfFats;}
inline TInt TFatBootSector::RootDirEntries() const
	{return iRootDirEntries;}
inline TInt TFatBootSector::TotalSectors() const
	{return iTotalSectors;}
inline TUint8 TFatBootSector::MediaDescriptor() const
	{return iMediaDescriptor;}
inline TInt TFatBootSector::FatSectors() const
	{return iFatSectors;}
inline TInt TFatBootSector::SectorsPerTrack() const
	{return iSectorsPerTrack;}
inline TInt TFatBootSector::NumberOfHeads() const
	{return iNumberOfHeads;}
inline TInt TFatBootSector::HiddenSectors() const
	{return iHiddenSectors;}
inline TInt TFatBootSector::HugeSectors() const
	{return iHugeSectors;}
inline TInt TFatBootSector::PhysicalDriveNumber() const
	{return iPhysicalDriveNumber;}
inline TInt TFatBootSector::ExtendedBootSignature() const
	{return iExtendedBootSignature;}
inline TUint32 TFatBootSector::UniqueID() const
	{return iUniqueID;}
inline const TPtrC8 TFatBootSector::VolumeLabel() const
	{return TPtrC8(iVolumeLabel,KVolumeLabelSize);}
inline const TPtrC8 TFatBootSector::FileSysType() const
	{return TPtrC8(iFileSysType,KFileSysTypeSize);}
inline TInt TFatBootSector::BootSectorSignature() const
	{return KBootSectorSignature;}

inline void TFatBootSector::SetJumpInstruction()
	{
	iJumpInstruction[0]=0xE9;iJumpInstruction[2]=0x90;
	}
inline void TFatBootSector::SetVendorID(const TDesC8& aDes)
	{
	__ASSERT_DEBUG(aDes.Length()<=8,Fault(EFatBadBootSectorParameter));
	TPtr8 buf(iVendorId,8);
	buf=aDes;
	}
inline void TFatBootSector::SetBytesPerSector(TInt aBytesPerSector)
	{
	__ASSERT_DEBUG(!(aBytesPerSector&~KMaxTUint16),Fault(EFatBadBootSectorParameter));
	iBytesPerSector=(TUint16)aBytesPerSector;
	}
inline void TFatBootSector::SetSectorsPerCluster(TInt aSectorsPerCluster)
	{
	__ASSERT_DEBUG(!(aSectorsPerCluster&~KMaxTUint8),Fault(EFatBadBootSectorParameter));
	iSectorsPerCluster=(TUint8)aSectorsPerCluster;
	}
inline void TFatBootSector::SetReservedSectors(TInt aReservedSectors)
	{
	__ASSERT_DEBUG(!(aReservedSectors&~KMaxTUint16),Fault(EFatBadBootSectorParameter));
	iReservedSectors=(TUint16)aReservedSectors;
	}
inline void TFatBootSector::SetNumberOfFats(TInt aNumberOfFats)
	{
	__ASSERT_DEBUG(!(aNumberOfFats&~KMaxTUint8),Fault(EFatBadBootSectorParameter));
	iNumberOfFats=(TUint8)aNumberOfFats;
	}
inline void TFatBootSector::SetRootDirEntries(TInt aRootDirEntries)
	{
	__ASSERT_DEBUG(!(aRootDirEntries&~KMaxTUint16),Fault(EFatBadBootSectorParameter));
	iRootDirEntries=(TUint16)aRootDirEntries;
	}
inline void TFatBootSector::SetTotalSectors(TInt aTotalSectors)
	{
	__ASSERT_DEBUG(!(aTotalSectors&~KMaxTUint16),Fault(EFatBadBootSectorParameter));
	iTotalSectors=(TUint16)aTotalSectors;
	}
inline void TFatBootSector::SetMediaDescriptor(TUint8 aMediaDescriptor)
	{iMediaDescriptor=aMediaDescriptor;}
inline void TFatBootSector::SetFatSectors(TInt aFatSectors)
	{
	__ASSERT_DEBUG(!(aFatSectors&~KMaxTUint16),Fault(EFatBadBootSectorParameter));
	iFatSectors=(TUint16)aFatSectors;
	}
inline void TFatBootSector::SetSectorsPerTrack(TInt aSectorsPerTrack)
	{
	__ASSERT_DEBUG(!(aSectorsPerTrack&~KMaxTUint16),Fault(EFatBadBootSectorParameter));
	iSectorsPerTrack=(TUint16)aSectorsPerTrack;
	}
inline void TFatBootSector::SetNumberOfHeads(TInt aNumberOfHeads)
	{
	__ASSERT_DEBUG(!(aNumberOfHeads&~KMaxTUint16),Fault(EFatBadBootSectorParameter));
	iNumberOfHeads=(TUint16)aNumberOfHeads;
	}
inline void TFatBootSector::SetHiddenSectors(TUint32 aHiddenSectors)
	{
	iHiddenSectors=(TUint32)(aHiddenSectors);
	}
inline void TFatBootSector::SetHugeSectors(TUint32 aHugeSectors)
	{iHugeSectors=aHugeSectors;}
inline void TFatBootSector::SetPhysicalDriveNumber(TInt aPhysicalDriveNumber)
	{
	__ASSERT_DEBUG(!(aPhysicalDriveNumber&~KMaxTUint8),Fault(EFatBadBootSectorParameter));
	iPhysicalDriveNumber=(TUint8)aPhysicalDriveNumber;
	}
inline void TFatBootSector::SetReservedByte(TUint8 aReservedByte)
	{iReserved=aReservedByte;}
inline void TFatBootSector::SetExtendedBootSignature(TInt anExtendedBootSignature)
	{
	__ASSERT_DEBUG(!(anExtendedBootSignature&~KMaxTUint8),Fault(EFatBadBootSectorParameter));
	iExtendedBootSignature=(TUint8)anExtendedBootSignature;
	}
inline void TFatBootSector::SetUniqueID(TUint32 anUniqueID)
	{iUniqueID=anUniqueID;}
inline void TFatBootSector::SetVolumeLabel(const TDesC8& aDes)
	{
	__ASSERT_DEBUG(aDes.Length()<=KVolumeLabelSize,Fault(EFatBadBootSectorParameter));
	TPtr8 buf(iVolumeLabel,KVolumeLabelSize);
	buf=aDes;
	}
inline void TFatBootSector::SetFileSysType(const TDesC8& aDes)
	{
	__ASSERT_DEBUG(aDes.Length()<=8,Fault(EFatBadBootSectorParameter));
	TPtr8 buf(iFileSysType,8);
	buf=aDes;
	}


#endif //SL_BPB_INL
