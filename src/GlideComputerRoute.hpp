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

#ifndef XCSOAR_GLIDE_COMPUTER_ROUTE_HPP
#define XCSOAR_GLIDE_COMPUTER_ROUTE_HPP

#include "GPSClock.hpp"

struct MoreData;
struct DerivedInfo;
struct SETTINGS_COMPUTER;
class ProtectedTaskManager;
class RasterTerrain;

class GlideComputerRoute {
  ProtectedTaskManager &m_task;

  GPSClock route_clock;
  GPSClock reach_clock;

  const RasterTerrain *terrain;

public:
  GlideComputerRoute(ProtectedTaskManager& task);

protected:
  void ResetFlight();
  void ProcessRoute(const MoreData &basic, DerivedInfo &calculated,
                    const DerivedInfo &last_calculated,
                    const SETTINGS_COMPUTER &settings_computer);

  void set_terrain(const RasterTerrain* _terrain);

private:
  void TerrainWarning(const MoreData &basic,
                      DerivedInfo &calculated,
                      const DerivedInfo &last_calculated);
  void Reach(const MoreData &basic, DerivedInfo &calculated,
             const SETTINGS_COMPUTER &settings_computer);
};

#endif