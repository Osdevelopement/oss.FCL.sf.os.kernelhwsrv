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
// Hardware Configuration Respoitory Platform Independent Layer (PIL)
//

// -- INCLUDES ----------------------------------------------------------------


#include <nkern/nkern.h>
#include <kernel/kernel.h>

#ifdef HCRTEST_NO_EXPORT
#undef EXPORT_C
#undef IMPORT_C
#define EXPORT_C
#define IMPORT_C
#endif // HCRTEST_NO_EXPORT

#include <drivers/hcr.h>
#include "hcr_debug.h"
#include "hcr_pil.h"


// -- FUNCTIONS ---------------------------------------------------------------

#ifndef MAKE_DEF_FILE
namespace HCR 
{

LOCAL_C TInt GetUValueWordSetting (const TSettingId& aId, 
                                    TSettingType aType, UValueWord& aValue)
    {
    HCR_FUNC("GetUValueWordSetting");
                 
    if (HCRNotReady)
        HCR_LOG_RETURN(KErrNotReady);

    TSettingRef sref(0,0);
    TInt err = 0;
    err = HCRSingleton->FindSetting(aId, aType, sref);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);

    err = sref.iRep->GetValue(sref, aValue);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);
    
    return KErrNone;	
    }
    
LOCAL_C TInt GetUValueLargeSetting64 (const TSettingId& aId, 
                                    TSettingType aType, UValueLarge& aValue)
    {
    HCR_FUNC("GetUValueLargeSetting64");
                 
    if (HCRNotReady)
		HCR_LOG_RETURN(KErrNotReady);

    TSettingRef sref(0,0);
    TInt err = 0;
    err = HCRSingleton->FindSetting(aId, aType, sref);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);

    err = sref.iRep->GetLargeValue(sref, aValue);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);
    
    return KErrNone;	
    }
    

LOCAL_C TInt GetUValueLargeSettingTDes8 (const TSettingId& aId, 
                                        TSettingType aType, TDes8& aValue)
    {
    HCR_FUNC("GetUValueLargeSettingTDes8");
                 
    if (HCRNotReady)
	    HCR_LOG_RETURN(KErrNotReady);

    TSettingRef sref(0,0);
    TInt err = 0;
    err = HCRSingleton->FindSetting(aId, aType, sref);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);

    UValueLarge value;
    err = sref.iRep->GetLargeValue(sref, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);
   
    TInt len = sref.iRep->GetLength(sref);
    if (len > aValue.MaxSize())
        HCR_LOG_RETURN(KErrTooBig);    
    
    if (aType == ETypeBinData)
        aValue.Copy(value.iData, len);
    else
        aValue.Copy(value.iString8, len);
    
    return KErrNone;	
    }

LOCAL_C TInt GetUValueLargeSettingTUint8 (const TSettingId& aId, TSettingType aType, 
                                    TUint16 aMaxLen, TUint8* aValue, TUint16& aLen)
    {
    HCR_FUNC("GetUValueLargeSettingTUint8");
                 
    if (HCRNotReady)
	    HCR_LOG_RETURN(KErrNotReady);

    TSettingRef sref(0,0);
    TInt err = 0;
    err = HCRSingleton->FindSetting(aId, aType, sref);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);

    UValueLarge value;
    err = sref.iRep->GetLargeValue(sref, value);
	    if (err != KErrNone)
        HCR_LOG_RETURN(err);
   
    aLen = sref.iRep->GetLength(sref);
    if (aLen > aMaxLen)
        HCR_LOG_RETURN(KErrTooBig);    
    
    if (aType == ETypeBinData)
        memcpy (aValue, value.iData, aLen);
    else
        memcpy (aValue, value.iString8, aLen);
    
    return KErrNone;	
    }
    
LOCAL_C TInt GetUValueLargeSettingArray (const TSettingId& aId, TSettingType aType, 
                                    TUint16 aMaxLen, TUint32* aValue, TUint16& aLen)
    {
    HCR_FUNC("GetUValueLargeSettingArray");
                 
    if (HCRNotReady)
	    HCR_LOG_RETURN(KErrNotReady);

    TSettingRef sref(0,0);
    TInt err = 0;
    err = HCRSingleton->FindSetting(aId, aType, sref);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);

    UValueLarge value;
    err = sref.iRep->GetLargeValue(sref, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);
   
    aLen = sref.iRep->GetLength(sref);
    if (aLen > aMaxLen)
        HCR_LOG_RETURN(KErrTooBig);    
    
    if (aType == ETypeArrayInt32)
        memcpy (aValue, value.iArrayInt32, aLen);
    else if (aType == ETypeArrayUInt32)
        memcpy (aValue, value.iArrayUInt32, aLen);
    else
    	HCR_LOG_RETURN(KErrGeneral);
    
    return KErrNone;	
    }
    
}

#endif // MAKE_DEF_FILE


// -- SETTING GET -------------------------------------------------------------


EXPORT_C TInt HCR::GetInt(const TSettingId& aId, TInt8& aValue)
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetInt8");
    
    UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeInt8, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iInt8;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    
    
EXPORT_C TInt HCR::GetInt(const TSettingId& aId, TInt16& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetInt16");
    
    UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeInt16, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iInt16;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    

EXPORT_C TInt HCR::GetInt(const TSettingId& aId, TInt32& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetInt32");
    
        UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeInt32, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iInt32;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    
    
EXPORT_C TInt HCR::GetInt(const TSettingId& aId, TInt64& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetInt64");
    
    UValueLarge value;
    TInt err = GetUValueLargeSetting64(aId, ETypeInt64, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = *(value.iInt64);
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    

EXPORT_C TInt HCR::GetBool(const TSettingId& aId, TBool& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetBool");
                 
    UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeBool, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iBool;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    

EXPORT_C TInt HCR::GetUInt(const TSettingId& aId, TUint8& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetUInt8");
                 
    UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeUInt8, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iUInt8;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    
    
EXPORT_C TInt HCR::GetUInt(const TSettingId& aId, TUint16& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetUInt16");
    
    UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeUInt16,value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iUInt16;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    
    
EXPORT_C TInt HCR::GetUInt(const TSettingId& aId, TUint32& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetUInt32");
    
    UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeUInt32, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iUInt32;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    
    
EXPORT_C TInt HCR::GetUInt(const TSettingId& aId, TUint64& aValue) 
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetUInt64");
    
    UValueLarge value;
    TInt err = GetUValueLargeSetting64(aId, ETypeUInt64, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = *(value.iUInt64);
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }

    
EXPORT_C TInt HCR::GetLinAddr(const TSettingId& aId, TLinAddr& aValue)
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetLinAddr");
    
    UValueWord value;
    TInt err = GetUValueWordSetting(aId, ETypeLinAddr, value);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    aValue = value.iAddress;
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
        
    
EXPORT_C TInt HCR::GetData(const TSettingId& aId, TUint16 aMaxLen, 
                                TUint8* aValue, TUint16& aLen)                             
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetDataTUint8");
    
    TInt err = GetUValueLargeSettingTUint8(aId, ETypeBinData, aMaxLen, aValue, aLen);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    

EXPORT_C TInt HCR::GetData(const TSettingId& aId, TDes8& aValue)
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetDataTDes8");
    
    TInt err = GetUValueLargeSettingTDes8(aId, ETypeBinData, aValue);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);  
             
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    
    
EXPORT_C TInt HCR::GetString(const TSettingId& aId, TUint16 aMaxLen, 
                                TText8* aValue, TUint16& aLen)
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetStringTUint8");
    
    TInt err = GetUValueLargeSettingTUint8(aId, ETypeText8, aMaxLen, aValue, aLen);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
    
    
EXPORT_C TInt HCR::GetString(const TSettingId& aId, TDes8& aValue)
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetStringTUint8");
    
    TInt err = GetUValueLargeSettingTDes8(aId, ETypeText8, aValue);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
      
EXPORT_C TInt HCR::GetArray(const TSettingId& aId, TUint16 aMaxLen, 
                                TInt32* aValue, TUint16& aLen)
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetArrayTInt32");
    
    TInt err = GetUValueLargeSettingArray(aId, ETypeArrayInt32, aMaxLen, (TUint32*)aValue, aLen);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
								       
EXPORT_C TInt HCR::GetArray(const TSettingId& aId, TUint16 aMaxLen, 
                                TUint32* aValue, TUint16& aLen)  
    {
#ifndef MAKE_DEF_FILE
    HCR_FUNC("HCR::GetArrayTUInt32");
    
    TInt err = GetUValueLargeSettingArray(aId, ETypeArrayUInt32, aMaxLen, aValue, aLen);
    if (err != KErrNone)
        HCR_LOG_RETURN(err);        
    
    return KErrNone;    
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }


// -- MULTIPLE GETS -----------------------------------------------------------
  
EXPORT_C TInt HCR::GetWordSettings(TInt /*aNum*/, const SSettingId /*aIds*/[],
        TInt32 /*aValues*/[], TSettingType /*aTypes*/[], TInt /*aErrors*/[])
    {
#ifndef MAKE_DEF_FILE

    return KErrNotSupported;
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }


// -- SETTING PROPERTIES ------------------------------------------------------

EXPORT_C TInt HCR::GetTypeAndSize(const TSettingId& /*aId*/, TSettingType& /*aType*/, 
                                        TUint16& /*aLen*/) 
    {
#ifndef MAKE_DEF_FILE

    return KErrNotSupported;
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
 

// -- SETTING SEARCHES --------------------------------------------------------

EXPORT_C TInt HCR::FindNumSettingsInCategory (TCategoryUid /*aCatUid*/)
	{
#ifndef MAKE_DEF_FILE

    return KErrNotSupported;
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
		
EXPORT_C TInt HCR::FindSettings(TCategoryUid /*aCatUid*/, 
					TInt /*aMaxNum*/, TUint32& /*aNumFound*/, 
					TElementId* /*aElIds*/, TSettingType* /*aTypes*/, TUint16* /*aLens*/)
	{
#ifndef MAKE_DEF_FILE

    return KErrNotSupported;
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }

EXPORT_C TInt HCR::FindSettings(TCategoryUid /*aCat*/, 
					TInt /*aMaxNum*/, TUint32 /*aAtId*/,
                    TUint32 /*aMask*/, TUint32 /*aPattern*/, TUint32& /*aNumFound*/,
                    TElementId* /*aElIds*/, TSettingType* /*aTypes*/, TUint16* /*aLens*/)
	{
#ifndef MAKE_DEF_FILE

    return KErrNotSupported;
#else    
    return KErrNotSupported;
#endif // MAKE_DEF_FILE
    }
						

// -- KERNEL ENTRY POINT ------------------------------------------------------


#ifdef MAKE_DEF_FILE

DECLARE_STANDARD_EXTENSION()
	{
	return KErrNone;
	}
	
#endif // MAKE_DEF_FILE
	
