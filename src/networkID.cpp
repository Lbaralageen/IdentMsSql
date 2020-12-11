
// GetMACAdapters.cpp : Defines the entry point for the console application.
//
// Author:    Khalid Shaikh [Shake@ShakeNet.com]
// Date:    April 5th, 2002
//
// This program fetches the MAC address of the localhost by fetching the 
// information through GetAdapatersInfo.  It does not rely on the NETBIOS
// protocol and the ethernet adapter need not be connect to a network.
//
// Supported in Windows NT/2000/XP
// Supported in Windows 95/98/Me
//
// Supports multiple NIC cards on a PC.

#include <winsock2.h>
#include <Iphlpapi.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")

// Fetches the MAC address and prints it
size_t GetMACaddress( unsigned long ulMACaddress[16], std::vector<std::string>& sMACaddress, char* szError )
{
  ::memset( ulMACaddress, 0x00, 16 * sizeof(unsigned long) );
  sMACaddress.clear();
  szError = nullptr;

  IP_ADAPTER_INFO AdapterInfo[16];       // Allocate information
                                         // for up to 16 NICs
  DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

  DWORD dwStatus = ::GetAdaptersInfo( AdapterInfo, &dwBufLen );

  if( dwStatus != ERROR_SUCCESS ) 
  {
      switch( dwStatus )
      {
        case ERROR_BUFFER_OVERFLOW : 
            szError = "The buffer to receive the adapter information is too small. "; 
            break;
        case ERROR_INVALID_DATA : 
            szError = "Invalid adapter information was retrieved"; 
            break;
        case ERROR_INVALID_PARAMETER : 
            szError = "One of the parameters is invalid "; 
            break;
        case ERROR_NO_DATA : 
            szError = "No adapter information exists for the local computer. "; 
            break;
        case ERROR_NOT_SUPPORTED : 
            szError = "The GetAdaptersInfo function is not supported by the operating system running on the local computer "; 
            break;
      }
      return dwStatus;
  }
  PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to
                                               // current adapter info
  size_t index = 0;
  do 
  {
    if( 0 == ulMACaddress[index] )
    {
        ulMACaddress[index] = pAdapterInfo->Address [5] + pAdapterInfo->Address [4] * 256 + 
                              pAdapterInfo->Address [3] * 256 * 256 + 
                              pAdapterInfo->Address [2] * 256 * 256 * 256;

        char szName[256] = {0};
            ::sprintf( szName, "%02X-%02X-%02X-%02X-%02X-%02X"
            , (int)pAdapterInfo->Address[0], (int)pAdapterInfo->Address[1], (int)pAdapterInfo->Address[2]
            , (int)pAdapterInfo->Address[3], (int)pAdapterInfo->Address[4], (int)pAdapterInfo->Address[5] );

        sMACaddress.push_back( szName );
        index++;
    }
    pAdapterInfo = pAdapterInfo->Next;    // Progress through linked list
  }
  while( pAdapterInfo );                    // Terminate if last adapter
  
  return index;
}
