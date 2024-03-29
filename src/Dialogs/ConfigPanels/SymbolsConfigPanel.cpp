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

#include "SymbolsConfigPanel.hpp"
#include "Profile/ProfileKeys.hpp"
#include "Profile/Profile.hpp"
#include "Form/Edit.hpp"
#include "Form/Util.hpp"
#include "DataField/Enum.hpp"
#include "Interface.hpp"
#include "Screen/Graphics.hpp"
#include "Language/Language.hpp"

static WndForm* wf = NULL;

static void
ShowTrailControls(bool show)
{
  ShowFormControl(*wf, _T("prpTrailDrift"), show);
  ShowFormControl(*wf, _T("prpSnailType"), show);
  ShowFormControl(*wf, _T("prpSnailWidthScale"), show);
}

void
SymbolsConfigPanel::OnTrailLength(DataField *Sender,
                                  DataField::DataAccessKind_t Mode)
{
  const DataFieldEnum &df = *(const DataFieldEnum *)Sender;
  TrailLength trail_length = (TrailLength)df.GetAsInteger();
  ShowTrailControls(trail_length != TRAIL_OFF);
}

void
SymbolsConfigPanel::Init(WndForm *_wf)
{
  assert(_wf != NULL);
  wf = _wf;

  const SETTINGS_MAP &settings_map = CommonInterface::SettingsMap();

  WndProperty *wp;

  wp = (WndProperty*)wf->FindByName(_T("prpWindArrowStyle"));
  if (wp) {
    DataFieldEnum* dfe;
    dfe = (DataFieldEnum*)wp->GetDataField();
    dfe->addEnumText(_("Arrow head"));
    dfe->addEnumText(_("Full arrow"));
    dfe->Set(settings_map.WindArrowStyle);
    wp->RefreshDisplay();
  }

  wp = (WndProperty*)wf->FindByName(_T("prpTrail"));
  if (wp) {
    DataFieldEnum* dfe;
    dfe = (DataFieldEnum*)wp->GetDataField();
    dfe->addEnumText(_("Off"), TRAIL_OFF);
    dfe->addEnumText(_("Long"), TRAIL_LONG);
    dfe->addEnumText(_("Short"), TRAIL_SHORT);
    dfe->addEnumText(_("Full"), TRAIL_FULL);
    dfe->Set(settings_map.trail_length);
    wp->RefreshDisplay();
  }

  LoadFormProperty(*wf, _T("prpTrailDrift"),
                   settings_map.EnableTrailDrift);

  wp = (WndProperty*)wf->FindByName(_T("prpSnailType"));
  if (wp) {
    DataFieldEnum* dfe;
    dfe = (DataFieldEnum*)wp->GetDataField();
    TCHAR tmp_text[30];
    _tcscpy(tmp_text, _("Vario"));
    _tcscat(tmp_text, _T(" #1"));
    dfe->addEnumText(tmp_text);
    _tcscpy(tmp_text, _("Vario"));
    _tcscat(tmp_text, _T(" #2"));
    dfe->addEnumText(tmp_text);
    dfe->addEnumText(_("Altitude"));
    dfe->Set((int)settings_map.SnailType);
    wp->RefreshDisplay();
  }

  LoadFormProperty(*wf, _T("prpSnailWidthScale"),
                   settings_map.SnailScaling);

  LoadFormProperty(*wf, _T("prpDetourCostMarker"),
                   settings_map.EnableDetourCostMarker);

  wp = (WndProperty*)wf->FindByName(_T("prpDisplayTrackBearing"));
  if (wp) {
    DataFieldEnum* dfe;
    dfe = (DataFieldEnum*)wp->GetDataField();
    dfe->addEnumText(_("Off"));
    dfe->addEnumText(_("On"));
    dfe->addEnumText(_("Auto"));
    dfe->Set(settings_map.DisplayTrackBearing);
    wp->RefreshDisplay();
  }

  LoadFormProperty(*wf, _T("prpEnableFLARMMap"),
                   settings_map.EnableFLARMMap);

  wp = (WndProperty*)wf->FindByName(_T("prpAircraftSymbol"));
  if (wp) {
    DataFieldEnum* dfe;
    dfe = (DataFieldEnum*)wp->GetDataField();
    dfe->addEnumText(_("Simple"), acSimple);
    dfe->addEnumText(_("Simple (large)"), acSimpleLarge);
    dfe->addEnumText(_("Detailed"), acDetailed);
    dfe->Set(settings_map.aircraft_symbol);
    wp->RefreshDisplay();
  }

  ShowTrailControls(settings_map.trail_length != TRAIL_OFF);
}


bool
SymbolsConfigPanel::Save()
{
  SETTINGS_MAP &settings_map = CommonInterface::SetSettingsMap();
  bool changed = false;
  WndProperty *wp;

  changed |= SaveFormProperty(*wf, _T("prpWindArrowStyle"),
                              szProfileWindArrowStyle,
                              settings_map.WindArrowStyle);

  changed |= SaveFormPropertyEnum(*wf, _T("prpTrail"),
                                  szProfileSnailTrail,
                                  settings_map.trail_length);

  changed |= SaveFormProperty(*wf, _T("prpTrailDrift"),
                              szProfileTrailDrift,
                              settings_map.EnableTrailDrift);

  wp = (WndProperty*)wf->FindByName(_T("prpSnailType"));
  if (wp) {
    if (settings_map.SnailType != (SnailType_t)wp->GetDataField()->GetAsInteger()) {
      settings_map.SnailType = (SnailType_t)wp->GetDataField()->GetAsInteger();
      Profile::Set(szProfileSnailType, (int)settings_map.SnailType);
      changed = true;
      Graphics::InitSnailTrail(settings_map);
    }
  }

  bool snailscaling_changed =
      SaveFormProperty(*wf, _T("prpSnailWidthScale"),
                       szProfileSnailWidthScale,
                       settings_map.SnailScaling);
  changed |= snailscaling_changed;
  if (snailscaling_changed)
    Graphics::InitSnailTrail(settings_map);

  changed |= SaveFormProperty(*wf, _T("prpDetourCostMarker"),
                              szProfileDetourCostMarker,
                              settings_map.EnableDetourCostMarker);

  changed |= SaveFormPropertyEnum(*wf, _T("prpDisplayTrackBearing"),
                              szProfileDisplayTrackBearing,
                              settings_map.DisplayTrackBearing);

  changed |= SaveFormProperty(*wf, _T("prpEnableFLARMMap"),
                              szProfileEnableFLARMMap,
                              settings_map.EnableFLARMMap);

  wp = (WndProperty*)wf->FindByName(_T("prpAircraftSymbol"));
  if (wp) {
    if (settings_map.aircraft_symbol != (AircraftSymbol_t)wp->GetDataField()->GetAsInteger()) {
      settings_map.aircraft_symbol = (AircraftSymbol_t)wp->GetDataField()->GetAsInteger();
      Profile::Set(szProfileAircraftSymbol, (int)settings_map.aircraft_symbol);
      changed = true;
    }
  }

  return changed;
}
