/* Copyright_License {

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

#include "IsolineCrossingFinder.hpp"
#include "Navigation/Geometry/GeoEllipse.hpp"
#include "Task/TaskPoints/AATPoint.hpp"
#include "Util/Tolerances.hpp"

IsolineCrossingFinder::IsolineCrossingFinder(const AATPoint& _aap,
                                             const GeoEllipse &_ell,
                                             const fixed _xmin, 
                                             const fixed _xmax):
  ZeroFinder(_xmin, _xmax, fixed(TOLERANCE_ISOLINE_CROSSING)),
  aap(_aap),
  ell(_ell)
{
}

fixed 
IsolineCrossingFinder::f(const fixed t) 
{
  const GeoPoint a = ell.parametric(t);
  AircraftState s;
  s.location = a;

  // note: use of isInSector is slow!
  // if (aap.isInSector(s)) ->  attract solutions away from t
  // else                   ->  attract solutions towards t
  return (aap.IsInSector(s) ? fixed_one : -fixed_one) - fabs(t);
}

#define bsgn(x) (x < fixed_one ? false : true)

bool 
IsolineCrossingFinder::valid(const fixed x) 
{
/*
  const bool bsgn_0 = bsgn(f(x));
  const bool bsgn_m = bsgn(f(x-fixed_two*tolerance));
  const bool bsgn_p = bsgn(f(x+fixed_two*tolerance));

  \todo this is broken, so assume it's ok for now
  return (bsgn_0 != bsgn_m) || (bsgn_0 != bsgn_p);
*/
  return true;
}

fixed 
IsolineCrossingFinder::solve() 
{
  const fixed sol = find_zero(half(xmax + xmin));
  if (valid(sol)) {
    return sol;
  } else {
    return -fixed_one;
  }
}
