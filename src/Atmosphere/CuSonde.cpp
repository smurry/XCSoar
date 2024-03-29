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

#include "Atmosphere/CuSonde.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"

#include "Units/Units.hpp"

#include <math.h>
#include <stdlib.h> /* for abs() */
#include <algorithm>

/**
 * Dry adiabatic lapse rate (degrees C per meter)
 *
 * DALR = dT/dz = g/c_p =
 * @see http://en.wikipedia.org/wiki/Lapse_rate#Dry_adiabatic_lapse_rate
 * @see http://pds-atmospheres.nmsu.edu/education_and_outreach/encyclopedia/adiabatic_lapse_rate.htm
 */
#define DALR fixed(-0.00974)

/** ThermalIndex threshold in degrees C */
#define TITHRESHOLD fixed(-1.6)

using std::max;

unsigned short CuSonde::last_level=fixed_zero;
fixed CuSonde::thermalHeight = fixed_zero;
fixed CuSonde::cloudBase = fixed_zero;
fixed CuSonde::hGround = fixed_zero;
fixed CuSonde::maxGroundTemperature = fixed(25);
CuSonde::Level CuSonde::cslevels[NUM_LEVELS];

// TODO accuracy: recalculate thermal index etc if maxGroundTemp changes

/**
 * Sets the predicted maximum ground temperature to val
 * @param val New predicted maximum ground temperature in degrees C
 */
void
CuSonde::setForecastTemperature(fixed val)
{
  maxGroundTemperature = val;

  unsigned zlevel = 0;

  // set these to invalid, so old values must be overwritten
  cloudBase = fixed_minus_one;
  thermalHeight = fixed_minus_one;

  // iterate through all levels
  for (unsigned level = 0; level < NUM_LEVELS; level++) {
    // update the ThermalIndex for each level with
    // the new maxGroundTemperature
    cslevels[level].updateThermalIndex((unsigned short)level, false);

    // determine to which level measurements are available
    if (cslevels[level].nmeasurements)
      zlevel = level;

    if ((cslevels[level].nmeasurements == 0) && (zlevel))
      break;
  }

  // iterate through all levels with measurements
  for (unsigned level = 0; level <= zlevel; level++) {
    // calculate ThermalHeight
    findThermalHeight((unsigned short)level);
    // calculate CloudBase
    findCloudBase((unsigned short)level);
  }
}

/**
 * Adjusts the maximum ground temperature by delta
 * @param delta Degrees C to be added to the maximum ground temperature
 */
void
CuSonde::adjustForecastTemperature(fixed delta)
{
  setForecastTemperature(maxGroundTemperature + delta);
}

/**
 * Update the measurements if new level reached
 * @param basic NMEA_INFO for temperature and humidity
 */
void
CuSonde::updateMeasurements(const NMEAInfo &basic,
                            const DerivedInfo &calculated)
{
  // if (not flying) nothing to update...
  if (!calculated.flight.flying)
    return;

  // if (no temperature or humidity available) nothing to update...
  if (!basic.temperature_available || !basic.humidity_available)
    return;

  // find appropriate level
  unsigned short level = (unsigned short)((int)max(basic.GetAltitudeBaroPreferred(),
                                                   fixed(0.0))
                                          / HEIGHT_STEP);

  // if (level out of range) cancel update
  if (level >= NUM_LEVELS)
    return;

  // if (level skipped) cancel update
  if (abs(level - last_level) > 1) {
    last_level = level;
    return;
  }

  // if (no level transition yet) wait for transition
  if (abs(level - last_level) == 0)
    return;

  // calculate ground height
  hGround = calculated.altitude_agl;

  // if (going up)
  if (level > last_level) {
    // we round down (level) because of potential lag of temp sensor
    cslevels[level].updateTemps(basic.humidity,
        Units::ToUserUnit(basic.temperature, unGradCelcius));
    cslevels[level].updateThermalIndex(level);

    if (level > 0) {
      findThermalHeight((unsigned short)(level - 1));
      findCloudBase((unsigned short)(level - 1));
    }

  // if (going down)
  } else {
    // we round up (level+1) because of potential lag of temp sensor
    cslevels[level + 1].updateTemps(basic.humidity,
        Units::ToUserUnit(basic.temperature, unGradCelcius));
    cslevels[level + 1].updateThermalIndex((unsigned short)(level + 1));

    if (level < NUM_LEVELS - 1) {
      findThermalHeight(level);
      findCloudBase(level);
    }
  }

  last_level = level;
}

/**
 * Finds the estimated ThermalHeight based on the given level and the one
 * above
 * @param level Level used for calculation
 */
void
CuSonde::findThermalHeight(unsigned short level)
{
  if (cslevels[level + 1].nmeasurements == 0)
    return;
  if (cslevels[level].nmeasurements == 0)
    return;

  // Delta of ThermalIndex
  fixed dti = cslevels[level + 1].thermalIndex - cslevels[level].thermalIndex;

  // Reset estimated ThermalHeight
  cslevels[level].thermalHeight = fixed_minus_one;

  if (fabs(dti) < fixed(0.001))
    return;

  // ti = dlevel * dti + ti0;
  // (-1.6 - ti0)/dti = dlevel;

  fixed dlevel = (TITHRESHOLD - cslevels[level].thermalIndex) / dti;
  fixed dthermalheight = (fixed(level) + dlevel) * HEIGHT_STEP;

  if ((dlevel > fixed_one)
      && (level + 2u < NUM_LEVELS)
      && (cslevels[level + 2].nmeasurements > 0))
      // estimated point should be in next level.
      return;

  if (positive(dlevel)) {
    // set the level thermal height to the calculated value
    cslevels[level].thermalHeight = dthermalheight;

    // set the overall thermal height to the calculated value
    thermalHeight = dthermalheight;

#ifdef DEBUG_CUSONDE
    LogDebug(_T("%g # thermal height \r\n"), (double)thermalHeight);
#endif
  }
}

/**
 * Finds the estimated CloudBase based on the given level and the one
 * above
 * @param level Level used for calculation
 */
void
CuSonde::findCloudBase(unsigned short level)
{
  if (cslevels[level + 1].nmeasurements == 0)
    return;
  if (cslevels[level].nmeasurements == 0)
    return;

  fixed dti = (cslevels[level + 1].tempDry - cslevels[level + 1].dewpoint)
               - (cslevels[level].tempDry - cslevels[level].dewpoint);

  // Reset estimated CloudBase
  cslevels[level].cloudBase = fixed_minus_one;

  if (fabs(dti) < fixed(0.001))
    return;

  // ti = dlevel * dti + ti0;
  // (-3 - ti0)/dti = dlevel;

  fixed dlevel = -(cslevels[level].tempDry - cslevels[level].dewpoint) / dti;
  fixed dcloudbase = (fixed(level) + dlevel) * HEIGHT_STEP;

  if ((dlevel > fixed_one)
      && (level + 2u < NUM_LEVELS)
      && (cslevels[level + 2].nmeasurements > 0))
    // estimated point should be in next level.
    return;

  if (positive(dlevel)) {
    // set the level cloudbase to the calculated value
    cslevels[level].cloudBase = dcloudbase;

    // set the overall cloudbase to the calculated value
    cloudBase = dcloudbase;

#ifdef DEBUG_CUSONDE
    LogDebug(_T("%g # cloud base \r\n"), (double)cloudBase);
#endif
  }
}

/**
 * Calculates the dew point and saves the measurement
 * @param rh Humidity in percent
 * @param t Temperature in degrees C
 */
void
CuSonde::Level::updateTemps(fixed rh, fixed t)
{
  fixed logEx, adewpoint;

  logEx = fixed(7.5) * t / (fixed(237.3) + t) +
          (fixed(log10((double)rh)) - fixed_two);
  adewpoint = logEx * fixed(237.3) / (fixed(7.5) - logEx);

  // update statistics
  if (nmeasurements == 0) {
    dewpoint = adewpoint;
    airTemp = t;
  } else {
    dewpoint = (adewpoint + dewpoint) / 2;
    airTemp = (t + airTemp) / 2;
  }

  nmeasurements++;
}

/**
 * Calculates the ThermalIndex for the given height level
 *
 * ThermalIndex is the difference in dry temp and environmental temp
 * at the specified altitude.
 * @param level level = altitude / HEIGHT_STEP
 * @param newdata Function logs data to debug file if true
 */
void
CuSonde::Level::updateThermalIndex(unsigned short level, bool newdata)
{
  fixed hlevel = fixed(level * HEIGHT_STEP);

  // Calculate the dry temperature at altitude = hlevel
  tempDry = DALR * (hlevel - CuSonde::hGround) + CuSonde::maxGroundTemperature;

  // Calculate ThermalIndex
  thermalIndex = airTemp - tempDry;

#ifdef DEBUG_CUSONDE
  if (newdata)
    LogDebug(_T("%g %g %g %g # temp measurement \r\n"),
             (double)hlevel, (double)airTemp, (double)dewpoint, 
             (double)thermalIndex);
#endif
}

/*
   - read sensor values
   - calculate dewpoint
   - update statistical model:

   - for each height:
       -- calculate temp of dry parcel of air from ground temp
             projected up at DALR
       -- thermal index is difference in dry temp and environmental temp
       -- calculate dew point
       -- extrapolate difference between DALR temp and dew point to
            find cloud base (where difference is 0)
       -- extrapolate thermal index to find estimated convection height
            (where thermal index = -3)
       -- extrapolation should occur at top band but may not if tow was
           higher than thermal height, or if climbing above cloud base
           (e.g. from wave lift)

   - summary:
       -- cloud base
       -- usable thermal height
       -- estimated thermal strength

   - complications:
       -- terrain
       -- temp higher in thermals?


DALR = -0.00974 degrees C per meter

C to Kelvin = +273.15
 */
