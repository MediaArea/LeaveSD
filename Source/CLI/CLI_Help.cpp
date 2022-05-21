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
        "Usage: \"" << Name << " Input [OutputPath] [Options...]\"\n";
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
        "    --skip-existing\n"
        "        Skip processing of files with corresponding out file name already existing.\n"
        "\n"
        "    --force-existing\n"
        "        Force processing of files with corresponding out file name already existing.\n"
        "        Warning: previous content will be erased.\n"
        "\n"
        "    --temp-path value\n"
        "        Set temporary path to the indicated value.\n"
        "        By defaut it is the system temp path.\n"
        "        It is advised to use a temporary path on the same disk as the output path\n"
        "        in order to avoid the cost of a file copy.\n"
        "\n"
        "    --keep-temp\n"
        "        Do not delete temporary files (useful for investiguation).\n"
        "\n"
        "    --threads value\n"
        "        Set count of parallel processings.\n"
        "        By defaut it is the count of (logical) processors.\n"
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
