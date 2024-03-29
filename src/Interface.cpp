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

#include "Interface.hpp"
#include "Thread/Mutex.hpp"
#include "MainWindow.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "Language/Language.hpp"
#include "Dialogs/Message.hpp"
#include "StatusMessage.hpp"
#include "InfoBoxes/InfoBoxManager.hpp"
#include "InfoBoxes/InfoBoxLayout.hpp"
#include "Screen/Layout.hpp"
#include "Asset.hpp"
#include "Components.hpp"
#include "DrawThread.hpp"
#include "Gauge/GlueGaugeVario.hpp"
#include "Gauge/GaugeFLARM.hpp"
#include "PeriodClock.hpp"
#include "Screen/Blank.hpp"
#include "LogFile.hpp"
#include "DeviceBlackboard.hpp"
#include "CalculationThread.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "UIState.hpp"

UIState CommonInterface::ui_state;

bool CommonInterface::movement_detected = false;

bool ActionInterface::doForceShutdown = false;

InterfaceBlackboard CommonInterface::blackboard;
StatusMessageList CommonInterface::status_messages;
MainWindow CommonInterface::main_window(status_messages);

// settings used only by interface thread scope
unsigned XCSoarInterface::debounceTimeout = 250;
unsigned ActionInterface::MenuTimeoutMax = 8 * 4;

bool
CommonInterface::IsPanning()
{
  return main_window.map != NULL && main_window.map->IsPanning();

}

void
XCSoarInterface::ExchangeBlackboard()
{
  ExchangeDeviceBlackboard();
  SendSettingsComputer();
  SendSettingsMap();
}

void
XCSoarInterface::ExchangeDeviceBlackboard()
{
  ScopeLock protect(device_blackboard.mutex);
  ReadBlackboardBasic(device_blackboard.Basic());

  const NMEAInfo &real = device_blackboard.RealState();
  movement_detected = real.connected && real.gps.real &&
    real.MovementDetected();

  ReadBlackboardCalculated(device_blackboard.Calculated());

  device_blackboard.ReadSettingsComputer(SettingsComputer());
}

void
ActionInterface::SendSettingsComputer()
{
  assert(calculation_thread != NULL);
  assert(main_window.map != NULL);

  main_window.map->SetSettingsComputer(SettingsComputer());
  calculation_thread->SetSettingsComputer(SettingsComputer());
  calculation_thread->SetScreenDistanceMeters(main_window.map->VisibleProjection().GetScreenDistanceMeters());
}

void
ActionInterface::SetMacCready(fixed mc, bool to_devices)
{
  /* update interface settings */

  GlidePolar &polar = SetSettingsComputer().glide_polar_task;
  polar.SetMC(mc);

  /* update InfoBoxes (that might show the MacCready setting) */

  InfoBoxManager::SetDirty();

  /* send to calculation thread and trigger recalculation */

  if (protected_task_manager != NULL)
    protected_task_manager->set_glide_polar(polar);

  if (calculation_thread != NULL) {
    calculation_thread->SetSettingsComputer(SettingsComputer());
    calculation_thread->Trigger();
  }

  /* send to external devices */

  if (to_devices)
    device_blackboard.SetMC(mc);
}

/**
 * Send the own SettingsMap to the DeviceBlackboard
 * @param trigger_draw Triggers the draw event after sending if true
 */
void
ActionInterface::SendSettingsMap(const bool trigger_draw)
{
  assert(main_window.map != NULL);

  // trigger_draw: asks for an immediate exchange of blackboard data
  // (via ProcessTimer()) rather than waiting for the idle timer every 500ms

  if (trigger_draw) {
    DisplayModes();
    InfoBoxManager::ProcessTimer();
  }

  main_window.map->SetSettingsMap(SettingsMap());

  if (trigger_draw)
    main_window.full_redraw();

  // TODO: trigger refresh if the settings are changed
}

void
ActionInterface::SignalShutdown(bool force)
{
  doForceShutdown = force;
  main_window.close(); // signals close
}

bool
XCSoarInterface::CheckShutdown()
{
  if (doForceShutdown)
    return true;

  return MessageBoxX(_("Quit program?"), _T("XCSoar"),
                     MB_YESNO | MB_ICONQUESTION) == IDYES;
}

// Debounce input buttons (does not matter which button is pressed)
bool
XCSoarInterface::Debounce(void)
{
#if defined(GNAV) || defined(PCGNAV)
  return true;
#else
  static PeriodClock fps_last;

  ResetDisplayTimeOut();

  if (GetUIState().screen_blanked)
    // prevent key presses working if screen is blanked,
    // so a key press just triggers turning the display on again
    return false;

  return fps_last.check_update(debounceTimeout);
#endif
}

/**
 * Determine whether the vario gauge should be drawn depending on the
 * display orientation and the infobox layout
 * @return True if vario gauge should be drawn, False otherwise
 */
static bool
vario_visible()
{
  return InfoBoxLayout::has_vario();
}

/**
 * Determine whether vario gauge, FLARM radar and infoboxes should be drawn
 */
void
ActionInterface::DisplayModes()
{
  bool full_screen = main_window.GetFullScreen();

  if (main_window.vario) {
    // Determine whether the vario gauge should be drawn
    main_window.vario->set_visible(!full_screen && vario_visible() &&
                                   !GetUIState().screen_blanked);
  }

  if (Basic().flarm.NewTraffic) {
    GaugeFLARM *gauge_flarm = main_window.flarm;
    if (gauge_flarm != NULL)
      gauge_flarm->Suppress = false;
  }
}
