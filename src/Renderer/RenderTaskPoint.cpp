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

#include "RenderTaskPoint.hpp"
#include "Screen/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "WindowProjection.hpp"
#include "Task/Tasks/BaseTask/UnorderedTaskPoint.hpp"
#include "Task/TaskPoints/AATIsolineSegment.hpp"
#include "Task/TaskPoints/StartPoint.hpp"
#include "Task/TaskPoints/ASTPoint.hpp"
#include "Task/TaskPoints/AATPoint.hpp"
#include "Task/TaskPoints/FinishPoint.hpp"
#include "Look/TaskLook.hpp"
#include "Math/Screen.hpp"
#include "RenderObservationZone.hpp"
#include "NMEA/Info.hpp"
#include "SettingsMap.hpp"
#include "Asset.hpp"

RenderTaskPoint::RenderTaskPoint(Canvas &_canvas, Canvas *_buffer,
                                 const WindowProjection &_projection,
                                 const SETTINGS_MAP &_settings_map,
                                 const TaskLook &_task_look,
                                 const TaskProjection &_task_projection,
                                 RenderObservationZone &_ozv,
                                 bool _draw_bearing,
                                 bool _draw_all,
                                 const GeoPoint &_location)
  :canvas(_canvas), buffer(_buffer), m_proj(_projection),
   map_canvas(_canvas, _projection,
              _projection.GetScreenBounds().scale(fixed(1.1))),
   settings_map(_settings_map),
   task_look(_task_look),
   task_projection(_task_projection),
   draw_bearing(_draw_bearing),
   draw_all(_draw_all),
   index(0),
   ozv(_ozv),
   active_index(0),
   layer(RENDER_TASK_OZ_SHADE),
   location(_location),
   mode_optional_start(false)
{
}

void 
RenderTaskPoint::DrawOrdered(const OrderedTaskPoint &tp)
{
  const bool visible = tp.boundingbox_overlaps(bb_screen);

  if (visible && (layer == RENDER_TASK_OZ_SHADE)) {
    // draw shaded part of observation zone
    DrawOZBackground(canvas, tp);
  }
  
  if (layer == RENDER_TASK_LEG) {
    if (index>0) {
      DrawTaskLine(last_point, tp.GetLocationRemaining());
    }
    last_point = tp.GetLocationRemaining();
  }
  
  if (visible && (layer == RENDER_TASK_OZ_OUTLINE)) {
    DrawOZForeground(tp);
  }
}

bool 
RenderTaskPoint::DoDrawTarget(const TaskPoint &tp) const
{
  if (!tp.HasTarget()) {
    return false;
  }
  return draw_all || PointCurrent();
}

void 
RenderTaskPoint::DrawBearing(const TaskPoint &tp) 
{
  if (!DoDrawBearing(tp)) 
    return;

  canvas.select(task_look.bearing_pen);
  map_canvas.offset_line(location, tp.GetLocationRemaining());
}

void 
RenderTaskPoint::DrawTarget(const TaskPoint &tp) 
{
  if (!DoDrawTarget(tp)) 
    return;
}

void 
RenderTaskPoint::DrawTaskLine(const GeoPoint &start, const GeoPoint &end)
{
  canvas.select(LegActive()
                ? task_look.leg_active_pen
                : task_look.leg_inactive_pen);
  canvas.background_transparent();
  map_canvas.line(start, end);
  canvas.background_opaque();
  
  // draw small arrow along task direction
  RasterPoint p_p;
  RasterPoint Arrow[3] = { {6,6}, {-6,6}, {0,0} };
  
  const RasterPoint p_start = m_proj.GeoToScreen(start);
  const RasterPoint p_end = m_proj.GeoToScreen(end);
  
  const Angle ang = Angle::radians(atan2(fixed(p_end.x - p_start.x),
                                         fixed(p_start.y - p_end.y))).as_bearing();

  ScreenClosestPoint(p_start, p_end, m_proj.GetScreenOrigin(), &p_p, Layout::Scale(25));
  PolygonRotateShift(Arrow, 2, p_p.x, p_p.y, ang);
  Arrow[2] = Arrow[1];
  Arrow[1] = p_p;
  
  canvas.select(task_look.arrow_pen);
  canvas.polyline(Arrow, 3);
}

void 
RenderTaskPoint::DrawIsoline(const AATPoint &tp)
{
  if (!tp.valid() || !DoDrawIsoline(tp))
    return;

  AATIsolineSegment seg(tp, task_projection);
  if (!seg.IsValid()) {
    return;
  }

  #define fixed_twentieth fixed(1.0 / 20.0)
  
  if (m_proj.GeoToScreenDistance(seg.Parametric(fixed_zero).
                                    distance(seg.Parametric(fixed_one)))>2) {
    
    RasterPoint screen[21];
    for (unsigned i = 0; i < 21; ++i) {
      fixed t = i * fixed_twentieth;
      GeoPoint ga = seg.Parametric(t);
      screen[i] = m_proj.GeoToScreen(ga);
    }

    canvas.select(task_look.isoline_pen);
    canvas.background_transparent();
    canvas.polyline(screen, 21);
    canvas.background_opaque();
  }
}

void 
RenderTaskPoint::DrawDeadzone(const AATPoint &tp)
{
  if (!DoDrawDeadzone(tp) || is_ancient_hardware()) {
    return;
  }
  /*
    canvas.set_text_color(Graphics::Colours[m_settings_map.
    iAirspaceColour[1]]);
    // get brush, can be solid or a 1bpp bitmap
    canvas.select(Graphics::hAirspaceBrushes[m_settings_map.
    iAirspaceBrush[1]]);
    */

  // erase where aircraft has been
  canvas.white_brush();
  canvas.white_pen();
  
  if (PointCurrent()) {
    // scoring deadzone should include the area to the next destination
    map_canvas.draw(tp.get_deadzone());
  } else {
    // scoring deadzone is just the samples convex hull
    map_canvas.draw(tp.GetSamplePoints());
  }
}

void 
RenderTaskPoint::DrawOZBackground(Canvas &canvas, const OrderedTaskPoint &tp)
{
  ozv.set_layer(RenderObservationZone::LAYER_SHADE);

  if (ozv.draw_style(canvas, settings_map.airspace, index - active_index)) {
    ozv.Draw(canvas, m_proj, *tp.get_oz());
    ozv.un_draw_style(canvas);
  }
}

void 
RenderTaskPoint::DrawOZForeground(const OrderedTaskPoint &tp)
{
  int offset = index - active_index;
  if (mode_optional_start) {
    offset = -1; // render optional starts as deactivated
  }
  ozv.set_layer(RenderObservationZone::LAYER_INACTIVE);
  if (ozv.draw_style(canvas, settings_map.airspace, offset)) {
    ozv.Draw(canvas, m_proj, *tp.get_oz());
    ozv.un_draw_style(canvas);
  }

  ozv.set_layer(RenderObservationZone::LAYER_ACTIVE);
  if (ozv.draw_style(canvas, settings_map.airspace, offset)) {
    ozv.Draw(canvas, m_proj, *tp.get_oz());
    ozv.un_draw_style(canvas);
  }
}

void
RenderTaskPoint::Draw(const TaskPoint &tp)
{
  const OrderedTaskPoint &otp = (const OrderedTaskPoint &)tp;
  const AATPoint &atp = (const AATPoint &)tp;

  switch (tp.GetType()) {
  case TaskPoint::UNORDERED:
    if (layer == RENDER_TASK_LEG)
      DrawTaskLine(location, tp.GetLocationRemaining());

    if (layer == RENDER_TASK_SYMBOLS)
      DrawBearing(tp);

    index++;
    break;

  case TaskPoint::START:
    index = 0;

    DrawOrdered(otp);
    if (layer == RENDER_TASK_SYMBOLS) {
      DrawBearing(tp);
      DrawTarget(tp);
    }

    break;

  case TaskPoint::AST:
    index++;

    DrawOrdered(otp);
    if (layer == RENDER_TASK_SYMBOLS) {
      DrawBearing(tp);
      DrawTarget(tp);
    }
    break;

  case TaskPoint::AAT:
    index++;

#ifndef ENABLE_OPENGL
    if (layer == RENDER_TASK_OZ_SHADE && buffer != NULL &&
        DoDrawDeadzone(tp) && !is_ancient_hardware()) {
      // Draw clear area on top indicating part of OZ already travelled in
      // This provides a simple and intuitive visual representation of
      // where in the OZ to go to increase scoring distance.

      if (!atp.boundingbox_overlaps(bb_screen))
        return;

      const SearchPointVector &dead_zone = PointCurrent()
        // scoring deadzone should include the area to the next destination
        ? atp.get_deadzone()
        // scoring deadzone is just the samples convex hull
        : atp.GetSamplePoints();

      /* need at least 3 points to draw the polygon */
      if (dead_zone.size() >= 3) {
        buffer->clear_white();

        /* draw the background shade into the buffer */
        DrawOZBackground(*buffer, atp);

        /* now erase the dead zone by drawing a white polygon over it */
        buffer->null_pen();
        buffer->white_brush();
        MapCanvas map_canvas(*buffer, m_proj,
                             m_proj.GetScreenBounds().scale(fixed(1.1)));
        map_canvas.draw(dead_zone);

        /* copy the result into the canvas */
        /* we use copy_and() here to simulate Canvas::mix_mask() */
        canvas.copy_and(*buffer);
        return;
      }
    }
#endif

    DrawOrdered(otp);
    if (layer == RENDER_TASK_OZ_SHADE) {
      // Draw clear area on top indicating part of OZ already travelled in
      // This provides a simple and intuitive visual representation of
      // where in the OZ to go to increase scoring distance.

      // DISABLED by Tobias.Bieniek@gmx.de
      // This code produced graphical bugs due to previously
      // modified code which should be fixed before re-enabling this call
      //draw_deadzone(tp);
    }

    if (layer == RENDER_TASK_SYMBOLS) {
      DrawIsoline(atp);
      DrawBearing(tp);
      DrawTarget(tp);
    }

    break;

  case TaskPoint::FINISH:
    index++;

    DrawOrdered(otp);
    if (layer == RENDER_TASK_SYMBOLS) {
      DrawBearing(tp);
      DrawTarget(tp);
    }
    break;

  case TaskPoint::ROUTE:
    /* unreachable */
    assert(false);
    break;
  }
}
