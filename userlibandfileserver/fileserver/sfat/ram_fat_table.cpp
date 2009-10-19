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
// f32\sfat\ram_fat_table.cpp
// FAT16 File Allocation Table classes implementation for the RAM media
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
//#     CRamFatTable class implementation 
//#######################################################################################################################################

/**
Constructor, the RamFatTable allows disk compression by redirecting the FAT

@param aOwner Owning mount.
*/
CRamFatTable::CRamFatTable(CFatMountCB& aOwner)
             :CFatTable(aOwner)
    {
    iFatTablePos=aOwner.FirstFatSector()<<aOwner.SectorSizeLog2();
    iIndirectionTablePos=iFatTablePos+aOwner.FatSizeInBytes();
    }

/** factory method */
CRamFatTable* CRamFatTable::NewL(CFatMountCB& aOwner)
{
    __PRINT1(_L("CRamFatTable::NewL() drv:%d"),aOwner.DriveNumber());

    CRamFatTable* pSelf = new (ELeave) CRamFatTable(aOwner);

    CleanupStack::PushL(pSelf);
    pSelf->InitializeL();
    CleanupStack::Pop();

    return pSelf;
}

void CRamFatTable::InitializeL() 
{
    CFatTable::InitializeL();

    ASSERT(iMediaAtt & KMediaAttVariableSize);

    iFatTablePos=iOwner->FirstFatSector()<<iOwner->SectorSizeLog2();
    iIndirectionTablePos=iFatTablePos+iOwner->FatSizeInBytes();

    //-- set RAM disk base
    TLocalDriveCapsV2 caps;
    TPckg<TLocalDriveCapsV2> capsPckg(caps);
    User::LeaveIfError(iOwner->LocalDrive()->Caps(capsPckg));
  
    iRamDiskBase = caps.iBaseAddress; 
}

/**
    Remount the FAT table. This method call means that the media parameters wasn't changed, 
    otherwise CFatMountCB::DoReMountL() would reject it. 
    Just do some re-initialisation work.
*/
void CRamFatTable::ReMountL()
{
    //-- re-initialise, actually
    ASSERT(iMediaAtt & KMediaAttVariableSize);
    ASSERT(FatType() == EFat16);

    iFatTablePos=iOwner->FirstFatSector()<<iOwner->SectorSizeLog2();
    iIndirectionTablePos=iFatTablePos+iOwner->FatSizeInBytes();

    //-- set RAM disk base
    TLocalDriveCapsV2 caps;
    TPckg<TLocalDriveCapsV2> capsPckg(caps);
    User::LeaveIfError(iOwner->LocalDrive()->Caps(capsPckg));
  
    iRamDiskBase = caps.iBaseAddress; 
}


/**
Return the start address of the Ram Drive

@return start address of the Ram Drive 
*/
TUint8 *CRamFatTable::RamDiskBase() const
    {
    return(iRamDiskBase);
    }


/**
Allocate a new cluster number

@return New cluster number
*/
TInt CRamFatTable::AllocateClusterNumber()
    {
    return(iOwner->MaxClusterNumber()-NumberOfFreeClusters());
    }

/**
Write a value to the FAT (indirection table) 

@param aFatIndex Cluster to write to
@param aValue value to write to Fat
@leave 
*/
void CRamFatTable::WriteL(TUint32 aFatIndex, TUint32 aValue)
    {
    //__PRINT(_L("CRamFatTable::WriteL"));

    __ASSERT_ALWAYS(aFatIndex>=2 && (aValue>=2 || aValue==0) && aValue<=0xFFFF,User::Leave(KErrCorrupt));
    TUint32 indirectCluster=aFatIndex;
    TUint32 indirectClusterNewVal=0;
    ReadIndirectionTable(indirectCluster);
//  If value in indirection table!=0 we assume we have already written to the indirection table
//  So just update the FAT table
    if (indirectCluster!=0 && aValue!=0)
        {
        WriteFatTable(aFatIndex,aValue);
        return;
        }
//  If value in indirection table is 0, we haven't written to it yet, though the memory has
//  already been allocated by the EnlargeL() function
    if (indirectCluster==0 && aValue!=0) // Assumes memory has already been allocated
        indirectClusterNewVal=AllocateClusterNumber();
//  Write aValue into aFaxIndex and indirectClusterNewVal into the corresponding position
//  in the indirection table    
    WriteFatTable(aFatIndex,aValue,indirectClusterNewVal);
    }   

/**
Read the value of a cluster in the Fat

@param aFatIndex A cluster to read
@leave 
@return The cluster value read
*/

TUint32 CRamFatTable::ReadL(TUint32 aFatIndex) const
    {
    __ASSERT_ALWAYS(aFatIndex>=KFatFirstSearchCluster,User::Leave(KErrCorrupt));
    TUint clusterVal=*(TUint16*)(RamDiskBase()+PosInBytes(aFatIndex)+iFatTablePos);
    return(clusterVal);
    }

/**
Write a value to the FAT and indirection table

@param aFatIndex Cluster number to write to
@param aFatValue Cluster value for Fat
@param anIndirectionValue Value for indirection table
*/
void CRamFatTable::WriteFatTable(TInt aFatIndex,TInt aFatValue,TInt anIndirectionValue)
    {
    TUint8* pos=RamDiskBase()+PosInBytes(aFatIndex);
    *(TUint16*)(pos+iFatTablePos)=(TUint16)aFatValue;
    *(TUint16*)(pos+iIndirectionTablePos)=(TUint16)anIndirectionValue;
    }

/**
Write to just the fat table

@param aFatIndex Cluster number to write to
@param aFatValue Cluster value for Fat
*/
void CRamFatTable::WriteFatTable(TInt aFatIndex,TInt aFatValue)
    {
    *(TUint16*)(RamDiskBase()+PosInBytes(aFatIndex)+iFatTablePos)=(TUint16)aFatValue;
    }

/**
Write to just the fat table

@param aFatIndex Cluster number to write to
@param aFatValue Value for indirection table
*/
void CRamFatTable::WriteIndirectionTable(TInt aFatIndex,TInt aFatValue)
    {
    *(TUint16*)(RamDiskBase()+PosInBytes(aFatIndex)+iIndirectionTablePos)=(TUint16)aFatValue;
    }

/**
Find the real location of aCluster

@param aCluster Cluster to read, contians cluster value upon return
*/
void CRamFatTable::ReadIndirectionTable(TUint32& aCluster) const
    {
    aCluster=*(TUint16*)(RamDiskBase()+PosInBytes(aCluster)+iIndirectionTablePos);
    }

/**
Copy memory in RAM drive area, unlocking required

@param aTrg Pointer to destination location
@param aSrc Pointer to source location
@param aLength Length of data to copy
@return Pointer to end of data copied
*/
TUint8* CRamFatTable::MemCopy(TAny* aTrg,const TAny* aSrc,TInt aLength)
    {
    TUint8* p=Mem::Copy(aTrg,aSrc,aLength);
    return(p);
    }

/**
    Copy memory with filling the source buffer with zeroes. Target and source buffers can overlap.
    Used on RAMDrive srinking in order to wipe data from the file that is being deleted.
    
    @param   aTrg       pointer to the target address
    @param   aSrc       pointer to the destination address
    @param   aLength    how many bytes to copy
    @return  A pointer to a location aLength bytes beyond aTrg (i.e. the location aTrg+aLength).
*/
TUint8* CRamFatTable::MemCopyFillZ(TAny* aTrg, TAny* aSrc,TInt aLength)
{
    //-- just copy src to the trg, the memory areas can overlap.
    TUint8* p=Mem::Copy(aTrg, aSrc, aLength);
    
    //-- now zero-fill the source memory area taking into account possible overlap.
    TUint8* pSrc = static_cast<TUint8*>(aSrc);
    TUint8* pTrg = static_cast<TUint8*>(aTrg);
    
    TUint8* pZFill = NULL; //-- pointer to the beginning of zerofilled area
    TInt    zFillLen = 0;  //-- a number of bytes to zero-fill
    
    if(aTrg < aSrc)
    {
        if(pTrg+aLength < pSrc)
        {//-- target and source areas do not overlap
         pZFill = pSrc;
         zFillLen = aLength;
        }
        else
        {//-- target and source areas overlap, try not to corrupt the target area
         zFillLen = pSrc-pTrg;
         pZFill = pTrg+aLength;
        }
    }
    else
    {
        if(pSrc+aLength < pTrg)
        {//-- target and source areas do not overlap
         pZFill = pSrc;
         zFillLen = aLength;
        }
        else
        {//-- target and source areas overlap, try not to corrupt the target area
         zFillLen = pSrc+aLength-pTrg;
         pZFill = pSrc;
        }
    }

    Mem::FillZ(pZFill, zFillLen);

    return(p);
}

/**
    Zero fill RAM area corresponding to the cluster number aCluster
    @param  aCluster a cluster number to be zero-filled
*/
void CRamFatTable::ZeroFillCluster(TInt aCluster)
{
        TLinAddr clusterPos= I64LOW(DataPositionInBytes(aCluster));
        Mem::FillZ(iRamDiskBase+clusterPos, 1<< iOwner->ClusterSizeLog2());     
    }


/**
Return the location of a Cluster in the data section of the media

@param aCluster to find location of
@return Byte offset of the cluster data 
*/
TInt64 CRamFatTable::DataPositionInBytes(TUint32 aCluster) const
    {
    //__PRINT(_L("CRamFatTable::DataPositionInBytes"));
    ReadIndirectionTable(aCluster);
    return(aCluster<<iOwner->ClusterSizeLog2());
    }

/**
Allocate and mark as EOF a single cluster as close as possible to aNearestCluster,
calls base class implementation but must Enlarge the RAM drive first. Allocated cluster RAM area will be zero-filled.

@param  aNearestCluster Cluster the new cluster should be nearest to
@leave  System wide error codes
@return The cluster number allocated
*/
TUint32 CRamFatTable::AllocateSingleClusterL(TUint32 aNearestCluster)
    {
    __PRINT(_L("CRamFatTable::AllocateSingleClusterL"));
    iOwner->EnlargeL(1<<iOwner->ClusterSizeLog2()); //  First enlarge the RAM drive
    TInt fileAllocated=CFatTable::AllocateSingleClusterL(aNearestCluster); //   Now update the free cluster and fat/fit
    ZeroFillCluster(fileAllocated);  //-- zero-fill allocated cluster 
    return(fileAllocated);
    }   


/**
    Extend a file or directory cluster chain, enlarging RAM drive first. Allocated clusters are zero-filled.
    Leaves if there are no free clusters (the disk is full).
    Note that method now doesn't call CFatTable::ExtendClusterListL() from its base class, be careful making changes there.

    @param aNumber      number of clusters to allocate
    @param aCluster     starting cluster number / ending cluster number after
    @leave              KErrDiskFull + system wide error codes
*/
void CRamFatTable::ExtendClusterListL(TUint32 aNumber,TInt& aCluster)
    {
    __PRINT(_L("CRamFatTable::ExtendClusterListL"));
    __ASSERT_DEBUG(aNumber>0,Fault(EFatBadParameter));
    
    iOwner->EnlargeL(aNumber<<iOwner->ClusterSizeLog2());

    while(aNumber && GetNextClusterL(aCluster))
        aNumber--;

    if(!aNumber)
        return;

    if (NumberOfFreeClusters() < aNumber)
        {
        __PRINT(_L("CRamFatTable::ExtendClusterListL - leaving KErrDirFull"));
        User::Leave(KErrDiskFull);
        }

    while (aNumber--)
        {
        const TInt freeCluster=FindClosestFreeClusterL(aCluster);

        WriteFatEntryEofL(freeCluster); //  Must write EOF for FindClosestFreeCluster to work again
        DecrementFreeClusterCount(1);
        WriteL(aCluster,freeCluster);
        aCluster=freeCluster;
        ZeroFillCluster(freeCluster); //-- zero fill just allocated cluster (RAM area)
        }

    SetFreeClusterHint(aCluster); 
  
    }

/**
Mark a chain of clusters as free in the FAT. Shrinks the RAM drive once the
clusters are free 

@param aCluster Start cluster of cluster chain to free
@leave System wide error codes
*/
void CRamFatTable::FreeClusterListL(TUint32 aCluster)
    {
    __PRINT1(_L("CRamFatTable::FreeClusterListL aCluster=%d"),aCluster);
    if (aCluster==0)
        return; // File has no cluster allocated

    const TInt clusterShift=iOwner->ClusterSizeLog2();
    TInt startCluster=aCluster;
    TInt endCluster=0;
    TInt totalFreed=0;
    TLinAddr srcEnd=0;

    while(endCluster!=EOF_16Bit)
        {
        TInt num=CountContiguousClustersL(startCluster,endCluster,KMaxTInt);
        if (GetNextClusterL(endCluster)==EFalse || endCluster==0)
            endCluster=EOF_16Bit;   // endCluster==0 -> file contained FAT loop

    //  Real position in bytes of the start cluster in the data area
        TLinAddr startClusterPos= I64LOW(DataPositionInBytes(startCluster));
    //  Sliding value when more than one block is freed
        TLinAddr trg=startClusterPos-(totalFreed<<clusterShift);
        __PRINT1(_L("trg=0x%x"),trg);

    //  Beginning of data area to move
        TLinAddr srcStart=startClusterPos+(num<<clusterShift);
        __PRINT1(_L("srcStart=0x%x"),srcStart);
    //  Position of next part of cluster chain or position of end of ram drive
        if (endCluster==EOF_16Bit)  //  Last cluster is the end of the chain
            {
        
    
        //  Fixed to use the genuine RAM drive size rather than the number
        //  of free clusters - though they *should* be the same
        //  It avoids the problem of iFreeClusters getting out of sync with 
        //  the RAM drive size but doesn't solve the issue of why it can happen...
            
            srcEnd=I64LOW(iOwner->Size());
            __PRINT1(_L("srcEnd=0x%x"),srcEnd);
            }
        else                        //  Just move up to the next part of the chain
            srcEnd=I64LOW(DataPositionInBytes(endCluster));

        //-- Copy (srcEnd-srcStart) bytes from iRamDiskBase+srcStart onto iRamDiskBase+trg
        //-- zero-filling free space to avoid leaving something important there
        ASSERT(srcEnd >= srcStart);
        if(srcEnd-srcStart > 0)
            { 
            MemCopyFillZ(iRamDiskBase+trg,iRamDiskBase+srcStart,srcEnd-srcStart);
            }    
        else
            {//-- we are freeing the cluster chain at the end of the RAMdrive; Nothing to copy to the drive space that has become free,
             //-- but nevertheless zero fill this space.
            Mem::FillZ(iRamDiskBase+trg, num<<clusterShift);
            }    
        
        totalFreed+=num;
        startCluster=endCluster;
        UpdateIndirectionTable(srcStart>>clusterShift,srcEnd>>clusterShift,totalFreed);
        }
    TInt bytesFreed=totalFreed<<clusterShift;
    
//  First free the cluster list
    CFatTable::FreeClusterListL(aCluster);
//  Now reduce the size of the RAM drive
    iOwner->ReduceSizeL(srcEnd-bytesFreed,bytesFreed);
    }

/**
Shift any clusters between aStart and anEnd backwards by aClusterShift

@param aStart Start of shift region
@param anEnd End of shift region
@param aClusterShift amount to shift cluster by
*/
void CRamFatTable::UpdateIndirectionTable(TUint32 aStart,TUint32 anEnd,TInt aClusterShift)
    {
    __PRINT(_L("CRamFatTable::UpdateIndirectionTable"));
#if defined(__WINS__)
    TUint32 count=iOwner->MaxClusterNumber();
    while (count--)
        {
        TUint32 cluster=count;
        ReadIndirectionTable(cluster);
        if (cluster>=aStart && cluster<anEnd)
            WriteIndirectionTable(count,cluster-aClusterShift);
        }
#else
    TUint16* table=(TUint16*)(RamDiskBase()+iIndirectionTablePos);
    TUint16* entry=table+iOwner->MaxClusterNumber();
    while (entry>table)
        {
        TUint32 cluster=*--entry;
        if (cluster<aStart)
            continue;
        if (cluster<anEnd)
            *entry=TUint16(cluster-aClusterShift);
        }
#endif
    }






