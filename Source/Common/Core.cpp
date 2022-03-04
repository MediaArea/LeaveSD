/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-2-Clause license that can
 *  be found in the LICENSE.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Common/Core.h"
#include "ZenLib/Ztring.h"
#include "ZenLib/Dir.h"
using namespace ZenLib;
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
Core::Core()
{
}

Core::~Core()
{
}

//***************************************************************************
// Process
//***************************************************************************

//---------------------------------------------------------------------------
return_value Core::Process()
{
    return_value ToReturn = ReturnValue_OK;

    if (!Scan)
    {
        if (Err)
            *Err << "Not yet implemented.\n";
        return ReturnValue_ERROR;
    }

    if (Scan)
    {
        vector<String> NsvFileNames;

        for (const auto Input : Inputs)
        {
            ZtringList AllFiles = Dir::GetAllFileNames(Input);
            for (const auto FileName : AllFiles)
            {
                if (FileName.size() > 4 && FileName.find(__T(".nsv"), FileName.size() - 4) != (size_t)-1)
                {
                    NsvFileNames.push_back(FileName);
                }
            }
        }

        size_t i = 0;
        size_t i_Bad = 0;
        for (const auto Input : NsvFileNames)
        {
            if (Err)
                *Err << "\rScanning file " << (++i) << "/" << NsvFileNames.size() << "...";

            MediaInfo MI;
            bool Problem = false;
            try
            {
                MI.Open(Input);
            }
            catch (exception e)
            {
                if (!Problem)
                {
                    if (Err)
                        *Err << "\r                                                   \r";
                    if (Out)
                        *Out << Ztring(MI.Get(Stream_General, 0, __T("FileName"))).To_UTF8();
                    Problem = true;
                }
                if (Out)
                    *Out << ";Crash";
            }
            if (MI.Get(Stream_General, 0, __T("Format")) != __T("NSV"))
            {
                if (!Problem)
                {
                    if (Err)
                        *Err << "\r                                                   \r";
                    if (Out)
                        *Out << Ztring(MI.Get(Stream_General, 0, __T("FileName"))).To_UTF8();
                    Problem = true;
                }
                if (Out)
                    *Out << ";No NSV detected";
            }
            auto Debug_Speakers = MI.Get(Stream_General, 0, __T("Debug_Speakers"));
            if (!Debug_Speakers.empty())
            {
                if (!Problem)
                {
                    if (Err)
                        *Err << "\r                                                   \r";
                    if (Out)
                        *Out << Ztring(MI.Get(Stream_General, 0, __T("FileName"))).To_UTF8();
                    Problem = true;
                }
                if (Out)
                    *Out << ";Issue with speakers;" << Ztring(Debug_Speakers).To_UTF8();
            }
            if (Problem)
            {
                if (Out)
                    *Out << "\n";
                i_Bad++;
            }
        }
        if (Err)
        {
            *Err << "\rScanning done, " << NsvFileNames.size() - i_Bad << " files well detected";
            if (i_Bad)
                *Err << ", " << i_Bad << " files with issues";
            *Err << "\n";
        }
    }

    return ToReturn;
}

//***************************************************************************
// Helpers
//***************************************************************************

string MediaInfo_Version()
{
    return Ztring(MediaInfo::Option_Static(__T("Info_Version"), String())).SubString(__T(" - v"), String()).To_UTF8();
}

