# IdentMsSql
Direct access to C++ native code from C++/CLI MSSQL Stored Procedure
https://www.codeproject.com/Articles/669177/Direct-access-to-Cplusplus-native-code-from-Cplusp

Some MS SQL developers want to protect their unique product and they want to link homemade code to the unique hardware parameters. 
The best way to do it - just read HDD serial number and model data, and optionally network MAC address.
The only code to rich such information I found was in most popular source code from Sysinternals and some C++/CLI variations of reading Window's serial code. 
Current project represents C++/CLI stored procedure which is the best solution to provide access from stored procedure to native C++ code.

Project is written with C++/CLI and TSQL languages.  Compiled in VS2019 
