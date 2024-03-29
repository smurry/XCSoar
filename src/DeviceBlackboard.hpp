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

#ifndef DEVICE_BLACKBOARD_H
#define DEVICE_BLACKBOARD_H

#include "Blackboard.hpp"
#include "SettingsComputerBlackboard.hpp"
#include "Device/Simulator.hpp"
#include "Device/List.hpp"
#include "Thread/Mutex.hpp"

#include <cassert>

class GlidePolar;

/**
 * Blackboard used by com devices: can write NMEA_INFO, reads DERIVED_INFO.
 * Also provides methods to emulate device updates e.g. from logger replay
 * 
 * The DeviceBlackboard is used as the global ground truth-state
 * since it is accessed quickly with only one mutex
 */
class DeviceBlackboard:
  public BaseBlackboard,
  public SettingsComputerBlackboard
{
  friend class MergeThread;

  Simulator simulator;

  /**
   * Data from each physical device.
   */
  NMEAInfo per_device_data[NUMDEV];

  /**
   * Merged data from the physical devices.
   */
  NMEAInfo real_data;

  /**
   * Data from simulator.
   */
  NMEAInfo simulator_data;

  /**
   * Data from replay.
   */
  NMEAInfo replay_data;

public:
  Mutex mutex;

public:
  void Initialise();
  void ReadBlackboard(const DerivedInfo &derived_info);
  void ReadSettingsComputer(const SETTINGS_COMPUTER &settings);

protected:
  NMEAInfo &SetBasic() { return gps_info; }
  MoreData &SetMoreData() { return gps_info; }

public:
  const NMEAInfo &RealState(unsigned i) const {
    assert(i < NUMDEV);
    return per_device_data[i];
  }

  NMEAInfo &SetRealState(unsigned i) {
    assert(i < NUMDEV);
    return per_device_data[i];
  }

  NMEAInfo &SetSimulatorState() { return simulator_data; }
  NMEAInfo &SetReplayState() { return replay_data; }

public:
  const NMEAInfo &RealState() const { return real_data; }

  /**
   * Is the specified device a FLARM?
   *
   * This method does not lock the blackboard, it is unprotected.  It
   * is assumed that this method is not important enough to implement
   * proper locking.
   */
  gcc_pure
  bool IsFLARM(unsigned i) const {
    return RealState(i).flarm.IsDetected();
  }

  void SetStartupLocation(const GeoPoint &loc, const fixed alt);
  void SetLocation(const GeoPoint &loc, const fixed speed, const Angle bearing,
                   const fixed alt, const fixed baroalt, const fixed t);
  void ProcessSimulation();
  void StopReplay();
  void SetTrack(Angle val);
  void SetSpeed(fixed val);
  void SetAltitude(fixed alt);
  void SetQNH(fixed qnh);
  void SetMC(fixed mc);

  /**
   * Check the expiry time of the device connection with the wall
   * clock time.  This method locks the blackboard, i.e. it may be
   * called from any thread.
   *
   * @return true if the connection has just expired, false if the
   * connection status has not changed
   */
  void expire_wall_clock();

  /**
   * Trigger the MergeThread, which will call Merge().  Call this
   * after a modification.  The caller doesn't need to hold the lock.
   */
  void ScheduleMerge();

  /**
   * Copy real_data or simulator_data or replay_data to gps_info.
   * Caller must lock the blackboard.
   */
  void Merge();
};

extern DeviceBlackboard device_blackboard;

#endif
