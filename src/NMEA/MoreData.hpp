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

#ifndef XCSOAR_MORE_DATA_HPP
#define XCSOAR_MORE_DATA_HPP

#include "NMEA/Info.hpp"

/**
 * A wrapper for NMEA_INFO which adds a few attributes that are cheap
 * to calculate.  They are managed by #BasicComputer inside
 * #MergeThread.
 */
struct MoreData : public NMEAInfo {
  /** Altitude used for navigation (GPS or Baro) */
  fixed NavAltitude;

  /** Energy height excess to slow to best glide speed */
  fixed EnergyHeight;

  /** Nav Altitude + Energy height (m) */
  fixed TEAltitude;

  /** GPS-based vario */
  fixed GPSVario;
  /** GPS-based vario including energy height */
  fixed GPSVarioTE;

  /**
   * Current vertical speed (total energy).  This is set to
   * TotalEnergyVario if available, and falls back to GPSVario.  It is
   * maintained by DeviceBlackboard::Vario().
   */
  fixed BruttoVario;

  void Reset();
};

#endif
