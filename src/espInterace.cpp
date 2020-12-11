/** @file
  * espInterace.cpp 
  *
  * @author 
  * Konstantin Tchoumak 
  *
  * interface between MSSQL API and C++ code
  *
  * licensed under The GENERAL PUBLIC LICENSE (GPL3)
  */

#include <windows.h>
#include "atlbase.h"

#include "espSupport.h"
#include "diskid.h"
#include "crc64.h"

const int DSK_VERSION = 1;

// Extended procedure error codes
#define SRV_MAXERROR            50000
#define GETTABLE_ERROR          SRV_MAXERROR + 1
#define GETTABLE_MSG            SRV_MAXERROR + 2

//--------------------------------------------------------------------------------------------------------
#ifdef ESPLIB_EXPORTS
// MS SQL 2000 declaration for Extended Server Procedure
#ifdef __cplusplus
extern "C" {
#endif

RETCODE ESPLIB_API xp_GetVersion( SRV_PROC *srvproc );    

RETCODE ESPLIB_API xp_DiskId(SRV_PROC *srvproc); 

#ifdef __cplusplus
}
#endif      // __cplusplus
#else

#endif

using namespace Utils;

// EXEC master.dbo.xp_DiskId
RETCODE ESPLIB_API xp_DiskId( SRV_PROC *pSrvProc )
{
    if( pSrvProc == 0 )
    {
        return 0;
    }
    char str[255] = {0x00};
    DiskInfo comp;
    try
    {
        std::vector<disk_t> _disk;
        int nRowsFetched = 0;

        comp.GetDrivesInfo( _disk );

        srv_describe(pSrvProc, 1, "controller", SRV_NULLTERM, SRVINT4,    sizeof(int),     SRVINT4,    sizeof(int), NULL); 
        srv_describe(pSrvProc, 2, "model",      SRV_NULLTERM, SRVVARCHAR, 32,              SRVVARCHAR, 32, NULL); 
        srv_describe(pSrvProc, 3, "serial",     SRV_NULLTERM, SRVVARCHAR, 32,              SRVVARCHAR, 32, NULL); 
        srv_describe(pSrvProc, 4, "vendor",     SRV_NULLTERM, SRVVARCHAR, 32,              SRVVARCHAR, 32, NULL); 
        srv_describe(pSrvProc, 5, "size",       SRV_NULLTERM, SRVINT4,    sizeof(int),     SRVINT4,    sizeof(int), NULL); 
        srv_describe(pSrvProc, 6, "duuid",      SRV_NULLTERM, SRVINT8,    sizeof(__int64), SRVINT8,    sizeof(__int64), NULL); 

        for( size_t i = 0; i < _disk.size(); i++ )
        {
            srv_setcollen  ( pSrvProc, 1, sizeof(_disk[i].num_controller) );    
            srv_setcoldata ( pSrvProc, 1, &_disk[i].num_controller );

            srv_setcollen  ( pSrvProc, 2, (__int32)::strlen( _disk[i].model ) + 1 );    
            srv_setcoldata ( pSrvProc, 2, _disk[i].model );

            srv_setcollen  ( pSrvProc, 3, (__int32)::strlen( _disk[i].serial ) + 1 );    
            srv_setcoldata ( pSrvProc, 3, _disk[i].serial );

            srv_setcollen  ( pSrvProc, 4, (__int32)::strlen( _disk[i].vendor ) + 1 );    
            srv_setcoldata ( pSrvProc, 4, _disk[i].vendor );

            __int32 sizeInGb = static_cast<__int32>( _disk[i].size / ( 1024 * 1024 * 1024 ) );

            srv_setcollen  ( pSrvProc, 5, sizeof(__int32) );    
            srv_setcoldata ( pSrvProc, 5, &sizeInGb );

            __int64 uuid = _disk[i].GetUUID();

            srv_setcollen  ( pSrvProc, 6, sizeof(__int64) );    
            srv_setcoldata ( pSrvProc, 6, &uuid );

            if( srv_sendrow (pSrvProc) == SUCCEED )
            {
                nRowsFetched++;                        // Go to the next row. 
            } 
        }
        if( nRowsFetched > 0 )
        {
            srv_senddone (pSrvProc, SRV_DONE_COUNT | SRV_DONE_MORE, (DBUSMALLINT) 0, nRowsFetched);
        }
        else 
        {
            srv_senddone (pSrvProc, SRV_DONE_MORE, (DBUSMALLINT) 0, (DBINT) 0);
        }
    }
    catch(...)
    {
        srv_sendmsg(pSrvProc, SRV_MSG_INFO, 777, SRV_INFO, (DBTINYINT) 0, NULL, 0, 0, str, SRV_NULLTERM);
    }
    return -1;
}


//-------------------------------------------------------------------------------------------------------------------------------
