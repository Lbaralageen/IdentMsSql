
#include <stdio.h>
#include <tchar.h>
#include <iostream>

#include "TCppUnit.h"

#include "diskid.h"

size_t GetMACaddress( unsigned long ulMACaddress[16], std::vector<std::string>& sMACaddress, char* szError );

class DiskInfoStub : public Utils::DiskInfo
{
private:
    char*               m_szResultOfConvertToString;

    GETVERSIONOUTPARAMS m_Result_VersionParams;
    bool                m_bResult_DeviceIoControl;
    char                m_szResult_ModelNumber[32];

    void*               m_hResult_OpenDevice;
    bool                m_bResult_DoIdentify;
    bool                m_bResult_GetIdeInfo;
public:
    DiskInfoStub() : 
        m_szResultOfConvertToString(nullptr)
      , m_bResult_DeviceIoControl(false)
      , m_hResult_OpenDevice(nullptr)
      , m_bResult_DoIdentify(false)
      , m_bResult_GetIdeInfo(false)
      {
         ::memset( (void*) &m_Result_VersionParams, 0x00, sizeof(m_Result_VersionParams) );
         ::memset( m_szResult_ModelNumber, 0x00, sizeof(m_szResult_ModelNumber) );
      };

    char*    GetCv(){ return m_cv; }

    void SetResultOfConvertToString( char*  result )
    {
        m_szResultOfConvertToString = result;
    }
    char* ConvertToString( unsigned __int32 diskData[256], const size_t firstIndex, const size_t lastIndex )
    {
        return m_szResultOfConvertToString;
    }
    bool  CloseDevice( void* hObject )
    {
        return true;
    }
    //-------------------------------------------------------------------------

    void SetResult_DeviceIoControl( bool result, unsigned __int8 bIDEDeviceMap, const char* szModelNumber )
    {
        m_Result_VersionParams.bIDEDeviceMap = bIDEDeviceMap;
        m_bResult_DeviceIoControl = result; 

        if( nullptr != szModelNumber )
        {
            ::strncpy( m_szResult_ModelNumber, szModelNumber, sizeof(m_szResult_ModelNumber) );
        }
    }
    bool IsDeviceIoControl(
                void* hDevice,
                unsigned long dwIoControlCode,
                void* lpInBuffer,
                unsigned long nInBufferSize,
                void* lpOutBuffer,
                unsigned long nOutBufferSize,
                unsigned long* lpBytesReturned
        )
    { 
        if( nullptr != lpOutBuffer )
        {
            GETVERSIONOUTPARAMS* object = (GETVERSIONOUTPARAMS *)lpOutBuffer;

            object->bIDEDeviceMap = m_Result_VersionParams.bIDEDeviceMap;

            SENDCMDOUTPARAMS* pOut = (SENDCMDOUTPARAMS *)( (char*)lpOutBuffer + sizeof (SRB_IO_CONTROL));
            IDSECTOR*         pId  = (IDSECTOR *)( pOut->bBuffer );

            if( 0x00 != *m_szResult_ModelNumber )
            {
                ::strcpy( pId->sModelNumber, m_szResult_ModelNumber );
            }
        }
        return m_bResult_DeviceIoControl;
    }
    //-------------------------------------------------------------------------

    void SetResult_OpenDevice( void* result )
    {
        m_hResult_OpenDevice = result; 
    }
    virtual void* OpenDevice( wchar_t *wzFileName, unsigned long dwDesiredAccess )
    {
        return m_hResult_OpenDevice;
    }
    //-------------------------------------------------------------------------

    void SetResult_DoIdentify(bool result )
    {
        m_bResult_DoIdentify = result; 
    }
    bool DoIdentify( void * hPhysicalDriveIOCTL, PSENDCMDINPARAMS pSCIP,
                                PSENDCMDOUTPARAMS pSCOP, unsigned __int8 nIDCmd, unsigned __int8 nDriveNum,
                                unsigned __int32 * lpcbBytesReturned)
    {
        return m_bResult_DoIdentify;
    }
    //-------------------------------------------------------------------------

    void SetResult_GetIdeInfo(bool result )
    {
        m_bResult_GetIdeInfo = result; 
    }
    bool GetIdeInfo( const size_t drive, unsigned __int32 diskdata[256], Utils::disk_t &_disk  )
    {
        return m_bResult_GetIdeInfo;
    }
};

class DiskIdTests : public TCppUnit
{
private:
    DiskInfoStub*      m_pObject;

    unsigned __int32   m_diskdata[256];
    DiskInfoStub::PSENDCMDINPARAMS   m_pSCIP;
    DiskInfoStub::PSENDCMDOUTPARAMS  m_pSCOP;
    unsigned __int32*  m_lpcbBytesReturned;
    void*              m_hPhysicalDriveIOCTL;

public:
    REGISTER_CLASS( DiskIdTests, 10 )
    {
    }
    virtual void SetUp()
    {
        m_pObject = new DiskInfoStub;
        m_pSCIP   = new DiskInfoStub::SENDCMDINPARAMS;
        m_pSCOP   = new DiskInfoStub::SENDCMDOUTPARAMS;
        m_lpcbBytesReturned = nullptr;
        m_hPhysicalDriveIOCTL = nullptr;
        ::memset( m_diskdata, 0x00, sizeof(m_diskdata) );

        CPPUNIT_ASSERT_MESSAGE( "cannot be zero", nullptr != m_pObject->GetCv() );
    }
    virtual void TearsDown()
    {
       delete m_pObject;
       delete m_pSCIP;
       delete m_pSCOP;
    }

    // -------------------------------------------------------------------------------------------
    void TestConvertToString_FailedDiskdataIsNull()
    {
        char *result = m_pObject->DiskInfo::ConvertToString( nullptr, 0, 0 );

        CPPUNIT_ASSERT_EQUAL_MESSAGE( "cannot convert string", nullptr, result );
    }

    void TestConvertToString_FailedFirstIndexBiggerLastIndex()
    {
        char *result = m_pObject->DiskInfo::ConvertToString( m_diskdata, 2, 1 );

        CPPUNIT_ASSERT_EQUAL_MESSAGE( "cannot convert string", nullptr, result );
    }

    void TestConvertToString_Failedm_PassedIsEmpty()
    {
        char *result = m_pObject->DiskInfo::ConvertToString( m_diskdata, 1, 2 );

        CPPUNIT_ASSERT_EQUAL_MESSAGE( "cannot convert string", 0x00, *result );
    }

    void TestConvertToString_Failedm_PassedIsSinglew()
    {
        m_diskdata[0] = 'w';
        char *result = m_pObject->DiskInfo::ConvertToString( m_diskdata, 0, 1 );

        CPPUNIT_ASSERT_EQUAL_MESSAGE( "cannot convert string", 0x00, *result );
    }

    void TestConvertToString_Failedm_PassedIsSingleFF()
    {
        m_diskdata[0] = 0xFF;
        char *result = m_pObject->DiskInfo::ConvertToString( m_diskdata, 0, 1 );

        CPPUNIT_ASSERT_EQUAL_MESSAGE( "cannot convert string", 0x00, *result );
    }

    void TestConvertToString_Failedm_PassedIsSingle0X1234()
    {
        m_diskdata[0] = 0x1234;
        char *result = m_pObject->DiskInfo::ConvertToString( m_diskdata, 0, 1 );

        CPPUNIT_ASSERT_EQUAL_MESSAGE( "cannot convert string", 18, *result );
    }

    void TestConvertToString_Failedm_PassedIsSingle0XFFFF()
    {
        m_diskdata[0] = 0xFFFF;
        char *result = m_pObject->DiskInfo::ConvertToString( m_diskdata, 0, 1 );

        CPPUNIT_ASSERT_EQUAL_MESSAGE( "cannot convert string", -1, *result );
    }
    // -------------------------------------------------------------------------------------------
    void TestDoIdentify_FailedPhysicalDriveIOCTLIsNull()
    {
        __int8 hPhysicalDriveIOCTL = {0x00};

        bool result = m_pObject->DiskInfo::DoIdentify( (void*)hPhysicalDriveIOCTL, nullptr, m_pSCOP, 0, 0, m_lpcbBytesReturned );

        CPPUNIT_ASSERT_MESSAGE( "failed to identify", !result );
    }

    void TestDoIdentify_FailedDeviceIoControl()
    {
        __int8 hPhysicalDriveIOCTL = {0x00};
        m_pObject->SetResult_DeviceIoControl( false, 0x00, nullptr );

        bool result = m_pObject->DiskInfo::DoIdentify( (void*)hPhysicalDriveIOCTL, m_pSCIP, m_pSCOP, 0, 0, m_lpcbBytesReturned );

        CPPUNIT_ASSERT_MESSAGE( "failed to identify", !result );
    }

    void TestDoIdentify_PassedDeviceIoControl()
    {
        __int8 hPhysicalDriveIOCTL = {0x00};
        m_pObject->SetResult_DeviceIoControl( true, 0x00, nullptr );

        bool result = m_pObject->DiskInfo::DoIdentify( (void*)hPhysicalDriveIOCTL, m_pSCIP, m_pSCOP, 0, 0, m_lpcbBytesReturned );

        CPPUNIT_ASSERT_MESSAGE( "process to identify", result );
    }
    // -------------------------------------------------------------------------------------------
    void TestReadPhysicalDriveInNTWithAdminRights_FailedOpenDevice()
    {
       std::vector<Utils::disk_t> m_disks;
        m_pObject->SetResult_OpenDevice( nullptr );
        m_pObject->SetResult_DoIdentify( false );

        bool result = m_pObject->DiskInfo::ReadPhysicalDriveInNTWithAdminRights( m_disks );

        CPPUNIT_ASSERT_MESSAGE( "failed to read rights", !result );
    }

    void TestReadPhysicalDriveInNTWithAdminRights_FailedDoIdentify()
    {
       std::vector<Utils::disk_t> m_disks;
        m_pObject->SetResult_OpenDevice( this );  // fake handle
        m_pObject->SetResult_DoIdentify( false );

        bool result = m_pObject->DiskInfo::ReadPhysicalDriveInNTWithAdminRights( m_disks );

        CPPUNIT_ASSERT_MESSAGE( "failed to identify", !result );
    }

    void TestReadPhysicalDriveInNTWithAdminRights_FailedDeviceIoControl()
    {
       std::vector<Utils::disk_t> m_disks;
        m_pObject->SetResult_OpenDevice( this );  // fake handle
        m_pObject->SetResult_DeviceIoControl( false, 0x00, nullptr );
        m_pObject->SetResult_DoIdentify( false );

        bool result = m_pObject->DiskInfo::ReadPhysicalDriveInNTWithAdminRights( m_disks );

        CPPUNIT_ASSERT_MESSAGE( "failed to identify", !result );
    }

    void TestReadPhysicalDriveInNTWithAdminRights_FailedIDEDeviceMapIs0()
    {
       std::vector<Utils::disk_t> m_disks;
        m_pObject->SetResult_OpenDevice( this );  // fake handle
        m_pObject->SetResult_DeviceIoControl( false, 0x00, nullptr );
        m_pObject->SetResult_DoIdentify( false );

        bool result = m_pObject->DiskInfo::ReadPhysicalDriveInNTWithAdminRights( m_disks );

        CPPUNIT_ASSERT_MESSAGE( "failed to identify", !result );
    }

    void TestReadPhysicalDriveInNTWithAdminRights_FailedGetIdeInfo()
    {
       std::vector<Utils::disk_t> m_disks;
        m_pObject->SetResult_OpenDevice( this );  // fake handle
        m_pObject->SetResult_DeviceIoControl( true, 0x01, nullptr );
        m_pObject->SetResult_DoIdentify( true );
        m_pObject->SetResult_GetIdeInfo( false );

        bool result = m_pObject->DiskInfo::ReadPhysicalDriveInNTWithAdminRights( m_disks );

        CPPUNIT_ASSERT_MESSAGE( "failed to identify", !result );
    }

    void TestReadPhysicalDriveInNTWithAdminRights_Passed()
    {
       std::vector<Utils::disk_t> m_disks;
        m_pObject->SetResult_OpenDevice( this );  // fake handle
        m_pObject->SetResult_DeviceIoControl( true, 0x01, nullptr );
        m_pObject->SetResult_DoIdentify( true );
        m_pObject->SetResult_GetIdeInfo( true );

        bool result = m_pObject->DiskInfo::ReadPhysicalDriveInNTWithAdminRights( m_disks );

        CPPUNIT_ASSERT_MESSAGE( "passed to identify", result );
    }
    // -------------------------------------------------------------------------------------------
    void TestReadScsiDriveInController_FailedIOCTLIsNull()
    {
        Utils::disk_t disk;
        HANDLE hScsiDriveIOCTL = nullptr;

        bool result = m_pObject->DiskInfo::ReadScsiDriveInController( hScsiDriveIOCTL, 0, 0, disk );

        CPPUNIT_ASSERT_MESSAGE( "failed to read rights", !result );
    }

    void TestReadScsiDriveInController_FailedExeedNumberDrives()
    {
        Utils::disk_t disk;
        HANDLE hScsiDriveIOCTL = this; // fake address

        bool result = m_pObject->DiskInfo::ReadScsiDriveInController( hScsiDriveIOCTL, 0, s_nMaxScsiDrivesAtController, disk );

        CPPUNIT_ASSERT_MESSAGE( "failed to read rights", !result );
    }

    void TestReadScsiDriveInController_FailedIsDeviceIoControl()
    {
        Utils::disk_t disk;
        HANDLE hScsiDriveIOCTL = this; // fake address
        m_pObject->SetResult_DeviceIoControl( false, 0x00, nullptr );

        bool result = m_pObject->DiskInfo::ReadScsiDriveInController( hScsiDriveIOCTL, 0, 0, disk );

        CPPUNIT_ASSERT_MESSAGE( "failed to read rights", !result );
    }

    void TestReadScsiDriveInController_FailedModelNumber()
    {
        Utils::disk_t disk;
        HANDLE hScsiDriveIOCTL = this; // fake address
        m_pObject->SetResult_DeviceIoControl( true, 0x00, nullptr );  // 0x00 - failed

        bool result = m_pObject->DiskInfo::ReadScsiDriveInController( hScsiDriveIOCTL, 0, 0, disk );

        CPPUNIT_ASSERT_MESSAGE( "failed to read rights", !result );
    }

    void TestReadScsiDriveInController_FailedGetIdeInfo()
    {
        Utils::disk_t disk;
        HANDLE hScsiDriveIOCTL = this; // fake address
        m_pObject->SetResult_DeviceIoControl( true, 0x01, nullptr );
        m_pObject->SetResult_GetIdeInfo( false );

        bool result = m_pObject->DiskInfo::ReadScsiDriveInController( hScsiDriveIOCTL, 0, 0, disk );

        CPPUNIT_ASSERT_MESSAGE( "failed to read rights", !result );
    }

    void TestReadScsiDriveInController_PassedGetIdeInfo()
    {
        Utils::disk_t disk;
        HANDLE hScsiDriveIOCTL = this; // fake address
        m_pObject->SetResult_DeviceIoControl( true, 0x01, "model" );
        m_pObject->SetResult_GetIdeInfo( true );

        bool result = m_pObject->DiskInfo::ReadScsiDriveInController( hScsiDriveIOCTL, 0, 0, disk );

        CPPUNIT_ASSERT_MESSAGE( "failed to read rights", !result );
    }
    // -------------------------------------------------------------------------------------------
    void TestGetDrivesInfo_Passed()
    {
        std::vector<Utils::disk_t> disks;
        Utils::DiskInfo comp;

        bool result = comp.GetDrivesInfo( disks );

        CPPUNIT_ASSERT_MESSAGE( "passed", result );

        for( size_t i = 0; i < disks.size(); i++ )
        {
            printf( "\n-----------------\n");
            printf( "Model   : %s\n", disks[i].model);
            printf( "Serial  : %s\n", disks[i].serial );
            printf( "Vendor  : %s\n", disks[i].vendor );
            printf( "Revision: %s\n", disks[i].revision );
            printf( "Size    : %I64d Gb\n", disks[i].size / (1024 * 1024 * 1024) );
            printf( "Fixed   : %s\n", disks[i].fixed ? "Yes" : "No" );
        }
    }
    // -------------------------------------------------------------------------------------------
    void TestGetNetworkMAC_Passed()
    {
        unsigned long     ulMACaddress[16]      = {0x00};
        std::vector<std::string>       sMACaddress;
        char* szError = nullptr;

        size_t nCards = ::GetMACaddress( ulMACaddress, sMACaddress, szError );

        CPPUNIT_ASSERT_MESSAGE( "must be at least one", 0 < nCards );

        for( size_t i = 0; i < nCards; i++ )
        {
            printf( "\n-----------------\n");
            printf( "MAC   : %ul\n",       (unsigned long)ulMACaddress[i] );
            printf( "address     : %s\n",  sMACaddress[i].c_str() );
        }
    }
    // -------------------------------------------------------------------------------------------

};

BEGIN_UNIT_REGISTATION( DiskIdTests )

    CPPUNIT_TEST( TestConvertToString_FailedDiskdataIsNull )
    CPPUNIT_TEST( TestConvertToString_FailedFirstIndexBiggerLastIndex )
    CPPUNIT_TEST( TestConvertToString_Failedm_PassedIsEmpty )
    CPPUNIT_TEST( TestConvertToString_Failedm_PassedIsSinglew )
    CPPUNIT_TEST( TestConvertToString_Failedm_PassedIsSingleFF )
    CPPUNIT_TEST( TestConvertToString_Failedm_PassedIsSingle0X1234 )
    CPPUNIT_TEST( TestConvertToString_Failedm_PassedIsSingle0XFFFF )

    CPPUNIT_TEST( TestDoIdentify_FailedPhysicalDriveIOCTLIsNull )
    CPPUNIT_TEST( TestDoIdentify_FailedDeviceIoControl )
    CPPUNIT_TEST( TestDoIdentify_PassedDeviceIoControl )

    CPPUNIT_TEST( TestReadPhysicalDriveInNTWithAdminRights_FailedOpenDevice )
    CPPUNIT_TEST( TestReadPhysicalDriveInNTWithAdminRights_FailedDoIdentify )
    CPPUNIT_TEST( TestReadPhysicalDriveInNTWithAdminRights_FailedDeviceIoControl )
    CPPUNIT_TEST( TestReadPhysicalDriveInNTWithAdminRights_FailedIDEDeviceMapIs0 )
    CPPUNIT_TEST( TestReadPhysicalDriveInNTWithAdminRights_FailedGetIdeInfo )
    CPPUNIT_TEST( TestReadPhysicalDriveInNTWithAdminRights_Passed )

    CPPUNIT_TEST( TestReadScsiDriveInController_FailedIOCTLIsNull )
    CPPUNIT_TEST( TestReadScsiDriveInController_FailedExeedNumberDrives )
    CPPUNIT_TEST( TestReadScsiDriveInController_FailedModelNumber )

    CPPUNIT_TEST( TestGetDrivesInfo_Passed )
    CPPUNIT_TEST( TestGetNetworkMAC_Passed )

END_UNIT_REGISTATION( DiskIdTests );
