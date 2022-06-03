/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-2-Clause license that can
 *  be found in the LICENSE.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Common/Core.h"
#include "ZenLib/Ztring.h"
#include "ZenLib/Dir.h"
#include "ZenLib/FileName.h"
using namespace ZenLib;
using namespace std;
#include "ZenLib/ZtringListList.h"
#include "ZenLib/File.h"
#include "ZenLib/BitStream.h"
#include "Windows.h"
#include "cstdlib"
#include <map>
#include <mutex>
#include <future>
//---------------------------------------------------------------------------

//***************************************************************************
// Convert
//***************************************************************************

struct data_per_thread
{
    size_t ID = 0;
    Core* C = nullptr;
    File F[2];
    bool IsChecking = false;
    String TempNamePrefix;
    String ChannelCount;
    vector<size_t> Stats_InvalidAudioPackets;
    vector<size_t> Stats_InvalidAacPackets;
    size_t Stats_AacPacketPos = 0;
    size_t Stats_JunkBytes = 0;
    size_t Stats_AudioPacketInvalidSize = 0;
    bool FullCheck = false;

    void Reset(size_t NewID, Core* NewC, String NewTempNamePrefix, String NewChannelCount = String())
    {
        *this = data_per_thread();
        ID = NewID;
        C = NewC;
        TempNamePrefix = NewTempNamePrefix;
        ChannelCount = NewChannelCount;
    }
};

struct all
{
    vector<data_per_thread> ThreadDatas;
    Core* C = nullptr;

    void AddFileName(const String& FileName)
    {
        NsvFileNames.push_back(FileName);
    }
    String& FileName(size_t Pos)
    {
        if (i >= NsvFileNames.size())
            Pos = NsvFileNames.size() - 1;
        return NsvFileNames[Pos];
    }
    String& CurrentFileName()
    {
        return FileName(i);
    }
    size_t NextFileNamePos()
    {
        if (i_Next >= NsvFileNames.size())
            return (size_t)-1;
        const lock_guard<mutex> lock(Mutex);
        return i_Next++;
    }
    void Finished(const String& Dest, vector<string> ErrorMessages, vector<string> WarningMessages, bool Skipped = false)
    {
        if (!ErrorMessages.empty() || !WarningMessages.empty())
        {
            auto Flatten = [](const string& Intro, const vector<string> Vec)
            {
                if (Vec.empty())
                    return string();
                string ToReturn(Intro + ": ");
                for (const auto& Value : Vec)
                    ToReturn += Value + " / ";
                ToReturn.resize(ToReturn.size() - 3);
                return ToReturn;
            };

            Out(Ztring(Dest).To_UTF8() + ';' + Flatten("Error", ErrorMessages) + ';' + Flatten("Warning", WarningMessages));
        }

        Mutex.lock();
        i++;
        if (!ErrorMessages.empty())
            i_Error++;
        if (!WarningMessages.empty())
            i_Warning++;
        if (Skipped)
            i_Skipped++;
        Mutex.unlock();
        DisplayStatus();
    }
    size_t Pos()
    {
        return i;
    }
    size_t Count()
    {
        return NsvFileNames.size();
    }
    size_t ErrorCount()
    {
        return i_Error;
    }
    size_t WarningCount()
    {
        return i_Warning;
    }
    size_t SkippedCount()
    {
        return i_Skipped;
    }
    void Err(const string& Message, bool CarriageReturn = false)
    {
        if (!C->Err)
            return;

        auto ToDisplay = '\r' + Message;
        ToDisplay.resize(77, ' ');
        const lock_guard<mutex> lock(ErrMutex);
        *C->Err << ToDisplay;
        if (CarriageReturn)
            *C->Err << '\n';
    }
    void Out(const string& Message)
    {
        if (!C->Out)
            return;

        string ErrMessage(1, '\r');
        ErrMessage.resize(77, ' ');
        ErrMessage += '\r';

        auto ToDisplay = Message + '\n';
        ErrMutex.lock();
        *C->Err << ErrMessage;
        *C->Out << ToDisplay;
        ErrMutex.unlock();
        DisplayStatus();
    }
    void DisplayStatus()
    {
        auto ShortenedFileName = ZenLib::FileName(CurrentFileName()).Name_Get().To_Local();
        if (ShortenedFileName.empty())
            return;
        if (ShortenedFileName.size() > 35)
        {
            ShortenedFileName.erase(15, ShortenedFileName.size() - 30);
            ShortenedFileName.insert(15, "[...]");
        }

        Err("Transcoding " + ShortenedFileName + " (" + to_string(Pos() + 1) + '/' + to_string(Count()) + ")...");
    }
    void Delete(const String& Name)
    {
        if (C->KeepTemp)
            return;
        File::Delete(Name);
    }

private:
    vector<String> NsvFileNames;

    mutex ErrMutex;
    mutex Mutex;
    size_t i = 0;
    size_t i_Next = 0;
    size_t i_Error = 0;
    size_t i_Warning = 0;
    size_t i_Skipped = 0;
};
all Data;


//***************************************************************************
// Callback
//***************************************************************************

void __stdcall Event_CallBackFunction(unsigned char* Data_Content, size_t Data_Size, void* UserHandler_Void)
{
    //Retrieving UserHandler
    data_per_thread* UserHandler = (data_per_thread*)UserHandler_Void;
    struct MediaInfo_Event_Generic* Event_Generic = (struct MediaInfo_Event_Generic*)Data_Content;
    unsigned char                       ParserID;
    unsigned short                      EventID;
    unsigned char                       EventVersion;

    //Integrity test
    if (Data_Size < 4)
        return; //There is a problem

    //Retrieving EventID
    ParserID = (unsigned char)((Event_Generic->EventCode & 0xFF000000) >> 24);
    EventID = (unsigned short)((Event_Generic->EventCode & 0x00FFFF00) >> 8);
    EventVersion = (unsigned char)(Event_Generic->EventCode & 0x000000FF);
    switch (ParserID)
    {
    case MediaInfo_Parser_General:
        switch (EventID)
        {
        case MediaInfo_Event_General_End: if (EventVersion == 0 && Data_Size >= sizeof(struct MediaInfo_Event_General_End_0)) UserHandler->Stats_JunkBytes += ((MediaInfo_Event_General_End_0*)Event_Generic)->Stream_Bytes_Junk; break;
        }
        break;
    case MediaInfo_Parser_Nsv:
        switch (EventID)
        {
        case MediaInfo_Event_Global_Demux: if (EventVersion == 4 && Data_Size >= sizeof(struct MediaInfo_Event_Global_Demux_4)) UserHandler->C->Frame(UserHandler->ID, (MediaInfo_Event_Global_Demux_4*)Event_Generic); break;
        }
        break;
    }
}

//***************************************************************************
// Convert
//***************************************************************************

void Core::Convert(size_t ID, size_t FilePos, bool FullCheck)
{
    Data.DisplayStatus();

    auto& ThreadData = Data.ThreadDatas[ID];
    ThreadData.Reset(ThreadData.ID, ThreadData.C, ThreadData.TempNamePrefix, ThreadData.ChannelCount);
    if (FullCheck)
        ThreadData.FullCheck = true;
    auto TempNamePrefix = ThreadData.TempNamePrefix + Ztring().From_Number(FilePos);
    if (ThreadData.FullCheck)
        TempNamePrefix += 'f';
    const auto& Input = Data.FileName(FilePos);

    String Dest;
    if (!MainInDir.empty())
    {
        Dest = OutputDir;
        ZtringList Temp;
        Temp.Separator_Set(0, __T("\\"));
        Temp.Write(Input);
        for (size_t i = MainInDir.size(); i < Temp.size(); i++)
        {
            Dest += __T('\\');
            Dest += Temp[i];
        }
    }
    else if (ImputIsDir)
    {
        Dest = Input;
        Dest.erase(0, Inputs[0].size());
        auto Dest_Slash = Dest.find_last_of(__T("/\\"));
        if (Dest_Slash != string::npos)
        {
            Dest.erase(0, Dest_Slash + 1);
        }
        Dest.insert(0, OutputDir);
    }
    else
    {
        Dest = Input;
        auto Dest_Slash = Dest.find_last_of(__T("/\\"));
        if (Dest_Slash != string::npos)
        {
            Dest.erase(0, Dest_Slash + 1);
        }
        Dest.insert(0, OutputDir);
    }
    Ztring OutSubDir(Dest);
    OutSubDir.erase(OutSubDir.find_last_of(__T('\\')));
    Dir::Create(OutSubDir);
    Dest.resize(Dest.size() - 3);
    Dest += __T("mkv");
    if (!Data.C->ForceExistingFiles && File::Exists(Dest))
    {
        Data.Finished(Dest, {}, {}, true);
        return;
    }


    vector<string> WarningMessages;
    vector<pair<String, String>> EraseBeginEnd;
    vector<pair<String, String>> Replace;

    auto CreateQuotedTempNamePrefix = [&](string const& CommandPrefix, string const& CommandSuffix)
    {
        string Command = "\"\"" + CommandPrefix;
        auto CommandSuffix_SpacePos = CommandSuffix.find(' ');
        if (CommandSuffix_SpacePos == string::npos)
            CommandSuffix_SpacePos = CommandSuffix.size();
        Command.append(CommandSuffix.c_str(), CommandSuffix_SpacePos);
        Command += '"';
        Command.append(CommandSuffix.c_str() + CommandSuffix_SpacePos, CommandSuffix.size() - CommandSuffix_SpacePos);
        Command += '"';
        return Command;
    };

    map<String, String> TagTemplate;



    ThreadData.F[0].Open(TempNamePrefix + __T(".avc"), File::Access_Write);
    ThreadData.F[1].Open(TempNamePrefix + __T(".aac"), File::Access_Write);

    // Demux
    MediaInfo MI;
    MI.Option(__T("File_Demux_Unpacketize"), __T("1"));
    MI.Option(__T("File_Macroblocks_Parse"), __T("-1")); // Used for parsing AAC frame, -1 means no check at all, 1 full check
    MI.Option(__T("File_Event_CallBackFunction"), __T("CallBack=memory://") + Ztring::ToZtring((size_t)&Event_CallBackFunction) + __T(";UserHandler=memory://") + Ztring::ToZtring((size_t)&ThreadData));
    MI.Open(Input);
    ThreadData.F[0].Truncate();
    ThreadData.F[1].Truncate();
    ThreadData.F[0].Close();
    ThreadData.F[1].Close();
    if (MI.Get(Stream_General, 0, __T("Format")) != __T("NSV"))
    {
        Data.Finished(Dest, { "No NSV detected" }, {});
        return;
    }
    bool HasVideo;
    if (!MI.Count_Get(Stream_Video) || MI.Get(Stream_Video, 0, __T("Format_Profile")).empty())
    {
        WarningMessages.push_back("no video detected");
        HasVideo = false;
    }
    else
        HasVideo = true;
    int SourceChannelCount;
    ThreadData.ChannelCount = MI.Get(Stream_Audio, 0, __T("Channel(s)"));
    if (!MI.Count_Get(Stream_Audio) || MI.Get(Stream_Audio, 0, __T("Format_Version")).empty())
    {
        EraseBeginEnd.push_back({ __T(",\r\n\"--default-track-flag\","), __T("_7.aac\"") });
        WarningMessages.push_back("no audio detected");
        SourceChannelCount = 0;
    }
    else if (ThreadData.ChannelCount == __T("1"))
    {
        WarningMessages.push_back("1-ch audio detected");
        SourceChannelCount = 1;
    }
    else if (ThreadData.ChannelCount.empty() || ThreadData.ChannelCount == __T("8"))
    {
        if (ThreadData.ChannelCount.empty())
            ThreadData.ChannelCount = __T("8"); // In practice files we got have a channel_configuration of 0 and in practice they have 8 channels
        SourceChannelCount = 8;
    }
    else
    {
        Data.Finished(Dest, { "Audio channel count not supported" }, {});
        return;
    }
    if (SourceChannelCount && MI.Get(Stream_Audio, 0, __T("Format")) != __T("AAC"))
    {
        Data.Finished(Dest, { "Only AAC audio is supported" }, {});
        return;
    }

    // Prepare tags
    TagTemplate = {
        {__T("%MEETING%"),      MI.Get(Stream_General, 0, __T("Meeting"))},
        {__T("%COMMISSION%"),   MI.Get(Stream_General, 0, __T("Commission"))},
        {__T("%ROOM%"),         MI.Get(Stream_General, 0, __T("Room"))},
        {__T("%DATE_ENCODED%"), MI.Get(Stream_General, 0, __T("Recorded_Date"))},
        {__T("%TEMPPATH%"), TempNamePrefix},
    };
    auto AdaptTemplate = [&](String const& InFileName, String const& OutFileName = String(), const vector<pair<String, String>>& EraseBeginEnd = {}, const vector<pair<String, String>>& Replace = {})
    {
        File MergeTemplate_Buffer_F;
        MergeTemplate_Buffer_F.Open(ExePath + InFileName);
        int8u MergeTemplate_Buffer[10000];
        auto MergeTemplate_Buffer_Size = MergeTemplate_Buffer_F.Read(MergeTemplate_Buffer, 10000);
        MergeTemplate_Buffer_F.Close();
        Ztring MergeTemplate;
        MergeTemplate.From_UTF8((const char*)MergeTemplate_Buffer, MergeTemplate_Buffer_Size);

        for (const auto& Item : EraseBeginEnd)
        {
            auto EraseBegin_Pos = MergeTemplate.find(Item.first);
            if (EraseBegin_Pos != string::npos)
            {
                auto EraseEnd_Pos = MergeTemplate.find(Item.second, EraseBegin_Pos);
                if (EraseEnd_Pos != string::npos)
                    MergeTemplate.erase(EraseBegin_Pos, EraseEnd_Pos + Item.second.size() - EraseBegin_Pos);
            }
        }

        for (const auto& Item : Replace)
        {
            auto EraseBegin_Pos = MergeTemplate.find(Item.first);
            if (EraseBegin_Pos != string::npos)
            {
                MergeTemplate.erase(EraseBegin_Pos, Item.first.size());
                MergeTemplate.insert(EraseBegin_Pos, Item.second);
            }
        }

        for (const auto& Item : TagTemplate)
            MergeTemplate.FindAndReplace(Item.first, Item.second, 0, Ztring_Recursive);

        if (OutFileName.size() > 5 && !OutFileName.compare(OutFileName.size() - 5, 5, __T(".json"), 5))
            MergeTemplate.FindAndReplace(__T("\\"), __T("\\\\"), 0, Ztring_Recursive);

        if (OutFileName.empty())
            return CreateQuotedTempNamePrefix(ExePathS, MergeTemplate.To_Local());
        File MergeTemplate_Buffer_F2;
        MergeTemplate_Buffer_F2.Open(TempNamePrefix + OutFileName, File::Access_Write);
        MergeTemplate_Buffer_F2.Write(MergeTemplate);
        MergeTemplate_Buffer_F2.Truncate();
        MergeTemplate_Buffer_F2.Close();
        return string();
    };

    auto CheckForErrors = [&](String const& LogFileSuffix, vector<string> const& MessagesToSearch)
    {
        File Decode_Result_Buffer_F;
        Decode_Result_Buffer_F.Open(TempNamePrefix + LogFileSuffix);
        int8u Decode_Result_Buffer[100000];
        auto Decode_Result_Buffer_Size = Decode_Result_Buffer_F.Read(Decode_Result_Buffer, 100000);
        Decode_Result_Buffer_F.Close();
        Data.Delete(TempNamePrefix + LogFileSuffix);
        string Decode_Result((const char*)Decode_Result_Buffer, Decode_Result_Buffer_Size);
        bool HasErr = false;
        for (const auto& MessageToSearch : MessagesToSearch)
        {
            if (Decode_Result.rfind(MessageToSearch) != string::npos)
                HasErr = true;
        }
        return HasErr;
    };

    // Decode audio
    if (SourceChannelCount)
    {
        system(AdaptTemplate(__T("LeaveSD_Decode.txt")).c_str());
        Data.Delete(TempNamePrefix + __T(".aac"));
        if (CheckForErrors(__T("_log_decode.txt"), { "Error: ", "\nError reading file." }))
        {
            Data.Delete(TempNamePrefix + __T(".avc"));
            Data.Delete(TempNamePrefix + __T(".aif"));

            if (!ThreadData.FullCheck)
            {
                Convert(ID, FilePos, true);
                return;
            }

            Data.Finished(Dest, { "problem during AAC decoding" }, {});
            return;
        }
    }

    // Encode audio
    int DestChannelCount = SourceChannelCount;
    if (SourceChannelCount)
    {
        // Probing silence
        if (SourceChannelCount > 1 && !KeepSilent)
        {
            system(AdaptTemplate(__T("LeaveSD_Probe.txt"), {}, {}, {}).c_str());
            File ProbeF;
            ProbeF.Open(TempNamePrefix + __T("_log_probe.txt"));
            char* ProbeC = new char[ProbeF.Size_Get()];
            ProbeF.Read((int8u*)ProbeC, ProbeF.Size_Get());
            string Probe(ProbeC, ProbeF.Size_Get());
            delete[] ProbeC;
            vector<float> Probe_Levels, Probe_Peaks;
            size_t Pos1 = (size_t)-1;
            for (;;)
            {
                Pos1 = Probe.find(" RMS level dB: ", Pos1 + 1);
                if (Pos1 == string::npos)
                    break;
                Probe_Levels.push_back((float)atof(Probe.c_str() + Pos1 + 15));
            }
            size_t Pos2 = (size_t)-1;
            for (;;)
            {
                Pos2 = Probe.find(" RMS peak dB: ", Pos2 + 1);
                if (Pos2 == string::npos)
                    break;
                Probe_Peaks.push_back((float)atof(Probe.c_str() + Pos2 + 14));
            }
            if (Probe_Levels.size() == 9 && Probe_Peaks.size() == 9)
            {
                float Level = -100;
                float Peak = -100;
                for (size_t i = 1; i < 8; i++) // Channel 0 and summary excluded
                {
                    if (Level < Probe_Levels[i])
                        Level = Probe_Levels[i];
                    if (Peak < Probe_Peaks[i])
                        Peak = Probe_Peaks[i];
                }
                if (Level < SilenceLevel && Peak < SilencePeak)
                {
                    if (Probe_Levels[0] < SilenceLevel || Probe_Peaks[0] < SilencePeak)
                    {
                        WarningMessages.push_back("silence detected in all channels");
                    }
                    else
                    {
                        WarningMessages.push_back("silence detected in all channels but the first one"
                            " (RMS level " + to_string((int)(Level - 0.5)) +
                            " RMS peak " + to_string((int)(Peak - 0.5)) + ")"
                            " so 1-ch audio encoded");
                        DestChannelCount = 1;
                    }
                }
            }
        }

        // Encode
        EraseBeginEnd.clear();
        Replace.clear();
        if (SourceChannelCount == 1)
        {
            Replace.push_back({ __T("-ac 8"), __T("-ac 2") });
        }
        if (DestChannelCount == 1)
        {
            EraseBeginEnd.push_back({ __T(" -map_channel 0.0.1"), __T("7.aac\"") });
        }
        if (LegacyAac)
        {
            Replace.push_back({ __T(" -profile:a aac_he"), String() });
        }
        system(AdaptTemplate(__T("LeaveSD_Encode.txt"), {}, EraseBeginEnd, Replace).c_str());
        Data.Delete(TempNamePrefix + __T(".aif"));
        if (CheckForErrors(__T("_log_encode.txt"), { "Conversion failed!" }))
        {
            Data.Delete(TempNamePrefix + __T(".avc"));
            Data.Delete(TempNamePrefix + __T("_0.aac"));
            Data.Delete(TempNamePrefix + __T("_1.aac"));
            Data.Delete(TempNamePrefix + __T("_2.aac"));
            Data.Delete(TempNamePrefix + __T("_3.aac"));
            Data.Delete(TempNamePrefix + __T("_4.aac"));
            Data.Delete(TempNamePrefix + __T("_5.aac"));
            Data.Delete(TempNamePrefix + __T("_6.aac"));
            Data.Delete(TempNamePrefix + __T("_7.aac"));

            Data.Finished(Dest, { "problem during AAC encoding" }, {});
            return;
        }
    }

    // Prepare chapters
    EraseBeginEnd.clear();
    Replace.clear();
    Ztring Chapters;
    auto Chapters_Begin = Ztring(MI.Get(Stream_Menu, 0, __T("Chapters_Pos_Begin"))).To_int32u();
    auto Chapters_End = Ztring(MI.Get(Stream_Menu, 0, __T("Chapters_Pos_End"))).To_int32u();
    if (Chapters_Begin < Chapters_End)
    {
        Chapters += __T("<?xml version=\"1.0\"?>\r\n<Chapters>\r\n  <EditionEntry>\r\n");
        bool ChapterIsOpen = false;
        for (auto i = Chapters_Begin; i < Chapters_End; i++)
        {
            Ztring TimeStamp = MI.Get(Stream_Menu, 0, i, Info_Name);
            Ztring Value = MI.Get(Stream_Menu, 0, i);
            bool IsSub = Value.size() > 2 && Value[0] == __T('+') && Value[0] == __T(' ');
            if (IsSub)
            {
                if (!ChapterIsOpen)
                {
                    Chapters += __T("    <ChapterAtom>\r\n");
                    ChapterIsOpen = true;
                }
                Chapters += __T("      <ChapterAtom>\r\n");
                Chapters += __T("        <ChapterTimeStart>") + TimeStamp + __T("</ChapterTimeStart>\r\n");
                Chapters += __T("        <ChapterDisplay>\r\n");
                Chapters += __T("          <ChapterString>") + Value.substr(2) + __T("</ChapterString>\r\n");
                Chapters += __T("          <ChapterLanguage>eng</ChapterLanguage>\r\n");
                Chapters += __T("        </ChapterDisplay>\r\n");
                Chapters += __T("      </ChapterAtom>\r\n");
            }
            else
            {
                if (ChapterIsOpen)
                    Chapters += __T("    </ChapterAtom>\r\n");
                else
                    ChapterIsOpen = true;
                Chapters += __T("    <ChapterAtom>\r\n");
                Chapters += __T("      <ChapterTimeStart>") + TimeStamp + __T("</ChapterTimeStart>\r\n");
                Chapters += __T("      <ChapterDisplay>\r\n");
                Chapters += __T("        <ChapterString>") + Value + __T("</ChapterString>\r\n");
                Chapters += __T("        <ChapterLanguage>eng</ChapterLanguage>\r\n");
                Chapters += __T("      </ChapterDisplay>\r\n");
            }
        }
        if (ChapterIsOpen)
        {
            Chapters += __T("    </ChapterAtom>\r\n");
        }
        Chapters += __T("  </EditionEntry>\r\n</Chapters>\r\n");
    }
    else
        EraseBeginEnd.push_back({ __T(",\r\n\"--chapters\","), __T(".xml\"") });
    File FI;
    FI.Open(TempNamePrefix + __T("_mux_chapters.xml"), File::Access_Write);
    FI.Write(Chapters);
    FI.Truncate();
    FI.Close();

    // Mux
    if (!HasVideo)
    {
        EraseBeginEnd.push_back({ __T(",\r\n\"--sync\","), __T(".avc\"") });
    }
    if (!DestChannelCount)
    {
        EraseBeginEnd.push_back({ __T(",\r\n\"--sync\",\r\n\"0:%DELAY_A%\",\r\n\"--language\",\r\n\"0:mul"), __T("_7.aac\"") });
    }
    if (DestChannelCount == 1)
    {
        EraseBeginEnd.push_back({ __T(",\r\n\"--sync\",\r\n\"0:%DELAY_A%\",\r\n\"--language\",\r\n\"0:ara"), __T("_7.aac\"") });
    }
    map<String, String> MuxTemplate;
    auto Delay = MI.Get(Stream_Audio, 0, __T("Video_Delay"));
    if (Delay.empty())
        Delay = __T("0"); // TEMP continue;
    if (Delay[0] == __T('-'))
        MuxTemplate = { {__T("%DELAY_V%"), Delay.substr(1)},  {__T("%DELAY_A%"),  __T("0")} };
    else
        MuxTemplate = { {__T("%DELAY_V%"),  __T("0")},  {__T("%DELAY_A%"), Delay} };
    TagTemplate.insert(MuxTemplate.begin(), MuxTemplate.end());
    AdaptTemplate(__T("LeaveSD_Mux_Command_Template.json"), __T("_mux_command.json"), EraseBeginEnd, Replace);
    AdaptTemplate(__T("LeaveSD_Mux_Tags_Template.xml"), __T("_mux_tags.xml"));

    Ztring TempNamePrefixSlashes(TempNamePrefix);
    TempNamePrefixSlashes.FindAndReplace(__T("\\"), __T("/"), 0, Ztring_Recursive);
    system(Ztring().From_Local(CreateQuotedTempNamePrefix(ExePathS, "mkvmerge \"@" + TempNamePrefixSlashes.To_Local() + "_mux_command.json\" >" + TempNamePrefixSlashes.To_Local() + "_log_mux2.txt")).To_Local().c_str());
    Data.Delete(TempNamePrefix + __T(".avc"));
    Data.Delete(TempNamePrefix + __T("_0.aac"));
    Data.Delete(TempNamePrefix + __T("_1.aac"));
    Data.Delete(TempNamePrefix + __T("_2.aac"));
    Data.Delete(TempNamePrefix + __T("_3.aac"));
    Data.Delete(TempNamePrefix + __T("_4.aac"));
    Data.Delete(TempNamePrefix + __T("_5.aac"));
    Data.Delete(TempNamePrefix + __T("_6.aac"));
    Data.Delete(TempNamePrefix + __T("_7.aac"));
    Data.Delete(TempNamePrefix + __T("_0.aac"));
    Data.Delete(TempNamePrefix + __T("_mux_chapters.xml"));
    Data.Delete(TempNamePrefix + __T("_mux_command.json"));
    Data.Delete(TempNamePrefix + __T("_mux_tags.xml"));
    bool Err0 = CheckForErrors(__T("_log_mux.txt"), { "Error: " });
    bool Err2 = CheckForErrors(__T("_log_mux2.txt"), { "Error: " });
    if (Err0 || Err2)
    {
        Data.Finished(Dest, { "problem during muxing" }, {});
        return;
    }

    // Move to target
    auto TempFileName = TempNamePrefix + __T(".mkv");
    if (!File::Move(TempFileName, Dest))
    {
        if (!File::Copy(TempFileName, Dest))
        {
            if (!Data.C->ForceExistingFiles)
            {
                Data.Delete(TempFileName);
                Data.Finished(Dest, { "can not move temp file to output location" }, {});
                return;
            }
            if (!File::Delete(Dest))
            {
                if (File::Exists(Dest))
                {
                    Data.Delete(TempFileName);
                    Data.Finished(Dest, { "can not delete already existing output file" }, {});
                    return;
                }
            }
            if (!File::Move(TempFileName, Dest))
            {
                if (!File::Copy(TempFileName, Dest))
                {
                    Data.Delete(TempFileName);
                    Data.Finished(Dest, { "can not move temp file to output location" }, {});
                    return;
                }
            }
        }
        else
            Data.Delete(TempFileName);
    }

    // Check
    ThreadData.IsChecking = true;
    uint64_t PacketCount[2][8];
    uint64_t PacketCheckingCount[2][8];
    uint64_t Duration, CheckingDuration;
    Duration = Ztring(MI.Get(Stream_General, 0, __T("Duration"))).To_int64u();
    PacketCount[0][0] = Ztring(MI.Get(Stream_Video, 0, __T("FrameCount"))).To_int64u();
    for (size_t i = 0; i < DestChannelCount; i++)
    {
        PacketCount[1][i] = Ztring(MI.Get(Stream_Audio, i, __T("FrameCount"))).To_int64u();
    }
    MI.Open(Dest);
    CheckingDuration = Ztring(MI.Get(Stream_General, 0, __T("Duration"))).To_int64u();
    PacketCheckingCount[0][0] = Ztring(MI.Get(Stream_Video, 0, __T("FrameCount"))).To_int64u();
    for (size_t i = 0; i < DestChannelCount; i++)
    {
        PacketCheckingCount[1][i] = Ztring(MI.Get(Stream_Audio, i, __T("FrameCount"))).To_int64u();
    }
    if (Duration && CheckingDuration)
    {
        uint64_t Ratio = Duration < 20000 ? 10 : 1;
        uint64_t Duration_Min = Duration * (40 - Ratio) / 40;
        uint64_t Duration_Max = Duration * (40 + Ratio) / 40;
        uint64_t CheckingDuration_Min = CheckingDuration * (40 - Ratio) / 40;
        uint64_t CheckingDuration_Max = CheckingDuration * (40 + Ratio) / 40;
        if (Duration_Max < CheckingDuration_Min
            || Duration_Min > CheckingDuration_Max)
        {
            std::ostringstream in;
            in.precision(Duration < 10000 ? 1 : 0);
            in << std::fixed << ((float)Duration) / 1000;
            std::ostringstream out;
            out.precision(Duration < 10000 ? 1 : 0);
            out << std::fixed << ((float)CheckingDuration) / 1000;
            WarningMessages.push_back("NSV duration not coherent with actual demuxed data (" + in.str() + "s vs " + out.str() + "s)");
        }
    }
    auto WithPercent = [](int64_t Count, size_t Total)
    {
        std::ostringstream out;
        out.precision(2);
        out << std::fixed << ((float)Count) * 100 / Total;
        return to_string(Count) + " (" + out.str() + "%)";
    };
    bool LaunchFullCheck = false;
    if (CheckingDuration == 0 || PacketCheckingCount[0][0] + PacketCheckingCount[1][0] == 0)
    {
        Data.Finished(Dest, { "can not read output file" }, {});
        return;
    }
    vector<string> ErrorMessages;
    if (PacketCount[0][0] != PacketCheckingCount[0][0])
        ErrorMessages.push_back(WithPercent((int64_t)(PacketCount[0][0] - PacketCheckingCount[0][0]), PacketCount[0][0]) + " missing video packets");
    for (size_t i = 0; i < DestChannelCount; i++)
    {
        if (PacketCount[1][i] / (LegacyAac ? 1 : 2) != PacketCheckingCount[1][i] && PacketCheckingCount[1][i] + 10 < PacketCount[1][i] / (LegacyAac ? 1 : 2)) // Temporary: there is some small issues in MediaInfo counting
        {
            ErrorMessages.push_back(WithPercent((int64_t)(PacketCount[1][i] - PacketCheckingCount[1][i]), PacketCount[1][i]) + " missing audio packets");
            LaunchFullCheck = true;
            break;
        }
    }
    if (DestChannelCount)
    {
        if (!ThreadData.Stats_InvalidAudioPackets.empty())
            WarningMessages.push_back(WithPercent(ThreadData.Stats_InvalidAudioPackets.size(), PacketCount[1][0]) + " invalid AAC syncs in audio packet (skipped)");
        if (!ThreadData.Stats_InvalidAacPackets.empty())
            WarningMessages.push_back(WithPercent(ThreadData.Stats_InvalidAacPackets.size(), PacketCount[1][0]) + " invalid AAC packets (replaced by silent)");
        if (ThreadData.Stats_AudioPacketInvalidSize)
            WarningMessages.push_back(WithPercent(ThreadData.Stats_AudioPacketInvalidSize, PacketCount[1][0]) + " invalid audio packets (skipped)");
    }
    if (ThreadData.Stats_JunkBytes)
        WarningMessages.push_back(to_string(ThreadData.Stats_JunkBytes) + " junk bytes");

    // Check for second pass and stats
    if (LaunchFullCheck && !ThreadData.FullCheck)
    {
        Data.Delete(Dest);
        Convert(ID, FilePos , true);
        return;
    }

    if (!ErrorMessages.empty())
    {
        if (Data.C->KeepTemp)
        {
            if (!File::Move(Dest, TempFileName))
            {
                if (!File::Copy(Dest, TempFileName))
                {
                    Data.Delete(Dest);
                    Data.Finished(Dest, { "can not revert move of temp file to output location" }, {});
                    return;
                }
            }
        }
        else
        {
            Data.Delete(Dest);
        }
    }

    Data.Finished(Dest, ErrorMessages, WarningMessages);
}

//***************************************************************************
// Threads
//***************************************************************************

int Launch_Thread(size_t ID)
{
    for (;;)
    {
        auto const Pos = Data.NextFileNamePos();
        if (Pos == (size_t)-1)
            return 0;
        Data.C->Convert(ID, Pos);
    }
}

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
    if (Inputs.empty())
        return ReturnValue_OK;

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
            {
                auto ShortenedFileName = FileName(Input).Name_Get().To_Local();
                if (ShortenedFileName.size() > 35)
                {
                    ShortenedFileName.erase(15, ShortenedFileName.size()-30);
                    ShortenedFileName.insert(15, "[...]");
                }
                auto DisplayInfo = [&](string const& Command)
                {
                    if (!Err)
                        return;
                    auto ToDisplay = Command + ' ' + ShortenedFileName + " (" + to_string(i) + '/' + to_string(i_Max) + ")...";
                    ToDisplay.resize(77, ' ');
                    *Err << '\r' << ToDisplay;
                };

                i++;
                DisplayInfo("Scanning file");
            }

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
                        *Err << "\r                                                                               \r";
                    if (Out)
                        *Out << Ztring(MI.Get(Stream_General, 0, __T("FileName"))).To_UTF8();
                    Problem = true;
                }
                if (Out)
                    *Out << ";Crash";
            }
            if (MI.Get(Stream_General, 0, __T("Format")) != __T("NSV")
                || (MI.Count_Get(Stream_Video) && MI.Get(Stream_Video, 0, __T("Format_Profile")).empty())
                || (MI.Count_Get(Stream_Audio) &&  MI.Get(Stream_Audio, 0, __T("Format_Version")).empty()))
            {
                if (!Problem)
                {
                    if (Err)
                        *Err << "\r                                                                               \r";
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
                        *Err << "\r                                                                               \r";
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
            *Err << "\r                                                                               \r";
            *Err << "\rScanning done, " << NsvFileNames.size() - i_Bad << " files well detected";
            if (i_Bad)
                *Err << ", " << i_Bad << " files with issues.";
            *Err << "\n";
        }

        return i_Bad ? ReturnValue_ERROR : ReturnValue_OK;
    }

    char Path_Buffer[MAX_PATH + 1] = { 0 };
    auto Path_Buffer_Size = ::GetModuleFileNameA(nullptr, Path_Buffer, MAX_PATH);
    ExePathS.assign(Path_Buffer, Path_Buffer_Size);
    auto Path_SlashPos = ExePathS.rfind('\\');
    if (Path_SlashPos == string::npos)
        return ReturnValue_ERROR;
    ExePathS.resize(Path_SlashPos + 1);
    ExePath = Ztring().From_Local(ExePathS).c_str();

    string TempPathS;
    if (TempPath.empty())
    {
        Path_Buffer_Size = ::GetTempPathA(MAX_PATH, Path_Buffer);
        TempPathS.assign(Path_Buffer, Path_Buffer_Size);
        TempPath = Ztring().From_Local(TempPathS);
    }
    else
    {
        if (TempPath.back() != '/' && TempPath.back() != '\\' && Dir::Exists(TempPath))
            TempPath += '/';
        TempPathS = Ztring(TempPath).To_Local();
    }
    auto TempNamePrefixS = TempPathS + "temp";
    String TempNamePrefix = Ztring().From_Local(TempNamePrefixS).c_str();

    MediaInfo::Option_Static(__T("Demux"), __T("container"));
    MediaInfo::Option_Static(__T("ParseSpeed"), __T("1"));
    MediaInfo::Option_Static(__T("ReadByHuman"), __T("0"));

    MainInDir.Separator_Set(0, __T("\\"));
    for (const auto& Input : Inputs)
    {
        ZtringList AllFiles = Dir::GetAllFileNames(Input);
        for (const auto& FileName : AllFiles)
        {
            if (FileName.size() > 4 && FileName.find(__T(".nsv"), FileName.size() - 4) != (size_t)-1)
            {
                if (MainInDir.empty())
                    MainInDir.Write(FileName);
                else
                {
                    ZtringList Temp;
                    Temp.Separator_Set(0, __T("\\"));
                    Temp.Write(FileName);
                    if (MainInDir.size() > Temp.size())
                        MainInDir.resize(Temp.size());
                    while (!MainInDir.empty() && MainInDir[MainInDir.size() - 1] != Temp[MainInDir.size() - 1])
                    {
                        MainInDir.pop_back();
                        Temp.pop_back();
                    }
                }
                Data.AddFileName(FileName);
            }
        }
    }
    if (Data.Count() == 1 && !MainInDir.empty())
        MainInDir.pop_back(); // if 1 file the last item is the file name
    if (OutputDir[OutputDir.size() - 1] == __T('\\') || OutputDir[OutputDir.size() - 1] == __T('/'))
        OutputDir.pop_back();
    if (File::Exists(OutputDir))
    {
        if (Err)
            *Err << "\n" << Ztring(OutputDir).To_UTF8() << " is a file, please provide a directory name.\n";
        return ReturnValue_ERROR;
    }
    if (!SkipExistingFiles && !ForceExistingFiles && (File::Exists(OutputDir) || Dir::Exists(OutputDir + __T('\\'))))
    {
        ZtringList AllFiles = Dir::GetAllFileNames(OutputDir + __T('\\'), (Dir::dirlist_t)((int)Dir::Include_Files | (int)Dir::Parse_SubDirs));
        if (!AllFiles.empty())
        {
            if (Err)
                *Err << "\n" << Ztring(OutputDir).To_UTF8() << " exists, please provide a non existing output directory name.\n";
            return ReturnValue_ERROR;
        }
    }
    Dir::Create(OutputDir);
    ImputIsDir = Dir::Exists(Inputs[0]);
    if (ImputIsDir && (Inputs[0][Inputs[0].size() - 1] != '\\' || Inputs[0][Inputs[0].size() - 1] != '/'))
        Inputs[0] += '\\';
    Data.C = this;
    if (!ThreadCount)
    {
        ::SYSTEM_INFO lpSystemInfo;
        ::GetSystemInfo(&lpSystemInfo);
        ThreadCount = lpSystemInfo.dwNumberOfProcessors;
        if (!ThreadCount)
            ThreadCount = 1;
    }
    Data.ThreadDatas.resize(ThreadCount);
    size_t ID = 0;
    vector<future<int>> Futures;
    for (auto& ThreadData : Data.ThreadDatas)
    {
        ThreadData.Reset(ID, this, TempNamePrefix);
        Futures.push_back(std::async(std::launch::async, Launch_Thread, ID));
        ID++;
    }
    for (auto& Future : Futures)
        Future.get();

    string Message = "Finished, " + to_string(Data.Count() - Data.SkippedCount()) + " file(s) transcoded";
    if (auto Count = Data.SkippedCount())
        Message += " + " + to_string(Count) + " skipped file(s)";
    Message += '.';
    if (auto Count = Data.ErrorCount())
        Message += ' ' + to_string(Count) + " error(s).";
    if (auto Count = Data.WarningCount())
        Message += ' ' + to_string(Count) + " warning(s).";
    Data.Err(Message, true);

    return Data.ErrorCount() ? ReturnValue_ERROR : ReturnValue_OK;
}

void Core::Frame(size_t ID, const MediaInfo_Event_Global_Demux_4* FrameData)
{
    auto& ThreadData = Data.ThreadDatas[ID];

    if (ThreadData.IsChecking)
        return;

    static const unsigned char ToSearch_Data[] = { 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x0D, 0xAC, 0x34, 0xE8, 0x16, 0x09, 0x6C, 0x04, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x0C, 0xA3, 0xC5, 0x0A, 0xA8, 0x00, 0x00, 0x00, 0x01 };
    static const size_t ToSearch_Size = sizeof(ToSearch_Data);
    if (!FrameData->StreamIDs[0])
    {
        if (FrameData->Content_Size >= ToSearch_Size)
        {
            auto Max = FrameData->Content_Size - ToSearch_Size;
            for (size_t i = 0; i < Max; i++)
            {
                bool IsNok = false;
                for (size_t j = 0; j < ToSearch_Size; j++)
                    if (FrameData->Content[i + j] != ToSearch_Data[j])
                        IsNok = true;
                if (!IsNok)
                {
                    ThreadData.F[FrameData->StreamIDs[0]].Write(FrameData->Content, i + 0x16);
                    static unsigned char ReplacedBy[] = { 0x0B };
                    ThreadData.F[FrameData->StreamIDs[0]].Write(ReplacedBy, 1);
                    ThreadData.F[FrameData->StreamIDs[0]].Write(FrameData->Content + i + 0x17, FrameData->Content_Size - (i + 0x17));
                    return;
                }
            }
        }
    }
    if (FrameData->StreamIDs[0])
    {
        if (!FrameData->Content_Size)
            ThreadData.Stats_AudioPacketInvalidSize++;
        else if (ThreadData.FullCheck)
        {
            size_t Pos = 0;
            while (Pos < FrameData->Content_Size)
            {
                BitStream BS(FrameData->Content + Pos, FrameData->Content_Size - Pos);
                auto Sync1 = BS.Get4(30);
                auto Size = BS.Get2(13);
                auto Sync2 = BS.Get2(13);
                if ((Sync1 & 0xFFFFFFE3) != 0x3ffc5400 || Sync2 != 0x1ffc || !Size)
                {
                    ThreadData.Stats_InvalidAudioPackets.push_back(ThreadData.Stats_AacPacketPos);

                    // Let's try to synchronize again
                    Pos++;
                    while (Pos + 1 < FrameData->Content_Size && (FrameData->Content[Pos] != 0xFF
                        || (FrameData->Content[Pos + 1] & 0xF6) != 0xF0))
                        Pos++;

                    if (Pos + 1 >= FrameData->Content_Size)
                        break;
                    continue;
                }
                MediaInfo MI;
                MI.Option(__T("File_ForceParser"), __T("Adts"));
                MI.Option(__T("File_Macroblocks_Parse"), __T("1")); // Used for parsing AAC frame, -1 means no check at all, 1 full check
                MI.Open_Buffer_Init(Size, 0);
                MI.Open_Buffer_Continue((MediaInfo_int8u*)FrameData->Content + Pos, Size);
                MI.Open_Buffer_Finalize();
                String Format = MI.Get(Stream_Audio, 0, __T("Format"));
                if (Format != __T("AAC") || !MI.Get(Stream_Audio, 0, __T("GainControl_Present")).empty() || !MI.Get(Stream_Audio, 0, __T("Errors")).empty() || MI.Get(Stream_Audio, 0, __T("Channel(s)")) != ThreadData.ChannelCount)
                {
                    static const unsigned char EmptyAac_1_Data[] = { 0xFF, 0xF1, 0x50, 0x40, 0x1B, 0x3F, 0xFC, 0x01, 0x16, 0x99, 0xFE, 0x8C, 0x16, 0xA8, 0x8D, 0x09, 0x5A, 0xE2, 0xE9, 0x72, 0x06, 0xB2, 0xF2, 0x4A, 0xB3, 0x07, 0x19, 0xAD, 0xBE, 0xDD, 0x2A, 0x7C, 0x1E, 0x82, 0x67, 0x5E, 0x4D, 0x55, 0xED, 0xE5, 0xA3, 0x71, 0x11, 0x61, 0x4E, 0x2D, 0xCC, 0x87, 0x2F, 0x22, 0x9F, 0xCB, 0xBB, 0x0B, 0x34, 0x7B, 0x3F, 0x5E, 0x9C, 0x72, 0xB7, 0xF1, 0xCE, 0x67, 0xFF, 0x4A, 0x6A, 0xEA, 0xCB, 0xD3, 0xCA, 0x8A, 0xEE, 0x93, 0x45, 0x59, 0xCB, 0x6D, 0x95, 0xD8, 0x49, 0x75, 0x3A, 0xB6, 0x04, 0xF3, 0xC7, 0x11, 0x70, 0x77, 0xBF, 0x51, 0xD4, 0xDE, 0x49, 0xFF, 0x11, 0x4E, 0xCD, 0x2D, 0x79, 0x80, 0x2D, 0x96, 0x3B, 0xA8, 0x06, 0x83, 0x94, 0x6C, 0x54, 0x08, 0x99, 0x06, 0xC2, 0x1B, 0xE4, 0xA5, 0x0D, 0x60, 0xAA, 0x3D, 0xCC, 0x45, 0x50, 0x83, 0x39, 0x14, 0xDD, 0xC3, 0x5A, 0x07, 0x56, 0x27, 0x4F, 0xB8, 0x12, 0xEC, 0x7C, 0x2F, 0x86, 0xDC, 0xA6, 0xAB, 0xD8, 0x55, 0x4B, 0x96, 0x3C, 0x30, 0xBA, 0xFC, 0x6B, 0x8B, 0xF7, 0x3E, 0x72, 0xA6, 0xD2, 0xA3, 0x39, 0xD7, 0xC3, 0xB8, 0xFE, 0x42, 0x71, 0xCE, 0x25, 0x17, 0xBB, 0xEA, 0x57, 0xE8, 0x69, 0xB1, 0x70, 0xF6, 0x9B, 0x3B, 0x3A, 0x1A, 0xBC, 0x36, 0xD1, 0xD7, 0xB6, 0x1A, 0x22, 0x7C, 0x9E, 0xE7, 0x69, 0x05, 0xEB, 0xC2, 0x41, 0xAA, 0xAE, 0x9F, 0x20, 0x0B, 0x3F, 0x0D, 0xF7, 0x12, 0x8D, 0x9E, 0x35, 0x3B, 0xC1, 0xD7, 0xED, 0x78, 0x76, 0x85, 0x9C };
                    static const size_t EmptyAac_1_Size = sizeof(EmptyAac_1_Data);
                    static const unsigned char EmptyAac_8_Data[] = { 0xFF, 0xF1, 0x50, 0x00, 0x42, 0x9F, 0xFC, 0xD8, 0x00, 0x00, 0xDE, 0x5E, 0x33, 0x58, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x33, 0xFD, 0xAA, 0x30, 0x58, 0xC1, 0x66, 0x89, 0xCB, 0x5F, 0x8B, 0x4A, 0xAA, 0xAA, 0x0C, 0xFA, 0x49, 0x8A, 0xA6, 0x68, 0x95, 0xC8, 0xDE, 0xCE, 0x14, 0x63, 0x66, 0x61, 0x9A, 0xBF, 0x97, 0x21, 0x4D, 0xDD, 0x16, 0x69, 0x23, 0xCE, 0x26, 0xB5, 0xB1, 0x99, 0xDE, 0x20, 0x84, 0xB7, 0xD0, 0x2A, 0x14, 0xC2, 0x1C, 0xBF, 0x92, 0xAC, 0x97, 0x2A, 0x6B, 0x02, 0x05, 0x1C, 0x90, 0x61, 0xCF, 0x0D, 0xC6, 0xCE, 0x6D, 0xC2, 0x10, 0xC3, 0x1F, 0xC4, 0x5C, 0x25, 0x5B, 0x96, 0xF6, 0x02, 0xE8, 0xFE, 0x7C, 0xFD, 0x7B, 0xBF, 0x9E, 0x76, 0x57, 0x81, 0x50, 0x2F, 0x94, 0xFA, 0xAB, 0x68, 0x66, 0xCA, 0x8D, 0x35, 0xF9, 0x2A, 0xA4, 0xAA, 0xE2, 0x25, 0xC2, 0x07, 0x3E, 0x54, 0x67, 0x01, 0x10, 0xA2, 0xF6, 0xF3, 0x05, 0x28, 0x13, 0x70, 0x0A, 0x3C, 0xB7, 0xF5, 0x8C, 0x5E, 0xB7, 0x4F, 0x5D, 0x55, 0x4C, 0x1A, 0x71, 0xAA, 0xF8, 0x9F, 0x1D, 0xB8, 0xA4, 0xED, 0x8C, 0x95, 0x50, 0x72, 0x2E, 0x9C, 0x74, 0x8E, 0x61, 0x9D, 0xAA, 0xB9, 0xEE, 0x58, 0x08, 0x5E, 0x99, 0x29, 0x08, 0x5C, 0xE2, 0x4C, 0xD6, 0x5F, 0x6C, 0xC9, 0x2F, 0x9A, 0xBF, 0x6F, 0x5D, 0x24, 0x8E, 0x8E, 0x04, 0x88, 0x62, 0x64, 0x64, 0x6A, 0x08, 0xB1, 0x23, 0x3D, 0xF5, 0x80, 0x03, 0xF0, 0x38, 0x40, 0xFB, 0xA5, 0xD0, 0xF2, 0xB6, 0x85, 0x02, 0x63, 0x55, 0xBF, 0x70, 0x2B, 0x98, 0xD6, 0xAC, 0x86, 0x78, 0x45, 0xF3, 0x93, 0x7F, 0x13, 0xF3, 0x75, 0xC5, 0x02, 0x3B, 0x3B, 0x5D, 0x8E, 0x39, 0x10, 0xA9, 0x50, 0xC0, 0xB9, 0x61, 0xCD, 0x05, 0x2C, 0x4B, 0x3E, 0x7F, 0x33, 0x14, 0x93, 0x06, 0x55, 0x76, 0x22, 0xA2, 0x52, 0xBC, 0x53, 0xBF, 0x94, 0x5E, 0x32, 0x77, 0xA6, 0x53, 0x55, 0x3A, 0xF0, 0xAB, 0xAC, 0x2B, 0x01, 0x95, 0x55, 0x68, 0x95, 0x18, 0xED, 0xAE, 0x50, 0x42, 0x83, 0xFD, 0xB7, 0x51, 0x0F, 0x22, 0x8F, 0x35, 0x29, 0x4B, 0x94, 0x02, 0x8C, 0x75, 0xCB, 0x01, 0xFE, 0x43, 0xBE, 0xC4, 0xF6, 0xE8, 0x21, 0xF8, 0x2E, 0x28, 0xED, 0xD5, 0xE9, 0x37, 0x9D, 0x0B, 0x0E, 0xE8, 0x0F, 0x40, 0x02, 0x34, 0x1F, 0xED, 0xB6, 0x88, 0x72, 0xD8, 0xB5, 0x04, 0x74, 0x90, 0x75, 0x92, 0xB5, 0x80, 0xBA, 0x62, 0xCF, 0x08, 0xEF, 0xB1, 0x09, 0x68, 0x08, 0x25, 0x41, 0xFE, 0xDB, 0xA8, 0x86, 0xB2, 0x8F, 0x8C, 0x15, 0x55, 0x0F, 0x08, 0x1C, 0xF0, 0xED, 0x41, 0xC7, 0x84, 0x3F, 0x35, 0x45, 0x68, 0x08, 0x02, 0xDC, 0x99, 0xFE, 0x88, 0x36, 0x40, 0x98, 0x81, 0x62, 0xE5, 0x14, 0x83, 0x9A, 0x87, 0xA4, 0x11, 0x79, 0x73, 0xA4, 0xA3, 0x96, 0x07, 0xCD, 0x5C, 0x72, 0x10, 0xFE, 0x90, 0x60, 0x1B, 0x39, 0x16, 0xB7, 0x3B, 0x61, 0x6E, 0x50, 0xA5, 0x65, 0x7A, 0x10, 0x08, 0x31, 0xB1, 0x85, 0x4E, 0x22, 0xF7, 0x99, 0x08, 0x6A, 0x59, 0x39, 0x8B, 0x13, 0x5C, 0xCD, 0x76, 0x34, 0x99, 0x24, 0x6A, 0x90, 0xA4, 0x0A, 0x75, 0x2C, 0x28, 0x59, 0xB0, 0x42, 0xF6, 0x8F, 0x82, 0xD0, 0x06, 0xFE, 0x2B, 0x3B, 0x84, 0xDC, 0x1A, 0xCB, 0xCD, 0x9C, 0x91, 0xC5, 0xD6, 0x85, 0x25, 0x40, 0x10, 0x10, 0xC6, 0x67, 0x54, 0x68, 0xB8, 0xAF, 0xB1, 0x25, 0x01, 0xAA, 0x86, 0x26, 0xDF, 0x28, 0x30, 0xBB, 0x81, 0x4C, 0x84, 0xEB, 0x8A, 0x63, 0x86, 0xAE, 0xDD, 0xBA, 0x3E, 0xDB, 0x1D, 0x2C, 0xD7, 0xCB, 0xF3, 0x30, 0x8D, 0x3C, 0xA3, 0x8D, 0xCE, 0xBE, 0x4E, 0x39, 0x6E, 0xD8, 0x56, 0xB6, 0x3A, 0x59, 0x67, 0xB1, 0x15, 0xAA, 0xC3, 0x6F, 0x9D, 0x9E, 0x79, 0x6D, 0xD4, 0x6C, 0x27, 0x4F, 0x46, 0x79, 0xFD, 0xB7 };
                    static const size_t EmptyAac_8_Size = sizeof(EmptyAac_8_Data);
                    if (ThreadData.ChannelCount == __T("1"))
                    {
                        ThreadData.Stats_InvalidAacPackets.push_back(ThreadData.Stats_AacPacketPos);
                        ThreadData.F[FrameData->StreamIDs[0]].Write(EmptyAac_1_Data, EmptyAac_1_Size);
                    }
                    else
                    {
                        ThreadData.Stats_InvalidAacPackets.push_back(ThreadData.Stats_AacPacketPos);
                        ThreadData.F[FrameData->StreamIDs[0]].Write(EmptyAac_8_Data, EmptyAac_8_Size);
                    }
                }
                else
                    ThreadData.F[FrameData->StreamIDs[0]].Write(FrameData->Content + Pos, Size);
                ThreadData.Stats_AacPacketPos++;
                Pos += Size;
            }
            return;
        }
    }
    ThreadData.F[FrameData->StreamIDs[0]].Write(FrameData->Content, FrameData->Content_Size);
}


//***************************************************************************
// Helpers
//***************************************************************************

string MediaInfo_Version()
{
    return Ztring(MediaInfo::Option_Static(__T("Info_Version"), String())).SubString(__T(" - v"), String()).To_UTF8();
}

