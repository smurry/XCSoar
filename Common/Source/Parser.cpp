/*
  $Id$

Copyright_License {

  XCSoar Glide Computer - http://xcsoar.sourceforge.net/
  Copyright (C) 2000 - 2005

  	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@bigfoot.com>

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

#include "StdAfx.h"

#include "externs.h"
#include "Utils.h"
#include "externs.h"
#include "VarioSound.h"
#include "Logger.h"
#include "GaugeFLARM.h"
#include "Parser.h"
#include "device.h"
#include "Geoid.h"

extern bool EnableCalibration;

#define MAX_NMEA_LEN	90
#define MAX_NMEA_PARAMS 18

static double EastOrWest(double in, TCHAR EoW);
static double NorthOrSouth(double in, TCHAR NoS);
//static double LeftOrRight(double in, TCHAR LoR);
static double MixedFormatToDegrees(double mixed);
static int NAVWarn(TCHAR c);

NMEAParser nmeaParser1;
NMEAParser nmeaParser2;
int NMEAParser::StartDay = -1;

NMEAParser::NMEAParser() {
  _Reset();
}

void NMEAParser::_Reset(void) {
  nSatellites = 0;
  gpsValid = false;
  activeGPS = true;
  GGAAvailable = FALSE;
  RMZAvailable = FALSE;
  RMZAltitude = 0;
  RMAAvailable = FALSE;
  RMAAltitude = 0;
  LastTime = 0;
}

void NMEAParser::Reset(void) {

  // clear status
  nmeaParser1._Reset();
  nmeaParser2._Reset();

  // trigger updates
  TriggerGPSUpdate();
  TriggerVarioUpdate();
}


void NMEAParser::UpdateMonitor(void)
{
  // does anyone have GPS?
  if (nmeaParser1.gpsValid || nmeaParser2.gpsValid) {
    if (nmeaParser1.gpsValid && nmeaParser2.gpsValid) {
      // both valid, just use first
      nmeaParser2.activeGPS = false;
      nmeaParser1.activeGPS = true;
    } else {
      nmeaParser1.activeGPS = nmeaParser1.gpsValid;
      nmeaParser2.activeGPS = nmeaParser2.gpsValid;
    }
  } else {
    // assume device 1 is active
    nmeaParser2.activeGPS = false;
    nmeaParser1.activeGPS = true;
  }
}


BOOL NMEAParser::ParseNMEAString(int device,
				 TCHAR *String, NMEA_INFO *GPS_INFO)
{
  switch (device) {
  case 0:
    return nmeaParser1.ParseNMEAString_Internal(String, GPS_INFO);
  case 1:
    return nmeaParser2.ParseNMEAString_Internal(String, GPS_INFO);
  };
  return FALSE;
}


/*
 * Copy a provided string into the supplied buffer, terminate on
 * the checksum separator, split into an array of parameters,
 * and return the number of parameters found.
 */
size_t NMEAParser::ExtractParameters(const TCHAR *src, TCHAR *dst, TCHAR **arr, size_t sz)
{
  TCHAR c, *p;
  size_t i = 0;

  _tcscpy(dst, src);
  p = _tcschr(dst, _T('*'));
  if (p)
    *p = _T('\0');

  p = dst;
  do {
    arr[i++] = p;
    p = _tcschr(p, _T(','));
    if (!p)
      break;
    c = *p;
    *p++ = _T('\0');
  } while (i != sz && c != _T('\0'));

  return i;
}


/*
 * Same as ExtractParameters, but also validate the length of
 * the string and the NMEA checksum.
 */
size_t NMEAParser::ValidateAndExtract(const TCHAR *src, TCHAR *dst, size_t dstsz, TCHAR **arr, size_t arrsz)
{
  int len = _tcslen(src);

  if (len <= 6 || len >= dstsz)
    return 0;
  if (!NMEAChecksum(src))
    return 0;

  return ExtractParameters(src, dst, arr, arrsz);
}


BOOL NMEAParser::ParseNMEAString_Internal(TCHAR *String, NMEA_INFO *GPS_INFO)
{
  TCHAR ctemp[MAX_NMEA_LEN];
  TCHAR *params[MAX_NMEA_PARAMS];
  size_t n_params;

  n_params = ValidateAndExtract(String, ctemp, MAX_NMEA_LEN, params, MAX_NMEA_PARAMS);
  if (n_params < 1 || params[0][0] != '$')
    return FALSE;
  if (EnableLogNMEA)
    LogNMEA(String);

  if(params[0][1] == 'P')
    {
      //Proprietary String

      if(_tcscmp(params[0] + 1,TEXT("PTAS1"))==0)
        {
          return PTAS1(&String[7], params + 1, n_params, GPS_INFO);
        }

      // FLARM sentences
      if(_tcscmp(params[0] + 1,TEXT("PFLAA"))==0)
        {
          return PFLAA(&String[7], params + 1, n_params, GPS_INFO);
        }

      if(_tcscmp(params[0] + 1,TEXT("PFLAU"))==0)
        {
          return PFLAU(&String[7], params + 1, n_params, GPS_INFO);
        }

    return FALSE;
    }

  if(_tcscmp(params[0] + 3,TEXT("GSA"))==0)
    {
      return GSA(&String[7], params + 1, n_params, GPS_INFO);
    }
  if(_tcscmp(params[0] + 3,TEXT("GLL"))==0)
    {
      //    return GLL(&String[7], params + 1, n_params, GPS_INFO);
      return FALSE;
    }
  if(_tcscmp(params[0] + 3,TEXT("RMB"))==0)
    {
      //return RMB(&String[7], params + 1, n_params, GPS_INFO);
          return FALSE;
      }
  if(_tcscmp(params[0] + 3,TEXT("RMC"))==0)
    {
      return RMC(&String[7], params + 1, n_params, GPS_INFO);
    }
  if(_tcscmp(params[0] + 3,TEXT("GGA"))==0)
    {
      return GGA(&String[7], params + 1, n_params, GPS_INFO);
    }
  if(_tcscmp(params[0] + 3,TEXT("RMZ"))==0)
    {
      return RMZ(&String[7], params + 1, n_params, GPS_INFO);
    }
  return FALSE;
}

void NMEAParser::ExtractParameter(const TCHAR *Source,
				  TCHAR *Destination,
				  int DesiredFieldNumber)
{
  int dest_index = 0;
  int CurrentFieldNumber = 0;
  int StringLength = _tcslen(Source);
  TCHAR *sptr = (TCHAR*)Source;
  const TCHAR *eptr = Source+StringLength;

  if (!Destination) return;

  while( (CurrentFieldNumber < DesiredFieldNumber) && (sptr<eptr) )
    {
      if (*sptr == ',' || *sptr == '*' )
        {
          CurrentFieldNumber++;
        }
      ++sptr;
    }

  Destination[0] = '\0'; // set to blank in case it's not found..

  if ( CurrentFieldNumber == DesiredFieldNumber )
    {
      while( (sptr < eptr)    &&
             (*sptr != ',') &&
             (*sptr != '*') &&
             (*sptr != '\0') )
        {
          Destination[dest_index] = *sptr;
          ++sptr; ++dest_index;
        }
      Destination[dest_index] = '\0';
    }
}


double EastOrWest(double in, TCHAR EoW)
{
  if(EoW == 'W')
    return -in;
  else
    return in;
}

double NorthOrSouth(double in, TCHAR NoS)
{
  if(NoS == 'S')
    return -in;
  else
    return in;
}

/*
double LeftOrRight(double in, TCHAR LoR)
{
  if(LoR == 'L')
    return -in;
  else
    return in;
}
*/

int NAVWarn(TCHAR c)
{
  if(c=='A')
    return FALSE;
  else
    return TRUE;
}

double NMEAParser::ParseAltitude(TCHAR *value, const TCHAR *format)
{
  double alt = StrToDouble(value, NULL);

  if (format[0] == _T('f') || format[0] == _T('F'))
    alt /= TOFEET;

  return alt;
}

double MixedFormatToDegrees(double mixed)
{
  double mins, degrees;

  degrees = (int) (mixed/100);
  mins = (mixed - degrees*100)/60;

  return degrees+mins;
}

double NMEAParser::TimeModify(double FixTime, NMEA_INFO* GPS_INFO)
{
  double hours, mins,secs;

  hours = FixTime / 10000;
  GPS_INFO->Hour = (int)hours;

  mins = FixTime / 100;
  mins = mins - (GPS_INFO->Hour*100);
  GPS_INFO->Minute = (int)mins;

  secs = FixTime - (GPS_INFO->Hour*10000) - (GPS_INFO->Minute*100);
  GPS_INFO->Second = (int)secs;

  FixTime = secs + (GPS_INFO->Minute*60) + (GPS_INFO->Hour*3600);

  if ((StartDay== -1) && (GPS_INFO->Day != 0)) {
    StartDay = GPS_INFO->Day;
  }
  if (StartDay != -1) {
    if (GPS_INFO->Day < StartDay) {
      // detect change of month (e.g. day=1, startday=31)
      StartDay = GPS_INFO->Day-1;
    }
    int day_difference = GPS_INFO->Day-StartDay;
    if (day_difference>0) {
      // Add seconds to fix time so time doesn't wrap around when
      // going past midnight in UTC
      FixTime += day_difference * 86400;
    }
  }
  return FixTime;
}


bool NMEAParser::TimeHasAdvanced(double ThisTime, NMEA_INFO *GPS_INFO) {
  if(ThisTime< LastTime) {
    LastTime = ThisTime;
    StartDay = -1; // reset search for the first day
    return false;
  } else {
    GPS_INFO->Time = ThisTime;
    LastTime = ThisTime;
    return true;
  }
}

BOOL NMEAParser::GSA(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  int iSatelliteCount =0;

  if (ReplayLogger::IsEnabled()) {
    return TRUE;
  }

  // satellites are in items 4-15 of GSA string,
  // but 1st item in string is not passed, so start at item 3
  for (int i = 0; i < MAXSATELLITES; i++)
  {
    if (3+i<nparams) {
      GPS_INFO->SatelliteIDs[i] = (int)(StrToDouble(params[3+i], NULL));
      if (GPS_INFO->SatelliteIDs[i] > 0)
	iSatelliteCount ++;
    }
  }


  GSAAvailable = TRUE;
  return TRUE;

}

BOOL NMEAParser::GLL(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  gpsValid = !NAVWarn(params[5][0]);

  if (!activeGPS)
    return TRUE;

  if (ReplayLogger::IsEnabled()) {
    // block actual GPS signal
    InterfaceTimeoutReset();
    return TRUE;
  }

  GPS_INFO->NAVWarning = !gpsValid;

  ////

  double ThisTime = TimeModify(StrToDouble(params[4],NULL), GPS_INFO);
  if (!TimeHasAdvanced(ThisTime, GPS_INFO))
    return FALSE;

  double tmplat;
  double tmplon;

  tmplat = MixedFormatToDegrees(StrToDouble(params[0], NULL));
  tmplat = NorthOrSouth(tmplat, params[1][0]);

  tmplon = MixedFormatToDegrees(StrToDouble(params[2], NULL));
  tmplon = EastOrWest(tmplon,params[3][0]);

  if (!((tmplat == 0.0) && (tmplon == 0.0))) {
    GPS_INFO->Latitude = tmplat;
    GPS_INFO->Longitude = tmplon;
  } else {

  }
  return TRUE;
}


BOOL NMEAParser::RMB(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  (void)GPS_INFO;
  (void)String;
  (void)params;
  (void)nparams;
  /* we calculate all this stuff now
  TCHAR ctemp[MAX_NMEA_LEN];

  GPS_INFO->NAVWarning = NAVWarn(params[0][0]);

  GPS_INFO->CrossTrackError = NAUTICALMILESTOMETRES * StrToDouble(params[1], NULL);
  GPS_INFO->CrossTrackError = LeftOrRight(GPS_INFO->CrossTrackError,params[2][0]);

  _tcscpy(ctemp, params[4]);
  ctemp[WAY_POINT_ID_SIZE] = '\0';
  _tcscpy(GPS_INFO->WaypointID,ctemp);

  GPS_INFO->WaypointDistance = NAUTICALMILESTOMETRES * StrToDouble(params[9], NULL);
  GPS_INFO->WaypointBearing = StrToDouble(params[10], NULL);
  GPS_INFO->WaypointSpeed = KNOTSTOMETRESSECONDS * StrToDouble(params[11], NULL);
  */

  return TRUE;
}


bool SetSystemTimeFromGPS = false;

BOOL NMEAParser::RMC(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  TCHAR *Stop;

  gpsValid = !NAVWarn(params[1][0]);

  GPSCONNECT = TRUE;

  if (!activeGPS)
    return true;

  double speed = StrToDouble(params[6], NULL);

  if (speed>2.0) {
    GPS_INFO->MovementDetected = TRUE;
    if (ReplayLogger::IsEnabled()) {
      // stop logger replay if aircraft is actually moving.
      ReplayLogger::Stop();
    }
  } else {
    GPS_INFO->MovementDetected = FALSE;
    if (ReplayLogger::IsEnabled()) {
      // block actual GPS signal if not moving and a log is being replayed
      return TRUE;
    }
  }

  GPS_INFO->NAVWarning = !gpsValid;

  // say we are updated every time we get this,
  // so infoboxes get refreshed if GPS connected
  TriggerGPSUpdate();

  // JMW get date info first so TimeModify is accurate
  GPS_INFO->Year = _tcstol(&params[8][4], &Stop, 10) + 2000;
  params[8][4] = '\0';
  GPS_INFO->Month = _tcstol(&params[8][2], &Stop, 10);
  params[8][2] = '\0';
  GPS_INFO->Day = _tcstol(&params[8][0], &Stop, 10);

  double ThisTime = TimeModify(StrToDouble(params[0],NULL), GPS_INFO);
  if (!TimeHasAdvanced(ThisTime, GPS_INFO))
    return FALSE;

  double tmplat;
  double tmplon;

  tmplat = MixedFormatToDegrees(StrToDouble(params[2], NULL));
  tmplat = NorthOrSouth(tmplat, params[3][0]);

  tmplon = MixedFormatToDegrees(StrToDouble(params[4], NULL));
  tmplon = EastOrWest(tmplon,params[5][0]);

  if (!((tmplat == 0.0) && (tmplon == 0.0))) {
    GPS_INFO->Latitude = tmplat;
    GPS_INFO->Longitude = tmplon;
  }

  GPS_INFO->Speed = KNOTSTOMETRESSECONDS * speed;

  if (GPS_INFO->Speed>1.0) {
    // JMW don't update bearing unless we're moving
    GPS_INFO->TrackBearing = AngleLimit360(StrToDouble(params[7], NULL));
  }

  // Altair doesn't have a battery-backed up realtime clock,
  // so as soon as we get a fix for the first time, set the
  // system clock to the GPS time.
  static bool sysTimeInitialised = false;

  if (!GPS_INFO->NAVWarning && (gpsValid)) {
#if defined(GNAV) && (!defined(WINDOWSPC) || (WINDOWSPC==0))
    SetSystemTimeFromGPS = true;
#endif
    if (SetSystemTimeFromGPS) {
      if (!sysTimeInitialised) {

        SYSTEMTIME sysTime;
        ::GetSystemTime(&sysTime);
        int hours = (int)GPS_INFO->Hour;
        int mins = (int)GPS_INFO->Minute;
        int secs = (int)GPS_INFO->Second;
        sysTime.wYear = (unsigned short)GPS_INFO->Year;
        sysTime.wMonth = (unsigned short)GPS_INFO->Month;
        sysTime.wDay = (unsigned short)GPS_INFO->Day;
        sysTime.wHour = (unsigned short)hours;
        sysTime.wMinute = (unsigned short)mins;
        sysTime.wSecond = (unsigned short)secs;
        sysTime.wMilliseconds = 0;
        sysTimeInitialised = (::SetSystemTime(&sysTime)==TRUE);

#if defined(GNAV) && (!defined(WINDOWSPC) || (WINDOWSPC==0))
        TIME_ZONE_INFORMATION tzi;
        tzi.Bias = -UTCOffset/60;
        _tcscpy(tzi.StandardName,TEXT("Altair"));
        tzi.StandardDate.wMonth= 0; // disable daylight savings
        tzi.StandardBias = 0;
        _tcscpy(tzi.DaylightName,TEXT("Altair"));
        tzi.DaylightDate.wMonth= 0; // disable daylight savings
        tzi.DaylightBias = 0;

        SetTimeZoneInformation(&tzi);
#endif
        sysTimeInitialised =true;

      }
    }
  }

  if (!ReplayLogger::IsEnabled()) {
    if(RMZAvailable)
      {
	// JMW changed from Altitude to BaroAltitude
	GPS_INFO->BaroAltitudeAvailable = true;
	GPS_INFO->BaroAltitude = RMZAltitude;
      }
    else if(RMAAvailable)
      {
	// JMW changed from Altitude to BaroAltitude
	GPS_INFO->BaroAltitudeAvailable = true;
	GPS_INFO->BaroAltitude = RMAAltitude;
      }
  }
  if (!GGAAvailable) {
    // update SatInUse, some GPS receiver dont emmit GGA sentance
    if (!gpsValid) {
      GPS_INFO->SatellitesUsed = 0;
    } else {
      GPS_INFO->SatellitesUsed = -1;
    }
  }

  return TRUE;
}

BOOL NMEAParser::GGA(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{

  if (ReplayLogger::IsEnabled()) {
    return TRUE;
  }

  GGAAvailable = TRUE;

  nSatellites = (int)(min(16,StrToDouble(params[6], NULL)));
  if (nSatellites==0) {
    gpsValid = false;
  }

  if (!activeGPS)
    return TRUE;

  GPS_INFO->SatellitesUsed = (int)(min(16,StrToDouble(params[6], NULL)));

  double ThisTime = TimeModify(StrToDouble(params[0],NULL), GPS_INFO);
  if (!TimeHasAdvanced(ThisTime, GPS_INFO))
    return FALSE;

  double tmplat;
  double tmplon;

  tmplat = MixedFormatToDegrees(StrToDouble(params[1], NULL));
  tmplat = NorthOrSouth(tmplat, params[2][0]);

  tmplon = MixedFormatToDegrees(StrToDouble(params[3], NULL));
  tmplon = EastOrWest(tmplon,params[4][0]);

  if (!((tmplat == 0.0) && (tmplon == 0.0))) {
    GPS_INFO->Latitude = tmplat;
    GPS_INFO->Longitude = tmplon;
  }

  if(RMZAvailable)
    {
      GPS_INFO->BaroAltitudeAvailable = true;
      GPS_INFO->BaroAltitude = RMZAltitude;
    }
  else if(RMAAvailable)
    {
      GPS_INFO->BaroAltitudeAvailable = true;
      GPS_INFO->BaroAltitude = RMAAltitude;
    }

  // "Altitude" should always be GPS Altitude.
  GPS_INFO->Altitude = ParseAltitude(params[8], params[9]);

  //
  double GeoidSeparation;
  if (_tcslen(params[10])>0) {
    // No real need to parse this value,
    // but we do assume that no correction is required in this case
    GeoidSeparation = ParseAltitude(params[10], params[11]);
  } else {
    // need to estimate Geoid Separation internally (optional)
    // FLARM uses MSL altitude
    //
    // Some others don't.
    //
    // If the separation doesn't appear in the sentence,
    // we can assume the GPS unit is giving ellipsoid height
    //
    GeoidSeparation = LookupGeoidSeparation(GPS_INFO->Latitude,
                                            GPS_INFO->Longitude);
    GPS_INFO->Altitude -= GeoidSeparation;
  }

  return TRUE;
}


BOOL NMEAParser::RMZ(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  (void)GPS_INFO;

  RMZAltitude = ParseAltitude(params[0], params[1]);
  //JMW?  RMZAltitude = AltitudeToQNHAltitude(RMZAltitude);
  RMZAvailable = TRUE;

  if (!devHasBaroSource()) {
    // JMW no in-built baro sources, so use this generic one
    if (!ReplayLogger::IsEnabled()) {
      GPS_INFO->BaroAltitudeAvailable = true;
      GPS_INFO->BaroAltitude = RMZAltitude;
    }
  }

  return FALSE;
}


BOOL NMEAParser::RMA(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  (void)GPS_INFO;

  RMAAltitude = ParseAltitude(params[0], params[1]);
  //JMW?  RMAAltitude = AltitudeToQNHAltitude(RMAAltitude);
  RMAAvailable = TRUE;
  GPS_INFO->BaroAltitudeAvailable = true;

  if (!devHasBaroSource()) {
    if (!ReplayLogger::IsEnabled()) {
      // JMW no in-built baro sources, so use this generic one
      GPS_INFO->BaroAltitudeAvailable = true;
      GPS_INFO->BaroAltitude = RMAAltitude;
    }
  }

  return FALSE;
}


BOOL NMEAParser::NMEAChecksum(const TCHAR *String)
{
  unsigned char CalcCheckSum = 0;
  unsigned char ReadCheckSum;
  int End;
  int i;
  TCHAR c1,c2;
  unsigned char v1 = 0,v2 = 0;
  TCHAR *pEnd;

  pEnd = _tcschr(String,'*');
  if(pEnd == NULL)
    return FALSE;

  if(_tcslen(pEnd)<3)
    return FALSE;

  c1 = pEnd[1], c2 = pEnd[2];

  //  iswdigit('0'); // what's this for?

  if(_istdigit(c1))
    v1 = (unsigned char)(c1 - '0');
  if(_istdigit(c2))
    v2 = (unsigned char)(c2 - '0');
  if(_istalpha(c1))
    v1 = (unsigned char)(c1 - 'A' + 10);
  if(_istalpha(c2))
    v2 = (unsigned char)(c2 - 'A' + 10);

  ReadCheckSum = (unsigned char)((v1<<4) + v2);

  End =(int)( pEnd - String);

  for(i=1;i<End;i++)
    {
      CalcCheckSum = (unsigned char)(CalcCheckSum ^ String[i]);
    }

  if(CalcCheckSum == ReadCheckSum)
    return TRUE;
  else
    return FALSE;
}

//////



BOOL NMEAParser::PTAS1(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  double wnet,baralt,vtas;

  wnet = (StrToDouble(params[0],NULL)-200)/(10*TOKNOTS);
  baralt = max(0,(StrToDouble(params[2],NULL)-2000)/TOFEET);
  vtas = StrToDouble(params[3],NULL)/TOKNOTS;

  GPS_INFO->AirspeedAvailable = TRUE;
  GPS_INFO->TrueAirspeed = vtas;
  GPS_INFO->VarioAvailable = TRUE;
  GPS_INFO->Vario = wnet;
  GPS_INFO->BaroAltitudeAvailable = TRUE;
  GPS_INFO->BaroAltitude = AltitudeToQNHAltitude(baralt);
  GPS_INFO->IndicatedAirspeed = vtas/AirDensityRatio(baralt);

  return FALSE;
}


double AccelerometerZero=100.0;

////////////// FLARM

void FLARM_RefreshSlots(NMEA_INFO *GPS_INFO) {
  int i;
  bool present = false;
  if (GPS_INFO->FLARM_Available) {

    for (i=0; i<FLARM_MAX_TRAFFIC; i++) {
      if (GPS_INFO->FLARM_Traffic[i].ID>0) {
	if ((GPS_INFO->Time> GPS_INFO->FLARM_Traffic[i].Time_Fix+2)
	    || (GPS_INFO->Time< GPS_INFO->FLARM_Traffic[i].Time_Fix)) {
	  // clear this slot if it is too old (2 seconds), or if
	  // time has gone backwards (due to replay)
	  GPS_INFO->FLARM_Traffic[i].ID= 0;
	  GPS_INFO->FLARM_Traffic[i].Name[0] = 0;
	} else {
          if (GPS_INFO->FLARM_Traffic[i].AlarmLevel>0) {
            GaugeFLARM::Suppress = false;
          }
	  present = true;
	}
      }
    }
  }
  GaugeFLARM::TrafficPresent(present);
}


#include "InputEvents.h"

double FLARM_NorthingToLatitude = 0.0;
double FLARM_EastingToLongitude = 0.0;


BOOL NMEAParser::PFLAU(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  static int old_flarm_rx = 0;

  GPS_INFO->FLARM_Available = true;

  // calculate relative east and north projection to lat/lon

  double delta_lat = 0.01;
  double delta_lon = 0.01;

  double dlat;
  DistanceBearing(GPS_INFO->Latitude, GPS_INFO->Longitude,
                  GPS_INFO->Latitude+delta_lat, GPS_INFO->Longitude,
                  &dlat, NULL);
  double dlon;
  DistanceBearing(GPS_INFO->Latitude, GPS_INFO->Longitude,
                  GPS_INFO->Latitude, GPS_INFO->Longitude+delta_lon,
                  &dlon, NULL);

  if ((fabs(dlat)>0.0)&&(fabs(dlon)>0.0)) {
    FLARM_NorthingToLatitude = delta_lat / dlat;
    FLARM_EastingToLongitude = delta_lon / dlon;
  } else {
    FLARM_NorthingToLatitude=0.0;
    FLARM_EastingToLongitude=0.0;
  }

  swscanf(String,
	  TEXT("%hu,%hu,%hu,%hu"),
	  &GPS_INFO->FLARM_RX, // number of received FLARM devices
	  &GPS_INFO->FLARM_TX, // Transmit status
	  &GPS_INFO->FLARM_GPS, // GPS status
	  &GPS_INFO->FLARM_AlarmLevel); // Alarm level of FLARM (0-3)

  // process flarm updates

  if ((GPS_INFO->FLARM_RX) && (old_flarm_rx==0)) {
    // traffic has appeared..
    InputEvents::processGlideComputer(GCE_FLARM_TRAFFIC);
  }
  if (GPS_INFO->FLARM_RX > old_flarm_rx) {
    // re-set suppression of gauge, as new traffic has arrived
    //    GaugeFLARM::Suppress = false;
  }
  if ((GPS_INFO->FLARM_RX==0) && (old_flarm_rx)) {
    // traffic has disappeared..
    InputEvents::processGlideComputer(GCE_FLARM_NOTRAFFIC);
  }
  // XX: TODO also another event for new traffic.

  old_flarm_rx = GPS_INFO->FLARM_RX;

  return FALSE;
}


int FLARM_FindSlot(NMEA_INFO *GPS_INFO, long Id)
{
  int i;
  for (i=0; i<FLARM_MAX_TRAFFIC; i++) {

    // find position in existing slot
    if (Id==GPS_INFO->FLARM_Traffic[i].ID) {
      return i;
    }

    // find old empty slot

  }
  // not found, so try to find an empty slot
  for (i=0; i<FLARM_MAX_TRAFFIC; i++) {
    if (GPS_INFO->FLARM_Traffic[i].ID==0) {
      // this is a new target
      GaugeFLARM::Suppress = false;
      return i;
    }
  }

  // still not found and no empty slots left, buffer is full
  return -1;
}



BOOL NMEAParser::PFLAA(TCHAR *String, TCHAR **params, size_t nparams, NMEA_INFO *GPS_INFO)
{
  int flarm_slot = 0;

  // 5 id, 6 digit hex
  long ID;
  swscanf(params[5],TEXT("%lx"), &ID);
//  unsigned long uID = ID;

  flarm_slot = FLARM_FindSlot(GPS_INFO, ID);
  if (flarm_slot<0) {
    // no more slots available,
    return FALSE;
  }

  // set time of fix to current time
  GPS_INFO->FLARM_Traffic[flarm_slot].Time_Fix = GPS_INFO->Time;

  swscanf(String,
	  TEXT("%hu,%lf,%lf,%lf,%hu,%lx,%lf,%lf,%lf,%lf,%hu"),
	  &GPS_INFO->FLARM_Traffic[flarm_slot].AlarmLevel, // unsigned short 0
	  &GPS_INFO->FLARM_Traffic[flarm_slot].RelativeNorth, // double?     1
	  &GPS_INFO->FLARM_Traffic[flarm_slot].RelativeEast, // double?      2
	  &GPS_INFO->FLARM_Traffic[flarm_slot].RelativeAltitude, // double   3
	  &GPS_INFO->FLARM_Traffic[flarm_slot].IDType, // unsigned short     4
	  &GPS_INFO->FLARM_Traffic[flarm_slot].ID, // 6 char hex
	  &GPS_INFO->FLARM_Traffic[flarm_slot].TrackBearing, // double       6
	  &GPS_INFO->FLARM_Traffic[flarm_slot].TurnRate, // double           7
	  &GPS_INFO->FLARM_Traffic[flarm_slot].Speed, // double              8
	  &GPS_INFO->FLARM_Traffic[flarm_slot].ClimbRate, // double          9
	  &GPS_INFO->FLARM_Traffic[flarm_slot].Type); // unsigned short     10
  // 1 relativenorth, meters
  GPS_INFO->FLARM_Traffic[flarm_slot].Latitude =
    GPS_INFO->FLARM_Traffic[flarm_slot].RelativeNorth
    *FLARM_NorthingToLatitude + GPS_INFO->Latitude;
  // 2 relativeeast, meters
  GPS_INFO->FLARM_Traffic[flarm_slot].Longitude =
    GPS_INFO->FLARM_Traffic[flarm_slot].RelativeEast
    *FLARM_EastingToLongitude + GPS_INFO->Longitude;
  // alt
  GPS_INFO->FLARM_Traffic[flarm_slot].Altitude =
    GPS_INFO->FLARM_Traffic[flarm_slot].RelativeAltitude +
    GPS_INFO->Altitude;

  TCHAR *name = GPS_INFO->FLARM_Traffic[flarm_slot].Name;
  if (!_tcslen(name)) {
    // need to lookup name for this target
    TCHAR *fname = LookupFLARMDetails(GPS_INFO->FLARM_Traffic[flarm_slot].ID);
    if (fname) {
      _tcscpy(name,fname);
    } else {
      name[0]=0;
    }
  }

  return FALSE;
}


//////

void NMEAParser::TestRoutine(NMEA_INFO *GPS_INFO) {
#ifdef DEBUG
  static int i=90;
  static TCHAR t1[] = TEXT("1,1,1,1");
  static TCHAR t2[] = TEXT("1,300,500,220,2,DD8F12,0,-4.5,30,-1.4,1");
  static TCHAR t3[] = TEXT("0,0,1200,50,2,DA8B06,270,-4.5,30,-1.4,1");
  //  static TCHAR b50[] = TEXT("0,.1,.0,0,0,1.06,0,-222");
  //  static TCHAR t4[] = TEXT("-3,500,1024,50");

  //  nmeaParser1.ParseNMEAString_Internal(TEXT("$PTAS1,201,200,02583,000*2A"), GPS_INFO);
  //  nmeaParser1.ParseNMEAString_Internal(TEXT("$GPRMC,082430.00,A,3744.09096,S,14426.16069,E,0.520294.90,301207,,,A*77"), GPS_INFO);
  //  nmeaParser1.ParseNMEAString_Internal(TEXT("$GPGGA,082430.00,3744.09096,S,1426.16069,E,1,08,1.37,157.6,M,-4.9,M,,*5B"), GPS_INFO);

  QNH=1013.25;
  double h;
  double altraw= 5.0;
  h = AltitudeToQNHAltitude(altraw);
  QNH = FindQNH(altraw, 50.0);
  h = AltitudeToQNHAltitude(altraw);

  ////

  i++;

  if (i>100) {
    i=0;
  }
  if (i<50) {
    GPS_INFO->FLARM_Available = true;
    TCHAR ctemp[MAX_NMEA_LEN];
    TCHAR *params[MAX_NMEA_PARAMS];
    size_t nr;
    nr = nmeaParser1.ExtractParameters(t1, ctemp, params, MAX_NMEA_PARAMS);
    nmeaParser1.PFLAU(t1, params, nr, GPS_INFO);
    nr = nmeaParser1.ExtractParameters(t2, ctemp, params, MAX_NMEA_PARAMS);
    nmeaParser1.PFLAA(t2, params, nr, GPS_INFO);
    nr = nmeaParser1.ExtractParameters(t3, ctemp, params, MAX_NMEA_PARAMS);
    nmeaParser1.PFLAA(t3, params, nr, GPS_INFO);
  }
#endif
}


///

bool EnableLogNMEA = false;
HANDLE nmeaLogFile = INVALID_HANDLE_VALUE;

void LogNMEA(TCHAR* text) {

  if (!EnableLogNMEA) {
    if (nmeaLogFile != INVALID_HANDLE_VALUE) {
       CloseHandle(nmeaLogFile);
       nmeaLogFile = INVALID_HANDLE_VALUE;
    }
    return;
  }

  DWORD dwBytesRead;

  if (nmeaLogFile == INVALID_HANDLE_VALUE) {
    nmeaLogFile = CreateFile(TEXT("\\SD Card\\xcsoar-nmea.log"),
			     GENERIC_WRITE, FILE_SHARE_WRITE,
			     NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  }

  WriteFile(nmeaLogFile, text, _tcslen(text)*sizeof(TCHAR), &dwBytesRead,
	    (OVERLAPPED *)NULL);
}

