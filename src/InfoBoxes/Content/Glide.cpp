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

#include "InfoBoxes/Content/Glide.hpp"

#include "InfoBoxes/InfoBoxWindow.hpp"
#include "Interface.hpp"

#include <tchar.h>
#include <stdio.h>

void
InfoBoxContentLDInstant::Update(InfoBoxWindow &infobox)
{
  if (XCSoarInterface::Calculated().ld == fixed(999)) {
    infobox.SetInvalid();
    return;
  }

  // Set Value
  SetValueFromFixed(infobox, _T("%2.0f"), XCSoarInterface::Calculated().ld);
}

void
InfoBoxContentLDCruise::Update(InfoBoxWindow &infobox)
{
  if (XCSoarInterface::Calculated().cruise_ld == fixed(999)) {
    infobox.SetInvalid();
    return;
  }

  // Set Value
  SetValueFromFixed(infobox, _T("%2.0f"), XCSoarInterface::Calculated().cruise_ld);
}

void
InfoBoxContentLDAvg::Update(InfoBoxWindow &infobox)
{
  if (XCSoarInterface::Calculated().average_ld == 0) {
    infobox.SetInvalid();
    return;
  }

  // Set Value
  if (XCSoarInterface::Calculated().average_ld < 0)
    infobox.SetValue(_T("^^^"));
  else if (XCSoarInterface::Calculated().average_ld >= 999)
    infobox.SetValue(_T("+++"));
  else
    SetValueFromFixed(infobox, _T("%2.0f"),
                      fixed(XCSoarInterface::Calculated().average_ld));
}

void
InfoBoxContentLDVario::Update(InfoBoxWindow &infobox)
{
  if (XCSoarInterface::Calculated().ld_vario == fixed(999) ||
      !XCSoarInterface::Basic().total_energy_vario_available ||
      !XCSoarInterface::Basic().airspeed_available) {
    infobox.SetInvalid();
    return;
  }

  // Set Value
  SetValueFromFixed(infobox, _T("%2.0f"), XCSoarInterface::Calculated().ld_vario);
}
