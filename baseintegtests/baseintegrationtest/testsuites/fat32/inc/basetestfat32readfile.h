// Copyright (c) 2006-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef BASETESTFAT32READFILE_H
#define BASETESTFAT32READFILE_H

#include <testexecutestepbase.h>
#include <testexecuteserverbase.h>
#include "basetestfat32base.h"

/**
Fat32 ReadFile Class. Inherits from the base class.
Contains functions needed to read a file, with other associated functions, 
such as:
	a) Reading a file
	b) Opening a file
	c) Listing a list of directory entries
	d) Obtaining the last modified date and time of a file
*/
class CBaseTestFat32ReadFile : public CBaseTestFat32Base
	{
	public:
		CBaseTestFat32ReadFile(); 
		~CBaseTestFat32ReadFile();
		// the actual test step
		virtual TVerdict doTestStepL();	
		TInt ReadFile(const TDesC16& aFile);
		TInt OpenFile(const TDesC16& aFile);
		TInt DirList(const TDesC16& aFile);
		TInt GetModDate(const TDesC16& aFile);	
		TInt GetModTime(const TDesC16& aFile);				
	protected:
	
	
};

_LIT(KTestStepReadFile, "ReadFile");

#endif // BASETESTFAT32READFILE_H
