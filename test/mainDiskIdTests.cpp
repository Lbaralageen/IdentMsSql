
#include "TCppUnit.h"

int wmain( int argc, wchar_t* argv[] )
{
    CPPUNIT_TEST_PACKAGE_START;
    {   
        if( !CallAllUnits( counters, fnMsg ) )
        {
            LogError( L"Could not pass ESP/CLR internal tests" );

            return -1;
        }
    }
    CPPUNIT_TEST_PACKAGE_END;
}

