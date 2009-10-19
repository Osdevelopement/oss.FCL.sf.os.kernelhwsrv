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
// f32\sfat\fat_table.cpp
// FAT12/16 File Allocation Table classes implementation
// 
//

/**
 @file
 @internalTechnology
*/



#include "sl_std.h"
#include "sl_fatcache.h"
#include "fat_table.h"


//#######################################################################################################################################
//#     CFatTable class implementation 
//#######################################################################################################################################

/**
    FAT object factory method.
    Constructs either CAtaFatTable or CRamFatTable depending on the media type parameter

    @param aOwner Pointer to the owning mount
    @param aLocDrvCaps local drive attributes
    @leave KErrNoMemory
    @return Pointer to the Fat table
*/
CFatTable* CFatTable::NewL(CFatMountCB& aOwner, const TLocalDriveCaps& aLocDrvCaps)
	{
    CFatTable* pFatTable=NULL;

    
    switch(aLocDrvCaps.iType)
        {
        case EMediaRam:
		    {//-- this is RAM media, try to create CRamFatTable instance.
            const TFatType fatType = aOwner.FatType();
            
            if(fatType != EFat16 )
                {//-- CRamFatTable doesn't support FAT12; FAT16 only.
                __PRINT1(_L("CFatTable::NewL() CRamFatTable doesn't support this FAT type:%d"), fatType);
                ASSERT(0);
                return NULL;
                }
            
                pFatTable = CRamFatTable::NewL(aOwner);            
            }
        break;

        default:
            //-- other media
		    pFatTable = CAtaFatTable::NewL(aOwner);
        break;
        };

	return pFatTable;
	}

CFatTable::CFatTable(CFatMountCB& aOwner)
{
    iOwner = &aOwner;
    ASSERT(iOwner);
}

CFatTable::~CFatTable()
{
    //-- destroy cache ignoring dirty data in cache
    //-- the destructor isn't an appropriate place to flush the data.
    Dismount(ETrue); 
}

//-----------------------------------------------------------------------------

/**
    Initialise the object, get data from the owning CFatMountCB
*/
void CFatTable::InitializeL()
	{
	ASSERT(iOwner);

    //-- get FAT type from the owner
    iFatType = iOwner->FatType();
    ASSERT(IsFat12() || IsFat16());

    iFreeClusterHint = KFatFirstSearchCluster;

    //-- cache the media attributes
	TLocalDriveCapsV2 caps;
	TPckg<TLocalDriveCapsV2> capsPckg(caps);
    User::LeaveIfError(iOwner->LocalDrive()->Caps(capsPckg));
	iMediaAtt = caps.iMediaAtt;
	
    //-- obtain maximal number of entries in the table
    iMaxEntries = iOwner->UsableClusters()+KFatFirstSearchCluster; //-- FAT[0] & FAT[1] are not in use

    __PRINT3(_L("CFatTable::InitializeL(), drv:%d, iMediaAtt = %08X, max Entries:%d"), iOwner->DriveNumber(), iMediaAtt, iMaxEntries);
	}

//-----------------------------------------------------------------------------

/** 
    Decrements the free cluster count.
    Note that can be quite expensive operation (especially for overrides with synchronisation), if it is called for every 
    cluster of a large file. Use more than one cluster granularity.
     
    @param  aCount a number of clusters 
*/
void CFatTable::DecrementFreeClusterCount(TUint32 aCount)
{
    __ASSERT_DEBUG(iFreeClusters >= aCount, Fault(EFatCorrupt));
    iFreeClusters -= aCount;
}

/** 
    Increments the free cluster count.
    Note that can be quite expensive operation (especially for overrides with synchronisation), if it is called for every 
    cluster of a large file. Use more than one cluster granularity.

    @param  aCount a number of clusters 
*/
void CFatTable::IncrementFreeClusterCount(TUint32 aCount)
{
	const TUint32 newVal = iFreeClusters+aCount;
    __ASSERT_DEBUG(newVal<=MaxEntries(), Fault(EFatCorrupt));
    
    iFreeClusters = newVal;
}

/** @return number of free clusters in the FAT */
TUint32 CFatTable::NumberOfFreeClusters(TBool /*aSyncOperation=EFalse*/) const
{
    return FreeClusters();
}

void CFatTable::SetFreeClusters(TUint32 aFreeClusters)
{   
    iFreeClusters=aFreeClusters;
}

/**
    Get the hint about the last known free cluster number.
    Note that can be quite expensive operation (especially for overrides with synchronisation), if it is called for every 
    cluster of a large file.

    @return cluster number supposedly close to the free one.
*/
TUint32 CFatTable::FreeClusterHint() const 
{
    ASSERT(ClusterNumberValid(iFreeClusterHint));
    return iFreeClusterHint;
} 

/**
    Set a free cluster hint. The next search fro the free cluster can start from this value.
    aCluster doesn't have to be a precise number of free FAT entry; it just needs to be as close as possible to the 
    free entries chain.
    Note that can be quite expensive operation (especially for overrides with synchronisation), if it is called for every 
    cluster of a large file.

    @param aCluster cluster number hint.
*/
void CFatTable::SetFreeClusterHint(TUint32 aCluster) 
{
    ASSERT(ClusterNumberValid(aCluster));
    iFreeClusterHint=aCluster;
} 

//-----------------------------------------------------------------------------

/**
    Find out the number of free clusters on the volume.
    Reads whole FAT and counts free clusters.
*/
void CFatTable::CountFreeClustersL()
{
    __PRINT1(_L("#- CFatTable::CountFreeClustersL(), drv:%d"), iOwner->DriveNumber());

    const TUint32 KUsableClusters = iOwner->UsableClusters();
    (void)KUsableClusters;
    
    TUint32 freeClusters = 0;
    TUint32 firstFreeCluster = 0;

    TTime   timeStart;
    TTime   timeEnd;
    timeStart.UniversalTime(); //-- take start time

    //-- walk through whole FAT table looking for free clusters
	for(TUint i=KFatFirstSearchCluster; i<MaxEntries(); ++i)
    {
	    if(ReadL(i) == KSpareCluster)
        {//-- found a free cluster
		    ++freeClusters;
            
            if(!firstFreeCluster)
                firstFreeCluster = i;
        }
	}

    timeEnd.UniversalTime(); //-- take end time
    const TInt msScanTime = (TInt)( (timeEnd.MicroSecondsFrom(timeStart)).Int64() / K1mSec);
    __PRINT1(_L("#- CFatTable::CountFreeClustersL() finished. Taken:%d ms"), msScanTime);
    (void)msScanTime;

    if(!firstFreeCluster) //-- haven't found free clusters on the volume
        firstFreeCluster = KFatFirstSearchCluster;

    ASSERT(freeClusters <= KUsableClusters);

    SetFreeClusters(freeClusters);
    SetFreeClusterHint(firstFreeCluster);
}

//-----------------------------------------------------------------------------

/**
Count the number of contiguous cluster from a start cluster

@param aStartCluster cluster to start counting from
@param anEndCluster contains the end cluster number upon return
@param aMaxCount Maximum cluster required
@leave System wide error values
@return Number of contiguous clusters from aStartCluster.
*/
TInt CFatTable::CountContiguousClustersL(TUint32 aStartCluster,TInt& anEndCluster,TUint32 aMaxCount) const
	{
	__PRINT2(_L("CFatTable::CountContiguousClustersL() start:%d, max:%d"),aStartCluster, aMaxCount);
	TUint32 clusterListLen=1;
	TInt endCluster=aStartCluster;
	TInt64 endClusterPos=DataPositionInBytes(endCluster);
	while (clusterListLen<aMaxCount)
		{
		TInt oldCluster=endCluster;
		TInt64 oldClusterPos=endClusterPos;
		if (GetNextClusterL(endCluster)==EFalse || (endClusterPos=DataPositionInBytes(endCluster))!=(oldClusterPos+(1<<iOwner->ClusterSizeLog2())))
			{
			endCluster=oldCluster;
			break;
			}
		clusterListLen++;
		}
	anEndCluster=endCluster;
	return(clusterListLen);
	}	

//-----------------------------------------------------------------------------

/**
    Extend a file or directory cluster chain, leaves if there are no free clusters (the disk is full).

    @param aNumber  amount of clusters to allocate
    @param aCluster FAT entry index to start with.

    @leave KErrDiskFull + system wide error codes
*/
void CFatTable::ExtendClusterListL(TUint32 aNumber,TInt& aCluster)
	{
	__PRINT2(_L("CFatTable::ExtendClusterListL() num:%d, clust:%d"), aNumber, aCluster);
	__ASSERT_DEBUG(aNumber>0,Fault(EFatBadParameter));
	
	while(aNumber && GetNextClusterL(aCluster))
		aNumber--;

    if(!aNumber)
        return;

	if (iFreeClusters<aNumber)
		{
		__PRINT(_L("CFatTable::ExtendClusterListL - leaving KErrDirFull"));
		User::Leave(KErrDiskFull);
		}


    TUint32 freeCluster = 0;
    
    //-- note: this can be impoved by trying to fing as long chain of free clusters as possible in FindClosestFreeClusterL()
    for(TUint i=0; i<aNumber; ++i)
		{
        freeCluster = FindClosestFreeClusterL(aCluster);
		WriteFatEntryEofL(freeCluster); //	Must write EOF for FindClosestFreeCluster to work again
		WriteL(aCluster,freeCluster);
		aCluster=freeCluster;
		}
    
    //-- decrement number of available clusters
    DecrementFreeClusterCount(aNumber);

    //-- update free cluster hint, it isn't required to be a precise value, just a hint where to start the from from
    SetFreeClusterHint(aCluster); 
    
	}

//-----------------------------------------------------------------------------

/**
Allocate and mark as EOF a single cluster as close as possible to aNearestCluster

@param aNearestCluster Cluster the new cluster should be nearest to
@leave System wide error codes
@return The cluster number allocated
*/
TUint32 CFatTable::AllocateSingleClusterL(TUint32 aNearestCluster)
	{
	__PRINT1(_L("CFatTable::AllocateSingleCluster() nearest:%d"), aNearestCluster);
	if (iFreeClusters==0)
		User::Leave(KErrDiskFull);
	const TInt freeCluster=FindClosestFreeClusterL(aNearestCluster);
	WriteFatEntryEofL(freeCluster);
	DecrementFreeClusterCount(1);

    //-- update free cluster hint, it isn't required to be a precise value, just a hint where to start the from from.
    SetFreeClusterHint(freeCluster); 

	return(freeCluster);
	}	

//-----------------------------------------------------------------------------

/**
Allocate and link a cluster chain, leaves if there are not enough free clusters.
Chain starts as close as possible to aNearestCluster, last cluster will be marked as EOF.

@param aNumber Number of clusters to allocate
@param aNearestCluster Cluster the new chain should be nearest to
@leave System wide error codes
@return The first cluster number allocated
*/
TUint32 CFatTable::AllocateClusterListL(TUint32 aNumber, TUint32 aNearestCluster)
	{
    __PRINT2(_L("#>> CFatTable::AllocateClusterList() N:%d,NearestCL:%d"),aNumber,aNearestCluster);
	__ASSERT_DEBUG(aNumber>0,Fault(EFatBadParameter));

	if (iFreeClusters<aNumber)
		{
		__PRINT(_L("CFatTable::AllocateClusterListL - leaving KErrDirFull"));
		User::Leave(KErrDiskFull);
		}

    TInt firstCluster = aNearestCluster = AllocateSingleClusterL(aNearestCluster);
	if (aNumber>1)
		ExtendClusterListL(aNumber-1, (TInt&)aNearestCluster);

	return(firstCluster);
	}	

//-----------------------------------------------------------------------------

/**
    Notify the media drive about media areas that shall be treated as "deleted" if this feature is supported.
    @param aFreedClusters array with FAT numbers of clusters that shall be marked as "deleted"
*/
void CFatTable::DoFreedClustersNotify(RClusterArray &aFreedClusters)
{
    ASSERT(iMediaAtt & KMediaAttDeleteNotify);

    const TUint clusterCount = aFreedClusters.Count();

    if(!clusterCount)
        return;
    
    FlushL(); //-- Commit the FAT changes to disk first to be safe

    const TUint bytesPerCluster = 1 << iOwner->ClusterSizeLog2();

    TInt64  byteAddress = 0;	
	TUint   deleteLen = 0;	// zero indicates no clusters accumulated yet

	for (TUint i=0; i<clusterCount; ++i)
	{
        const TUint currCluster = aFreedClusters[i];
        
        if (deleteLen == 0)
		    byteAddress = DataPositionInBytes(currCluster); //-- start of the media range
        
        deleteLen += bytesPerCluster;

        //-- if this is the last entry in the array or the net cluster number is not consecutive, notify the driver
		if ((i+1) == clusterCount || aFreedClusters[i+1] != (currCluster+1))
        {
            //__PRINT3(_L("DeleteNotify(%08X:%08X, %u), first cluster %u last cluster #%u"), I64HIGH(byteAddress), I64LOW(byteAddress), deleteLen);
			//__PRINT2(_L("   first cluster %u last cluster #%u"), I64LOW((byteAddress - iOwner->ClusterBasePosition()) >> iOwner->ClusterSizeLog2()) + 2, cluster);
            const TInt r = iOwner->LocalDrive()->DeleteNotify(byteAddress, deleteLen);
			if(r != KErrNone)
                {//-- if DeleteNotify() failed, it means that something terribly wrong happened to the NAND media; 
                 //-- in normal circumstances it can not happen. One of the reasons: totally worn out media.
                const TBool platSecEnabled = PlatSec::ConfigSetting(PlatSec::EPlatSecEnforcement);
                __PRINT3(_L("CFatTable::DoFreedClustersNotify() DeleteNotify failure! drv:%d err:%d, PlatSec:%d"),iOwner->DriveNumber(), r, platSecEnabled);

                if(platSecEnabled)
                    {
                    //-- if PlatSec is enabled, we can't afford jeopardize the security; without DeleteNotify()
                    //-- it's possible to pick up data from deleted files, so, panic the file server.
                    Fault(EFatBadLocalDrive);
                    }
                else
                    {
                    //-- if PlatSec is disabled, it's OK to ignore the NAND fault in release mode.
                    __ASSERT_DEBUG(0, Fault(EFatBadLocalDrive));
                    }        
                }
			
            
            deleteLen = 0;
        }

    }

    //-- empty the array.
    aFreedClusters.Reset();
}

//-----------------------------------------------------------------------------
/**
Mark a chain of clusters as free in the FAT. 

@param aCluster Start cluster of cluster chain to free
@leave System wide error codes
*/
void CFatTable::FreeClusterListL(TUint32 aCluster)
	{
	__PRINT1(_L("CFatTable::FreeClusterListL startCluster=%d"),aCluster);
	if (aCluster == KSpareCluster)
		return; 

	//-- here we can store array of freed cluster numbers in order to 
    //-- notify media drive about the media addresses marked as "invalid"
    RClusterArray deletedClusters;      
	CleanupClosePushL(deletedClusters);

    //-- if ETrue, we need to notify media driver about invalidated media addressses
    const TBool bFreeClustersNotify = iMediaAtt & KMediaAttDeleteNotify;

    //-- this is a maximal number of FAT entries in the deletedClusters array.
    //-- as soon as we collect this number of entries in the array, FAT cache will be flushed
    //-- and driver notified. The array will be emptied. Used to avoid huge array when deleting
    //--  large files on NAND media 
    const TUint KSubListLen = 4096;
    ASSERT(IsPowerOf2(KSubListLen));

    TUint32 lastKnownFreeCluster = FreeClusterHint();
    TUint32 cntFreedClusters = 0;

    TUint32 currCluster = aCluster;
    TInt    nextCluster = aCluster;

    for(;;)
		{
        const TBool bEOF = !GetNextClusterL(nextCluster);    
        WriteL(currCluster, KSpareCluster);

        lastKnownFreeCluster = Min(currCluster, lastKnownFreeCluster);

		// Keep a record of the deleted clusters so that we can subsequently notify the media driver. This is only safe 
		// to do once the FAT changes have been written to disk.
        if(bFreeClustersNotify)
            deletedClusters.Append(currCluster);

        ++cntFreedClusters;
        currCluster = nextCluster;

		if (bEOF || aCluster == KSpareCluster)
			break;

        if(bFreeClustersNotify && cntFreedClusters && (cntFreedClusters & (KSubListLen-1))==0)
        {//-- reached a limit of the entries in the array. Flush FAT cache, notify the driver and empty the array.
            IncrementFreeClusterCount(cntFreedClusters);
            cntFreedClusters = 0;

            SetFreeClusterHint(lastKnownFreeCluster);
            DoFreedClustersNotify(deletedClusters);
        }

    }

    //-- increase the number of free clusters and notify the driver if required.
    IncrementFreeClusterCount(cntFreedClusters);
    SetFreeClusterHint(lastKnownFreeCluster);
    
    if(bFreeClustersNotify)
    DoFreedClustersNotify(deletedClusters);

	CleanupStack::PopAndDestroy(&deletedClusters);
	}

//-----------------------------------------------------------------------------

/**
Find a free cluster nearest to aCluster, Always checks to the right of aCluster first 
but checks in both directions in the Fat.

@param aCluster Cluster to find nearest free cluster to.
@leave KErrDiskFull + system wide error codes
@return cluster number found
*/
TUint32 CFatTable::FindClosestFreeClusterL(TUint32 aCluster)
	{
    __PRINT2(_L("CFatTable::FindClosestFreeClusterL() drv:%d cl:%d"),iOwner->DriveNumber(),aCluster);

    if(!ClusterNumberValid(aCluster))
        {
        ASSERT(0);
        User::Leave(KErrCorrupt);
        }


    if(iFreeClusters==0)
	    {//-- there is no at least 1 free cluster available
    	__PRINT(_L("CFatTable::FindClosestFreeClusterL() leaving KErrDiskFull #1"));
		User::Leave(KErrDiskFull);
        }
	
    //-- 1. look if the given index contains a free entry 
    if(ReadL(aCluster) != KSpareCluster)
        {//-- no, it doesn't...
        
        //-- 2. look in both directions starting from the aCluster, looking in the right direction first
        
        const TUint32 maxEntries = MaxEntries();
        const TUint32 MinIdx = KFatFirstSearchCluster;
        const TUint32 MaxIdx = maxEntries-1; 

        TBool canGoRight = ETrue;
        TBool canGoLeft = ETrue;
    
        TUint32 rightIdx = aCluster;
        TUint32 leftIdx  = aCluster;
        
        for(TUint i=0; i<maxEntries; ++i)
            {
            if(canGoRight)
                {
                if(rightIdx < MaxIdx)
                    ++rightIdx;
                else
                    canGoRight = EFalse;
                }

            if(canGoLeft)
                {
                if(leftIdx > MinIdx)
                    --leftIdx;
                else        
                    canGoLeft = EFalse;
                }

        if(!canGoRight && !canGoLeft)
	        {
    	    __PRINT(_L("CFatTable::FindClosestFreeClusterL() leaving KErrDiskFull #2"));
            User::Leave(KErrDiskFull);
            }

        if (canGoRight && ReadL(rightIdx) == KSpareCluster)
			{
			aCluster = rightIdx;
			break;
			}

		if (canGoLeft && ReadL(leftIdx) == KSpareCluster)
			{
			aCluster = leftIdx;
			break;
			}
            }//for(..)

        }//if(ReadL(aCluster) != KSpareCluster)


    //-- note: do not update free cluster hint here by calling SetFreeClusterHint(). This is going to be 
    //-- expensive especially if overridden methods with synchronisation are called. Instead, set the number of 
    //-- the last known free cluster in the caller of this internal method.

//    __PRINT1(_L("CFatTable::FindClosestFreeClusterL found:%d"),aCluster);

    return aCluster;
	}

//-----------------------------------------------------------------------------

/**
    Converts a cluster number to byte offset in the FAT

@param aFatIndex Cluster number
    @return Number of bytes from the beginning of the FAT
*/
TUint32 CFatTable::PosInBytes(TUint32 aFatIndex) const
	{
    switch(FatType())
        {
        case EFat12:
            return (((aFatIndex>>1)<<1) + (aFatIndex>>1)); //-- 1.5 bytes per FAT entry

        case EFat16:
            return aFatIndex<<1; //-- 2 bytes per FAT entry

        default:
            ASSERT(0);
            return 0;//-- get rid of warning
        };

	}

//-----------------------------------------------------------------------------

/**
    Checks if we have at least aClustersRequired clusters free in the FAT.
    This is, actually a dummy implementation.

    @param  aClustersRequired number of free clusters required
    @return ETrue if there is at least aClustersRequired free clusters available, EFalse otherwise.
*/
TBool CFatTable::RequestFreeClusters(TUint32 aClustersRequired) const
{
    //ASSERT(aClustersRequired >0 && aClustersRequired <= iOwner->UsableClusters());
    ASSERT(aClustersRequired >0);
    return (NumberOfFreeClusters() >= aClustersRequired);
}

//-----------------------------------------------------------------------------
/**
    @return ETrue if the cluster number aClusterNo is valid, i.e. belongs to the FAT table
*/
TBool CFatTable::ClusterNumberValid(TUint32 aClusterNo) const 
    {
    return (aClusterNo >= KFatFirstSearchCluster) && (aClusterNo < iMaxEntries); 
    }



//#######################################################################################################################################
//#     CAtaFatTable class implementation 
//#######################################################################################################################################

/**
Constructor
*/
CAtaFatTable::CAtaFatTable(CFatMountCB& aOwner)
             :CFatTable(aOwner)
	{
	}


/** factory method */
CAtaFatTable* CAtaFatTable::NewL(CFatMountCB& aOwner)
{
    __PRINT1(_L("CAtaFatTable::NewL() drv:%d"),aOwner.DriveNumber());
    CAtaFatTable* pSelf = new (ELeave) CAtaFatTable(aOwner);

    CleanupStack::PushL(pSelf);
    pSelf->InitializeL();
    CleanupStack::Pop();

    return pSelf;
}


//---------------------------------------------------------------------------------------------------------------------------------------

/**
    CAtaFatTable's FAT cache factory method.
    Creates fixed cache for FAT12 or FAT16
*/
void CAtaFatTable::CreateCacheL()
{
    ASSERT(iOwner);
    const TUint32 fatSize=iOwner->FatSizeInBytes();
    __PRINT3(_L("CAtaFatTable::CreateCacheL drv:%d, FAT:%d, FAT Size:%d"), iOwner->DriveNumber(), FatType(), fatSize);
	

    //-- according to FAT specs:
    //-- FAT12 max size is 4084 entries or 6126 bytes                                               => create fixed cache for whole FAT
    //-- FAT16 min size is 4085 entries or 8170 bytes, max size is 65525 entries or 131048 bytes    => create fixed cache for whole FAT

    ASSERT(!iCache);

    //-- this is used for chaches granularity sanity check 
    const TUint32 KMaxGranularityLog2 = 18; //-- 256K is a maximal allowed granularity
    const TUint32 KMinGranularityLog2 = KDefSectorSzLog2;  //-- 512 bytes is a minimal allowed granularity

    switch(FatType())
    {
        case EFat12: //-- create fixed FAT12 cache
            iCache = CFat12Cache::NewL(iOwner, fatSize); 
        break;
    
        case EFat16: //-- create fixed FAT16 cache
        {
            TUint32 fat16_ReadGranularity_Log2; //-- FAT16 cache read granularity Log2
            TUint32 fat16_WriteGranularity_Log2;//-- FAT16 cache write granularity Log2
            
            iOwner->FatConfig().Fat16FixedCacheParams(fat16_ReadGranularity_Log2, fat16_WriteGranularity_Log2);
            
            //-- check if granularity values look sensible
            const TBool bParamsValid = fat16_ReadGranularity_Log2  >= KMinGranularityLog2 && fat16_ReadGranularity_Log2  <= KMaxGranularityLog2 &&
                                       fat16_WriteGranularity_Log2 >= KMinGranularityLog2 && fat16_WriteGranularity_Log2 <= KMaxGranularityLog2;
            
            __ASSERT_ALWAYS(bParamsValid, Fault(EFatCache_BadGranularity)); 

        
            iCache = CFat16FixedCache::NewL(iOwner, fatSize, fat16_ReadGranularity_Log2, fat16_WriteGranularity_Log2); 
        }
        break;

        default:
        ASSERT(0);
		User::Leave(KErrCorrupt);
        break;
    };

    ASSERT(iCache);
}

//---------------------------------------------------------------------------------------------------------------------------------------


/**
    Flush the FAT cache on disk
@leave System wide error codes
*/
void CAtaFatTable::FlushL()
	{
    //-- the data can't be written if the mount is inconsistent
    iOwner->CheckStateConsistentL();

	if (iCache)
		iCache->FlushL();
	}

/**
Clear any cached data
    @param aDiscardDirtyData if ETrue, non-flushed data in the cache will be discarded.
*/
void CAtaFatTable::Dismount(TBool aDiscardDirtyData)
	{
	if (iCache)
		{
        //-- cache's Close() can check if the cache is clean. 
        //-- ignore dirty data in cache if the mount is not in consistent state (it's impossible to flush cache data)
        //-- or if we are asked to do so.
        const TBool bIgnoreDirtyData = aDiscardDirtyData || !iOwner->ConsistentState();
        iCache->Close(bIgnoreDirtyData);

		delete iCache;
		iCache=NULL;
		}

	}

//---------------------------------------------------------------------------------------------------------------------------------------

/**
    Invalidate whole FAT cache.
    Depending of cache type this may just mark cache invalid with reading on demand or re-read whole cache from the media
*/
void CAtaFatTable::InvalidateCacheL()
{
    __PRINT1(_L("CAtaFatTable::InvalidateCache(), drv:%d"), iOwner->DriveNumber());

    //-- if we have a cache, invalidate it entirely
    if(iCache)
    {
        User::LeaveIfError(iCache->Invalidate());
    }
}


//---------------------------------------------------------------------------------------------------------------------------------------

/**
    Invalidate specified region of the FAT cache
    Depending of cache type this may just mark part of the cache invalid with reading on demand later
    or re-read whole cache from the media.

    @param aPos absolute media position where the region being invalidated starts.
    @param aLength length in bytes of region to invalidate / refresh
*/
void CAtaFatTable::InvalidateCacheL(TInt64 aPos, TUint32 aLength)
	{
    __PRINT3(_L("CAtaFatTable::InvalidateCacheL() drv:%d, pos:%LU, len:%u,"), iOwner->DriveNumber(), aPos, aLength);

    if(I64HIGH(aPos) || !aLength || I64HIGH(aPos+aLength))
        return; //-- FAT tables can't span over 4G 

    const TUint32 mediaPos32 = I64LOW(aPos);

    //-- we do not use other copies of FAT, so trach changes only in FAT1
    const TUint32 fat1StartPos = iOwner->StartOfFatInBytes();
    const TUint32 fat1EndPos   = fat1StartPos + iOwner->FatSizeInBytes();

    TUint32 invRegionPosStart = 0; //-- media pos where the invalidated region starts
    TUint32 invRegionLen = 0;      //-- size of the invalidated region, bytes
    
    //-- calculate the FAT1 region being invalidated
    if(mediaPos32 < fat1StartPos)
    {
        if((mediaPos32 + aLength) <= fat1StartPos)
            return;

        invRegionPosStart = fat1StartPos;
        invRegionLen = aLength - (fat1StartPos-mediaPos32);
    }
    else //if(mediaPos32 < fat1StartPos)
    {//-- mediaPos32 >= fat1StartPos)
        if(mediaPos32 >= fat1EndPos)
            return;
    
        invRegionPosStart = mediaPos32;
        
        if((mediaPos32 + aLength) <= fat1EndPos)
        {
            invRegionLen = aLength;
        }
        else 
        {
            invRegionLen = mediaPos32+aLength-fat1EndPos;
        }
    }

    //-- convert the media pos of the region into FAT entries basis, depending on the FAT type
    ASSERT(invRegionPosStart >= fat1StartPos && invRegionLen <= (TUint)iOwner->FatSizeInBytes());
    
    TUint32 startFatEntry=0;
    TUint32 numEntries = 0;

    switch(FatType())
    {
        case EFat12:
        //-- invalidate whole cache; it is not worth making calculations for such small memory region.
        User::LeaveIfError(iCache->Invalidate());
        return;

        case EFat16:
        startFatEntry = (invRegionPosStart-fat1StartPos) >> KFat16EntrySzLog2;
        numEntries = (invRegionLen + (sizeof(TFat16Entry)-1)) >> KFat16EntrySzLog2;
        break;

        default:
        ASSERT(0);
        return;
    };

    if(startFatEntry < KFatFirstSearchCluster)
    {//-- FAT[0] and FAT[1] can't be legally accessed, they are reserved entries. We need to adjust region being refreshed.
        if(numEntries <= KFatFirstSearchCluster)
            return; //-- nothing to refresh
                    
        startFatEntry += KFatFirstSearchCluster;
        numEntries -= KFatFirstSearchCluster;
    }

    User::LeaveIfError(iCache->InvalidateRegion(startFatEntry, numEntries));
	}


//-----------------------------------------------------------------------------
/**
    Initialize the object, create FAT cache if required
@leave KErrNoMemory
*/
void CAtaFatTable::InitializeL()
	{
    __PRINT1(_L("CAtaFatTable::InitializeL() drv:%d"), iOwner->DriveNumber());
    CFatTable::InitializeL();

    //-- create the FAT cache.
    ASSERT(!iCache);
    CreateCacheL();
	}


//-----------------------------------------------------------------------------
/**
    Remount the FAT table. This method call means that the media parameters wasn't changed, 
    otherwise CFatMountCB::DoReMountL() would reject it. 
    Just do some re-initialisation work.
*/
void CAtaFatTable::ReMountL()
{
    __PRINT1(_L("CAtaFatTable::ReMountL() drv:%d"), iOwner->DriveNumber());

    if(iCache)
        {
        iCache->Invalidate();
        }
    else
        {
        //-- this situation can happen when someone called CAtaFatTable::Dismount() that deletes the cache object
        //-- and then ReMount happens. We need to re-initialise this object.
        InitializeL();
        }
}


//-----------------------------------------------------------------------------
/**
    Read an entry from the FAT table

    @param aFatIndex FAT entry number to read
    @return FAT entry value
*/
TUint32 CAtaFatTable::ReadL(TUint32 aFatIndex) const
	{
    if(!ClusterNumberValid(aFatIndex))
        {
        //ASSERT(0); //-- for some silly reason some callers pass 0 here and expect it to leave
        User::Leave(KErrCorrupt);
        }


    const TUint entry = iCache->ReadEntryL(aFatIndex);
    return entry;
    }


//-----------------------------------------------------------------------------
/**
    Write an entry to the FAT table

    @param aFatIndex    aFatIndex FAT entry number to write
    @param aValue       FAT entry to write
@leave
*/
void CAtaFatTable::WriteL(TUint32 aFatIndex, TUint32 aValue)
	{
    const TUint32 KFat16EntryMask = 0x0FFFF;
    
    __PRINT2(_L("CAtaFatTable::WriteL() entry:%d, val:0x%x"), aFatIndex, aValue);
    
    if(!ClusterNumberValid(aFatIndex))
        {
        ASSERT(0); 
        User::Leave(KErrCorrupt);
        }
    
    if(aValue != KSpareCluster && (aValue < KFatFirstSearchCluster || aValue > KFat16EntryMask))
        {
        ASSERT(0);
        User::Leave(KErrCorrupt);
        }
    iCache->WriteEntryL(aFatIndex, aValue);
	}


/**
Get the next cluster in the chain from the FAT

@param aCluster number to read, contains next cluster upon return
@leave
@return False if end of cluster chain
*/
TBool CFatTable::GetNextClusterL(TInt& aCluster) const
    {
	__PRINT1(_L("CAtaFatTable::GetNextClusterL(%d)"), aCluster);
    
    const TInt nextCluster = ReadL(aCluster);
    TBool ret = EFalse; 

    switch(FatType())
        {
        case EFat12:
            ret=!IsEof12Bit(nextCluster);
        break;

        case EFat16:
            ret=!IsEof16Bit(nextCluster);
        break;

        default:
            ASSERT(0);
            return EFalse;//-- get rid of warning
        };
	
    if (ret)
        {
		aCluster=nextCluster;
	    }
	
    return ret;

    }

/**
Write EOF to aFatIndex
    @param aFatIndex index in FAT (cluster number) to be written
*/
void CFatTable::WriteFatEntryEofL(TUint32 aFatIndex)
	{
	__PRINT1(_L("CAtaFatTable::WriteFatEntryEofL(%d)"), aFatIndex);

    //-- use EOF_16Bit (0x0ffff) for all types of FAT, FAT cache will mask it appropriately
    WriteL(aFatIndex, EOF_16Bit);
	}



/** 
    Mark cluster number aFatIndex in FAT as bad 
    @param aFatIndex index in FAT (cluster number) to be written
*/
void CFatTable::MarkAsBadClusterL(TUint32 aFatIndex)
    {
    __PRINT1(_L("CAtaFatTable::MarkAsBadClusterL(%d)"),aFatIndex);

    //-- use KBad_16Bit (0x0fff7) for all types of FAT, FAT cache will mask it appropriately
    WriteL(aFatIndex, KBad_16Bit);
    
    FlushL();
	}


/**
Return the location of a Cluster in the data section of the media

@param aCluster to find location of
@return Byte offset of the cluster data 
*/
TInt64 CAtaFatTable::DataPositionInBytes(TUint32 aCluster) const
	{
    __ASSERT_DEBUG(ClusterNumberValid(aCluster), Fault(EFatTable_InvalidIndex));

    const TInt clusterBasePosition=iOwner->ClusterBasePosition();
	return(((TInt64(aCluster)-KFatFirstSearchCluster) << iOwner->ClusterSizeLog2()) + clusterBasePosition);
	}






