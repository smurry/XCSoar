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

#include "Internal.hpp"
#include "Protocol.hpp"
#include "Device/Port.hpp"
#include "Operation.hpp"
#include "Units/Units.hpp"

#include <tchar.h>
#include <stdio.h>
#include <assert.h>

#ifdef _UNICODE
#include <windows.h>
#endif

static int DeclIndex = 128;

static void
convert_string(char *dest, size_t size, const TCHAR *src)
{
#ifdef _UNICODE
  size_t length = _tcslen(src);
  if (length >= size)
    length = size - 1;

  int length2 = ::WideCharToMultiByte(CP_ACP, 0, src, length, dest, size,
                                      NULL, NULL);
  if (length2 < 0)
    length2 = 0;
  dest[length2] = '\0';
#else
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
#endif
}

static bool
cai302DeclAddWaypoint(Port *port, const Waypoint &way_point)
{
  int DegLat, DegLon;
  double tmp, MinLat, MinLon;
  char NoS, EoW;

  tmp = way_point.Location.Latitude.value_degrees();
  NoS = 'N';
  if (tmp < 0) {
    NoS = 'S';
    tmp = -tmp;
  }
  DegLat = (int)tmp;
  MinLat = (tmp - DegLat) * 60;

  tmp = way_point.Location.Longitude.value_degrees();
  EoW = 'E';
  if (tmp < 0) {
    EoW = 'W';
    tmp = -tmp;
  }
  DegLon = (int)tmp;
  MinLon = (tmp - DegLon) * 60;

  char Name[13];
  convert_string(Name, sizeof(Name), way_point.Name.c_str());

  char szTmp[128];
  sprintf(szTmp, "D,%d,%02d%07.4f%c,%03d%07.4f%c,%s,%d\r",
          DeclIndex,
          DegLat, MinLat, NoS,
          DegLon, MinLon, EoW,
          Name,
          (int)way_point.Altitude);

  DeclIndex++;

  port->Write(szTmp);

  return port->ExpectString("dn>");
}

static bool
ReadBlock(Port &port, void *dest, size_t max_length, size_t length)
{
  assert(dest != NULL);
  assert(max_length > 0);

  if (length > max_length)
    length = max_length;

  if (length == 0)
    return true;

  return port.FullRead(dest, length, 3000);
}

static bool
DeclareInner(Port *port, const Declaration *decl,
             gcc_unused OperationEnvironment &env)
{
  unsigned size = decl->size();

  port->SetRxTimeout(500);

  env.SetProgressRange(6 + size);
  env.SetProgressPosition(0);

  port->Flush();
  port->Write('\x03');

  port->GetChar();

  /* empty rx buffer */
  port->SetRxTimeout(0);
  while (port->GetChar() != EOF) {}

  port->SetRxTimeout(500);
  port->Write('\x03');
  if (!port->ExpectString("cmd>"))
    return false;

  port->Write("upl 1\r");
  if (!port->ExpectString("up>"))
    return false;

  port->Flush();

  port->Write("O\r");

  port->SetRxTimeout(1500);

  cai302_OdataNoArgs_t cai302_OdataNoArgs;
  if (!ReadBlock(*port, &cai302_OdataNoArgs, sizeof(cai302_OdataNoArgs),
                 sizeof(cai302_OdataNoArgs)) ||
      !port->ExpectString("up>"))
    return false;

  env.SetProgressPosition(1);

  port->Write("O 0\r"); // 0=active pilot

  cai302_OdataPilot_t cai302_OdataPilot;
  if (!ReadBlock(*port, &cai302_OdataPilot, sizeof(cai302_OdataPilot),
                 cai302_OdataNoArgs.PilotRecordSize + 3) ||
      !port->ExpectString("up>"))
    return false;

  env.SetProgressPosition(2);

  swap(cai302_OdataPilot.ApproachRadius);
  swap(cai302_OdataPilot.ArrivalRadius);
  swap(cai302_OdataPilot.EnrouteLoggingInterval);
  swap(cai302_OdataPilot.CloseTpLoggingInterval);
  swap(cai302_OdataPilot.TimeBetweenFlightLogs);
  swap(cai302_OdataPilot.MinimumSpeedToForceFlightLogging);
  swap(cai302_OdataPilot.UnitWord);
  swap(cai302_OdataPilot.MarginHeight);

  port->Write("G\r");

  cai302_GdataNoArgs_t cai302_GdataNoArgs;
  if (!ReadBlock(*port, &cai302_GdataNoArgs, sizeof(cai302_GdataNoArgs),
                 sizeof(cai302_GdataNoArgs)) ||
      !port->ExpectString("up>"))
    return false;

  env.SetProgressPosition(3);

  port->Write("G 0\r");

  cai302_Gdata_t cai302_Gdata;
  if (!ReadBlock(*port, &cai302_Gdata, sizeof(cai302_Gdata),
                 cai302_GdataNoArgs.GliderRecordSize + 3) ||
      !port->ExpectString("up>"))
    return false;

  env.SetProgressPosition(4);

  swap(cai302_Gdata.WeightInLiters);
  swap(cai302_Gdata.BallastCapacity);
  swap(cai302_Gdata.ConfigWord);
  swap(cai302_Gdata.WingArea);

  port->Write('\x03');
  if (!port->ExpectString("cmd>"))
    return false;

  port->Write("dow 1\r");
  if (!port->ExpectString("dn>"))
    return false;

  char PilotName[25], GliderType[13], GliderID[13];
  convert_string(PilotName, sizeof(PilotName), decl->PilotName);
  convert_string(GliderType, sizeof(GliderType), decl->AircraftType);
  convert_string(GliderID, sizeof(GliderID), decl->AircraftReg);

  char szTmp[255];
  sprintf(szTmp, "O,%-24s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r",
          PilotName,
          cai302_OdataPilot.OldUnit,
          cai302_OdataPilot.OldTemperaturUnit,
          cai302_OdataPilot.SinkTone,
          cai302_OdataPilot.TotalEnergyFinalGlide,
          cai302_OdataPilot.ShowFinalGlideAltitude,
          cai302_OdataPilot.MapDatum,
          cai302_OdataPilot.ApproachRadius,
          cai302_OdataPilot.ArrivalRadius,
          cai302_OdataPilot.EnrouteLoggingInterval,
          cai302_OdataPilot.CloseTpLoggingInterval,
          cai302_OdataPilot.TimeBetweenFlightLogs,
          cai302_OdataPilot.MinimumSpeedToForceFlightLogging,
          cai302_OdataPilot.StfDeadBand,
          cai302_OdataPilot.ReservedVario,
          cai302_OdataPilot.UnitWord,
          cai302_OdataPilot.MarginHeight);

  port->Write(szTmp);
  if (!port->ExpectString("dn>"))
    return false;

  env.SetProgressPosition(5);

  sprintf(szTmp, "G,%-12s,%-12s,%d,%d,%d,%d,%d,%d,%d,%d\r",
          GliderType,
          GliderID,
          cai302_Gdata.bestLD,
          cai302_Gdata.BestGlideSpeed,
          cai302_Gdata.TwoMeterSinkAtSpeed,
          cai302_Gdata.WeightInLiters,
          cai302_Gdata.BallastCapacity,

          0,
          cai302_Gdata.ConfigWord,
          cai302_Gdata.WingArea);

  port->Write(szTmp);
  if (!port->ExpectString("dn>"))
    return false;

  env.SetProgressPosition(6);

  DeclIndex = 128;

  for (unsigned i = 0; i < size; ++i) {
    if (!cai302DeclAddWaypoint(port, decl->get_waypoint(i)))
      return false;

    env.SetProgressPosition(7 + i);
  }

  port->Write("D,255\r");
  port->SetRxTimeout(1500); // D,255 takes more than 800ms
  return port->ExpectString("dn>");
}

bool
CAI302Device::Declare(const Declaration *decl, OperationEnvironment &env)
{
  port->StopRxThread();

  bool success = DeclareInner(port, decl, env);

  port->SetRxTimeout(500);

  port->Write('\x03');
  port->ExpectString("cmd>");

  port->Write("LOG 0\r");

  port->SetRxTimeout(0);
  port->StartRxThread();

  return success;
}