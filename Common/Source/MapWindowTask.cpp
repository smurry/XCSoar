/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000 - 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>

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
#include "MapWindow.h"
#include "XCSoar.h"
#include "Protection.hpp"
#include "Task.h"
#include "SettingsTask.hpp"
#include "WayPoint.hpp"
#include "Screen/Util.hpp"
#include "Screen/Graphics.hpp"
#include "Screen/Fonts.hpp"
#include "Screen/LabelBlock.hpp"
#include "InfoBoxLayout.h"
#include "AATDistance.h"
#include "Math/FastMath.h"
#include "Math/Screen.hpp"
#include "Math/Earth.hpp"
#include "Compatibility/gdi.h"
#include "options.h" /* for IBLSCALE() */
#include "WayPointList.hpp"
#include "Components.hpp"

#include <math.h>

#include "TaskVisitor.hpp"

//////////////////////

class DrawAbortedTaskVisitor: 
  public AbsoluteTaskPointVisitor 
{
public:
  DrawAbortedTaskVisitor(Canvas &_canvas, const POINT &_orig):
    canvas(&_canvas),orig(_orig) {}

  void visit_task_point_start(TASK_POINT &point, const unsigned index) 
  {
    canvas->line(way_points.get_calc(point.Index).Screen, orig);
  };
  void visit_task_point_intermediate(TASK_POINT &point, const unsigned index) 
  {
    visit_task_point_start(point,index);
  };
  void visit_task_point_final(TASK_POINT &point, const unsigned index) { 
    visit_task_point_start(point,index);
  };
private:
  Canvas *canvas;
  POINT orig;
};


void
MapWindow::DrawAbortedTask(Canvas &canvas)
{
  Pen dash_pen(Pen::DASH, IBLSCALE(1), MapGfx.TaskColor);
  canvas.select(dash_pen);
  DrawAbortedTaskVisitor dv(canvas, Orig_Aircraft);
  TaskScan::scan_point_forward(dv);
}

//////////////////



class DrawTaskVisitor: 
  public AbsoluteTaskPointVisitor,
  public AbsoluteTaskLegVisitor
{
public:
  DrawTaskVisitor(MapWindow &_map_window, 
		  Canvas &_canvas,
		  POINT &_orig,
		  TaskScreen_t &_task_screen,
		  StartScreen_t &_start_screen):
    map_window(&_map_window),
    canvas(&_canvas),
    orig(_orig),
    task_screen(&_task_screen),
    start_screen(&_start_screen),
    pent1(Pen::SOLID, IBLSCALE(1), MapGfx.TaskColor),
    penb2(Pen::SOLID, IBLSCALE(2), Color(0,0,255)),
    dash_pen3(Pen::DASH, IBLSCALE(3), MapGfx.TaskColor),
    dash_pen5(Pen::DASH, IBLSCALE(5), MapGfx.TaskColor),
    dash_pen2(Pen::DASH, IBLSCALE(2), Color(127, 127, 127))
  {

  }

  void visit_start_point(START_POINT &point, const unsigned index) 
  {
    DrawStartSector((*start_screen)[index].SectorStart, 
                    (*start_screen)[index].SectorEnd, point.Index); 
  };
  void visit_task_point_start(TASK_POINT &point, const unsigned index) 
  {
    DrawStartSector((*task_screen)[index].SectorStart, 
                    (*task_screen)[index].SectorEnd, point.Index); 
  };


  void visit_task_point_intermediate_aat(TASK_POINT &point, const unsigned i) 
  {
    // JMW added iso lines
    if (((int)i==ActiveTaskPoint) 
	|| (map_window->SettingsMap().TargetPan 
	    && ((int)i==map_window->SettingsMap().TargetPanIndex))) {
      // JMW 20080616 flash arc line if very close to target
      static bool flip = false;
      
      if (map_window->Calculated().WaypointDistance<200.0) { // JMW hardcoded AATCloseDistance
	flip = !flip;
      } else {
	flip = true;
      }
      if (flip) {
	for (int j=0; j<MAXISOLINES-1; j++) {
	  if (task_stats[i].IsoLine_valid[j]
	      && task_stats[i].IsoLine_valid[j+1]) {
	    canvas->select(penb2);
	    canvas->line((*task_screen)[i].IsoLine_Screen[j],
			 (*task_screen)[i].IsoLine_Screen[j + 1]);
	  }
	}
      }
    }
  }

  void visit_task_point_intermediate_non_aat(TASK_POINT &point, const unsigned i) 
  {
    const POINT &wp = way_points.get_calc(point.Index).Screen;

    canvas->select(dash_pen2);
    canvas->two_lines((*task_screen)[i].SectorStart, 
		      wp,
		     (*task_screen)[i].SectorEnd);
    
    canvas->hollow_brush();
    canvas->black_pen();

    if (SectorType== 0) {
      unsigned tmp = map_window->DistanceMetersToScreen(SectorRadius);
      canvas->circle(wp.x, wp.y, tmp);
    } else if (SectorType==1) {
      unsigned tmp = map_window->DistanceMetersToScreen(SectorRadius);
      canvas->segment(wp.x, wp.y, tmp, 
		      map_window->GetMapRect(),
		      task_points[i].AATStartRadial-map_window->GetDisplayAngle(),
		      task_points[i].AATFinishRadial-map_window->GetDisplayAngle());
    } else if(SectorType== 2) {
      unsigned tmp;
      tmp = map_window->DistanceMetersToScreen(500);
      canvas->circle(wp.x, wp.y, tmp);
      
      tmp = map_window->DistanceMetersToScreen(10000);
      canvas->segment(wp.x, wp.y, tmp, map_window->GetMapRect(),
		      task_points[i].AATStartRadial-map_window->GetDisplayAngle(),
		      task_points[i].AATFinishRadial-map_window->GetDisplayAngle());
    }
  }


  void visit_task_point_intermediate(TASK_POINT &point, const unsigned index) 
  {
    if(AATEnabled) {
      visit_task_point_intermediate_aat(point, index);
    } else {
      visit_task_point_intermediate_non_aat(point, index);
    }
  };

  //////

  void visit_task_point_final(TASK_POINT &point, const unsigned index) { 

    if (ActiveTaskPoint>1) {
      // only draw finish line when past the first
      // waypoint.
      const POINT &wp = way_points.get_calc(point.Index).Screen;

      if(FinishLine) {
	canvas->select(dash_pen5);
	canvas->two_lines((*task_screen)[index].SectorStart, 
			  wp,
			  (*task_screen)[index].SectorEnd);
	canvas->select(MapGfx.hpStartFinishThin);
	canvas->two_lines((*task_screen)[index].SectorStart, 
			  wp,
			  (*task_screen)[index].SectorEnd);
      } else {
	unsigned tmp = map_window->DistanceMetersToScreen(FinishRadius);
	canvas->hollow_brush();
	canvas->select(MapGfx.hpStartFinishThick);
	canvas->circle(wp.x, wp.y, tmp);
	canvas->select(MapGfx.hpStartFinishThin);
	canvas->circle(wp.x, wp.y, tmp); 
      }
    }
  };

  void visit_leg_multistart(START_POINT &start, const unsigned index0, TASK_POINT &point) 
  {
    // nothing to draw
  };
  void visit_leg_intermediate(TASK_POINT &point0, const unsigned index0,
			      TASK_POINT &point1, const unsigned index1) 
  {
    visit_leg_final(point0, index0, point1, index1);
  };
  void visit_leg_final(TASK_POINT &point0, const unsigned index0,
		       TASK_POINT &point1, const unsigned index1) 
  {
    bool is_first = (point0.Index < point1.Index);
    int imin = min(point0.Index,point1.Index);
    int imax = max(point0.Index,point1.Index);
    // JMW AAT!
    double bearing = point0.OutBound;
    POINT sct1, sct2;
    
    canvas->select(dash_pen3);
    
    if (AATEnabled && !map_window->SettingsMap().TargetPan) {
      map_window->LonLat2Screen(task_stats[index0].AATTargetLocation,
		    sct1);
      map_window->LonLat2Screen(task_stats[index1].AATTargetLocation,
		    sct2);
      bearing = Bearing(task_stats[index0].AATTargetLocation,
                        task_stats[index1].AATTargetLocation);
      
      // draw nominal track line
      canvas->line(way_points.get_calc(imin).Screen,
                   way_points.get_calc(imax).Screen);
    } else {
      sct1 = way_points.get_calc(point0.Index).Screen;
      sct2 = way_points.get_calc(point1.Index).Screen;
    }
    
    if (is_first) {
      canvas->line(sct1, sct2);
    } else {
      canvas->line(sct2, sct1);
    }
    
    // draw small arrow along task direction
    POINT p_p;
    POINT Arrow[3] = { {6,6}, {-6,6}, {0,0} };
    ScreenClosestPoint(sct1, sct2, orig, &p_p, IBLSCALE(25));
    PolygonRotateShift(Arrow, 2, p_p.x, p_p.y,
		       bearing-map_window->GetDisplayAngle());
    Arrow[2] = Arrow[1];
    Arrow[1] = p_p;
    
    canvas->select(pent1);
    canvas->polyline(Arrow, 3);
  };

private:
  MapWindow *map_window;
  Canvas* canvas;
  const POINT orig;
  const TaskScreen_t *task_screen;
  const StartScreen_t *start_screen;
  const Pen pent1;
  const Pen penb2;
  const Pen dash_pen3;
  const Pen dash_pen5;
  const Pen dash_pen2;

  void DrawStartSector(const POINT &Start, 
		       const POINT &End, const unsigned Index)
  {
    if (ActiveTaskPoint>=2) {
      // don't draw if on second leg or beyond
      return;
    }

    const WPCALC &wpcalc = way_points.get_calc(Index);
    if(StartLine) {

      canvas->select(MapGfx.hpStartFinishThick);
      canvas->line(wpcalc.Screen, Start);
      canvas->line(wpcalc.Screen, End);
      canvas->select(MapGfx.hpStartFinishThin);
      canvas->line(wpcalc.Screen, Start);
      canvas->line(wpcalc.Screen, End);
    } else {
      unsigned tmp = map_window->DistanceMetersToScreen(StartRadius);
      canvas->hollow_brush();
      canvas->select(MapGfx.hpStartFinishThick);
      canvas->circle(wpcalc.Screen.x, wpcalc.Screen.y, tmp);
      canvas->select(MapGfx.hpStartFinishThin);
      canvas->circle(wpcalc.Screen.x, wpcalc.Screen.y, tmp);
    }
  };

};


void MapWindow::DrawTask(Canvas &canvas, RECT rc)
{
  DrawTaskVisitor dv(*this, canvas, Orig_Aircraft, task_screen, task_start_screen);
  TaskScan::scan_leg_forward(dv);
  TaskScan::scan_point_forward(dv);
}

///////


void MapWindow::DrawTaskAAT(Canvas &canvas, const RECT rc, Canvas &buffer)
{
  int i;
  unsigned tmp;

  if (!AATEnabled) return;

  ScopeLock scopeLock(mutexTaskData); // protect from extrnal task changes

  Color whitecolor = Color(0xff,0xff, 0xff);
  buffer.set_text_color(whitecolor);
  buffer.white_pen();
  buffer.white_brush();
  buffer.rectangle(rc.left, rc.top, rc.right, rc.bottom);

  for (i = MAXTASKPOINTS - 2; i > 0; i--) {
    if(task.ValidTaskPoint(i) && task.ValidTaskPoint(i+1)) {
      const WPCALC &wpcalc = way_points.get_calc(task_points[i].Index);

      if(task_points[i].AATType == CIRCLE) {
        tmp = DistanceMetersToScreen(task_points[i].AATCircleRadius);

        // this color is used as the black bit
        buffer.set_text_color(MapGfx.Colours[iAirspaceColour[AATASK]]);

        // this color is the transparent bit
        buffer.set_background_color(whitecolor);

        if (i<ActiveTaskPoint) {
          buffer.hollow_brush();
        } else {
          buffer.select(MapGfx.hAirspaceBrushes[iAirspaceBrush[AATASK]]);
        }
        buffer.black_pen();

        buffer.circle(wpcalc.Screen.x, wpcalc.Screen.y, tmp);
      } else {

        // this color is used as the black bit
        buffer.set_text_color(MapGfx.Colours[iAirspaceColour[AATASK]]);

        // this color is the transparent bit
        buffer.set_background_color(whitecolor);

        if (i<ActiveTaskPoint) {
          buffer.hollow_brush();
        } else {
          buffer.select(MapGfx.hAirspaceBrushes[iAirspaceBrush[AATASK]]);
        }
        buffer.black_pen();

        tmp = DistanceMetersToScreen(task_points[i].AATSectorRadius);

        buffer.segment(wpcalc.Screen.x,
                       wpcalc.Screen.y, tmp, rc,
                       task_points[i].AATStartRadial-DisplayAngle,
                       task_points[i].AATFinishRadial-DisplayAngle);

        buffer.two_lines(task_screen[i].AATStart, wpcalc.Screen,
                         task_screen[i].AATFinish);
      }

    }
  }

  canvas.copy_transparent_white(buffer, rc);
}


void MapWindow::DrawBearing(Canvas &canvas, int bBearingValid)
{ /* RLD bearing is invalid if GPS not connected and in non-sim mode,
   but we can still draw targets */

  if (!task.Valid()) {
    return;
  }

  mutexTaskData.Lock();  // protect from extrnal task changes

  GEOPOINT start = Basic().Location;
  GEOPOINT target;

  if (AATEnabled && (ActiveTaskPoint>0) 
      && task.ValidTaskPoint(ActiveTaskPoint+1)) {
    target = task_stats[ActiveTaskPoint].AATTargetLocation;
  } else {
    target = way_points.get(task_points[ActiveTaskPoint].Index).Location;
  }
  mutexTaskData.Unlock();
  if (bBearingValid) {
    DrawGreatCircle(canvas, start, // RLD skip if bearing invalid
                    target);       // RLD bc Lat/Lon invalid

    if (SettingsMap().TargetPan) {
      // Draw all of task if in target pan mode
      start = target;

      ScopeLock scopeLock(mutexTaskData);

      for (int i=ActiveTaskPoint+1; i<MAXTASKPOINTS; i++) {
        if (task.ValidTaskPoint(i)) {

          if (AATEnabled && task.ValidTaskPoint(i+1)) {
            target = task_stats[i].AATTargetLocation;
          } else {
            target = way_points.get(task_points[i].Index).Location;
          }

          DrawGreatCircle(canvas, start, target);

          start = target;
        }
      }
    } // TargetPan
  } // bearing valid

  // JMW draw symbol at target, makes it easier to see
  // RLD always draw all targets ahead so visible in pan mode
  // JMW ok then, only if in pan mode
  if (AATEnabled) {
    ScopeLock scopeLock(mutexTaskData);

    for (int i=ActiveTaskPoint; i<MAXTASKPOINTS; i++) {
      // RLD skip invalid targets and targets at start and finish
      if((i>0) && task.ValidTaskPoint(i) && task.ValidTaskPoint(i+1)) {
        if ((i== ActiveTaskPoint)
	    || ((SettingsMap().EnablePan || SettingsMap().TargetPan) && (i>ActiveTaskPoint))) {
	  draw_masked_bitmap_if_visible(canvas, MapGfx.hBmpTarget, 
					task_stats[i].AATTargetLocation,
					10, 10);
        }
      }
    }
  }

}



void
MapWindow::DrawOffTrackIndicator(Canvas &canvas)
{
  if ((ActiveTaskPoint<=0) || !task.Valid()) {
    return;
  }
  if (fabs(Basic().TrackBearing-Calculated().WaypointBearing)<10) {
    // insignificant error
    return;
  }
  if (Calculated().Circling || task.TaskIsTemporary() || 
      SettingsMap().TargetPan) {
    // don't display in various modes
    return;
  }

  double distance_max = min(Calculated().WaypointDistance,
			    GetScreenDistanceMeters()*0.7);
  if (distance_max < 5000.0) {
    // too short to bother
    return;
  }

  mutexTaskData.Lock();  // protect from extrnal task changes

  GEOPOINT start = Basic().Location;
  GEOPOINT target;
  GEOPOINT dloc;

  if (AATEnabled 
      && task.ValidTaskPoint(ActiveTaskPoint+1) 
      && task.ValidTaskPoint(ActiveTaskPoint)) {
    target = task_stats[ActiveTaskPoint].AATTargetLocation;
  } else {
    target = way_points.get(task_points[ActiveTaskPoint].Index).Location;
  }
  mutexTaskData.Unlock();

  canvas.select(TitleWindowFont);
  canvas.set_text_color(Color(0x0, 0x0, 0x0));

  int ilast = 0;
  for (double d=0.25; d<=1.0; d+= 0.25) {
    FindLatitudeLongitude(start, 
			  Basic().TrackBearing,
			  distance_max*d,
			  &dloc);

    double distance0 = Distance(start, dloc);
    double distance1 = Distance(dloc, target);
    double distance = (distance0+distance1)/Calculated().WaypointDistance;
    int idist = iround((distance-1.0)*100);

    if ((idist != ilast) && (idist>0) && (idist<1000)) {

      TCHAR Buffer[5];
      _stprintf(Buffer, TEXT("%d"), idist);
      POINT sc;
      RECT brect;
      LonLat2Screen(dloc, sc);
      SIZE tsize = canvas.text_size(Buffer);

      brect.left = sc.x-4;
      brect.right = brect.left+tsize.cx+4;
      brect.top = sc.y-4;
      brect.bottom = brect.top+tsize.cy+4;

      if (label_block.check(brect)) {
        canvas.text(sc.x - tsize.cx / 2, sc.y - tsize.cy / 2, Buffer);
	ilast = idist;
      }
    }
  }
}



void
MapWindow::DrawProjectedTrack(Canvas &canvas)
{
  if ((ActiveTaskPoint<=0) || !task.Valid() || !AATEnabled) {
    return;
  }
  if (Calculated().Circling || task.TaskIsTemporary()) {
    // don't display in various modes
    return;
  }

  // TODO feature: maybe have this work even if no task?
  // TODO feature: draw this also when in target pan mode

  mutexTaskData.Lock();  // protect from extrnal task changes

  GEOPOINT start = Basic().Location;
  GEOPOINT previous_loc;
  unsigned previous_point = max(0,ActiveTaskPoint-1);
  if (AATEnabled) {
    previous_loc = task_stats[previous_point].AATTargetLocation;
  } else {
    previous_loc = way_points.get(task_points[previous_point].Index).Location;
  }
  mutexTaskData.Unlock();

  double distance_from_previous, bearing;
  DistanceBearing(previous_loc, start,
		  &distance_from_previous,
		  &bearing);

  if (distance_from_previous < 100.0) {
    bearing = Basic().TrackBearing;
    // too short to have valid data
  }
  POINT pt[2] = {{0,-75},{0,-400}};
  if (SettingsMap().TargetPan) {
    double screen_range = GetScreenDistanceMeters();
    double f_low = 0.4;
    double f_high = 1.5;
    screen_range = max(screen_range, Calculated().WaypointDistance);
    
    GEOPOINT p1, p2;
    FindLatitudeLongitude(start, 
			  bearing, f_low*screen_range,
			  &p1);
    FindLatitudeLongitude(start,
			  bearing, f_high*screen_range,
			  &p2);
    LonLat2Screen(p1, pt[0]);
    LonLat2Screen(p2, pt[1]);
  } else if (fabs(bearing-Calculated().WaypointBearing)<10) {
    // too small an error to bother
    return;
  } else {
    pt[1].y = (long)(-max(MapRectBig.right-MapRectBig.left,
			  MapRectBig.bottom-MapRectBig.top)*1.2);
    PolygonRotateShift(pt, 2, Orig_Aircraft.x, Orig_Aircraft.y,
		       bearing-DisplayAngle);
  }

  Pen dash_pen(Pen::DASH, IBLSCALE(2), Color(0, 0, 0));
  canvas.select(dash_pen);
  canvas.line(pt[0], pt[1]);
}


class ScreenPositionsTaskVisitor:
  public AbsoluteTaskPointVisitor {
public:
  ScreenPositionsTaskVisitor(MapWindow& _map,
			     TaskScreen_t &_task_screen,
			     StartScreen_t &_start_screen):
    map(&_map),
    task_screen(&_task_screen),
    start_screen(&_start_screen)
  {}
  void visit_start_point(START_POINT &point, const unsigned i) 
  { 
    map->LonLat2Screen(point.SectorEnd,
		       (*task_screen)[i].SectorEnd);
    map->LonLat2Screen(point.SectorStart,
		       (*task_screen)[i].SectorStart);

  };
  void visit_task_point_start(TASK_POINT &point, const unsigned i) 
  { 
    if (AATEnabled) {
      map->LonLat2Screen(task_stats[i].AATTargetLocation, 
			 (*task_screen)[i].Target);
    }
    map->LonLat2Screen(point.SectorEnd, 
		       (*task_screen)[i].SectorEnd);
    map->LonLat2Screen(point.SectorStart, 
		       (*task_screen)[i].SectorStart);
    if((AATEnabled) && (point.AATType == SECTOR)) {
      map->LonLat2Screen(point.AATStart, 
			 (*task_screen)[i].AATStart);
      map->LonLat2Screen(point.AATFinish, 
			 (*task_screen)[i].AATFinish);
    }

  };
  void visit_task_point_intermediate(TASK_POINT &point, const unsigned i) 
  { 
    visit_task_point_start(point, i);
    if (AATEnabled && (((int)i==ActiveTaskPoint) ||
		       (map->SettingsMap().TargetPan 
			&& ((int)i==map->SettingsMap().TargetPanIndex)))) {
      
      for (int j=0; j<MAXISOLINES; j++) {
	if (task_stats[i].IsoLine_valid[j]) {
	  map->LonLat2Screen(task_stats[i].IsoLine_Location[j],
			     (*task_screen)[i].IsoLine_Screen[j]);
	}
      }
    }
  };
  void visit_task_point_final(TASK_POINT &point, const unsigned i) 
  { 
    visit_task_point_start(point, i);
  };
private:
  MapWindow* map;
  TaskScreen_t *task_screen;
  StartScreen_t *start_screen;
};



void MapWindow::CalculateScreenPositionsTask() {

  ScreenPositionsTaskVisitor sv(*this, task_screen, task_start_screen);
  TaskScan::scan_point_forward(sv);
}

