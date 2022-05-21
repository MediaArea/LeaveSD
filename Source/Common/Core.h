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
#include "MediaInfo/MediaInfo_Events.h"
#include <vector>
using namespace MediaInfoNameSpace;
#include "iostream"
#include "map"
#include "string"
#include "ZenLib/ZtringListList.h"
using namespace std;
using namespace ZenLib;
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
    String          OutputDir;
    String          TempPath;
    ostream*        Out = nullptr;
    ostream*        Err = nullptr;
    size_t          ThreadCount = 0;
    bool            KeepTemp = false;
    bool            ForceExistingFiles = false;
    bool            SkipExistingFiles = false;

    bool Scan = false;

    // Process
    return_value    Process();
    void Frame(size_t ID, const MediaInfo_Event_Global_Demux_4* FrameData);
    void Convert(size_t ID, size_t FilePos, bool FullCheck = false);

private:
    //Stats
    String ExePath;
    string ExePathS;
    ZtringList MainInDir;
    bool ImputIsDir;
    size_t i_Max;
};

string MediaInfo_Version();

