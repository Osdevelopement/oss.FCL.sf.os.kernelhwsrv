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
// f32\sfat\inc\fat_table.h
// FAT12/16 File Allocation Table classes definitions
// 
//

/**
 @file
 @internalTechnology
*/

#ifndef FAT_TABLE_H
#define FAT_TABLE_H


//---------------------------------------------------------------------------------------------------------------------------------------

/**
    Fat table used for all media except RAM, manages the Fat table for all cluster requests.
*/
class CAtaFatTable : public CFatTable
	{
public:
    static CAtaFatTable* NewL(CFatMountCB& aOwner);

    //-- overrides from the base class
	void FlushL();
	void Dismount(TBool aDiscardDirtyData);
    void ReMountL();
	void InvalidateCacheL(TInt64 aPos, TUint32 aLength);
	void InvalidateCacheL();
	
	TUint32 ReadL(TUint32 aFatIndex) const;
	void WriteL(TUint32 aFatIndex, TUint32 aValue);

	TInt64 DataPositionInBytes(TUint32 aCluster) const;

private:
	CAtaFatTable(CFatMountCB& aOwner);
    void InitializeL();
    void CreateCacheL();

private:
    CFatCacheBase* iCache;  ///< FAT cache, fixed or LRU depending on the FAT type
	};

//---------------------------------------------------------------------------------------------------------------------------------------

/**
    Fat table used for RAM media, manages the Fat table for all cluster requests. 
    RAM media only supports Fat12/16.
*/
class CRamFatTable : public CFatTable
	{
public:
    static CRamFatTable* NewL(CFatMountCB& aOwner);

    void ReMountL();
	TUint32 ReadL(TUint32 aFatIndex) const;
	void WriteL(TUint32 aFatIndex, TUint32 aValue);
	TInt64 DataPositionInBytes(TUint32 aCluster) const;
	void FreeClusterListL(TUint32 aCluster);
	TUint32 AllocateSingleClusterL(TUint32 aNearestCluster);
	void ExtendClusterListL(TUint32 aNumber,TInt& aCluster);

private:
	CRamFatTable(CFatMountCB& aOwner);

	void InitializeL();

	inline TUint8 *RamDiskBase() const;
	inline TInt AllocateClusterNumber();
	inline void WriteFatTable(TInt aFatIndex,TInt aValue);
	inline void WriteFatTable(TInt aFatIndex,TInt aFatValue,TInt anIndirectionTableValue);
	inline void ReadIndirectionTable(TUint32& aCluster) const;
	inline void WriteIndirectionTable(TInt aFatIndex,TInt aValue);
	inline TUint8* MemCopy(TAny* aTrg,const TAny* aSrc,TInt aLength);
	inline TUint8* MemCopyFillZ(TAny* aTrg, TAny* aSrc, TInt aLength);
	inline void ZeroFillCluster(TInt aCluster); 
	
	void UpdateIndirectionTable(TUint32 aStartCluster,TUint32 anEndCluster,TInt aNum);

protected:

	TInt iFatTablePos;          ///< Current position in the fat table
	TInt iIndirectionTablePos;  ///< Current position in indirection table, second fat used for this
	TUint8* iRamDiskBase;       ///< Pointer to the Ram disk base
	};



//---------------------------------------------------------------------------------------------------------------------------------


#endif //FAT_TABLE_H























