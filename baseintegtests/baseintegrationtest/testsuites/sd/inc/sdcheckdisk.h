// Copyright (c) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
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
//

#ifndef SDCHECKDISK_H
#define SDCHECKDISK_H

#include "sdbase.h"

/*
SD Test Step. Check the file allocation table on the drive under test for consistency.
*/
class CBaseTestSDCheckDisk : public CBaseTestSDBase
	{
public:
	CBaseTestSDCheckDisk();
	virtual TVerdict doTestStepPreambleL();
	virtual TVerdict doTestStepL();
	};

_LIT(KTestStepCheckDisk, "CheckDisk");

#endif // SDCHECKDISK_H
