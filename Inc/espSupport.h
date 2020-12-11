/** @file
  * xpInterace.cpp 
  *
  * @author 
  * Konstantin Tchoumak 
  *
  * interface between MSSQL API and C++ code
  *
  * licensed under The GENERAL PUBLIC LICENSE (GPL3)
  */

#ifndef __ESP_SUPPORT_INC_FILE
#define __ESP_SUPPORT_INC_FILE

#define DBNTWIN32

#ifdef __cplusplus
extern "C" {
#endif 

#include <Srv.h>

#ifdef __cplusplus
}
#endif 

#ifdef ESPLIB_EXPORTS
#define ESPLIB_API __declspec(dllexport)
#else
#define ESPLIB_API
#endif

#endif      // end of __ESP_SUPPORT_INC_FILE
