/** @file
  * ClrDiskId.cpp
  *
  * @author 
  * Konstantin Tchoumak 
  *
  * display the manufacturor's information from your hard drives on MSSQL server
  * http://www.codeproject.com/Articles/669177/Direct-access-to-Cplusplus-native-code-from-Cplusp
  * CPOl license
  */

#using <System.dll>
#using <System.Data.dll>
#using <System.Xml.dll>

#include <vcclr.h>
#include <wchar.h>
#include <vector>

// http://t-sql.ru/post/disk_size.aspx

using namespace System;
using namespace System::Data;
using namespace System::Data::Sql;
using namespace System::Data::SqlClient;
using namespace System::Data::SqlTypes;
using namespace Microsoft::SqlServer::Server;
using namespace System::Text;
using namespace Microsoft::Win32;

#include "diskid.h"

__int64  crc64( const void *data, const size_t length );
size_t GetMACaddress( unsigned long ulMACaddress[16], std::vector<std::string>& sMACaddress, char* szError );


public ref class StoredProcedures
{
public:
    static initonly double pi = 3.14;

    static Byte ShuffleBytes( Byte bytes[] )
    {
        int nRet = 0;

        for(int nByte = 14; nByte >= 0; --nByte)
        {
            nRet = nRet << 8;
            Byte indx = bytes[nByte];
            nRet += indx;
            Byte rs = (Byte)(((__int64)nRet * 0x2AAAAAAB) >> 34);
            bytes[nByte] = rs;
            nRet = nRet % 24;
        }
        return nRet;
    }
    static void ProcessKey( Byte bytes[], array<Byte>^ szKey )
    {
        Byte szKeySymbols[] = "BCDFGHJKMPQRTVWXY2346789";
        int shift = 4;
        int pointer = 0;
        for( int nSymbol = sizeof(szKeySymbols)-1; ; --nSymbol )
        {
            Byte index = ShuffleBytes(bytes);
            szKey[pointer+nSymbol+shift] = szKeySymbols[ index ];
            if( 0 == nSymbol )
            {
                break;
            }
            if( 0 == nSymbol % 5 )
            {
                pointer-- ;
                szKey[pointer+nSymbol + shift] = '-';
            }
        }
    }
    static String^ GetWindowsKey()
    {
        array<Byte>^ szKey = gcnew array<Byte>(30);
        Byte ptr[] = "XXXXX-XXXXX-XXXXX-XXXXX-XXXXX\x0"; 

        for(int i = 0; i < szKey->Length; i++ )
        {
            szKey[i] = ptr[i]; 
        }
        String^        szKeyPath =  L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        String^        szKeyName =  L"DigitalProductID";

        RegistryKey^ HKLM = Registry::LocalMachine;
        RegistryKey^ reg = HKLM->OpenSubKey( szKeyPath, false );

        String^ CurrentVersion = reg->GetValue(L"CurrentVersion")->ToString();
        String^ ProductName = reg->GetValue( L"ProductName" )->ToString();
        array<Byte>^ Bytes = (array<Byte>^)reg->GetValue( szKeyName );

        if( Bytes->Length )
        {
            Byte *pData = new Byte[ Bytes->Length ];
            for(int i = 0; i < Bytes->Length; i++ )
            {
                pData[i] = Bytes[i];
            }
            reg->Close();
            szKey[29] = L'\x0';
            ProcessKey( pData + 52, szKey );

            Encoding^ ascii = Encoding::ASCII;

            array<Char>^asciiChars = gcnew array<Char>(ascii->GetCharCount( szKey, 0, szKey->Length ));
            ascii->GetChars( szKey, 0, szKey->Length, asciiChars, 0 );
            String^ asciiString = gcnew String( asciiChars );

            delete [] pData;
            return asciiString;
        }
        return L"";
    }

    // CREATE FUNCTION dbo.GetWinSerial() 
    //   RETURNS sysname WITH EXECUTE AS CALLER AS EXTERNAL NAME CLIESP.StoredProcedures.GetWinSerial;

    [SqlFunction(DataAccess = DataAccessKind::Read)]                                     // return windows serial number
    static SqlString GetWinSerial()                                                      // select dbo.GetWinSerial() as Serial
    {
        return GetWindowsKey();
    }

    [SqlFunction(DataAccess = DataAccessKind::Read)]
    static SqlInt64 crc64( SqlString buffer )
    {
        pin_ptr<const wchar_t> wzBuffer = ::PtrToStringChars( buffer.ToString() );

        const size_t length = ::wcslen( wzBuffer );

        return ::crc64( (const void *)wzBuffer, length, s_i64Seed );
    }

    [SqlProcedure]
    static void GetDiskID()  // EXEC dbo.GetDiskID
    {
        SqlPipe^ spPipe = Microsoft::SqlServer::Server::SqlContext::Pipe;

        Utils::DiskInfo comp;

        try
        {
            std::vector<Utils::disk_t> disks;
            comp.GetDrivesInfo( disks );

            SqlDataRecord ^ row = gcnew SqlDataRecord ( 
              gcnew SqlMetaData( L"Controller", SqlDbType::Int ),
              gcnew SqlMetaData( L"Model",      SqlDbType::VarChar, 64 ),
              gcnew SqlMetaData( L"Serial",     SqlDbType::VarChar, 255 ),
              gcnew SqlMetaData( L"Vendor",     SqlDbType::VarChar, 64 ),
              gcnew SqlMetaData( L"Size",       SqlDbType::Int ),
              gcnew SqlMetaData( L"UUID",       SqlDbType::BigInt ) );

            spPipe->SendResultsStart( row );

            for( size_t i = 0; i < disks.size(); i++ )
            {
                Utils::disk_t& disk = disks[ i ];

                row->SetInt32 ( 0, disk.num_controller );
                row->SetString( 1, gcnew String( disk.model )  );
                row->SetString( 2, gcnew String( disk.serial ) );
                row->SetString( 3, gcnew String( disk.vendor ) );
                row->SetInt32( 4,  (int)( disk.size / (1024 * 1024 * 1024) ) );
                row->SetInt64 ( 5, disk.GetUUID() );

                spPipe->SendResultsRow (row); 
            }
            spPipe->SendResultsEnd (); 

        }catch(...)
        {
            spPipe->Send( L"Unknown exception" );
        }
    }

    [SqlProcedure]
    static void GetNetworkMAC()  // EXEC dbo.GetNetworkMAC
    {
        SqlPipe^ spPipe = Microsoft::SqlServer::Server::SqlContext::Pipe;

        std::vector<std::string>  sMACaddress;
        try
        {
            char* szError = nullptr;
            unsigned long   ulMACaddress[16]      = {0x00};
            size_t nCards = ::GetMACaddress( ulMACaddress, sMACaddress, szError );

            if( 0 == nCards )
            {
                spPipe->Send( L"Cannot read network card's info" );

                return;
            }

            SqlDataRecord ^ row = gcnew SqlDataRecord ( 
              gcnew SqlMetaData( L"Controller", SqlDbType::Int ),
              gcnew SqlMetaData( L"Adress",     SqlDbType::VarChar, 255 ),
              gcnew SqlMetaData( L"MAC",        SqlDbType::Int ) );

            spPipe->SendResultsStart( row );

            for( __int32 i = 0; i < nCards; i++ )
            {
                row->SetInt32 ( 0, i );

                String^ mac = gcnew String( sMACaddress[i].c_str() );

                row->SetString( 1, mac );
                row->SetInt32(  2, ulMACaddress[i] );

                spPipe->SendResultsRow (row); 
            }
            spPipe->SendResultsEnd (); 

        }catch(...)
        {
            spPipe->Send( L"Unknown exception" );
        }
    }

    [SqlFunction(DataAccess = DataAccessKind::Read)]
    static SqlInt64 GetComputerId()
    {
        __int64 result = ~0;
        Utils::DiskInfo comp;
        std::vector<std::string>  sMACaddress;
        try
        {
            unsigned long   ulMACaddress[16] = {0x00};
            std::vector<Utils::disk_t> disks;
            comp.GetDrivesInfo( disks );

            for( size_t i = 0; i < disks.size(); i++ )
            {
                Utils::disk_t& disk = disks[ i ];

                result ^= disk.GetUUID();
            }
            char* szError = nullptr;
            size_t nCards = ::GetMACaddress( ulMACaddress, sMACaddress, szError );

            for( __int32 i = 0; i < nCards; i++ )
            {
                String^ mac = gcnew String( sMACaddress[i].c_str() );

                result ^= ( ( 0 == i % 2 ) ? static_cast<__int64>( ulMACaddress[i] ) : ( static_cast<__int64>( ulMACaddress[i] ) << 32 ) );
            }
            return result;
        }catch(...)
        {
            return 0;
        }
    }

};