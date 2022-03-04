/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-2-Clause license that can
 *  be found in the LICENSE.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/CommandLine_Parser.h"
#include "Common/Core.h"
#include <iostream>
//---------------------------------------------------------------------------

//***************************************************************************
// Main
//***************************************************************************

int main(int argc, const char* argv[])
{
    // Environment
    setlocale(LC_ALL, "");

    // Configure
    Core C;
    C.Out = &cout;
    C.Err = &cerr;
    if (auto ReturnValue = Parse(C, argc, argv))
        return ReturnValue;

    // Process
    auto ReturnValue = C.Process();

    // Exit
    return ReturnValue;
}
