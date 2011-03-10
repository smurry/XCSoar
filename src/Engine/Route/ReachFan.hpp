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
#ifndef REACHFAN_HPP
#define REACHFAN_HPP

#include "Navigation/Flat/FlatGeoPoint.hpp"
#include "Navigation/Flat/FlatBoundingBox.hpp"
#include <vector>

class RoutePolars;
class RasterMap;
class TaskProjection;
struct GeoBounds;
struct RouteLink;

struct ReachFanParms;

class FlatTriangleFan {
protected:
  std::vector<FlatGeoPoint> vs;
  FlatBoundingBox bb_self;
  short height;

public:
  FlatTriangleFan():
    bb_self(FlatGeoPoint(0,0)), height(0) {};

  friend class PrintHelper;

  virtual void calc_bb();

  void add_point(const FlatGeoPoint &p);

  bool is_inside(const FlatGeoPoint &p) const;

  virtual void clear() {
    vs.clear();
  }
  bool empty() const {
    return vs.empty();
  }
  short get_height() const {
    return height;
  }
};

class TriangleFanVisitor;

class FlatTriangleFanTree: public FlatTriangleFan {
protected:
  FlatBoundingBox bb_children;
  std::vector<FlatTriangleFanTree> children;
  unsigned depth;

public:
  friend class PrintHelper;

  FlatTriangleFanTree(const int _depth=0):
    FlatTriangleFan(),
    bb_children(FlatGeoPoint(0,0)),
    depth(_depth) {};

  virtual void clear() {
    FlatTriangleFan::clear();
    children.clear();
  }

  virtual void calc_bb();

  bool is_inside_tree(const FlatGeoPoint &p, const bool include_children=true) const;

  void fill_reach(const AFlatGeoPoint &origin, const ReachFanParms& parms);

  void fill_reach(const AFlatGeoPoint &origin,
                  const int index_low, const int index_high,
                  const ReachFanParms& parms);

  bool check_gap(const AFlatGeoPoint& n,
                 const RouteLink& e_1,
                 const RouteLink& e_2,
                 const ReachFanParms& parms);

  bool find_positive_arrival(const FlatGeoPoint& n,
                             const ReachFanParms& parms,
                             short& arrival_height) const;

  void accept_in_range(const FlatBoundingBox& bb,
                       const TaskProjection& task_proj,
                       TriangleFanVisitor& visitor) const;
};

class TriangleFanVisitor {
public:
  virtual void start_fan() = 0;
  virtual void add_point(const GeoPoint& p) = 0;
  virtual void end_fan() = 0;
};

#include "AbstractReach.hpp"

class ReachFan: public AbstractReach {
protected:
  FlatTriangleFanTree root;

public:
  ReachFan():AbstractReach() {};

  friend class PrintHelper;

  virtual void reset();

  virtual bool solve(const AGeoPoint origin,
                     const RoutePolars &rpolars,
                     const RasterMap* terrain);

  virtual bool find_positive_arrival(const AGeoPoint dest,
                                     const RoutePolars &rpolars,
                                     short& arrival_height) const;

  virtual bool is_inside(const GeoPoint origin, const bool turning) const;

  void accept_in_range(const GeoBounds& bounds,
                       TriangleFanVisitor& visitor) const;
};

#endif