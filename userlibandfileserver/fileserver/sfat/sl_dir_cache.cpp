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
// f32\sfat\sl_dir_cache.cpp
//
//

#include "sl_std.h"
#include "sl_dir_cache.h"

//======================================================================
TDynamicDirCachePage::~TDynamicDirCachePage()
	{
	}

/**
The static cache page creation function.
Cache page objects are not supposed to be created on the stack, so this factory function is required.
*/
TDynamicDirCachePage* TDynamicDirCachePage::NewL(CDynamicDirCache* aOwnerCache, TInt64 aStartMedPos, TUint8* aStartRamAddr)
	{
	return new(ELeave) TDynamicDirCachePage(aOwnerCache, aStartMedPos, aStartRamAddr);
	}

/**
Cache page constructor.
@param	aOwnerCache	pointer of the cache that owns this page
@param	aStartMedPos	the start address on the media that this page caches
@param	aStartRamAddr	the start address in the ram that this page content lives
*/
TDynamicDirCachePage::TDynamicDirCachePage(CDynamicDirCache* aOwnerCache, TInt64 aStartMedPos, TUint8* aStartRamAddr)
:iStartMedPos(aStartMedPos),
iStartRamAddr(aStartRamAddr),
iOwnerCache(aOwnerCache),
iValid(EFalse),
iLocked(EFalse)
	{
//	__PRINT3(_L("TDynamicDirCachePage::TDynamicDirCachePage(aStartMedPos=%lx, aStartRamAddr=0x%X, aPageSize=%u)"), aStartMedPos, aStartRamAddr, PageSizeInBytes());
	iType = EUnknown;
	}

/////////////////////////////// class CDynamicDirCache::TLookupEntry ///////////////////////////
/**
Required by RHashSet<TLookupEntry> to identify individual hash set entries.
@see	RHashSet
*/
TBool IdentityFunction(const TLookupEntry& aEntry1, const TLookupEntry& aEntry2)
	{
	// only check starting med pos for hash searching
	return aEntry1.iPos == aEntry2.iPos;
	}
/**
Required by RHashSet<TLookupEntry> to generate hash value.
@see	RHashSet
*/
TUint32 HashFunction(const TLookupEntry& aEntry)
	{
	return (DefaultHash::Integer(I64HIGH(aEntry.iPos)) + DefaultHash::Integer(I64LOW(aEntry.iPos)));
	}

/////////////////////////////// class CDynamicDirCache ///////////////////////////
CDynamicDirCache::~CDynamicDirCache()
	{
//	__PRINT(_L("CDynamicDirCache::~CDynamicDirCache()"));

	// we should never decommit locked pages
    while (!iLockedQ.IsEmpty())
		{
		TDynamicDirCachePage* page = iLockedQ.Last();
		DeQueue(page);		// remove from queue
		LookupTblRemove(page->StartPos());	// remove from lookuptable
		delete page;
		}
	ASSERT(iLockedQCount == 0);

	while (!iUnlockedQ.IsEmpty())
		{
		TDynamicDirCachePage* page = iUnlockedQ.Last();
		DeQueue(page);		// remove from queue
		LookupTblRemove(page->StartPos());	// remove from lookuptable
		DecommitPage(page);	// inform cache client to decommit page memory
		delete page;
		}
	ASSERT(iUnlockedQCount == 0);

	delete iActivePage;

	ASSERT(iLookupTable.Count() == 0);
	iLookupTable.Close();
    if (iCacheMemoryClient)
    	iCacheMemoryClient->Reset();
	}

/**
Constructor of CDynamicDirCache.
@param	aDrive	local drive interface to read/write media
@param	aMinPageNum	the minimum page number for the cache, includes iActive page and locked pages.
@param	aMaxPageNum	the maximum page number for the cache, includes iActive page, locked pages and unlocked pages.
@param	aPageSizeInBytesLog2	the log2 value of page size in bytes, assumes page size is always a power of two
*/
CDynamicDirCache::CDynamicDirCache(TFatDriveInterface& aDrive, TUint32 aMinPageNum, TUint32 aMaxPageNum, TUint32 aPageSizeInBytesLog2)
:iPageSizeLog2(aPageSizeInBytesLog2),
iMinSizeInPages(aMinPageNum),
iMaxSizeInPages(aMaxPageNum),
iDrive(aDrive),
iLockedQ(_FOFF(TDynamicDirCachePage, iLink)),
iUnlockedQ(_FOFF(TDynamicDirCachePage, iLink)),
iLockedQCount(0),
iUnlockedQCount(0),
iHashFunction(HashFunction),
iIdentityFunction(IdentityFunction),
iLookupTable(iHashFunction, iIdentityFunction)
	{
	iPageSizeInBytes = 1 << aPageSizeInBytesLog2;
	iCacheDisabled = EFalse;
    iMinCacheSizeInBytes = aMinPageNum << aPageSizeInBytesLog2;
    iMaxCacheSizeInBytes = aMaxPageNum << aPageSizeInBytesLog2;
    ASSERT(iPageSizeInBytes && iPageSizeInBytes <= iMinCacheSizeInBytes && iMinCacheSizeInBytes <= iMaxCacheSizeInBytes);
	// initial value, will be reset from outside
	iCacheBasePos = 0;
	}

/**
Second phase constructor of CDynamicDirCache.
@param	aClientName the identification of cache memeory client this cache connects
*/
void CDynamicDirCache::ConstructL(const TDesC& aClientName)
	{
//    __PRINT3(_L("CDynamicDirCache::ConstructL(Min=%u, Max=%u, page=%u)"), iMinCacheSizeInBytes, iMaxCacheSizeInBytes, iPageSizeInBytes);
	CCacheMemoryManager* manager = CCacheMemoryManagerFactory::CacheMemoryManager();
	if (manager)
		{
		// client will register itself onto cache memory manager when created
		// note this operation may leave under OOM condition
		iCacheMemoryClient = manager->ConnectClientL(aClientName, iMinSizeInPages * PageSizeInSegs(), iMaxSizeInPages * PageSizeInSegs());
		}
	else
		{
		User::Leave(KErrNotSupported);
		}

	ASSERT(iCacheMemoryClient);
	if (!iCacheMemoryClient)
		{
		User::Leave(KErrNoMemory);
		}

	// reserve active page
	iActivePage = AllocateAndLockNewPageL(0);
	ASSERT(iActivePage);
	if (!iActivePage)
		{
		User::Leave(KErrNoMemory);
		}
	iActivePage->SetPageType(TDynamicDirCachePage::EActivePage);
	}

/**
Static factory function of CDynamicDirCache
*/
CDynamicDirCache* CDynamicDirCache::NewL(TFatDriveInterface& aDrive, TUint32 aMinPageNum, TUint32 aMaxPageNum, TUint32 aPageSizeLog2, const TDesC& aClientName)
    {
//    __PRINT3(_L("CDynamicDirCache::NewL(MinPageNum=%u, MaxPageNum=%u, page=%u)"), aMinPageNum, aMaxPageNum, 1<<aPageSizeLog2);
    CDynamicDirCache* pSelf = new (ELeave) CDynamicDirCache(aDrive, aMinPageNum, aMaxPageNum, aPageSizeLog2);
    CleanupStack::PushL(pSelf);
    pSelf->ConstructL(aClientName);
    CleanupStack::Pop();
    return pSelf;
    }

/**
Insert an unlocked page into the last position of the locked queue, may squeeze the original last page into
the unlocked queue.
This function is used on last visited but 'unlocked' pages to avoid excessive lock/unlock calls to cache memory
manager as contiguous entry reading/writing often happens on the same page.
@param	aPage	the page to be inserted.
@pre	the page type of aPage should only be TDynamicDirCachePage::EUnknown
*/
void CDynamicDirCache::MakePageLastLocked(TDynamicDirCachePage* aPage)
	{
	// this function should not be called on active pages
	ASSERT(aPage->iType == TDynamicDirCachePage::EUnknown);

	if (iLockedQ.IsEmpty())
		{
		// if locked queue is empty, add it onto the locked queue directly
		AddFirstOntoQueue(aPage, TDynamicDirCachePage::ELocked);
		}
	else
		{
		// otherwise, we squeeze for the last position on locked queue
		while (iLockedQCount + 1 >= iMinSizeInPages)
			{
			TDynamicDirCachePage* last = iLockedQ.Last();
			DeQueue(last);
			UnlockPage(last);
			AddFirstOntoQueue(last, TDynamicDirCachePage::EUnlocked);
			}

		// iLockedQCount + 1 < iMinSizeInPages
		iLockedQ.AddLast(*aPage);
		aPage->SetPageType(TDynamicDirCachePage::ELocked);
		iLockedQCount++;
		}
	}

/**
Read data from a single page. If the page is not found or not valid anymore, read media onto iActive page
first.
@param	aPos	the starting position of the media address to be read.
@param	aLength	the length of the content to be read.
@param	aDes	the descriptor to contain the content.
@pre	aLength should be no more than page size.
*/
void CDynamicDirCache::ReadDataFromSinglePageL(TInt64 aPos, TInt aLength, TDes8& aDes)
	{
    //-- the data section is in the cache page entirely, take data directly from the cache
	TDynamicDirCachePage* pPage = FindPageByPos(aPos);
    if (pPage)
    	{
		// lock page before reading,
    	if (LockPage(pPage) != NULL)
    		{
    		// read data
            aDes.Copy(pPage->PtrInPage(aPos), aLength);

            // if page is from unlocked queue, insert it onto the last page of the locked
            //  queue. this is to avoid excessive locking and unlocking operations that is
            //  highly likely to happen for following reads.
            if (pPage->PageType() == TDynamicDirCachePage::EUnlocked)
            	{
            	DeQueue(pPage);
            	MakePageLastLocked(pPage);
            	}
    		}
    	else	// page locking failed
    		{
    		ASSERT(pPage->PageType() == TDynamicDirCachePage::EUnlocked);
    		DeQueue(pPage);
    		LookupTblRemove(pPage->StartPos());
    		DecommitPage(pPage);
    		delete pPage;
    		pPage = NULL;
    		}
    	}

	if (!pPage)
		{
        // if page not found or page data not valid anymore, use active page to read data in
        pPage = UpdateActivePageL(aPos);
        // read data
        aDes.Copy(pPage->PtrInPage(aPos), aLength);
    	}

	}

//====================================================================
/**
Implementation of pure virtual function.
@see	MWTCacheInterface::ReadL()
*/
void CDynamicDirCache::ReadL(TInt64 aPos, TInt aLength, TDes8& aDes)
	{
#ifdef _DEBUG
    if(iCacheDisabled)
        {
        // cache is disabled for debug purposes
        __PRINT(_L("CDynamicDirCache disabled"));
        User::LeaveIfError(iDrive.ReadNonCritical(aPos, aLength, aDes));
        return;
        }
#endif //_DEBUG

    aDes.Zero();
    const TUint32 PageSz = iPageSizeInBytes;//-- cache page size

    TInt64 pageStartMedPos = CalcPageStartPos(aPos);
    const TUint32 bytesToPageEnd = (TUint32)(pageStartMedPos + PageSz - aPos); //-- number of bytes from aPos to the end of the page

//    __PRINT5(_L("CDynamicDirCache::ReadL: aPos=%lx, aLength=%x, page:%lx, pageSz:%x, bytesToPageEnd=%x"), aPos, aLength, pageStartMedPos, PageSz, bytesToPageEnd);
    // if all data needed is on a single page
    if((TUint32)aLength <= bytesToPageEnd)
        {
        ReadDataFromSinglePageL(aPos, aLength, aDes);
        }
    // or data to be read cross cache page boundary or probably we have more than 1 page to read
    else
        {
        __PRINT(_L("CDynamicDirCache::ReadL() CROSS PAGE!"));
        TUint32 dataLen(aLength);   //-- current data length
        TInt64  currMediaPos(aPos); //-- current media position

        //-- 1. read data that are already in the current page
        ReadDataFromSinglePageL(currMediaPos, bytesToPageEnd, aDes);
        dataLen -= bytesToPageEnd;
        currMediaPos += bytesToPageEnd;

        TPtr8 dataNext = aDes.MidTPtr(aDes.Length());

        //-- 2. read whole pages of data
        while (dataLen >= PageSz)
        	{
        	//-- find out if currMediaPos is in cache. If not, find a spare page and read data there
        	ReadDataFromSinglePageL(currMediaPos, PageSz, dataNext);
            currMediaPos += PageSz;
            dataLen -= PageSz;
            dataNext = dataNext.MidTPtr(dataNext.Length());
        	}

        //-- 3. read the rest of the data
        if(dataLen > 0)
            {
        	ReadDataFromSinglePageL(currMediaPos, dataLen, dataNext);
            }
        } //else((TUint32)aLength <= bytesToPageEnd)
	}

/**
Write data through a single page. If the page is not found or not valid anymore, read media onto iActive page
first, then write data through iActive page.
@param	aPos	the starting position of the media address to be write.
@param	aData	the starting address that the writing content lives in the ram.
@param	aDataLen	the length of the content to be written.
@pre	aDataLen	should be no more than page size.
*/
void CDynamicDirCache::WriteDataOntoSinglePageL(TInt64 aPos, const TUint8* aData, TUint32 aDataLen)
	{
	ASSERT(aDataLen <= iPageSizeInBytes);
    //-- the data section is in the cache page entirely, take data directly from the cache
	TDynamicDirCachePage* pPage = FindPageByPos(aPos);
    if (pPage)
    	{
		// lock page before writing,
    	if (LockPage(pPage) != NULL)
    		{
    		//-- update cache
            Mem::Copy(pPage->PtrInPage(aPos), aData, aDataLen);
    		}
    	else
    		{
    		ASSERT(pPage->PageType() == TDynamicDirCachePage::EUnlocked);
    		DeQueue(pPage);
    		LookupTblRemove(pPage->StartPos());
    		DecommitPage(pPage);
    		delete pPage;
    		pPage = NULL;
    		}
    	}

    // if page not found or page data not valid anymore, use active page to read data in
    if (!pPage)
    	{
        pPage = UpdateActivePageL(aPos);
        //-- update cache
        Mem::Copy(pPage->PtrInPage(aPos), aData, aDataLen);
    	}

	// make sure the page is unlocked after use
	if (pPage->PageType() == TDynamicDirCachePage::EUnlocked)
		{
		UnlockPage(pPage);
		}

	// always make writting events MRU
	MakePageMRU(aPos);
    return;
	}

/**
Implementation of pure virtual function.
@see	MWTCacheInterface::WriteL()
*/
void CDynamicDirCache::WriteL(TInt64 aPos,const TDesC8& aDes)
	{
#ifdef _DEBUG
    if(iCacheDisabled)
        {
        // cache is disabled for debug purposes
        __PRINT(_L("CDynamicDirCache disabled"));
        User::LeaveIfError(iDrive.WriteCritical(aPos,aDes));
        return;
        }
#endif //_DEBUG

    TUint32 dataLen = aDes.Size();
    const TUint8* pData   = aDes.Ptr();
    const TUint32 PageSz  = iPageSizeInBytes; //-- cache page size

    TInt64 pageStartMedPos = CalcPageStartPos(aPos);
    TUint32 bytesToPageEnd = (TUint32)(pageStartMedPos + PageSz - aPos);

//    __PRINT5(_L("CDynamicDirCache::WriteL: aPos=%lx, aLength=%x, page:%lx, pageSz:%x, bytesToPageEnd=%x"), aPos, dataLen, pageStartMedPos, PageSz, bytesToPageEnd);

    if(dataLen <= bytesToPageEnd)
        {
        WriteDataOntoSinglePageL(aPos, pData, dataLen);
        }
    else
        {
        __PRINT(_L("CDynamicDirCache::WriteL() CROSS PAGE!"));

        //-- Data to be written cross cache page boundary or probably we have more than 1 page to write
        TInt64  currMediaPos(aPos);

        //-- 1. update the current page
        WriteDataOntoSinglePageL(currMediaPos, pData, bytesToPageEnd);

        pData += bytesToPageEnd;
        currMediaPos += bytesToPageEnd;
        dataLen -= bytesToPageEnd;

        //-- 2. write whole pages of data to the cache
        while (dataLen >= PageSz)
        	{
            WriteDataOntoSinglePageL(currMediaPos, pData, PageSz);

            pData += PageSz;
            currMediaPos += PageSz;
            dataLen -= PageSz;
        	}

        //-- 3. write the rest of the data
        if(dataLen > 0)
            {
            WriteDataOntoSinglePageL(currMediaPos, pData, dataLen);
            }
        }// else(dataLen <= bytesToPageEnd)


    //-- write data to the media
    const TInt nErr = iDrive.WriteCritical(aPos,aDes);
    if(nErr != KErrNone)
        {//-- some serious problem occured during writing, invalidate cache.
        InvalidateCache();
        User::Leave(nErr);
        }
	}

/**
Implementation of pure virtual function.
@see	MWTCacheInterface::InvalidateCache()
*/
void CDynamicDirCache::InvalidateCache(void)
	{
	__PRINT2(_L("CDynamicDirCache::InvalidateCache(locked=%d, unlocked=%d)"), iLockedQCount, iUnlockedQCount);
	// we should never decommit locked pages as they needs to be reserved anyway
	// the overhead of unnecessary page committing operations
	while(!iLockedQ.IsEmpty())
		{
		TDynamicDirCachePage* page = iLockedQ.Last();
		DeQueue(page);						// remove from queue
		LookupTblRemove(page->StartPos());	// remove from lookuptable
		delete page;
		}
	ASSERT(iLockedQCount == 0);

	// however we should decommit unlocked pages here
	while (!iUnlockedQ.IsEmpty())
		{
		TDynamicDirCachePage* page = iUnlockedQ.Last();
		DeQueue(page);						// remove from queue
		LookupTblRemove(page->StartPos());	// remove from lookuptable
		DecommitPage(page);					// inform cache client to decommit page memory
		delete page;
		}
	ASSERT(iUnlockedQCount == 0);

    ASSERT(iLookupTable.Count() == 0);
	iLookupTable.Close();

	ASSERT(iCacheMemoryClient);

	// initialize cache state.
	// Note that once the client is reset, all pages lose connection with the client
	//	including the active page. So we will need to reset and re-allocate active page
	//	properly.
	if (iCacheMemoryClient)
    	iCacheMemoryClient->Reset();

	// reset and re-allocate active page
	ResetPagePos(iActivePage);				// reset start media position (0), invalidate page content
	TUint8* startRamAddr = iCacheMemoryClient->AllocateAndLockSegments(PageSizeInSegs());
	// this should always succeed as the client has just been reset and there are always reserved pages
	ASSERT(startRamAddr);
	iActivePage->SetStartPtr(startRamAddr);	// set RAM address
	}


/** this method isn't implemented*/
void CDynamicDirCache::InvalidateCachePage(TUint64 /*aPos*/)
    {
    ASSERT(0);
    }


/**
Implementation of pure virtual function.
@see	MWTCacheInterface::PosCached()
*/
TUint32 CDynamicDirCache::PosCached(const TInt64& aPos, TInt64& aCachedPosStart)
	{
	const TInt64 pageStartMedPos = CalcPageStartPos(aPos);

	// only search the page in lookup table
	// NOTE: we don't count the active page into acount here,
	// this is to avoid pulling next pages recursively
	TDynamicDirCachePage* pPage = LookupTblFind(pageStartMedPos);

	// then check if page is still valid if page is on Unlocked Page Queue
	if (pPage && pPage->PageType() == TDynamicDirCachePage::EUnlocked)
		{
		if (LockPage(pPage) != NULL)
			{
//			__PRINT1(_L("CDynamicDirCache::PosCached: page(0x%lx) found on Unlocked Queue!"), aPos);
			// have to unlock it before returning, otherwise there will be memory leak
			UnlockPage(pPage);
    	    aCachedPosStart = pPage->StartPos();
			return pPage->PageSizeInBytes();
			}
		else	// if the unlocked page is not valid anymore, remove it
			{
    		DeQueue(pPage);
    		LookupTblRemove(pPage->StartPos());
    		DecommitPage(pPage);
    		delete pPage;
    		pPage = NULL;
			}
		}
	// otherwise if page is already locked or valid active page
	else if (pPage)
		{
		__PRINT1(_L("CDynamicDirCache::PosCached: page(0x%lx) on Locked Queue!"), aPos);
	    aCachedPosStart = pPage->StartPos();
		return pPage->PageSizeInBytes();
		}

	// page is not found or not valid anymore
	return 0;
	}

/**
Implementation of pure virtual function.
@see	MWTCacheInterface::CacheSizeInBytes()
*/
TUint32 CDynamicDirCache::CacheSizeInBytes()  const
	{
	return iMaxCacheSizeInBytes;
	}

/**
Implementation of pure virtual function.
@see	MWTCacheInterface::Control()
*/
TInt CDynamicDirCache::Control(TUint32 aFunction, TUint32 aParam1, TAny* aParam2)
	{
    TInt r = KErrNotSupported;
#ifdef _DEBUG
    (void)aParam2;
    switch(aFunction)
        {
        // disable / enable cache, for debug
        // if aParam1 != 0 cache will be disabled, enabled otherwise
        case EDisableCache:
            iCacheDisabled = aParam1 ? 1 : 0;
            r = KErrNone;
        break;

        // dump cache, for debug
        case EDumpCache:
        	{
        	RFs fs;
        	fs.Connect();
        	const TUint32 debugRegister = DebugRegister();
        	fs.SetDebugRegister(debugRegister|KFSYS);
        	Dump();
        	fs.SetDebugRegister(debugRegister);
        	fs.Close();
        	break;
        	}
        case ECacheInfo:
        	{
        	RFs fs;
        	fs.Connect();
        	const TUint32 debugRegister = DebugRegister();
        	fs.SetDebugRegister(debugRegister|KFSYS);
        	Info();
        	fs.SetDebugRegister(debugRegister);
        	fs.Close();
        	break;
        	}

        default:
            __PRINT1(_L("CDynamicDirCache::Control() invalid function: %d"), aFunction);
            ASSERT(0);
        break;
        }

#else
    (void)aFunction; //-- supress warnings
    (void)aParam1;
    (void)aParam2;
    User::Invariant(); //-- don't call this method in release build
#endif //_DEBUG

    return r;
	}

/**
Implementation of pure virtual function.
@see	MWTCacheInterface::SetCacheBasePos()
*/
void CDynamicDirCache::SetCacheBasePos(TInt64 aBasePos)
	{
	iCacheBasePos = aBasePos;
	}

/**
Implementation of pure virtual function.
@see	MWTCacheInterface::SetCacheBasePos()
*/
TUint32 CDynamicDirCache::PageSizeInBytesLog2() const
	{
	return iPageSizeLog2;
	}

/**
Implementation of pure virtual function.
@see	MWTCacheInterface::MakePageMRU()
*/
void CDynamicDirCache::MakePageMRU(TInt64 aPos)
	{
	__PRINT1(_L("MakePageMRU (%lx)"), aPos);
//	__PRINT4(_L("Current Cache State: iLockedQCount=%d, iUnlockedQCount=%d, iLookupTbl=%d, iMaxSizeInPages=%d"), iLockedQCount, iUnlockedQCount, iLookupTable.Count(), iMaxSizeInPages);
	// check the MRU page first, if it is already the MRU page, we can return immediately
	TInt64 pageStartMedPos = CalcPageStartPos(aPos);
	if (!iLockedQ.IsEmpty())
		{
		if (iLockedQ.First()->StartPos() == pageStartMedPos)
			{
			return;
			}
		}

	TDynamicDirCachePage* pPage = FindPageByPos(aPos);
    if (pPage)
    	{
    	ASSERT(pPage->IsValid());
		// lock page before make it MRU
    	if (pPage->PageType() == TDynamicDirCachePage::EUnlocked)
    		{
    		ASSERT(!pPage->IsLocked());
        	if (LockPage(pPage) == NULL)
        		{
        		DeQueue(pPage);
        		LookupTblRemove(pPage->StartPos());
        		DecommitPage(pPage);
        		delete pPage;
        		pPage = NULL;
        		}
    		}
    	else
    		{
    		// error checking: page should either be locked or active
    		ASSERT(LockPage(pPage) != NULL);
    		}
    	}

    // if page not found or page data not valid anymore, use active page to read data
    if (!pPage)
    	{
        TRAPD(err, pPage = UpdateActivePageL(aPos));
        if (err != KErrNone)
        	{
        	// problem occurred reading active page, return immediately.
        	return;
        	}
    	}

    // by now, the page is either locked or active page
	ASSERT(pPage && pPage->IsValid() && pPage->IsLocked());

	switch (pPage->PageType())
		{
		// if the page is the active page, we will need to find a new active page for replacement
		case TDynamicDirCachePage::EActivePage:
			{
			TDynamicDirCachePage* newAP = NULL;
			// if there is more cache room available, try to create a new page first
			if (!CacheIsFull())
				{
				// allocate and lock a new page
				TRAPD(err, newAP = AllocateAndLockNewPageL(0));
				// if any error ocurrs, return immediately
				if (err != KErrNone)
					{
					// unlock the page that was originally unlocked before leave
					if (pPage->PageType() == TDynamicDirCachePage::EUnlocked)
						{
						UnlockPage(pPage);
						}
					return;
					}

				if (newAP)
					{
					// replace the active page with the new page
					newAP->SetPageType(TDynamicDirCachePage::EActivePage);
					iActivePage = newAP;
					}
				}

			// if cache has grown to its max size, or new page allocation failed
			if (!newAP)
				{
				// try to lock the LRU page on the unlocked page queque first
				if (!iUnlockedQ.IsEmpty())
					{
					newAP = iUnlockedQ.Last();
					ASSERT(newAP->IsValid());
					if (LockPage(newAP) != NULL)
						{
						// deque, reset pos, set new type
						DeQueue(newAP);
		        		LookupTblRemove(newAP->StartPos());
						ResetPagePos(newAP);
						newAP->SetPageType(TDynamicDirCachePage::EActivePage);
						// replace active page
						iActivePage = newAP;
						}
					// if falied locking the LRU page from unclocked queque,
					// delete it
					else
						{
						DeQueue(newAP);
		        		LookupTblRemove(newAP->StartPos());
		        		DecommitPage(newAP);
		        		delete newAP;
		        		newAP = NULL;
						}
					}
				}

			// if still have not found new active page
			// grab the LRU page from Locked Page Queue for active page
			if (!newAP)
				{
				ASSERT(!iLockedQ.IsEmpty());
				newAP = iLockedQ.Last();
				// deque, reset pos, set new type
				DeQueue(newAP);
        		LookupTblRemove(newAP->StartPos());
				ResetPagePos(newAP);
				newAP->SetPageType(TDynamicDirCachePage::EActivePage);
				// replace active page
				iActivePage = newAP;
				}

			// we should always be able to find a locked page for active page
			ASSERT(newAP != NULL);

			// make original page (i.e. former active page) MRU
			// add onto locked queue
			AddFirstOntoQueue(pPage, TDynamicDirCachePage::ELocked);
			// add onto lookuptbl, as active page is not on lookup tbl originally
			LookupTblAdd(pPage);
			// check cache limit
			CheckThresholds();
			return;
			}
		case TDynamicDirCachePage::EUnlocked:
			{
			// if page was originally on Unlocked Page Queque, remove it from Unlocked Page Queue, add it
			// to the Locked Page Queue and make it MRU
			DeQueue(pPage);
			AddFirstOntoQueue(pPage, TDynamicDirCachePage::ELocked);
			// check cache limit
			CheckThresholds();
			return;
			}
		case TDynamicDirCachePage::ELocked:
			{
			// otherwise the page was on Locked Page Queue, make it MRU
			// no need to check cache limit
			if (pPage != iLockedQ.First())
				{
				DeQueue(pPage);
				AddFirstOntoQueue(pPage, TDynamicDirCachePage::ELocked);
				return;
				}
			break;
			}
		default:
			ASSERT(0);
		}
	}

//====================================================================
/**
Internal query function, to check if aPos is cached or not. iActive page is included in searching.
*/
TDynamicDirCachePage* CDynamicDirCache::FindPageByPos(TInt64 aPos)
	{
	__PRINT1(_L("CDynamicDirCache::FindPageByPos(aPos=%lx)"), aPos);
    // align the page position
	TInt64 pageStartMedPos = CalcPageStartPos(aPos);

	if ((iActivePage->StartPos() == pageStartMedPos))
		{
		ASSERT(iActivePage->IsValid());
		return iActivePage;
		}

	// search in lookup table
	return LookupTblFind(pageStartMedPos);
	}

/**
read a page length data into iActive page and return iActive page if read is successful.
*/
TDynamicDirCachePage* CDynamicDirCache::UpdateActivePageL(TInt64 aPos)
	{
    // align the page position
	TInt64 pageStartMedPos = CalcPageStartPos(aPos);

	if (iActivePage->StartPos() == pageStartMedPos && iActivePage->IsValid())
		{
		return iActivePage;
		}

	__PRINT2(_L("CDynamicDirCache::UpdateActivePageL(aPos=%lx, active=%lx)"), aPos, iActivePage->StartPos());

	// set start med pos value, no other effects, only available to active page
	iActivePage->SetPos(pageStartMedPos);

	// read data, make active page valid
	TUint8* data = iActivePage->PtrInPage(iActivePage->iStartMedPos);
    TPtr8 dataPtr(data, iPageSizeInBytes);
    const TInt nErr = iDrive.ReadNonCritical(iActivePage->iStartMedPos, iPageSizeInBytes, dataPtr);
    if(nErr !=KErrNone)
        {
        // some serious problem occured during reading, invalidate cache.
        InvalidateCache();
        User::Leave(nErr);
        }
    iActivePage->SetValid(ETrue);

    return iActivePage;
	}

/**
Check if the number of (locked pages + iActive page) and unlocked pages have exceeded minimum allowed page
number and maximum allowed page number respectively.
*/
void CDynamicDirCache::CheckThresholds()
	{
	while (iLockedQCount + 1 > iMinSizeInPages)
		{
		TDynamicDirCachePage* movePage = iLockedQ.Last();
		UnlockPage(movePage);
		DeQueue(movePage);
		TInt err = LookupTblRemove(movePage->StartPos());
		ASSERT(err == KErrNone);

		// if it is a valid page, add onto unlocked queue
		if (movePage->StartPos() != 0)
			{
			ASSERT(movePage->IsValid());
			AddFirstOntoQueue(movePage, TDynamicDirCachePage::EUnlocked);
			err = LookupTblAdd(movePage);
			ASSERT(err == KErrNone);
			}
		else // reserved page, delete
			{
			DecommitPage(movePage);
			delete movePage;
			}
		}

	// if unlocked queue exceeds limit, delete LRU page
	// note: all pages on unlocked queue should be valid
	while (iUnlockedQCount > iMaxSizeInPages - iMinSizeInPages)
		{
		TDynamicDirCachePage* removePage = iUnlockedQ.Last();
		ASSERT(removePage->StartPos() != 0 && removePage->IsValid());
		DeQueue(removePage);
		LookupTblRemove(removePage->StartPos());
		DecommitPage(removePage);
		delete removePage;
		}
	}

/**
Try to create a new page and lock the page content when it is created. This function should only be called
when creating iActive page or making a page MRU (which might result in page evictions).
@return	the pointer of the newly created page, or NULL if allocation failed.
@param	aStartMedPos	the starting media address of the page to be created.
@pre	aStartMedPos should not already be existing in the cache.
*/
TDynamicDirCachePage* CDynamicDirCache::AllocateAndLockNewPageL(TInt64 aStartMedPos)
	{
	__PRINT1(_L("CDynamicDirCache::AllocateAndLockNewPageL(aStartMedPos=%lx)"), aStartMedPos);

	TUint8* startRamAddr = iCacheMemoryClient->AllocateAndLockSegments(PageSizeInSegs());
	if (startRamAddr)
		{
		// create new page and return
		TDynamicDirCachePage* pPage = TDynamicDirCachePage::NewL(this, aStartMedPos, startRamAddr);
		pPage->SetLocked(ETrue);
		pPage->SetValid(EFalse);
		return pPage;
		}

	return NULL;
	}

#ifdef _DEBUG
/**
Dump cache information, only enabled in debug mode.
@see CDynamicDirCache::Control()
*/
void CDynamicDirCache::Info() const
	{
	__PRINT(_L("======== CDynamicDirCache::Info ========="));
	const TUint32 SegmentSizeInBytesLog2 = CCacheMemoryManagerFactory::CacheMemoryManager()->SegmentSizeInBytesLog2();
	// page size
	__PRINT1(_L("=== Pages size:               [%d Bytes]"), iPageSizeInBytes);
	__PRINT1(_L("=== Segment size:             [%d Bytes]"), 1 << SegmentSizeInBytesLog2);

	// data size:
	__PRINT1(_L("=== Min data size:            [%d Bytes]"), iMinSizeInPages << iPageSizeLog2);
	__PRINT1(_L("=== Max data size:            [%d Bytes]"), iMaxSizeInPages << iPageSizeLog2);

	// memory size:
	const TUint32 pageMemSizeLog2 = iPageSizeLog2 > SegmentSizeInBytesLog2 ? iPageSizeLog2 : SegmentSizeInBytesLog2;
	__PRINT1(_L("=== Min memory size:          [%d Bytes]"), iMinSizeInPages << pageMemSizeLog2);
	__PRINT1(_L("=== Max memory size:          [%d Bytes]"), iMaxSizeInPages << pageMemSizeLog2);

	// reserved pages
	__PRINT1(_L("=== Number of pages reserved: [%d]"), iMinSizeInPages);
	__PRINT1(_L("=== Reserved memory:          [%d Bytes]"), (iMinSizeInPages * PageSizeInSegs()) << SegmentSizeInBytesLog2);
	// locked page num
	__PRINT1(_L("=== Number of pages locked:   [%d]"), iLockedQCount);
	__PRINT1(_L("=== Locked memory:            [%d Bytes]"), (iLockedQCount * PageSizeInSegs()) << SegmentSizeInBytesLog2);
	// unlocked page num
	__PRINT1(_L("=== Number of pages unlocked: [%d]"), iUnlockedQCount);
	__PRINT1(_L("=== Unlocked memory:          [%d Bytes]"), (iUnlockedQCount * PageSizeInSegs()) << SegmentSizeInBytesLog2);
	}

/**
Dump cache content, only enabled in debug mode.
@see CDynamicDirCache::Control()
*/
void CDynamicDirCache::Dump()
	{
	__PRINT(_L("======== CDynamicDirCache::Dump ========="));
	if (!iLockedQ.IsEmpty())
		{
		TDblQueIter<TDynamicDirCachePage> q(iLockedQ);
		q.SetToFirst();
		TInt i = 0;
		while((TDynamicDirCachePage*)q)
			{
			TDynamicDirCachePage* pP = q++;
			__PRINT3(_L("=== CDynamicDirCache::iLockedQ\t[%4d](pos=%lx, size=%d)"), i++, pP->StartPos(), pP->PageSizeInBytes());
			}
		}
	if (!iUnlockedQ.IsEmpty())
		{
		TDblQueIter<TDynamicDirCachePage> q(iUnlockedQ);
		q.SetToFirst();
		TInt i = 0;
		while((TDynamicDirCachePage*)q)
			{
			TDynamicDirCachePage* pP = q++;
			__PRINT3(_L("=== CDynamicDirCache::iUnlockedQ\t[%4d](pos=%lx, size=%u)"), i++, pP->StartPos(), pP->PageSizeInBytes());
			}
		}
	__PRINT2(_L("=== CDynamicDirCache::iActivePage\t[*](pos=%lx, size=%u)"), iActivePage->StartPos(), iActivePage->PageSizeInBytes());

	if (iLookupTable.Count())
		{
		TInt i = 0;
		THashSetIter<TLookupEntry> iter(iLookupTable);
		TLookupEntry* pEntry;
		pEntry = (TLookupEntry*) iter.Next();
		while(pEntry)
			{
			TDynamicDirCachePage* pP = pEntry->iPage;
			__PRINT3(_L("=== CDynamicDirCache::iLookupTable\t[%4d](pos=%lx, size=%u)"), i++, pP->StartPos(), pP->PageSizeInBytes());
			pEntry = (TLookupEntry*) iter.Next();
			};
		}
	__PRINT(_L("===========================================\n"));
	}
#endif //_DEBUG

/**
Lock an unlocked page, or do nothing if the page is already locked.
@return	TUint8*	pointer of the page to be locked, if locking is successful, otherwise return NULL.
@param	aPage	the pointer of the page to be locked.
*/
TUint8* CDynamicDirCache::LockPage(TDynamicDirCachePage* aPage)
	{
	ASSERT(aPage != NULL);
	if (aPage->IsLocked())
		return aPage->StartPtr();

	TInt r = iCacheMemoryClient->LockSegments(aPage->StartPtr(), PageSizeInSegs());
	if (r == KErrNone)
		{
		aPage->SetLocked(ETrue);
		return aPage->StartPtr();
		}

	return NULL;
	}

/**
Unlock a locked page.
@return	TInt	KErrNone if unlocking was successful, otherwise system-wide error code.
@param	aPage	the pointer of the page to be unlocked.
*/
TInt CDynamicDirCache::UnlockPage(TDynamicDirCachePage* aPage)
	{
	ASSERT(aPage != NULL);
	__PRINT1(_L("CDynamicDirCache::UnlockPage(%lx)"), aPage->StartPos());
	TInt r = iCacheMemoryClient->UnlockSegments(aPage->StartPtr(), PageSizeInSegs());
	if (r == KErrNone)
		{
		aPage->SetLocked(EFalse);
		}
	return r;
	}

/**
Decommit a locked or unlocked page.
@return	TInt	KErrNone if decommition was successful, otherwise system-wide error code.
@param	aPage	the pointer of the page to be decommitted.
*/
TInt CDynamicDirCache::DecommitPage(TDynamicDirCachePage* aPage)
	{
	ASSERT(aPage != NULL);
	__PRINT1(_L("CDynamicDirCache::DecommitPage(%lx)"), aPage->StartPos());
	if (aPage)
		{
		TInt r = iCacheMemoryClient->DecommitSegments(aPage->StartPtr(), PageSizeInSegs());
		if (r == KErrNone)
			{
			aPage->SetLocked(EFalse);
			aPage->SetValid(EFalse);
			}
		return r;
		}
	return KErrArgument;
	}

/////////////////////////// aluxiliary functions //////////////////////////////////
/**
Calculate the page size in segments. Segment size is the size of the kernel memory unit that cache memory manager manages.
We are making assumption here about the page size: page size should always be either less than segment size
or multiple times of segment size
@return	TUint32	the page size in segments.
*/
TUint32 CDynamicDirCache::PageSizeInSegs() const
	{
	// initialize cache memory manager as all file systems have mounted by now
	ASSERT(CCacheMemoryManagerFactory::CacheMemoryManager());
	const TUint32 SegmentSizeInBytesLog2 = CCacheMemoryManagerFactory::CacheMemoryManager()->SegmentSizeInBytesLog2();

	// Page size should be non-zero
	ASSERT(iPageSizeInBytes);

	TUint32 segs = iPageSizeInBytes >> SegmentSizeInBytesLog2;
	return segs > 0 ? segs : 1;
	}

/**
Deque the page from locked queue or unlocked queue. All pages are managed through these two queues, expect iActive
page.
@param	aPage	the pointer of the page to be dequeued
@return	TInt	KErrArgument if aPage is invalid, otherwise KErrNone.
*/
TInt CDynamicDirCache::DeQueue(TDynamicDirCachePage* aPage)
	{
	ASSERT(aPage);
	if (!aPage)
		return KErrArgument;

	if (aPage->iType == TDynamicDirCachePage::ELocked)
		{
		aPage->Deque();
		aPage->SetPageType(TDynamicDirCachePage::EUnknown);
		--iLockedQCount;
		}
	else if (aPage->iType == TDynamicDirCachePage::EUnlocked)
		{
		aPage->Deque();
		aPage->SetPageType(TDynamicDirCachePage::EUnknown);
		--iUnlockedQCount;
		}
	else
		{
		ASSERT(0);
		return KErrArgument;
		}
	return KErrNone;
	}

/**
Insert a page to the first position of locked queue or unlocked queue.
@param	aPage	the pointer of the page to be inserted.
@param	aType	the type of the queue to be inserted.
@return	TInt	KErrArgument if aPage is invalid, otherwise KErrNone.
*/
TInt CDynamicDirCache::AddFirstOntoQueue(TDynamicDirCachePage* aPage, TDynamicDirCachePage::TPageType aType)
	{
	ASSERT(aPage);
	if (!aPage)
		return KErrArgument;

	// page must be dequed first or it is active page
	if (aPage->iType != TDynamicDirCachePage::EActivePage && aPage->iType != TDynamicDirCachePage::EUnknown)
		{
		ASSERT(0);
		return KErrArgument;
		}

	if (aType == TDynamicDirCachePage::ELocked)
		{
		iLockedQ.AddFirst(*aPage);
		aPage->SetPageType(TDynamicDirCachePage::ELocked);
		++iLockedQCount;
		}
	else if (aType == TDynamicDirCachePage::EUnlocked)
		{
		iUnlockedQ.AddFirst(*aPage);
		aPage->SetPageType(TDynamicDirCachePage::EUnlocked);
		++iUnlockedQCount;
		}
	else
		{
		ASSERT(0);
		return KErrArgument;
		}

	return KErrNone;
	}

/**
Remove a page from the lookup table, indexed by the starting media address of the page content.
@param	aPagePos	the starting media position of the page to be removed.
*/
TInt CDynamicDirCache::LookupTblRemove(TInt64 aPagePos)
	{
	if (aPagePos == 0)
		{
		return KErrNone;
		}

	TInt r = iLookupTable.Remove(TLookupEntry(aPagePos, 0, NULL));
	return r;
	}

/**
Insert a page to the lookup table, indexed by the starting media address of the page content.
@param	aPagePos	the starting media position of the page to be inserted.
*/
TInt CDynamicDirCache::LookupTblAdd(TDynamicDirCachePage* aPage)
	{
	ASSERT(aPage);
	if (!aPage)
		return KErrArgument;

	if (aPage->StartPos() == 0)
		{
		return KErrNone;
		}

	TInt r = iLookupTable.Insert(TLookupEntry(aPage->StartPos(), iPageSizeInBytes, aPage));
	return r;
	}

/**
Reset the media address of the page to 0, also invalidate the page.
@param	aPage	the pointer of the page to be reset.
*/
TInt CDynamicDirCache::ResetPagePos(TDynamicDirCachePage* aPage)
	{
	ASSERT(aPage);
	if (!aPage)
		return KErrArgument;

	aPage->ResetPos();
	return KErrNone;
	}

/**
Search the lookup table to find the page start with a specific media address.
@param	aPos	the starting media address to be searched.
*/
TDynamicDirCachePage* CDynamicDirCache::LookupTblFind(TInt64 aPos)
	{
	if (aPos == 0)
		{
		ASSERT(0);
		return NULL;
		}

	TLookupEntry* entry = iLookupTable.Find(TLookupEntry(aPos, 0, NULL));
	if(entry)
		{
		ASSERT(entry->iPage->IsValid());
		return entry->iPage;
		}

	return NULL;
	}
