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

#include "DeviceBlackboard.hpp"
#include "Protection.hpp"
#include "Math/Earth.hpp"
#include "UtilsSystem.hpp"
#include "Asset.hpp"
#include "Device/All.hpp"
#include "Math/Constants.h"
#include "GlideSolvers/GlidePolar.hpp"
#include "Simulator.hpp"
#include "OS/Clock.hpp"

#include <limits.h>

#ifdef WIN32
#include <windows.h>
#endif

DeviceBlackboard device_blackboard;

/**
 * Initializes the DeviceBlackboard
 */
void
DeviceBlackboard::Initialise()
{
  ScopeLock protect(mutex);

  // Clear the gps_info and calculated_info
  gps_info.Reset();
  calculated_info.Reset();

  // Set GPS assumed time to system time
  gps_info.UpdateClock();
  gps_info.date_time_utc = BrokenDateTime::NowUTC();
  gps_info.time = fixed(gps_info.date_time_utc.GetSecondOfDay());

  for (unsigned i = 0; i < NUMDEV; ++i)
    per_device_data[i] = gps_info;

  real_data = simulator_data = replay_data = gps_info;
}

/**
 * Sets the location and altitude to loc and alt
 *
 * Called at startup when no gps data available yet
 * @param loc New location
 * @param alt New altitude
 */
void
DeviceBlackboard::SetStartupLocation(const GeoPoint &loc, const fixed alt)
{
  ScopeLock protect(mutex);

  if (Calculated().flight.flying)
    return;

  for (unsigned i = 0; i < NUMDEV; ++i)
    if (!per_device_data[i].location_available)
      per_device_data[i].SetFakeLocation(loc, alt);

  if (!real_data.location_available)
    real_data.SetFakeLocation(loc, alt);

  simulator_data.SetFakeLocation(loc, alt);

  ScheduleMerge();
}

/**
 * Sets the location, altitude and other basic parameters
 *
 * Used by the IgcReplay
 * @param loc New location
 * @param speed New speed
 * @param bearing New bearing
 * @param alt New altitude
 * @param baroalt New barometric altitude
 * @param t New time
 * @see IgcReplay::UpdateInternal()
 */
void
DeviceBlackboard::SetLocation(const GeoPoint &loc,
                              const fixed speed, const Angle bearing,
                              const fixed alt, const fixed baroalt,
                              const fixed t)
{
  ScopeLock protect(mutex);
  NMEAInfo &basic = SetReplayState();

  basic.clock = t;
  basic.connected.Update(basic.clock);
  basic.ProvideTime(t);
  basic.gps.satellites_used = -1;
  basic.acceleration.available = false;
  basic.location = loc;
  basic.location_available.Update(t);
  basic.ground_speed = speed;
  basic.ground_speed_available.Update(t);
  basic.airspeed_available.Clear(); // Clear airspeed as it is not given by any value.
  basic.airspeed_real = false;
  basic.track = bearing;
  basic.track_available.Update(t);
  basic.gps_altitude = alt;
  basic.gps_altitude_available.Update(t);
  basic.ProvidePressureAltitude(baroalt);
  basic.ProvideBaroAltitudeTrue(baroalt);
  basic.total_energy_vario_available.Clear();
  basic.netto_vario_available.Clear();
  basic.external_wind_available.Clear();
  basic.gps.real = false;
  basic.gps.replay = true;
  basic.gps.simulator = false;

  ScheduleMerge();
};

/**
 * Stops the replay
 */
void DeviceBlackboard::StopReplay() {
  ScopeLock protect(mutex);

  replay_data.connected.Clear();

  ScheduleMerge();
}

void
DeviceBlackboard::ProcessSimulation()
{
  if (!is_simulator())
    return;

  ScopeLock protect(mutex);

  simulator.Process(simulator_data);
  ScheduleMerge();
}

/**
 * Sets the GPS speed and indicated airspeed to val
 *
 * not in use
 * @param val New speed
 */
void
DeviceBlackboard::SetSpeed(fixed val)
{
  ScopeLock protect(mutex);
  NMEAInfo &basic = simulator_data;

  basic.ground_speed = val;
  basic.ProvideBothAirspeeds(val);

  ScheduleMerge();
}

/**
 * Sets the TrackBearing to val
 *
 * not in use
 * @param val New TrackBearing
 */
void
DeviceBlackboard::SetTrack(Angle val)
{
  ScopeLock protect(mutex);
  simulator_data.track = val.as_bearing();

  ScheduleMerge();
}

/**
 * Sets the altitude and barometric altitude to val
 *
 * not in use
 * @param val New altitude
 */
void
DeviceBlackboard::SetAltitude(fixed val)
{
  ScopeLock protect(mutex);
  NMEAInfo &basic = simulator_data;

  basic.gps_altitude = val;
  basic.ProvidePressureAltitude(val);
  basic.ProvideBaroAltitudeTrue(val);

  ScheduleMerge();
}

/**
 * Reads the given derived_info usually provided by the
 * GlideComputerBlackboard and saves it to the own Blackboard
 * @param derived_info Calculated information usually provided
 * by the GlideComputerBlackboard
 */
void
DeviceBlackboard::ReadBlackboard(const DerivedInfo &derived_info)
{
  calculated_info = derived_info;
}

/**
 * Reads the given settings usually provided by the InterfaceBlackboard
 * and saves it to the own Blackboard
 * @param settings SettingsComputer usually provided by the
 * InterfaceBlackboard
 */
void
DeviceBlackboard::ReadSettingsComputer(const SETTINGS_COMPUTER
					      &settings)
{
  settings_computer = settings;
}

void
DeviceBlackboard::expire_wall_clock()
{
  ScopeLock protect(mutex);
  if (!Basic().connected)
    return;

  bool modified = false;
  for (unsigned i = 0; i < NUMDEV; ++i) {
    NMEAInfo &basic = per_device_data[i];
    if (!basic.connected)
      continue;

    basic.ExpireWallClock();
    if (!basic.connected)
      modified = true;
  }

  if (modified)
    ScheduleMerge();
}

void
DeviceBlackboard::ScheduleMerge()
{
  TriggerMergeThread();
}

void
DeviceBlackboard::Merge()
{
  real_data.Reset();
  for (unsigned i = 0; i < NUMDEV; ++i) {
    if (!per_device_data[i].connected)
      continue;

    per_device_data[i].UpdateClock();
    per_device_data[i].Expire();
    real_data.Complement(per_device_data[i]);
  }

  if (replay_data.connected) {
    /* the replay may run at a higher speed; use NMEA_INFO::Time as a
       "fake wallclock" to prevent them from expiring too quickly */
    replay_data.clock = replay_data.time;

    replay_data.Expire();
    SetBasic() = replay_data;
  } else if (simulator_data.connected) {
    simulator_data.UpdateClock();
    simulator_data.Expire();
    SetBasic() = simulator_data;
  } else {
    SetBasic() = real_data;
  }
}

void
DeviceBlackboard::SetQNH(fixed qnh)
{
  AllDevicesPutQNH(AtmosphericPressure(qnh));
}

void
DeviceBlackboard::SetMC(fixed mc)
{
  AllDevicesPutMacCready(mc);
}
