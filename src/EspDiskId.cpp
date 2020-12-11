/** @file
  * EPSDiskID\EpsdiskId.cpp
  *
  * @author 
  * Konstantin Tchoumak 
  *
  * for reading in MSSQL the manufacturor's information from your hard drives
  * 2012
  *
  * licensed under The GENERAL PUBLIC LICENSE (GPL3)
  */

#include "srv.h"

const int DSK_VERSION = 6;


//--------------------------------------------------------------------------------------------------------
BOOL APIENTRY DllMain( HANDLE, DWORD  ul_reason_for_call, LPVOID )
{
    if( DLL_PROCESS_ATTACH == ul_reason_for_call )
    {

    }
    return TRUE;
}

//--------------------------------------------------------------------------------------------------------
