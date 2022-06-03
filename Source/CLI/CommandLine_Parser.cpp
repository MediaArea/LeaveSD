/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-2-Clause license that can
 *  be found in the LICENSE.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/CommandLine_Parser.h"
#include "CLI/CLI_Help.h"
#include "Common/Core.h"
#if defined(UNICODE) && defined(_WIN32)
struct IUnknown; // Workaround for "combaseapi.h(229): error C2187: syntax error: 'identifier' was unexpected" with old SDKs and /permisive-
#include <windows.h>
#else
#include <ZenLib/Ztring.h>
#endif
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Command line parser
//***************************************************************************

//---------------------------------------------------------------------------
return_value Parse(Core& C, int argc, const char* argv_ansi[], LPCWSTR argv[])
{
    return_value ReturnValue = ReturnValue_OK;
    bool ClearInput = false;

    for (int i = 1; i < argc; i++)
    {
             if (!strcmp(argv_ansi[i], "--help") || !strcmp(argv_ansi[i], "-h"))
        {
            if (!C.Out)
                return ReturnValue_ERROR;
            if (auto Value = Help(*C.Out, argv_ansi[0], true))
                return Value;
            ClearInput = true;
        }
        else if (strcmp(argv_ansi[i], "--force-existing") == 0)
        {
            C.ForceExistingFiles = true;
        }
        else if (strcmp(argv_ansi[i], "--keep-temp") == 0)
        {
            C.KeepTemp = true;
        }
        else if (strcmp(argv_ansi[i], "--keep-silent") == 0)
        {
            C.KeepSilent = true;
        }
        else if (!strcmp(argv_ansi[i], "--legacy-aac"))
        {
            C.LegacyAac = true;
        }
        else if (!strcmp(argv_ansi[i], "--scan"))
        {
            C.Scan = true;
        }
        else if (strcmp(argv_ansi[i], "--skip-existing") == 0)
        {
            C.SkipExistingFiles = true;
        }
        else if (strcmp(argv_ansi[i], "--temp-path") == 0)
        {
            if (++i >= argc)
            {
                if (C.Err)
                    *C.Err << "Error: missing value after " << argv_ansi[i - 1] << ".\n";
                return ReturnValue_ERROR;
            }
            C.TempPath = argv[i];
        }
        else if (strcmp(argv_ansi[i], "--threads") == 0)
             {
                 if (++i >= argc)
                 {
                     if (C.Err)
                         *C.Err << "Error: missing value after " << argv_ansi[i - 1] << ".\n";
                     return ReturnValue_ERROR;
                 }
                 C.ThreadCount = atoi(argv_ansi[i]);
             }
        else if (!strcmp(argv_ansi[i], "--version"))
        {
            if (!C.Out)
                return ReturnValue_ERROR;
            if (auto Value = NameVersion(*C.Out))
                return Value;
            ClearInput = true;
        }
        else if (!strncmp(argv_ansi[i], "--", 2))
        {
            //Library options, we may accept either --option value or --option=value
            String Option(argv[i] + 2);
            auto EqualPos = Option.find('=');
            String Value;
            if (EqualPos != string::npos)
            {
                Value.assign(Option, EqualPos + 1);
                Option.resize(EqualPos);
                EqualPos = 0;
            }
            else
            {
                if (++i >= argc)
                {
                    if (C.Err)
                        *C.Err << "Error: missing value after " << argv_ansi[i - 1] << ".\n";
                    return ReturnValue_ERROR;
                }
                Value = argv[i];
                EqualPos = 1;
            }
            String Result = MediaInfoNameSpace::MediaInfo::Option_Static(Option, Value);
            if (C.Err && !Result.empty())
                *C.Err << "Warning: issue with " << argv_ansi[i - EqualPos];
        }
        else
        {
            if (C.Inputs.empty())
                C.Inputs.push_back(argv[i]);
            else if (C.OutputDir.empty())
                C.OutputDir=argv[i];
            else
            {
                if (C.Err)
                    *C.Err << "Error: too many parameters.\n";
                return ReturnValue_ERROR;
            }
        }
    }

    if (!ClearInput && C.Inputs.empty())
    {
        if (!C.Out)
            return ReturnValue_ERROR;
        if (auto Value = Help(*C.Out, argv_ansi[0]))
            return Value;
        return ReturnValue_ERROR;
    }

    if (ClearInput)
        C.Inputs.clear();

    if (!C.Inputs.empty() && C.OutputDir.empty() && !C.Scan)
    {
        if (C.Err)
            *C.Err << "Error: missing output directory.\n";
        return ReturnValue_ERROR;
    }

    if (C.ForceExistingFiles && C.SkipExistingFiles)
    {
        if (C.Err)
            *C.Err << "Error: --force-exisitng and --skip_existing are incompatible.\n";
        return ReturnValue_ERROR;
    }

    return ReturnValue;
}

//---------------------------------------------------------------------------
return_value Parse(Core& C, int argc, const char* argv_ansi[])
{
    //Get command line args in main()
#ifdef UNICODE
#ifdef _WIN32
    LPCWSTR* argv = (LPCWSTR*)CommandLineToArgvW(GetCommandLineW(), &argc);
#else //WIN32
    std::vector<MediaInfoNameSpace::String> argv_Temp;
    for (int i = 0; i < argc; i++)
    {
        ZenLib::Ztring FileName;
        FileName.From_Local(argv_ansi[i]);
        argv_Temp.push_back(FileName);
    }
    auto argv = new const MediaInfoNameSpace::Char * [argc];
    for (int i = 0; i < argc; i++)
    {
        argv[i] = argv_Temp[i].c_str();
    }
#endif //WIN32
#else //UNICODE
    auto argv = argv_ansi;
#endif //UNICODE

    return_value ReturnValue = Parse(C, argc, argv_ansi, argv);

    // Manage memory
#ifdef UNICODE
#ifdef _WIN32
    LocalFree(argv);
#else //WIN32
    delete[] argv;
#endif //WIN32
#endif //UNICODE

    return ReturnValue;
}
