/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Profile/Profile.hpp"
#include "IO/KeyValueFileWriter.hpp"
#include "LogFile.hpp"
#include "Asset.hpp"
#include "LocalPath.hpp"
#include "StringUtil.hpp"
#include "IO/KeyValueFileReader.hpp"
#include "IO/FileLineReader.hpp"
#include "IO/TextWriter.hpp"
#include "OS/FileUtil.hpp"

#include <string.h>
#include <windef.h> /* for MAX_PATH */

#define XCSPROFILE "xcsoar-registry.prf"

static TCHAR startProfileFile[MAX_PATH];

const TCHAR *
Profile::GetPath()
{
  return startProfileFile;
}

void
Profile::Load()
{
  LogStartUp(_T("Loading profiles"));
  LoadFile(startProfileFile);
}

void
Profile::LoadFile(const TCHAR *szFile)
{
  if (string_is_empty(szFile))
    return;

  FileLineReader reader(szFile);
  if (reader.error())
    return;

  LogStartUp(_T("Loading profile from %s"), szFile);

  KeyValueFileReader kvreader(reader);
  KeyValuePair pair;
  while (kvreader.Read(pair))
    Set(pair.key, pair.value);
}

void
Profile::Save()
{
  LogStartUp(_T("Saving profiles"));
  SaveFile(startProfileFile);
}

void
Profile::SaveFile(const TCHAR *szFile)
{
  if (string_is_empty(szFile))
    return;

  // Try to open the file for writing
  TextWriter writer(szFile);
  // ... on error -> return
  if (writer.error())
    return;

  KeyValueFileWriter kvwriter(writer);

  LogStartUp(_T("Saving profile to %s"), szFile);
  Export(kvwriter);
}


void
Profile::SetFiles(const TCHAR* override)
{
  if (!string_is_empty(override)) {
    CopyString(startProfileFile, override, MAX_PATH);
    return;
  }

  // Set the default profile file
  LocalPath(startProfileFile, _T(XCSPROFILE));

  if (is_altair() && !File::Exists(startProfileFile)) {
    /* backwards compatibility with old Altair firmware */
    LocalPath(startProfileFile, _T("config/")_T(XCSPROFILE));
    if (!File::Exists(startProfileFile))
      LocalPath(startProfileFile, _T(XCSPROFILE));
  }
}

bool
Profile::GetPath(const TCHAR *key, TCHAR *value)
{
  if (!Get(key, value, MAX_PATH) || string_is_empty(value))
    return false;

  ExpandLocalPath(value);
  return true;
}

bool
Profile::GetPathIsEqual(const TCHAR *key, const TCHAR *value)
{
  TCHAR saved[MAX_PATH];
  if (!Get(key, saved, MAX_PATH))
    return false;

  ExpandLocalPath(saved);
  return _tcscmp(saved, value) == 0;
}

void
Profile::SetPath(const TCHAR *key, const TCHAR *value)
{
  TCHAR path[MAX_PATH];

  if (string_is_empty(value))
    path[0] = '\0';
  else {
    CopyString(path, value, MAX_PATH);
    ContractLocalPath(path);
  }

  Set(key, path);
}
