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

#ifndef SCOREDTASKPOINT_HPP
#define SCOREDTASKPOINT_HPP

#include "SampledTaskPoint.hpp"
#include "Compiler.h"

/**
 * Abstract specialisation of SampledTaskPoint to manage scoring
 * of progress along a task.  To do this, this class keeps track
 * of the aircraft state at entry and exit of the observation zone,
 * and provides methods to retrieve various reference locations used
 * in scoring calculations.
 *
 * \todo 
 * - better documentation of this class!
 */
class ScoredTaskPoint:
  public SampledTaskPoint
{
  AircraftState state_entered;
  AircraftState state_exited;

public:
  /**
   * Constructor.  Clears entry/exit states on instantiation.
   *
   * @param wp Waypoint associated with the task point
   * @param tb Task Behaviour defining options (esp safety heights)
   * @param b_scored Whether distance within OZ is scored
   *
   * @return Partially initialised object
   */
  ScoredTaskPoint(Type _type, const Waypoint &wp, bool b_scored);
  virtual ~ScoredTaskPoint() {}

  /** Reset the task (as if never flown) */
  virtual void Reset();

  /** 
   * Test whether aircraft has entered the OZ
   * 
   * @return True if aircraft has entered the OZ
   */
  bool HasEntered() const {
    return state_entered.time > fixed_zero;
  }

  /**
   * Test whether aircraft has exited the OZ
   *
   * @return True if aircraft has exited the OZ
   */
  bool HasExited() const {
    return state_exited.time > fixed_zero;
  }

  /**
   * Get entry state of aircraft
   *
   * @return State on entry
   */
  const AircraftState &GetEnteredState() const {
    return state_entered;
  }

  /**
   * Test whether aircraft has entered observation zone and
   * was previously outside; records this transition.
   *
   * @param ref_now State current
   * @param ref_last State at last sample
   *
   * @return True if observation zone is entered now
   */
  bool TransitionEnter(const AircraftState &ref_now,
                       const AircraftState &ref_last);

  /**
   * Test whether aircraft has exited observation zone and
   * was previously inside; records this transition.
   *
   * @param ref_now State current
   * @param ref_last State at last sample
   *
   * @return True if observation zone is exited now
   */
  bool TransitionExit(const AircraftState &ref_now,
                      const AircraftState &ref_last,
                      const TaskProjection &projection);

  /** Retrieve location to be used for the scored task. */
  gcc_pure
  const GeoPoint &GetLocationScored() const;

  /**
   * Retrieve location to be used for the task already travelled.
   * This is always the scored best location for prior-active task points.
   */
  gcc_pure
  const GeoPoint &GetLocationTravelled() const;

  /**
   * Retrieve location to be used for remaining task.
   * This is either the reference location or target for post-active,
   * or scored best location for prior-active task points.
   */
  gcc_pure
  virtual const GeoPoint &GetLocationRemaining() const;

private:
  /**
   * Set OZ entry state
   *
   * @param state State at entry
   */
  void SetStateEntered(const AircraftState &state) {
    state_entered = state;
  }

  /**
   * Set OZ exit state
   *
   * @param state State at exit
   */
  void SetStateExited(const AircraftState &state) {
    state_exited = state;
  }

  gcc_pure
  virtual bool EntryPrecondition() const {
    return true;
  }

  gcc_pure
  virtual bool ScoreLastExit() const {
    return false;
  }

  gcc_pure
  virtual bool ScoreFirstEntry() const {
    return false;
  }
};

#endif
