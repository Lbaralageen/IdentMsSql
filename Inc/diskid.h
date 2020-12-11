
/** @file
  * EpsDiskId/diskid.h
  *
  * @author 
  * Lynn McGuire
  *
  * @modified by 
  * Konstantin Tchoumak 
  *
  * for reading the manufacturor's information from your hard drives
  *
  * 06/11/00  Lynn McGuire  written with many contributions from others,
  *                           IDE drives only under Windows NT/2K and 9X,
  *                           maybe SCSI drives later
  * 11/20/03  Lynn McGuire  added ReadPhysicalDriveInNTWithZeroRights
  * 10/26/05  Lynn McGuire  fix the flipAndCodeBytes function
  * 01/22/08  Lynn McGuire  incorporate changes from Gonzalo Diethelm,
  *                            remove media serial number code since does 
  *                            not work on USB hard drives or thumb drives
  * 01/29/08  Lynn McGuire  add ReadPhysicalDriveInNTUsingSmart
  * 01/29/08  Konstantin Tchoumak  adopted for VS2010  x86/x64
  *
  * http://www.winsim.com/diskid32/diskid32.html
  */

#ifndef __Utils_IPS_
#define __Utils_IPS_

#include <vector>
#include <string>
#include "crc64.h"


enum eDISKID_STATE{ DID_INVALID_HANDLE_VALUE_e, DID_FAILED_DFP_GET_VERSION_e, DID_RequestIsNotValid_e
                  , DID_RequestIsNotSupported_e, DID_ERROR_MEDIA_SERIAL_NUMBER_e, DID_IOCTL_STORAGE_QUERY_e
                  , DID_Unable_Open_SCSI_e, DID_Unknown_Exception_e, eDISKID_STATE_e };

#define WINDOWS_KEY_LENGTH    30    // "XXXXX-XXXXX-XXXXX-XXXXX-XXXXX\x0"

static const size_t s_nMaxScsiControllers         = 16;
static const size_t s_nMaxScsiDrivesAtController =  2;

namespace Utils
{
    //  Required to ensure correct PhysicalDrive IOCTL structure setup

    #define  IDENTIFY_BUFFER_SIZE  512
       //  Valid values for the bCommandReg member of IDEREGS.
    #define  IDE_ATAPI_IDENTIFY  0xA1  //  Returns ID sector for ATAPI.
    #define  IDE_ATA_IDENTIFY    0xEC  //  Returns ID sector for ATA.
    
    struct disk_t
    {
        int             num_controller; // 0-3
        bool            master_slave;
        char            vendor[256];    // vendor ??
        char            model[256];     // Product Id
        char            serial[256];    // 
        char            revision[256];
        __int64         buffer;         //Controller Buffer Size on Drive in bytes
        __int64         sectors;
        __int64         size;           // Drive Size
        size_t          drive;
        int             type;
        unsigned long   version;
        int             deviceTypeModifier;
        bool            fixed;

        disk_t(){ ::memset( this, 0x00, sizeof(disk_t) ); };
        __int64  GetUUID(){ return ::crc64c( ::crc64c( ~0, model ), serial ); }
    };

    class DiskInfo
    {
        friend class DiskInfoStub;
        friend class DiskIdTests;

        public:
    #pragma pack(push, 1)
                //  GETVERSIONOUTPARAMS contains the data returned from the 
                //  Get Driver Version function.
            typedef struct _GETVERSIONOUTPARAMS
            {
                unsigned __int8 bVersion;      // Binary driver version.
                unsigned __int8 bRevision;     // Binary driver revision.
                unsigned __int8 bReserved;     // Not used.
                unsigned __int8 bIDEDeviceMap; // Bit map of IDE devices.
                unsigned __int32 fCapabilities; // Bit mask of driver capabilities.
                unsigned __int32 dwReserved[4]; // For future use.
            } GETVERSIONOUTPARAMS, *PGETVERSIONOUTPARAMS, *LPGETVERSIONOUTPARAMS;

                //  IDE registers
            typedef struct _IDEREGS
            {
                unsigned __int8 bFeaturesReg;       // Used for specifying SMART "commands".
                unsigned __int8 bSectorCountReg;    // IDE sector count register
                unsigned __int8 bSectorNumberReg;   // IDE sector number register
                unsigned __int8 bCylLowReg;         // IDE low order cylinder value
                unsigned __int8 bCylHighReg;        // IDE high order cylinder value
                unsigned __int8 bDriveHeadReg;      // IDE drive/head register
                unsigned __int8 bCommandReg;        // Actual IDE command.
                unsigned __int8 bReserved;          // reserved for future use.  Must be zero.
            } IDEREGS, *PIDEREGS, *LPIDEREGS;

                //  SENDCMDINPARAMS contains the input parameters for the 
                //  Send Command to Drive function.
            typedef struct _SENDCMDINPARAMS
            {
                unsigned __int32     cBufferSize;   //  Buffer size in bytes
                IDEREGS   irDriveRegs;   //  Structure with drive register values.
                unsigned __int8 bDriveNumber;       //  Physical drive number to send 
                                        //  command to (0,1,2,3).
                unsigned __int8 bReserved[3];       //  Reserved for future expansion.
                unsigned __int32     dwReserved[4]; //  For future use.
                unsigned __int8      bBuffer[1];    //  Input buffer.
            } SENDCMDINPARAMS, *PSENDCMDINPARAMS, *LPSENDCMDINPARAMS;

            // Status returned from driver
            typedef struct _DRIVERSTATUS
            {
                unsigned __int8  bDriverError;  //  Error code from driver, or 0 if no error.
                unsigned __int8  bIDEStatus;    //  Contents of IDE Error register.
                                    //  Only valid when bDriverError is SMART_IDE_ERROR.
                unsigned __int8  bReserved[2];  //  Reserved for future expansion.
                unsigned __int32  dwReserved[2];  //  Reserved for future expansion.
            } DRIVERSTATUS, *PDRIVERSTATUS, *LPDRIVERSTATUS;

                // Structure returned by PhysicalDrive IOCTL for several commands
            typedef struct _SENDCMDOUTPARAMS
            {
                unsigned __int32         cBufferSize;   //  Size of bBuffer in bytes
                DRIVERSTATUS  DriverStatus;  //  Driver status structure.
                unsigned __int8          bBuffer[1];    //  Buffer of arbitrary length in which to store the data read from the                                                       // drive.
            } SENDCMDOUTPARAMS, *PSENDCMDOUTPARAMS, *LPSENDCMDOUTPARAMS;

                // The following struct defines the interesting part of the IDENTIFY
                // buffer:
            typedef struct _IDSECTOR
            {
                unsigned short  wGenConfig;
                unsigned short  wNumCyls;
                unsigned short  wReserved;
                unsigned short  wNumHeads;
                unsigned short  wBytesPerTrack;
                unsigned short  wBytesPerSector;
                unsigned short  wSectorsPerTrack;
                unsigned short  wVendorUnique[3];
                char            sSerialNumber[20];
                unsigned short  wBufferType;
                unsigned short  wBufferSize;
                unsigned short  wECCSize;
                char            sFirmwareRev[8];
                char            sModelNumber[40];
                unsigned short  wMoreVendorUnique;
                unsigned short  wDoubleWordIO;
                unsigned short  wCapabilities;
                unsigned short  wReserved1;
                unsigned short  wPIOTiming;
                unsigned short  wDMATiming;
                unsigned short  wBS;
                unsigned short  wNumCurrentCyls;
                unsigned short  wNumCurrentHeads;
                unsigned short  wNumCurrentSectorsPerTrack;
                unsigned long   ulCurrentSectorCapacity;
                unsigned short  wMultSectorStuff;
                unsigned long   ulTotalAddressableSectors;
                unsigned short  wSingleWordDMA;
                unsigned short  wMultiWordDMA;
                unsigned __int8    bReserved[128];
            } IDSECTOR, *PIDSECTOR;

            typedef struct _SRB_IO_CONTROL
            {
                unsigned long HeaderLength;
                unsigned char Signature[8];
                unsigned long Timeout;
                unsigned long ControlCode;
                unsigned long ReturnCode;
                unsigned long Length;
            } SRB_IO_CONTROL, *PSRB_IO_CONTROL;

    #pragma pack(pop)
            // wrapper for unit tests
            virtual bool IsDeviceIoControl( void* hDevice,
                                            unsigned long dwIoControlCode,
                                            void* lpInBuffer,
                                            unsigned long nInBufferSize,
                                            void* lpOutBuffer,
                                            unsigned long nOutBufferSize,
                                            unsigned long* lpBytesReturned
                                            );

             virtual void* OpenDevice( wchar_t *wzFileName, unsigned long dwDesiredAccess );

             virtual void* OpenDeviceSmart( wchar_t* wzFileName );

             virtual bool  CloseDevice( void* hObject );


            /*
            *  convert string
            *  
            *  @param [in] diskData      data taken from controller
            *  @param [in] firstIndex    first index
            *  @param [in] lastIndex     last index
            *  
            *  @return processed data string
            */
            char *ConvertToString( unsigned __int32 diskData[256], const size_t firstIndex, const size_t lastIndex );

            /*
            *  Send an IDENTIFY command to the drive
            *  
            *  @param [in]  hPhysicalDriveIOCTL   IOCTL
            *  @param [in]  pSCIP                 pointer to SCIP
            *  @param [in]  nIDCmd                IDE_ATA_IDENTIFY or IDE_ATAPI_IDENTIFY
            *  @param [in]  nDriveNum             contrioller id [ 0- nn]
            *  @param [out] lpcbBytesReturned     number of bytes will be returned
            *  
            *  @return processed data string
            */
            virtual bool DoIdentify( void * hPhysicalDriveIOCTL, PSENDCMDINPARAMS pSCIP,
                                     PSENDCMDOUTPARAMS pSCOP, unsigned __int8 nIDCmd, unsigned __int8 nDriveNum,
                                     unsigned __int32 * lpcbBytesReturned);

            /*
            *  Read Physical Drive In NT With Admin Rights
            *  
            *  @param [out] disks   drives info
            *  
            *  @return true if successed
            */
            bool ReadPhysicalDriveInNTWithAdminRights( std::vector<disk_t> &disks );

            /*
            *  get information about drive
            *  
            *  @param [out] disks   drives info
            *  
            *  @return true if successed
            */
            virtual bool GetIdeInfo( const size_t drive, unsigned __int32 diskdata[256], disk_t &_disk  );

            virtual bool ReadScsiDriveInController( void* hScsiDriveIOCTL, const size_t controller, const size_t drive, disk_t& disk );
            /*
            *  Read scse Drives
            *  
            *  @param [out] disks   drives info
            *  
            *  @return true if successed
            */
            bool ReadIdeDriveAsScsiDriveInNT( std::vector<disk_t> &_disk );

            bool ReadPhysicalDriveInNTWithZeroRights( std::vector<disk_t> &_disk );

            bool ReadPhysicalDriveInNTUsingSmart( std::vector<disk_t> &disks );

            char * FlipAndCodeBytes( char * str, int pos, int flip, char * buf );

            static void strMACaddress( unsigned char MACData[], char string[256] );

        // Define global buffers.
            unsigned __int8  m_szIdOutCmd [sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1];
            char             m_szHardDriveSerialNumber[1024];
            char             m_szHardDriveModelNumber[1024];
            char             m_flipped[1024];
            char             m_cv[1024];
        public:
            typedef std::pair<eDISKID_STATE, std::wstring> pairtype;
            std::vector< pairtype >    errors;

            void LogMessage( eDISKID_STATE state, std::wstring wsMessage, const int codeLine, const char* szFuncName )
            {
                wchar_t msg[512] = {0};
                ::_snwprintf( msg, _countof(msg), L"%d [%hs] %s", codeLine, szFuncName, wsMessage.c_str() );
                pairtype val = std::pair<eDISKID_STATE, std::wstring>( state, msg );
                errors.push_back( val );
            }

            __int64 GetHardDriveComputerID();

            bool  GetDrivesInfo( std::vector<disk_t> &_disk );

            DiskInfo();
    };
}
#endif
