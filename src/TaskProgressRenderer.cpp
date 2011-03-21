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

#include "TaskProgressRenderer.hpp"
#include "Engine/Task/TaskStats/TaskSummary.hpp"
#include "Screen/Canvas.hpp"
#include "Screen/Graphics.hpp"
#include "Appearance.hpp"
#include <algorithm>
#include "Screen/Layout.hpp"
#include "Screen/Icon.hpp"

void 
TaskProgressRenderer::DrawTaskProgress(const TaskSummary& summary,
                                       Canvas &canvas, 
                                       const RECT &rc)
{
  const int radius = std::min(rc.right-rc.left, rc.bottom-rc.top)/2-IBLSCALE(3);
  RasterPoint center;
  center.x = (rc.left+rc.right)/2;
  center.y = (rc.bottom+rc.top)/2;

  const fixed sweep = fixed_two_pi*fixed(0.9);
  Pen pen_f(1, Appearance.InverseInfoBox? Color::WHITE: Color::BLACK);

  if (summary.p_remaining<fixed(0.99)) {
    canvas.select(Graphics::hbWind);
    canvas.null_pen();
    canvas.segment(center.x, center.y, radius, Angle::radians(fixed_zero),
                   Angle::radians(sweep*(fixed_one-summary.p_remaining)));
  }

  canvas.select(pen_f);
  canvas.hollow_brush();
  canvas.circle(center.x, center.y, radius);

  unsigned i=0;
  canvas.select(pen_f);
  for (TaskSummary::TaskSummaryPointVector::const_iterator it= summary.pts.begin();
       it != summary.pts.end(); ++it, ++i) {
    Angle a= Angle::radians(it->p*sweep);
    int x = center.x+(int)(radius*a.fastsine());
    int y = center.y-(int)(radius*a.fastcosine());
    int w;
    if (i== summary.active) {
      if (it->achieved) {
        canvas.select(Graphics::hbGreen);
      } else {
        canvas.select(Graphics::hbOrange);
      }
      w = IBLSCALE(3);
    } else if (i< summary.active) {
      if (it->achieved) {
        canvas.select(Graphics::hbGreen);
      } else {
        canvas.select(Graphics::hbNotReachableTerrain);
      }
      w = IBLSCALE(2);
    } else {
      if (it->achieved) {
        canvas.select(Graphics::hbGreen);
      } else {
        canvas.select(Graphics::hbLightGray);
      }
      w = IBLSCALE(1);
    }
    canvas.rectangle(x-w, y-w, x+w, y+w);
  }
}