USE master

EXEC sp_addextendedproc 'xp_DiskId', 'EspDiskId.dll'

exec master..xp_DiskId
