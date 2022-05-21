/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-2-Clause license that can
 *  be found in the LICENSE.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/CLI_Help.h"
#include "Common/Core.h"
#include <ostream>
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Help
//***************************************************************************

//---------------------------------------------------------------------------
return_value Help(ostream& Out, const char* Name, bool Full)
{
    Out <<
        "Usage: \"" << Name << " FileName1 [Filename2...] [Options...]\"\n";
    if (!Full)
    {
        Out << "\"" << Name << " --help\" for displaying more information.\n"
            << endl;
        return ReturnValue_OK;
    }
    Out << "\n"
        "Options:\n"
        "    --help, -h\n"
        "        Display this help and exit.\n"
        "\n"
        "    --version\n"
        "        Display LeaveSD version and exit.\n"
        "\n"
        "    --scan\n"
        "        Scan for files with parsing issues.\n"
        "\n"
        << endl;

    return ReturnValue_OK;
}

//---------------------------------------------------------------------------
return_value NameVersion(ostream& Out)
{
    Out <<
        NameVersion_Text() << ".\n"
        ;

    return ReturnValue_OK;
}

//***************************************************************************
// Info
//***************************************************************************

//---------------------------------------------------------------------------
string NameVersion_Text()
{
    return
        "LeaveSD v." Program_Version
        " (MediaInfoLib v." + MediaInfo_Version() + ")"
        ;
}
