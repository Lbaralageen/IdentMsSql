CREATE DATABASE myTestDb
GO
USE master
GO
EXEC sp_configure 'show advanced options', 1
RECONFIGURE
EXEC sp_configure 'clr enabled', 1
RECONFIGURE
GO
ALTER DATABASE dfg SET TRUSTWORTHY ON
GO
--- drop existing code
IF EXISTS (SELECT * FROM sysobjects WHERE NAME = 'GetWinSerial' AND xtype = 'FS')
	DROP FUNCTION dbo.GetWinSerial
GO
IF EXISTS (SELECT * FROM sysobjects WHERE NAME = 'crc64' AND xtype = 'FS')
	DROP FUNCTION dbo.crc64
GO
IF EXISTS (SELECT * FROM sysobjects WHERE NAME = 'GetComputerId' AND xtype = 'FS')
	DROP FUNCTION dbo.GetComputerId
GO
IF EXISTS (SELECT * FROM sys.assembly_modules WHERE assembly_method = 'GetDiskID' )
    DROP PROCEDURE dbo.GetDiskID
GO
IF EXISTS (SELECT * FROM sys.assembly_modules WHERE assembly_method = 'GetNetworkMAC' )
    DROP PROCEDURE dbo.GetNetworkMAC
GO
IF EXISTS (SELECT * FROM sys.assemblies WHERE Name = 'CLIESP')
  DROP ASSEMBLY CLIESP
GO

--- copy ClrDiskId.dll  to "c:\Program Files\Microsoft SQL Server\100\Tools\Binn\ClrDiskId.dll" 

--- apply new code

CREATE ASSEMBLY CLIESP from 'c:\Program Files\Microsoft SQL Server\100\Tools\Binn\ClrDiskId.dll' WITH PERMISSION_SET = UNSAFE;
GO
	
GO
CREATE FUNCTION dbo.GetWinSerial() 
    RETURNS sysname WITH EXECUTE AS CALLER AS EXTERNAL NAME CLIESP.StoredProcedures.GetWinSerial;
GO    
select dbo.GetWinSerial() as Serial
GO

CREATE FUNCTION dbo.crc64( @buffer nvarchar(max) ) 
    RETURNS bigint WITH EXECUTE AS CALLER AS EXTERNAL NAME CLIESP.StoredProcedures.crc64;
GO    
select dbo.crc64( 'Time it takes time' )
GO

CREATE PROCEDURE dbo.GetNetworkMAC
    WITH EXECUTE AS CALLER AS EXTERNAL NAME CLIESP.StoredProcedures.GetNetworkMAC;
GO    
EXEC dbo.GetNetworkMAC
GO

CREATE FUNCTION dbo.GetComputerId() 
    RETURNS bigint WITH EXECUTE AS CALLER AS EXTERNAL NAME CLIESP.StoredProcedures.GetComputerId;
GO    
select dbo.GetComputerId() as ComputerUUID
GO


CREATE PROCEDURE dbo.GetDiskID
    WITH EXECUTE AS CALLER AS EXTERNAL NAME CLIESP.StoredProcedures.GetDiskID;
GO    


EXEC dbo.GetDiskID
GO

-- EXEC master.dbo.xp_DiskId
GO
