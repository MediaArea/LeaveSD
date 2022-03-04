/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-2-Clause license that can
 *  be found in the LICENSE.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#pragma once
#include "Common/Config.h"
class Core;
//---------------------------------------------------------------------------

//***************************************************************************
// Command line parser
//***************************************************************************

return_value Parse(Core& C, int argc, const char* argv[]);
