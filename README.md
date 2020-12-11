# IdentMsSql
Direct access to C++ native code from C++/CLI MSSQL Stored Procedure

https://www.codeproject.com/Articles/669177/Direct-access-to-Cplusplus-native-code-from-Cplusp

Some MS SQL developers want to protect their unique product and they want to link homemade code to the unique hardware parameters. 
The best way to do it - just read HDD serial number and model data, and optionally network MAC address.
The only code to rich such information I found was in most popular source code from Sysinternals and some C++/CLI variations of reading Window's serial code. 
Current project represents C++/CLI stored procedure which is the best solution to provide access from stored procedure to native C++ code.

Project is written with C++/CLI and TSQL languages.  Compiled in VS2019 

Creating C++/CLI procedure  

To create C++/CLI procedure developer has to follow next flow:

Add New Project->Visual C++->CLR->Class Library. The shortest working code will be like that:
Hide   Copy Code

public ref class StoredProcedures
{
  [SqlFunction(DataAccess = DataAccessKind::Read)]
    static SqlString MyFunction( SqlString buffer )
  {
    return buffer;
  }
  [SqlProcedure]
    static void GetHelloWorld()
  {
    SqlPipe^ spPipe = Microsoft::SqlServer::Server::SqlContext::Pipe;
    spPipe->Send( L"Hello World" );
  }
 } 

At current time there is no need to make x86 version and my example made to support only x64 WinNT platform, but it's not a big deal to add x86 platform too. Before compiling the actual assembly let make sure in the project properties there are next options:

    Common Language Runtime Support: Pure MSIL Common Language Runtime Support (/clr:pure);
    C/C++ -> Preprocessor Definitions -> Win64
    Linker -> Target Machine -> /MACHINE:X64;

Now is tricky part - Microsoft does not provide an option to set a version of .NET platform. The only way I found : open project file as XML document and change to platform you want to support. If we you use native C++ code then version 2.0 is more then enough and you need to have installed VS2008. (But you can use any toolset (it will add more useless Microsoft code into final assembly)).  
Hide   Copy Code

<TargetFrameworkVersion>v2.0</TargetFrameworkVersion> 

Since this moment everything is ready to build an assembly for running with MS SQL Server.

    Select Configuration as Release and Platform as x64, build the project.
    Copy assembly [ClrDiskId.dll] to SQL Server BINN folder (it must be 64-bit instance!). In my case MSSQL2008R with default instance path is: [c:\Program Files\Microsoft SQL Server\MSSQL10_50.MSSQLSERVER\MSSQL\Binn]
    Register assembly.

Register assembly 

Before this you have to make MSSQL server be prepared for using UNSAFE assembles:
Hide   Copy Code

CREATE DATABASE myTestDb
GO
USE master
GO
EXEC sp_configure 'show advanced options', 1
RECONFIGURE
EXEC sp_configure 'clr enabled', 1
RECONFIGURE
GO
USE myTestDb
GO
ALTER DATABASE myTestDb SET TRUSTWORTHY ON
GO 

Now we can register our assembly:
Hide   Copy Code

CREATE ASSEMBLY CLIESP from 'c:\Program Files\Microsoft SQL 
   Server\MSSQL10_50.MSSQLSERVER\MSSQL\Binn\ClrDiskId.dll' WITH PERMISSION_SET = UNSAFE;

Register required procedure as clr procedure:
Hide   Copy Code

CREATE PROCEDURE dbo.GetHelloWorld
    WITH EXECUTE AS CALLER AS EXTERNAL NAME CLIESP.StoredProcedures.GetHelloWorld;
GO 

And final run:
Hide   Copy Code

EXEC dbo.GetHelloWorld 

Now when we figure out how to use C++/CLI procedure we can add C++ code and call it directly from our procedures.

Let do something more complicated and start from displaying current hard drives on our computer. I use code to read Hard Drive Manufacturing Information from WinSim. This code was looked unsafe for me and I add unit tests and make the code more safe for using in such critical mission application as MS SQL Server - nobody wants to have a crush on a server.

Attached solution has 3 projects:

    EspDiskId - Extended Stored Procedure xp_DiskId display list of hard drives (MS recommends stop  using it)
    ClrDiskId - CLR procedure GetDiskID display list of hard drives
    mainDiskIdTest - several unit tests to make sure that original C++ code is more or less safe

After executing
Hide   Copy Code

EXEC dbo.GetDiskID

I have next result on my computer:
Id		Model		Serial		Vendor		Size [Gb]		UUID	
0		HDS723020BLA642		MN1220F32AJ4PD		Hitachi		1863		-5791026690894825696	
1		WD3000HLHX-01JJPV0		WD-WXW1E31HHMV5		WD		279		4862279922329562629 	

Final code has several usefull functions:

    GetWinSerial - display windows serial number { select dbo.GetWinSerial() as Serial }
    crc64 - to calc crc64 { select dbo.crc64( 'Time it takes time' ) }
    GetNetworkMAC - display list of installed network adapters {select dbo.GetComputerId() as ComputerUUID }
    GetComputerId - calculate unique identifier of computer

License

This article, along with any associated source code and files, is licensed under The Code Project Open License (CPOL)
