/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-2-Clause license that can
 *  be found in the LICENSE.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#pragma once
#include "Common/Config.h"
#ifdef MEDIAINFO_DLL
    #include "MediaInfoDLL/MediaInfoDLL.h"
    #define MediaInfoNameSpace MediaInfoDLL
#elif defined MEDIAINFO_STATIC
    #include "MediaInfoDLL/MediaInfoDLL_Static.h"
    #define MediaInfoNameSpace MediaInfoDLL
#else
    #include "MediaInfo/MediaInfoList.h"
    #define MediaInfoNameSpace MediaInfoLib
#endif
#include <vector>
using namespace MediaInfoNameSpace;
#include "iostream"
#include "string"
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Class core
//***************************************************************************

class Core
{
public:
    // Constructor/Destructor
    Core();
    ~Core();

    // Input
    vector<String>  Inputs;
    ostream* Out = nullptr;
    ostream* Err = nullptr;

    bool Scan = false;

    // Process
    return_value    Process();
};

string MediaInfo_Version();

