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
// f32\sfat\sl_check.cpp
// 
//

#include "sl_std.h"
#include "sl_scandrv.h"

const TInt KMaxBufferSize=8192;


TBool CCheckFatTable::IsEof16Bit(TInt aCluster) const
{
    return(aCluster>=0xFFF8 && aCluster<=0xFFFF);
}

TBool CCheckFatTable::IsEof12Bit(TInt aCluster) const
{
    return(aCluster>=0xFF8 && aCluster<=0xFFF);
}

TInt CCheckFatTable::MaxFatIndex() const
    {   
	__ASSERT_DEBUG((TUint)iMaxFatIndex>=KFatFirstSearchCluster, Fault(ECheckFatIndexZero));
	return(iMaxFatIndex);
	}


CCheckFatTable* CCheckFatTable::NewL(CFatMountCB* aOwner)
//
// Create a CCheckFatTable
//
	{
	CCheckFatTable* fatTable;
	fatTable=new(ELeave) CCheckFatTable(aOwner);
	return(fatTable);
	}


CCheckFatTable::CCheckFatTable(CFatMountCB* aOwner)
//
// Constructor
//
	{
	iOwner=aOwner;
	}

CCheckFatTable::~CCheckFatTable()
//
// Destructor
//
	{
	User::Free(iCheckFat);
	}


void CCheckFatTable::InitializeL()
//
// Initialize the check fat table
//
	{
	__PRINT(_L("CCheckFatTable::InitializeL"));

	TInt fatSize=iOwner->FatSizeInBytes();

	if(iCheckFat==NULL)
		iCheckFat=(TUint8*)User::AllocL(fatSize);
	else
		iCheckFat=(TUint8*)User::ReAllocL(iCheckFat,fatSize);
	Mem::FillZ(iCheckFat,fatSize);
	iMaxFatIndex=iOwner->UsableClusters()+1;
	if(iOwner->Is16BitFat())
		{
		__ASSERT_ALWAYS(iMaxFatIndex>0 && iMaxFatIndex<EOF_16Bit && !IsEof16Bit(iMaxFatIndex),User::Leave(KErrCorrupt));
		}
	else
		{
		__ASSERT_ALWAYS(iMaxFatIndex>0 && iMaxFatIndex<EOF_12Bit && !IsEof12Bit(iMaxFatIndex),User::Leave(KErrCorrupt));
		}
	WriteMediaDescriptor();
	
	__PRINT2(_L("fatSize=%d,iCheckFat=0x%x"),fatSize,iCheckFat);
	}


/**
@return ETrue if found errors in FAT 
*/
TBool CCheckFatTable::FlushL()
//
// Flush iCheckFat to the media, comparing each sector to corresponding 
// sector in all fats (cf.CFixedCache::FlushL)
//	
	{
	TBool bErrFound = EFalse;
    
    __PRINT(_L("CCheckFatTable::FlushL()"));
	HBufC8* hBuf=HBufC8::New(KMaxBufferSize);
	if (hBuf==NULL)
		hBuf=HBufC8::NewL(KMaxBufferSize/4);
	CleanupStack::PushL(hBuf);

	TUint8* ptr=(TUint8*)hBuf->Ptr();
	TInt maxSize=hBuf->Des().MaxSize();

	TPtr8 fatBuffer(ptr,maxSize);
	TInt fatSize=iOwner->FatSizeInBytes();
	TInt remainder=fatSize;
	TInt offset=iOwner->StartOfFatInBytes();
	TUint8* dataPtr=iCheckFat;
    TFatDriveInterface& drive = iOwner->DriveInterface();
	TInt fatNumber=iOwner->NumberOfFats();
	
	while(remainder)
		{
		TInt s=Min(fatBuffer.MaxSize(),remainder);
		TInt fatCount=fatNumber;
		TInt fatPos=0;
		while(fatCount)
			{
			TInt fatOffset=offset+fatPos;
			User::LeaveIfError(drive.ReadCritical(fatOffset,s,fatBuffer));
			TInt rem2=s;
			TInt offset2=fatOffset;
			TUint8* dataPtr2=dataPtr;
			TInt bufOffset=0;
			while(rem2)
				{
				TInt s2=Min(rem2,512);
				TInt r=Mem::Compare(dataPtr2,s2,fatBuffer.Ptr()+bufOffset,s2);
				if (r!=0)
					{
					bErrFound = ETrue;
                    TPtrC8 dataBuf(dataPtr2,s2);
					User::LeaveIfError(drive.WriteCritical(offset2,dataBuf));
					}
				rem2-=s2;
				offset2+=s2;
				dataPtr2+=s2;
				bufOffset+=s2;
				}
			--fatCount;
			fatPos+=fatSize;
			}
		offset+=s;
		dataPtr+=s;
		remainder-=s;
		}

	CleanupStack::PopAndDestroy();

    return bErrFound;
	}

void CCheckFatTable::WriteMediaDescriptor()
//
// Write media descriptor to first byte and 0xFF to
// remaining bytes of first two entries
//
	{
	__PRINT(_L("CCheckFatTable::WriteMediaDescriptor"));
	iCheckFat[0]=KBootSectorMediaDescriptor;
	iCheckFat[1]=0xFF;
	iCheckFat[2]=0xFF;
	if (iOwner->Is16BitFat())
		iCheckFat[3]=0xFF;
	}

TInt CCheckFatTable::PosInBytes(TInt aFatIndex) const
//
// Return number of bytes into the fat
//
	{
	TInt fatPosInBytes;
	if (iOwner->Is16BitFat())
		fatPosInBytes=aFatIndex<<1;
	else
		// this is used since 8-bit access will be used for reading/writing
		fatPosInBytes=(aFatIndex*3>>1);
	return(fatPosInBytes);
	}


TInt CCheckFatTable::PosInIndex(TInt aBytePos) const
//
// Return index given byte position in fat
//
	{
	if(iOwner->Is16BitFat())
		return(aBytePos>>1);
	else
		return((aBytePos<<1)/3);
	}


TInt CCheckFatTable::ReadL(TInt aFatIndex) const
//
// Read a value from the check fat
//
	{
	__ASSERT_ALWAYS((TUint32)aFatIndex >=KFatFirstSearchCluster && aFatIndex<=MaxFatIndex(),User::Leave(KErrCorrupt));
	TUint clusterVal;
	if(iOwner->Is16BitFat())
		clusterVal=*(TUint16*)(iCheckFat+PosInBytes(aFatIndex));
	else
		{
		TUint8* pCluster=iCheckFat+PosInBytes(aFatIndex);
		clusterVal=pCluster[0]|(pCluster[1]<<8);
		TBool oddCluster=(aFatIndex)&1;
		if(oddCluster)
			clusterVal>>=4;
		clusterVal&=0xFFF;
		}
	return(clusterVal);
	}


void CCheckFatTable::WriteL(TInt aFatIndex,TInt aValue)
//
// Write a value to the check fat
//
	{
	if(iOwner->Is16BitFat())
		__ASSERT_ALWAYS((TUint32)aFatIndex>=KFatFirstSearchCluster && aFatIndex<=MaxFatIndex() && aValue>=0 && aValue<=0xFFFF,User::Leave(KErrCorrupt));
	else
		__ASSERT_ALWAYS((TUint32)aFatIndex>=KFatFirstSearchCluster && aFatIndex<=MaxFatIndex() && aValue>=0 && aValue<=0xFFF,User::Leave(KErrCorrupt));
	TUint8* p=(TUint8*)(iCheckFat+PosInBytes(aFatIndex));
	if (iOwner->Is16BitFat())
		{ 
		*(TUint16*)p=(TUint16)aValue;
		return;
		}
	TUint8 mask=0x0F;
	TBool odd=(aFatIndex)&1;
	TUint8 fatVal;
	if(odd)
		{
		mask<<=4;
		aValue<<=4;
		fatVal=p[0];
		fatVal&=~mask;
		fatVal|=(TUint8)(aValue&0xFF);
		p[0]=fatVal;
		p[1]=(TUint8)(aValue>>8);
		}
	else
		{
		p[0]=(TUint8)(aValue&0xFF);
		fatVal=p[1];
		fatVal&=~mask;
		fatVal|=(TUint8)(aValue>>8);
		p[1]=fatVal;
		}
	return;
	}
	

TBool CCheckFatTable::GetNextClusterL(TInt& aCluster) const
//
// Get the next cluster in the chain from the check fat.
//
    {
	__PRINT(_L("CCheckFatTable::GetNextClusterL"));
	TBool ret;
	TInt nextCluster=ReadL(aCluster);
	if (iOwner->Is16BitFat())
		ret=!IsEof16Bit(nextCluster);
	else
		ret=!IsEof12Bit(nextCluster);
	if (ret)
		aCluster=nextCluster;
	return(ret);
	}

void CCheckFatTable::WriteFatEntryEofFL(TInt aCluster)
//
// Write EOF to aCluster
//
	{
	__PRINT(_L("CCheckFatTable::WriteFatEntryEofF"));
	if (iOwner->Is16BitFat())
		WriteL(aCluster,EOF_16Bit);
	else
		WriteL(aCluster,EOF_12Bit);
	}


