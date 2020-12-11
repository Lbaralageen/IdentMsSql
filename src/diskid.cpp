
/** @file
  * EpsDiskId/diskid32.cpp
  *
  * @author 
  * Lynn McGuire
  *
  * @modified by 
  * Konstantin Tchoumak 
  *
  * Freeware code
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
  * http://www.winsim.com/diskid32/diskid32.html
  * 01/29/10  Konstantin Tchoumak  adopted for VS2010  x86/x64
  * 10/10/13  Konstantin Tchoumak  adopted for VS2010  C++/CLI. Removed Win95 platform
  */


#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include <Winsock2.h>

#include <windows.h>
#include <winioctl.h>


#include "diskid.h"

#define  TITLE   "DiskId32"

using namespace std;

#pragma warning (disable : 4996)

   //  IOCTL commands
#define  DFP_GET_VERSION          0x00074080
#define  DFP_SEND_DRIVE_COMMAND   0x0007c084
#define  DFP_RECEIVE_DRIVE_DATA   0x0007c088

#define  FILE_DEVICE_SCSI              0x0000001b
#define  IOCTL_SCSI_MINIPORT_IDENTIFY  ((FILE_DEVICE_SCSI << 16) + 0x0501)
#define  IOCTL_SCSI_MINIPORT 0x0004D008  //  see NTDDSCSI.H for definition

#define SMART_GET_VERSION               CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)
#define SMART_SEND_DRIVE_COMMAND        CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define SMART_RCV_DRIVE_DATA            CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


   //  Bits returned in the fCapabilities member of GETVERSIONOUTPARAMS 
#define  CAP_IDE_ID_FUNCTION             1  // ATA ID command supported
#define  CAP_IDE_ATAPI_ID                2  // ATAPI ID command supported
#define  CAP_IDE_EXECUTE_SMART_FUNCTION  4  // SMART commannds supported

   //  Valid values for the bCommandReg member of IDEREGS.
#define  IDE_ATAPI_IDENTIFY  0xA1  //  Returns ID sector for ATAPI.
#define  IDE_ATA_IDENTIFY    0xEC  //  Returns ID sector for ATA.

   // The following struct defines the interesting part of the IDENTIFY
   // buffer:
typedef struct _IDSECTOR
{
   USHORT  wGenConfig;
   USHORT  wNumCyls;
   USHORT  wReserved;
   USHORT  wNumHeads;
   USHORT  wBytesPerTrack;
   USHORT  wBytesPerSector;
   USHORT  wSectorsPerTrack;
   USHORT  wVendorUnique[3];
   CHAR    sSerialNumber[20];
   USHORT  wBufferType;
   USHORT  wBufferSize;
   USHORT  wECCSize;
   CHAR    sFirmwareRev[8];
   CHAR    sModelNumber[40];
   USHORT  wMoreVendorUnique;
   USHORT  wDoubleWordIO;
   USHORT  wCapabilities;
   USHORT  wReserved1;
   USHORT  wPIOTiming;
   USHORT  wDMATiming;
   USHORT  wBS;
   USHORT  wNumCurrentCyls;
   USHORT  wNumCurrentHeads;
   USHORT  wNumCurrentSectorsPerTrack;
   ULONG   ulCurrentSectorCapacity;
   USHORT  wMultSectorStuff;
   ULONG   ulTotalAddressableSectors;
   USHORT  wSingleWordDMA;
   USHORT  wMultiWordDMA;
   BYTE    bReserved[128];
} IDSECTOR, *PIDSECTOR;


typedef struct _SRB_IO_CONTROL
{
   ULONG HeaderLength;
   UCHAR Signature[8];
   ULONG Timeout;
   ULONG ControlCode;
   ULONG ReturnCode;
   ULONG Length;
} SRB_IO_CONTROL, *PSRB_IO_CONTROL;


   // Define global buffers.
BYTE IdOutCmd [sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1];

   //  Max number of drives assuming primary/secondary, master/slave topology
#define  MAX_IDE_DRIVES  16

namespace Utils
{
    bool DiskInfo::DoIdentify ( void * hPhysicalDriveIOCTL, PSENDCMDINPARAMS pSCIP,
                        PSENDCMDOUTPARAMS pSCOP, unsigned __int8 nIDCmd, unsigned __int8 nDriveNum,
                        unsigned __int32 * lpcbBytesReturned )
    {
        if( nullptr == pSCIP )
        {
            return false;
        }
            // Set up data structures for IDENTIFY command.
        pSCIP->cBufferSize                 = IDENTIFY_BUFFER_SIZE;
        pSCIP->irDriveRegs.bFeaturesReg    = 0;
        pSCIP->irDriveRegs.bSectorCountReg = 1;
        pSCIP->irDriveRegs.bCylLowReg      = 0;
        pSCIP->irDriveRegs.bCylHighReg     = 0;

            // Compute the drive number.
        pSCIP->irDriveRegs.bDriveHeadReg = 0xA0 | ((nDriveNum & 1) << 4);

            // The command can either be IDE identify or ATAPI identify.
        pSCIP->irDriveRegs.bCommandReg = nIDCmd;
        pSCIP->bDriveNumber            = nDriveNum;
        pSCIP->cBufferSize             = IDENTIFY_BUFFER_SIZE;

        return IsDeviceIoControl( hPhysicalDriveIOCTL, DFP_RECEIVE_DRIVE_DATA,
                                  (LPVOID) pSCIP,
                                  sizeof(SENDCMDINPARAMS) - 1,
                                  (LPVOID) pSCOP,
                                  sizeof(SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE - 1,
                                  (LPDWORD)lpcbBytesReturned );
    }
    //----------------------------------------------------------------------------------------------------------------------
    char *DiskInfo::ConvertToString( unsigned __int32 diskData[256], const size_t firstIndex, const size_t lastIndex )
    {
        if( nullptr == diskData || lastIndex < firstIndex )
        {
            return nullptr;
        }
        ::memset( m_cv, 0, sizeof(m_cv) );
        size_t position = 0;

            //  each integer has two characters stored in it backwards
        for(size_t index = firstIndex; index <= lastIndex; index++)
        {
                //  get high byte for 1st character
            m_cv[position] = static_cast<char>( diskData [index] / 256 );
            position++;

                //  get low byte for 2nd character
            m_cv[position] = static_cast<char>( diskData [index] % 256 );
            position++;
        }
            //  end the string 
        m_cv[position] = '\0';

            //  cut off the trailing blanks
        if( 0 < position )
        {
            for( size_t index = position - 1; index > 0 && ' ' == m_cv[index]; index-- )
            {
                m_cv [index] = '\0';
            }
        }
        return m_cv;
    }
    //----------------------------------------------------------------------------------------------------------------------
    bool DiskInfo::ReadPhysicalDriveInNTWithAdminRights( std::vector<disk_t> &disks )
    {
        bool done = false;
        disks.clear();

        for( size_t drive = 0; drive < MAX_IDE_DRIVES; drive++)
        {
            wchar_t wzMsg[255] = {0};

                //  Try to get a handle to PhysicalDrive IOCTL, report failure
                //  and exit if can't.
            wchar_t wzDriveName[256] = {0};

            ::_snwprintf( wzDriveName, _countof(wzDriveName)-1, L"\\\\.\\PhysicalDrive%d", (int)drive );

            HANDLE hPhysicalDriveIOCTL = OpenDevice( wzDriveName, GENERIC_READ | GENERIC_WRITE );  //  Windows NT, Windows 2000, must have admin rights

            if( INVALID_HANDLE_VALUE == hPhysicalDriveIOCTL )
            {
                ::_snwprintf( wzMsg, _countof(wzMsg)-1,
                            L"Unable to open physical drive %d, error code: 0x%lX\n",
                            (int)drive, GetLastError () );
                LogMessage( DID_INVALID_HANDLE_VALUE_e, wzMsg, __LINE__, __FUNCTION__ );

                return false;
            }
            if( INVALID_HANDLE_VALUE != hPhysicalDriveIOCTL )
            {
                GETVERSIONOUTPARAMS VersionParams;
                unsigned __int32    cbBytesReturned = 0x00;

                // Get the version, etc of PhysicalDrive IOCTL
                ::memset( (void*) &VersionParams, 0x00, sizeof(VersionParams) );

                if ( !IsDeviceIoControl( hPhysicalDriveIOCTL, DFP_GET_VERSION, NULL, 0
                                       , &VersionParams, sizeof(VersionParams), (LPDWORD)&cbBytesReturned ) )
                {         
                    ::_snwprintf( wzMsg, _countof(wzMsg)-1, L"DFP_GET_VERSION failed for drive %d\n", (int)drive );
                    LogMessage( DID_FAILED_DFP_GET_VERSION_e, wzMsg, __LINE__, __FUNCTION__ );

                    continue;
                }
                // If there is a IDE device at number "drive" issue commands
                // to the device
                if( 0 < VersionParams.bIDEDeviceMap )
                {
                    SENDCMDINPARAMS  scip;

                    // Now, get the ID sector for all IDE devices in the system.
                        // If the device is ATAPI use the IDE_ATAPI_IDENTIFY command,
                        // otherwise use the IDE_ATA_IDENTIFY command
                    BYTE bIDCmd = ( VersionParams.bIDEDeviceMap >> drive & 0x10 ) ? IDE_ATAPI_IDENTIFY : IDE_ATA_IDENTIFY;  

                    ::memset( &scip,        0x00, sizeof(scip) );
                    ::memset( m_szIdOutCmd, 0x00, sizeof(m_szIdOutCmd) );

                    if( DoIdentify( hPhysicalDriveIOCTL, 
                                &scip, 
                                (PSENDCMDOUTPARAMS)&m_szIdOutCmd, 
                                (BYTE) bIDCmd,
                                (BYTE) drive,
                                &cbBytesReturned) )
                    {
                        unsigned __int32 diskdata [256] = {0};
                        unsigned short *pIdSector = (unsigned short *)( (PSENDCMDOUTPARAMS)m_szIdOutCmd )->bBuffer;

                        for( int ijk = 0; ijk < 256; ijk++ )
                        {
                            diskdata [ijk] = pIdSector [ijk];
                        }
                        disk_t _disk;
                        done = GetIdeInfo( drive, diskdata, _disk );

                        if( done )
                        {
                            disks.push_back( _disk );
                        }
                    }
                }
                CloseDevice( hPhysicalDriveIOCTL );
            }
        }
        return done;
    }

// IDENTIFY data (from ATAPI driver source)
//

#pragma pack(1)

typedef struct _IDENTIFY_DATA {
    USHORT GeneralConfiguration;            // 00 00
    USHORT NumberOfCylinders;               // 02  1
    USHORT Reserved1;                       // 04  2
    USHORT NumberOfHeads;                   // 06  3
    USHORT UnformattedBytesPerTrack;        // 08  4
    USHORT UnformattedBytesPerSector;       // 0A  5
    USHORT SectorsPerTrack;                 // 0C  6
    USHORT VendorUnique1[3];                // 0E  7-9
    USHORT SerialNumber[10];                // 14  10-19
    USHORT BufferType;                      // 28  20
    USHORT BufferSectorSize;                // 2A  21
    USHORT NumberOfEccBytes;                // 2C  22
    USHORT FirmwareRevision[4];             // 2E  23-26
    USHORT ModelNumber[20];                 // 36  27-46
    UCHAR  MaximumBlockTransfer;            // 5E  47
    UCHAR  VendorUnique2;                   // 5F
    USHORT DoubleWordIo;                    // 60  48
    USHORT Capabilities;                    // 62  49
    USHORT Reserved2;                       // 64  50
    UCHAR  VendorUnique3;                   // 66  51
    UCHAR  PioCycleTimingMode;              // 67
    UCHAR  VendorUnique4;                   // 68  52
    UCHAR  DmaCycleTimingMode;              // 69
    USHORT TranslationFieldsValid:1;        // 6A  53
    USHORT Reserved3:15;
    USHORT NumberOfCurrentCylinders;        // 6C  54
    USHORT NumberOfCurrentHeads;            // 6E  55
    USHORT CurrentSectorsPerTrack;          // 70  56
    ULONG  CurrentSectorCapacity;           // 72  57-58
    USHORT CurrentMultiSectorSetting;       //     59
    ULONG  UserAddressableSectors;          //     60-61
    USHORT SingleWordDMASupport : 8;        //     62
    USHORT SingleWordDMAActive : 8;
    USHORT MultiWordDMASupport : 8;         //     63
    USHORT MultiWordDMAActive : 8;
    USHORT AdvancedPIOModes : 8;            //     64
    USHORT Reserved4 : 8;
    USHORT MinimumMWXferCycleTime;          //     65
    USHORT RecommendedMWXferCycleTime;      //     66
    USHORT MinimumPIOCycleTime;             //     67
    USHORT MinimumPIOCycleTimeIORDY;        //     68
    USHORT Reserved5[2];                    //     69-70
    USHORT ReleaseTimeOverlapped;           //     71
    USHORT ReleaseTimeServiceCommand;       //     72
    USHORT MajorRevision;                   //     73
    USHORT MinorRevision;                   //     74
    USHORT Reserved6[50];                   //     75-126
    USHORT SpecialFunctionsEnabled;         //     127
    USHORT Reserved7[128];                  //     128-255
} IDENTIFY_DATA, *PIDENTIFY_DATA;

#pragma pack()

    #define ID_CMD          0xEC            // Returns ID sector for ATA

    bool DiskInfo::ReadPhysicalDriveInNTUsingSmart(  std::vector<disk_t> &disks )
    {
       disks.clear();
       bool done = false;

       for( size_t drive = 0; drive < MAX_IDE_DRIVES; drive++ )
       {
          HANDLE hPhysicalDriveIOCTL = 0;

             //  Try to get a handle to PhysicalDrive IOCTL, report failure
             //  and exit if can't.
          wchar_t wzDriveName[256] = {L'\0'};

          ::_snwprintf( wzDriveName, _countof(wzDriveName)-1, L"\\\\.\\PhysicalDrive%d", (int)drive );

             //  Windows NT, Windows 2000, Windows Server 2003, Vista
          hPhysicalDriveIOCTL = OpenDeviceSmart( wzDriveName );

          if( hPhysicalDriveIOCTL == INVALID_HANDLE_VALUE )
          {
              wchar_t wzMsg[1024] = { L'\0'};
              ::_snwprintf( wzMsg, _countof(wzMsg)-1, L"Unable to open physical drive %d, error code: 0x%lX", (int)drive, GetLastError () );

                LogMessage( DID_INVALID_HANDLE_VALUE_e, wzMsg, __LINE__, __FUNCTION__ );
          }
          else
          {
             GETVERSIONINPARAMS GetVersionParams;
             DWORD cbBytesReturned = 0;

                // Get the version, etc of PhysicalDrive IOCTL
             ::memset ((void*) & GetVersionParams, 0, sizeof(GetVersionParams));

            if( IsDeviceIoControl( hPhysicalDriveIOCTL, SMART_GET_VERSION,
                       NULL, 
                       0,
                        &GetVersionParams, sizeof (GETVERSIONINPARAMS),
                       &cbBytesReturned ) )
             {         
                     // Print the SMART version
                   // PrintVersion (& GetVersionParams);
                   // Allocate the command buffer
                ULONG CommandSize        = sizeof(SENDCMDINPARAMS) + IDENTIFY_BUFFER_SIZE;
                PSENDCMDINPARAMS pCommand = (PSENDCMDINPARAMS)::malloc( CommandSize );

                if( nullptr == pCommand )
                {
                    return false;
                }
                // Retrieve the IDENTIFY data
                pCommand->irDriveRegs.bCommandReg = ID_CMD;            // Prepare the command
                DWORD BytesReturned = 0x00;

               if( !IsDeviceIoControl( hPhysicalDriveIOCTL, SMART_RCV_DRIVE_DATA, pCommand, sizeof(SENDCMDINPARAMS),
                                       pCommand, CommandSize, &BytesReturned ) )
                {
                       // Print the error
                    //PrintError ("SMART_RCV_DRIVE_DATA IOCTL", GetLastError());
                } 
                else
                {
                    unsigned int diskdata[256] = {0x00};
                    USHORT *pIdSector   = (USHORT *)(PIDENTIFY_DATA) ((PSENDCMDOUTPARAMS) pCommand)->bBuffer;

                    for( size_t ijk = 0; ijk < 256; ijk++ )
                    {
                       diskdata[ijk] = pIdSector[ijk];
                    }
                    done = GetIdeInfo( drive, diskdata, disks[drive]  );
                }
                ::CloseHandle( hPhysicalDriveIOCTL );                             // Done
                ::free( pCommand );
             }
          }
       }

       return done;
    }


        //----------------------------------------------------------------------------------------------------------------------
    //  Required to ensure correct PhysicalDrive IOCTL structure setup
    #pragma pack(4)

    //
    // IOCTL_STORAGE_QUERY_PROPERTY
    //
    // Input Buffer:
    //      a STORAGE_PROPERTY_QUERY structure which describes what type of query
    //      is being done, what property is being queried for, and any additional
    //      parameters which a particular property query requires.
    //
    //  Output Buffer:
    //      Contains a buffer to place the results of the query into.  Since all
    //      property descriptors can be cast into a STORAGE_DESCRIPTOR_HEADER,
    //      the IOCTL can be called once with a small buffer then again using
    //      a buffer as large as the header reports is necessary.
    //
    //
    // Types of queries
    //
    /*
    typedef enum _STORAGE_QUERY_TYPE 
    {
        PropertyStandardQuery = 0,          // Retrieves the descriptor
        PropertyExistsQuery,                // Used to test whether the descriptor is supported
        PropertyMaskQuery,                  // Used to retrieve a mask of writeable fields in the descriptor
        PropertyQueryMaxDefined     // use to validate the value
    } STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;
    */
    //
    // define some initial property id's
    //

    typedef enum _STORAGE_PROPERTY_ID 
    {
        StorageDeviceProperty = 0,
        StorageAdapterProperty
    } STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

    //
    // Query structure - additional parameters for specific queries can follow
    // the header
    //

    typedef struct _STORAGE_PROPERTY_QUERY {

        //
        // ID of the property being retrieved
        //

        STORAGE_PROPERTY_ID PropertyId;

        //
        // Flags indicating the type of query being performed
        //

        STORAGE_QUERY_TYPE QueryType;

        //
        // Space for additional parameters if necessary
        //

        UCHAR AdditionalParameters[1];

    } STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;


    #define IOCTL_STORAGE_QUERY_PROPERTY   CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

    //
    // Device property descriptor - this is really just a rehash of the inquiry
    // data retrieved from a scsi device
    //
    // This may only be retrieved from a target device.  Sending this to the bus
    // will result in an error
    //

    #pragma pack(4)

    typedef struct _STORAGE_DEVICE_DESCRIPTOR 
    {

        //
        // Sizeof(STORAGE_DEVICE_DESCRIPTOR)
        //

        ULONG Version;

        //
        // Total size of the descriptor, including the space for additional
        // data and id strings
        //

        ULONG Size;

        //
        // The SCSI-2 device type
        //

        UCHAR DeviceType;

        //
        // The SCSI-2 device type modifier (if any) - this may be zero
        //

        UCHAR DeviceTypeModifier;

        //
        // Flag indicating whether the device's media (if any) is removable.  This
        // field should be ignored for media-less devices
        //

        BOOLEAN RemovableMedia;

        //
        // Flag indicating whether the device can support mulitple outstanding
        // commands.  The actual synchronization in this case is the responsibility
        // of the port driver.
        //

        BOOLEAN CommandQueueing;

        //
        // Byte offset to the zero-terminated ascii string containing the device's
        // vendor id string.  For devices with no such ID this will be zero
        //

        ULONG VendorIdOffset;

        //
        // Byte offset to the zero-terminated ascii string containing the device's
        // product id string.  For devices with no such ID this will be zero
        //

        ULONG ProductIdOffset;

        //
        // Byte offset to the zero-terminated ascii string containing the device's
        // product revision string.  For devices with no such string this will be
        // zero
        //

        ULONG ProductRevisionOffset;

        //
        // Byte offset to the zero-terminated ascii string containing the device's
        // serial number.  For devices with no serial number this will be zero
        //

        ULONG SerialNumberOffset;

        //
        // Contains the bus type (as defined above) of the device.  It should be
        // used to interpret the raw device properties at the end of this structure
        // (if any)
        //

        STORAGE_BUS_TYPE BusType;

        //
        // The number of bytes of bus-specific data which have been appended to
        // this descriptor
        //

        ULONG RawPropertiesLength;

        //
        // Place holder for the first byte of the bus specific property data
        //

        UCHAR RawDeviceProperties[1];

    } STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;


    //--------------------------------------------------------------------------------------------------------
    //  function to decode the serial numbers of IDE hard drives
    //  using the IOCTL_STORAGE_QUERY_PROPERTY command 
    char* DiskInfo::FlipAndCodeBytes( char * str, int pos, int flip, char * buf )
    {
       int i;
       int j = 0;
       int k = 0;

       buf [0] = '\0';
       if (pos <= 0)
          return buf;

       if ( ! j)
       {
          char p = 0;

          // First try to gather all characters representing hex digits only.
          j = 1;
          k = 0;
          buf[k] = 0;
          for (i = pos; j && str[i] != '\0'; ++i)
          {
              char c = static_cast<char>( ::tolower( str[i] ) );

             if (isspace(c))
                c = '0';

             ++p;
             buf[k] <<= 4;

             if (c >= '0' && c <= '9')
                buf[k] |= (unsigned char) (c - '0');
             else if (c >= 'a' && c <= 'f')
                buf[k] |= (unsigned char) (c - 'a' + 10);
             else
             {
                j = 0;
                break;
             }

             if (p == 2)
             {
                if (buf[k] != '\0' && ! isprint(buf[k]))
                {
                   j = 0;
                   break;
                }
                ++k;
                p = 0;
                buf[k] = 0;
             }
          }
       }

       if ( ! j)
       {
          // There are non-digit characters, gather them as is.
          j = 1;
          k = 0;
          for( i = pos; j && str[i] != '\0'; ++i )
          {
             char c = str[i];

             if ( ! isprint(c))
             {
                j = 0;
                break;
             }

             buf[k++] = c;
          }
       }

       if ( ! j)
       {
          // The characters are not there or are not printable.
          k = 0;
       }

       buf[k] = '\0';

       if (flip)
          // Flip adjacent characters
          for (j = 0; j < k; j += 2)
          {
             char t = buf[j];
             buf[j] = buf[j + 1];
             buf[j + 1] = t;
          }

       // Trim any beginning and end space
       i = j = -1;
       for (k = 0; buf[k] != '\0'; ++k)
       {
          if (! isspace(buf[k]))
          {
             if (i < 0)
                i = k;
             j = k;
          }
       }

       if ((i >= 0) && (j >= 0))
       {
          for (k = i; (k <= j) && (buf[k] != '\0'); ++k)
             buf[k - i] = buf[k];
          buf[k - i] = '\0';
       }
       return buf;
    }

#define IOCTL_DISK_GET_DRIVE_GEOMETRY_EX CTL_CODE(IOCTL_DISK_BASE, 0x0028, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DISK_GEOMETRY_EX {
  DISK_GEOMETRY  Geometry;
  LARGE_INTEGER  DiskSize;
  UCHAR  Data[1];
} DISK_GEOMETRY_EX, *PDISK_GEOMETRY_EX;

static const unsigned long s_ulLengthInternalBuffer = 16385UL;

    //--------------------------------------------------------------------------------------------------------
    bool DiskInfo::ReadPhysicalDriveInNTWithZeroRights( std::vector<disk_t> &lst_disk )
    {
        bool done = false;
        lst_disk.clear();
        wchar_t wzMsg[255] = {0};

        for( int drive = 0; drive < MAX_IDE_DRIVES; drive++)
        {
            wchar_t wzDriveName[256] = {0};

            ::_snwprintf( wzDriveName, _countof(wzDriveName)-1, L"\\\\.\\PhysicalDrive%d", drive );
             wzDriveName[ _countof(wzDriveName) - 1 ] = L'\0';

            HANDLE hPhysicalDriveIOCTL = OpenDevice( wzDriveName, 0 ); // //  Windows NT, Windows 2000, Windows XP - admin rights not required

            if( INVALID_HANDLE_VALUE == hPhysicalDriveIOCTL )
            {
                _snwprintf( wzMsg, _countof(wzMsg)-1,
                            L"Unable to open physical drive %d, error code: 0x%lX", (int)drive, GetLastError () );

                LogMessage( DID_INVALID_HANDLE_VALUE_e, wzMsg, __LINE__, __FUNCTION__ );
            }
            disk_t disk;

            if( hPhysicalDriveIOCTL != INVALID_HANDLE_VALUE )
            {
                STORAGE_PROPERTY_QUERY query;
                DWORD cbBytesReturned = 0;
                char pLocalBuffer[ s_ulLengthInternalBuffer ] = {0};

                ::memset( (void *) &query, 0, sizeof(query) );

                query.PropertyId = StorageDeviceProperty;
                query.QueryType = PropertyStandardQuery;

                if( IsDeviceIoControl( hPhysicalDriveIOCTL, IOCTL_STORAGE_QUERY_PROPERTY,
                        & query,
                        sizeof(query),
                        &pLocalBuffer,
                        s_ulLengthInternalBuffer,
                        & cbBytesReturned ) )
                {         
                    STORAGE_DEVICE_DESCRIPTOR* descrip = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR *>( &pLocalBuffer );
                    char serialNumber    [1024] = {0};
                    char modelNumber     [1024] = {0};
                    char vendorId        [1024] = {0};
                    char productRevision [1024] = {0};

                    disk.version = descrip->Version;
                    disk.size    = descrip->Size;
                    disk.type    = descrip->DeviceType;
                    disk.deviceTypeModifier = descrip->DeviceTypeModifier;
                    disk.num_controller     = drive;

                    FlipAndCodeBytes( pLocalBuffer, descrip->VendorIdOffset,         0, vendorId );
                    FlipAndCodeBytes( pLocalBuffer, descrip->ProductIdOffset,        0, modelNumber );
                    FlipAndCodeBytes( pLocalBuffer, descrip->ProductRevisionOffset,  0, productRevision );
                    FlipAndCodeBytes( pLocalBuffer, descrip->SerialNumberOffset,     1, serialNumber );

                    done = true;

                    if( '\0' == *m_szHardDriveSerialNumber &&
                            //  serial number must be alphanumeric
                            //  (but there can be leading spaces on IBM drives)
                        ( ::isalnum(serialNumber[0]) || ::isalnum(serialNumber[19])))
                    {
                        ::strcpy( m_szHardDriveSerialNumber, serialNumber );
                        ::strcpy( m_szHardDriveModelNumber,  modelNumber );
                    }
                    ::strncpy( disk.vendor,    vendorId,        sizeof(disk.vendor) );
                    ::strncpy( disk.model,     modelNumber,     sizeof(disk.model) );
                    ::strncpy( disk.revision,  productRevision, sizeof(disk.revision) );
                    ::strncpy( disk.serial,    serialNumber,    sizeof(disk.serial) );
                }
                else
                {
                    _snwprintf( wzMsg, _countof(wzMsg)-1,
                                L"DeviceIOControl IOCTL_STORAGE_QUERY_PROPERTY error = %d", GetLastError () );
                    LogMessage( DID_IOCTL_STORAGE_QUERY_e, wzMsg, __LINE__, __FUNCTION__ );
                }
                ::memset( pLocalBuffer, 0x00, s_ulLengthInternalBuffer );

                if( IsDeviceIoControl( hPhysicalDriveIOCTL, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        NULL,
                        0,
                        &pLocalBuffer,
                        s_ulLengthInternalBuffer,
                        & cbBytesReturned ) )
                {   
                    DISK_GEOMETRY_EX* geom = reinterpret_cast<DISK_GEOMETRY_EX *>( &pLocalBuffer[0] );

                    disk.fixed   = ( geom->Geometry.MediaType == FixedMedia );
                    disk.size    = geom->DiskSize.QuadPart;
                }
                else
                {
                    DWORD err = ::GetLastError ();
    
                    switch (err)
                    {
                      case 1: 
                        _snwprintf( wzMsg, _countof(wzMsg)-1, L"DeviceIOControl IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER error = %d The request is not valid for this device.", 
                            GetLastError () );
                        LogMessage( DID_RequestIsNotValid_e, wzMsg, __LINE__, __FUNCTION__ );
                        break;
                      case 50:
                        _snwprintf( wzMsg, _countof(wzMsg)-1, L"DeviceIOControl IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER error = %d The request is not supported for this device.", 
                            GetLastError () );
                        LogMessage( DID_RequestIsNotSupported_e, wzMsg, __LINE__, __FUNCTION__ );
                        break;
                      default:
                        _snwprintf( wzMsg, _countof(wzMsg)-1, L"DeviceIOControl IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER error = %d", 
                                    GetLastError () );
                        LogMessage( DID_ERROR_MEDIA_SERIAL_NUMBER_e, wzMsg, __LINE__, __FUNCTION__ );
                    }
    
                }
                if( done )
                {
                    lst_disk.push_back( disk );
                }
                CloseDevice( hPhysicalDriveIOCTL ) ;
            }
        }
        return done;
    }

    //  ----------------------------------------------------------------------------------------------
    #define  SENDIDLENGTH  sizeof( SENDCMDOUTPARAMS ) + IDENTIFY_BUFFER_SIZE

    bool DiskInfo::ReadScsiDriveInController( void* hScsiDriveIOCTL, const size_t controller, const size_t drive, disk_t& disk )
    {
        if( nullptr == hScsiDriveIOCTL || s_nMaxScsiDrivesAtController <= drive || s_nMaxScsiControllers <= controller )
        {
            return false;
        }
        const size_t bufferLength   = sizeof(SRB_IO_CONTROL) + SENDIDLENGTH;
        char buffer[ bufferLength ] = {'\0'};

        SRB_IO_CONTROL*    p   = (SRB_IO_CONTROL *) buffer;
        SENDCMDINPARAMS*   pin = (SENDCMDINPARAMS *) (buffer + sizeof(SRB_IO_CONTROL) );

        p->HeaderLength = sizeof( SRB_IO_CONTROL );
        p->Timeout      = 10000;
        p->Length       = SENDIDLENGTH;
        p->ControlCode  = IOCTL_SCSI_MINIPORT_IDENTIFY;
        ::strncpy( (char *)p->Signature, "SCSIDISK", 8 );
      
        pin->irDriveRegs.bCommandReg = IDE_ATA_IDENTIFY;
        pin->bDriveNumber            = (unsigned char)drive;
        DWORD dummy = 0x00;

        if( IsDeviceIoControl(hScsiDriveIOCTL, IOCTL_SCSI_MINIPORT, 
                                buffer,
                                sizeof (SRB_IO_CONTROL) + sizeof (SENDCMDINPARAMS) - 1,
                                buffer,
                                sizeof (SRB_IO_CONTROL) + SENDIDLENGTH,
                                &dummy ))
        {
            SENDCMDOUTPARAMS *pOut = (SENDCMDOUTPARAMS *) (buffer + sizeof (SRB_IO_CONTROL));
            IDSECTOR *pId = (IDSECTOR *) (pOut->bBuffer);

            if( nullptr != pId && 0x00 != pId->sModelNumber[0] )
            {
                unsigned __int32 diskdata[256] = {0x00};
                USHORT*          pIdSector     = (USHORT *)pId;
              
                for( int i = 0; i < 256; i++ )
                {
                    diskdata[i] = pIdSector[i];
                }
                return GetIdeInfo( controller * 2 + drive, diskdata, disk );
            }
        }
        return false;
    }
    //  ----------------------------------------------------------------------------------------------
    bool DiskInfo::ReadIdeDriveAsScsiDriveInNT( std::vector<disk_t>& lst_disk )
    {
        bool done = false;

        lst_disk.clear();

        for( size_t controller = 0; controller < s_nMaxScsiControllers; controller++ )
        {
            HANDLE hScsiDriveIOCTL   = nullptr;
            wchar_t wzDriveName[256] = {0};

            _snwprintf( wzDriveName, _countof(wzDriveName)-1, L"\\\\.\\Scsi%d:", (int)controller );

            //  Try to get a handle to PhysicalDrive IOCTL, report failure and exit if can't.

            hScsiDriveIOCTL = OpenDevice( wzDriveName, GENERIC_READ | GENERIC_WRITE );  //  Windows NT, Windows 2000, any rights should do

            if( hScsiDriveIOCTL == INVALID_HANDLE_VALUE )
            {
                wchar_t wzMsg[512] = {0};
                _snwprintf( wzMsg, _countof(wzMsg)-1,
                    L"Unable to open SCSI controller %d, error code: 0x%lX", (int)controller, GetLastError () );
                LogMessage( DID_Unable_Open_SCSI_e, wzMsg, __LINE__, __FUNCTION__ );

                continue;
            }
            for( size_t drive = 0; drive < s_nMaxScsiDrivesAtController; drive++)
            {
                disk_t disk;

                if( ReadScsiDriveInController( hScsiDriveIOCTL, controller, drive, disk ) )
                {
                    lst_disk.push_back( disk );
                }
            }
            CloseDevice( hScsiDriveIOCTL );
        }
        return done;
    }

    DiskInfo::DiskInfo()
    {
        ::memset( m_szIdOutCmd,              0x00, sizeof(m_szIdOutCmd) );
        ::memset( m_szHardDriveSerialNumber, '\0', sizeof(m_szHardDriveSerialNumber) );
        ::memset( m_szHardDriveModelNumber,  '\0', sizeof(m_szHardDriveModelNumber) );
        ::memset( m_flipped,                 '\0', sizeof(m_flipped) );
        ::memset( m_cv,                      '\0', sizeof(m_cv) );
    }

    //----------------------------------------------------------------------------------------------------------------------
    bool DiskInfo::GetIdeInfo( const size_t drive, unsigned __int32 diskdata [256], disk_t &_disk )    // void PrintIdeInfo (int drive, DWORD diskdata [256])
    {
       char serialNumber   [1024] = {0x00};
       char modelNumber    [1024] = {0x00};
       char revisionNumber [1024] = {0x00};
       char bufferSize    [32]    = {0x00};

          //  copy the hard drive serial number to the buffer
       ::strcpy( serialNumber, ConvertToString ( diskdata, 10, 19 ) );
       ::strcpy( modelNumber,  ConvertToString ( diskdata, 27, 46 ) );
       ::strcpy( revisionNumber, ConvertToString ( diskdata, 23, 26 ) );
       ::sprintf( bufferSize, "%u", diskdata[21] * 512  );

       if( 0 == m_szHardDriveSerialNumber [0] &&
           //  serial number must be alphanumeric
           //  (but there can be leading spaces on IBM drives)
           ( ::isalnum( serialNumber[0] ) || ::isalnum( serialNumber[19]) ) )
       {
          ::strcpy( m_szHardDriveSerialNumber, serialNumber);
          ::strcpy( m_szHardDriveModelNumber,  modelNumber);
       }
       _disk.drive = drive;

        switch (drive / 2)
        {
            case 0: _disk.num_controller = 0; break;  //Primary Controller
            case 1: _disk.num_controller = 1; break;  //Secondary Controller
            case 2: _disk.num_controller = 2; break;  //Tertiary Controller
            case 3: _disk.num_controller = 3; break;  //Quaternary Controller
        }
        switch (drive % 2)
        {
            case 0: _disk.master_slave = true; break;
            case 1: _disk.master_slave = false; break;
        }

        ::strncpy( _disk.model,    modelNumber,    sizeof(_disk.model)-1 );
        ::strncpy( _disk.serial,   serialNumber,   sizeof(_disk.serial)-1 );
        ::strncpy( _disk.revision, revisionNumber, sizeof(_disk.revision)-1 );

        _disk.buffer = ::_atoi64(bufferSize);

        if( diskdata [0] & 0x0080 )
        {
            _disk.fixed = false;
        }else 
            if( diskdata [0] & 0x0040 )
        {
            _disk.fixed = true;
        }
            //  calculate size based on 28 bit or 48 bit addressing
            //  48 bit addressing is reflected by bit 10 of word 83
        if( diskdata [83] & 0x400 ) 
        {
            _disk.sectors = diskdata [103] * 65536I64 * 65536I64 * 65536I64 + 
                            diskdata [102] * 65536I64 * 65536I64 + 
                            diskdata [101] * 65536I64 + 
                            diskdata [100];
        }
        else
        {
            _disk.sectors = diskdata [61] * 65536 + diskdata [60];
        }
            //  there are 512 bytes in a sector
        _disk.size = _disk.sectors * 512;

        return true;
    }

    //-------------------------------------------------------------------------------------------------------------------
    bool DiskInfo::GetDrivesInfo( std::vector<disk_t> &disks )
    {
       OSVERSIONINFO version;

       ::memset( &version, 0, sizeof (version) );
       disks.clear();
       *m_szHardDriveSerialNumber = '\0';

       version.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
       GetVersionEx (&version);

       if( version.dwPlatformId == VER_PLATFORM_WIN32_NT )
       {
            //  this works under WinNT4 or Win2K if you have admin rights
            bool done = ReadPhysicalDriveInNTWithAdminRights( disks );                          //  Trying to read the drive IDs using physical access with admin rights

            //  this should work in WinNT or Win2K if previous did not work
            //  this is kind of a backdoor via the SCSI mini port driver into
            //     the IDE drives
            if( !done )
            {
               done = ReadIdeDriveAsScsiDriveInNT( disks );                                // Trying to read the drive IDs using the SCSI back door
            }
            //  this works under WinNT4 or Win2K or WinXP if you have any rights
            if( !done )
            {
               done = ReadPhysicalDriveInNTWithZeroRights( disks );                        // nTrying to read the drive IDs using physical access with zero rights
            }
            //  this works under WinNT4 or Win2K or WinXP or Windows Server 2003 or Vista if you have any rights
            if( !done )
            {
               ReadPhysicalDriveInNTUsingSmart( disks );                                   // Trying to read the drive IDs using Smart
            }
       }
       // find vendors regards: http://en.wikipedia.org/wiki/List_of_defunct_hard_disk_manufacturers
       for( size_t i = 0; i < disks.size(); i++ )
       {
           char model[256] = {'\0'};
           const char* vendorName[]      = {"MAXTOR", "WDC", "SAMSUNG", "CONNER", "QUANTUM",             "FUJITSU", "TOSHIBA", "FUJI", "HITACHI", "IBM", "LACIE", "ST",                 "NEC", "OSZ",            "INTEL", "ADATA", "LEXAR", "SANDISK", "MICRON",            "MUSHKIN" };
           const char* vendorShortName[] = {"Maxtor", "WD",  "Samsung", "Conner", "Quantum Corporation", "Fujitsu", "Toshiba", "Fuji", "Hitachi", "IBM", "LaCie", "Seagate Technology", "NEC", "OCZ Technology", "Intel", "ADATA", "Lexar", "SanDisk", "Micron Technology", "Mushkin" };

           ::strncpy( model, disks[i].model, sizeof(model)-1 );
           model[ sizeof(model) - 1 ] = '\0';

           for( char* ptr = model; *ptr; ++ptr )                          // turn to uppercase
           {
              *ptr = static_cast<char>( ::toupper( *ptr ) );
           }
           for( size_t i = 0; i < _countof(vendorName); ++i )             // find vendor
           {
               if( nullptr != ::strstr( model,  vendorName[i] ) )
               {
                   ::strcpy( disks[i].vendor, vendorShortName[i] );
               }
           }
       }
       return (disks.size() > 0);
    }

    bool DiskInfo::IsDeviceIoControl(
                    void* hDevice,
                    unsigned long dwIoControlCode,
                    void* lpInBuffer,
                    unsigned long nInBufferSize,
                    void* lpOutBuffer,
                    unsigned long nOutBufferSize,
                    unsigned long* lpBytesReturned
                    )
    { 
        try
        {
            return ( 1 == ::DeviceIoControl( hDevice, dwIoControlCode, lpInBuffer, nInBufferSize
                                        , lpOutBuffer, nOutBufferSize, lpBytesReturned, NULL ) ); 
        }catch(...)
        {
            LogMessage( DID_Unknown_Exception_e, L"unknown exception", __LINE__, __FUNCTION__ );
            return false;
        }
    }

    bool DiskInfo::CloseDevice( void* hObject )
    {
        if( nullptr == hObject || ((void*)~0) == hObject )
        {
            return false;
        }
        try
        {
            return ( TRUE == ::CloseHandle( hObject ) );
        }
        catch(...)
        {
            LogMessage( DID_Unknown_Exception_e, L"unknown exception", __LINE__, __FUNCTION__ );
            return false;
        }
    }

    void* DiskInfo::OpenDevice( wchar_t* wzFileName, unsigned long dwDesiredAccess )
    {
        if( nullptr == wzFileName || L'\0' == *wzFileName )
        {
            return nullptr;
        }
        try
        {
          return ::CreateFileW( wzFileName, dwDesiredAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
        }
        catch(...)
        {
            LogMessage( DID_Unknown_Exception_e, L"unknown exception", __LINE__, __FUNCTION__ );
            return nullptr;
        }
    };

    void* DiskInfo::OpenDeviceSmart( wchar_t* wzFileName )
    {
        if( nullptr == wzFileName || L'\0' == *wzFileName )
        {
            return nullptr;
        }
        try
        {
          return ::CreateFileW( wzFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
        }
        catch(...)
        {
            LogMessage( DID_Unknown_Exception_e, L"unknown exception", __LINE__, __FUNCTION__ );
            return nullptr;
        }
    };

    __int64 DiskInfo::GetHardDriveComputerID()
    {
        DiskInfo comp;
        __int64 result = ~0;
        try
        {
            std::vector<Utils::disk_t> disks;
            comp.GetDrivesInfo( disks );

            for( size_t i = 0; i < disks.size(); i++ )
            {
                disk_t& disk = disks[ i ];

                result ^= disk.GetUUID();
            }
            return result;
        }
        catch(...)
        {
            return 0;

            LogMessage( DID_Unknown_Exception_e, L"unknown exception", __LINE__, __FUNCTION__ );
        }
    }

}
