// Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
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



/**
 @file
 @internalTechnology
*/

#ifndef CUSBHOSTSERVER_H
#define CUSBHOSTSERVER_H


class CUsbHost;

// Receives client requests
class CUsbHostServer : public CServer2
	{
public:
    friend class CUsbHostSession;

	static CUsbHostServer* NewLC();
	virtual ~CUsbHostServer();

protected:
	CUsbHostServer();
	void ConstructL();

public:
    void AddSession();
    void RemoveSession();

	virtual CSession2* NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const;
	TInt RunError(TInt aError);


private:
    CUsbHost* iUsbHost;
	TInt iSessionCount;
	};



#endif // CUSBHOSTSERVER_H
