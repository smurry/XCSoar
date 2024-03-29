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

#include "DataField/Enum.hpp"
#include "DataField/ComboList.hpp"
#include "Form/Edit.hpp"
#include "Form/Util.hpp"
#include "Form/Frame.hpp"
#include "Profile/ProfileKeys.hpp"
#include "Profile/Profile.hpp"
#include "Dialogs/Dialogs.h"
#include "Interface.hpp"
#include "Profile/DeviceConfig.hpp"
#include "Device/Register.hpp"
#include "Device/List.hpp"
#include "Device/Parser.hpp"
#include "Device/Driver.hpp"
#include "Asset.hpp"
#include "Protection.hpp"
#include "DevicesConfigPanel.hpp"
#include "Language/Language.hpp"
#include "Compatibility/string.h"
#include "Util/Macros.hpp"

#ifdef _WIN32_WCE
#include "Device/Windows/Enumerator.hpp"
#endif

#ifdef ANDROID
#include "Android/BluetoothHelper.hpp"
#ifdef IOIOLIB
#include "Device/AndroidIOIOUartPort.hpp"
#endif
#endif

static WndForm* wf = NULL;
static bool changed = false;

static const struct {
  enum DeviceConfig::port_type type;
  const TCHAR *label;
} port_types[] = {
  { DeviceConfig::DISABLED, N_("Disabled") },
#ifdef _WIN32_WCE
  { DeviceConfig::AUTO, N_("GPS Intermediate Driver") },
#endif
#ifdef ANDROID
  { DeviceConfig::INTERNAL, N_("Built-in GPS") },
#endif

  /* label not translated for now, until we have a TCP port
     selection UI */
  { DeviceConfig::TCP_LISTENER, _T("TCP port 4353") },

  { DeviceConfig::SERIAL, NULL } /* sentinel */
};

/** the number of fixed port types (excludes Serial, Bluetooth and IOIOUart) */
static const unsigned num_port_types = ARRAY_SIZE(port_types) - 1;

static DeviceConfig device_config[NUMDEV];

gcc_pure
static bool
SupportsBulkBaudRate(const DataField &df)
{
  const TCHAR *driver_name = df.GetAsString();
  if (driver_name == NULL)
    return false;

  const struct DeviceRegister *driver = FindDriverByName(driver_name);
  if (driver == NULL)
    return false;

  return driver->SupportsBulkBaudRate();
}

gcc_pure
static enum DeviceConfig::port_type
GetPortType(WndProperty &port_field)
{
  unsigned port = port_field.GetDataField()->GetAsInteger();

  if (port < num_port_types)
    return port_types[port].type;

  return (enum DeviceConfig::port_type)(port >> 16);
}

static void
UpdateDeviceControlVisibility(WndProperty &port_field,
                              WndProperty &speed_field,
                              WndProperty &bulk_baud_rate_field,
                              WndProperty &driver_field)
{
  const enum DeviceConfig::port_type type = GetPortType(port_field);

  speed_field.set_visible(DeviceConfig::UsesSpeed(type));
  bulk_baud_rate_field.set_visible(DeviceConfig::UsesSpeed(type) &&
                                   DeviceConfig::UsesDriver(type) &&
                                   SupportsBulkBaudRate(*driver_field.GetDataField()));
  driver_field.set_visible(DeviceConfig::UsesDriver(type));
}

void
DevicesConfigPanel::OnDeviceAPort(DataField *Sender,
                                  DataField::DataAccessKind_t Mode)
{
  UpdateDeviceControlVisibility(*(WndProperty *)wf->FindByName(_T("prpComPort1")),
                                *(WndProperty *)wf->FindByName(_T("prpComSpeed1")),
                                *(WndProperty *)wf->FindByName(_T("BulkBaudRate1")),
                                *(WndProperty *)wf->FindByName(_T("prpComDevice1")));
}

void
DevicesConfigPanel::OnDeviceBPort(DataField *Sender,
                                  DataField::DataAccessKind_t Mode)
{
  UpdateDeviceControlVisibility(*(WndProperty *)wf->FindByName(_T("prpComPort2")),
                                *(WndProperty *)wf->FindByName(_T("prpComSpeed2")),
                                *(WndProperty *)wf->FindByName(_T("BulkBaudRate2")),
                                *(WndProperty *)wf->FindByName(_T("prpComDevice2")));
}

void
DevicesConfigPanel::OnDeviceAData(DataField *Sender, DataField::DataAccessKind_t Mode)
{
  switch (Mode) {
  case DataField::daChange:
    UpdateDeviceControlVisibility(*(WndProperty *)wf->FindByName(_T("prpComPort1")),
                                  *(WndProperty *)wf->FindByName(_T("prpComSpeed1")),
                                  *(WndProperty *)wf->FindByName(_T("BulkBaudRate1")),
                                  *(WndProperty *)wf->FindByName(_T("prpComDevice1")));
    break;

  case DataField::daSpecial:
    return;
  }
}


void
DevicesConfigPanel::OnDeviceBData(DataField *Sender, DataField::DataAccessKind_t Mode)
{
  switch (Mode) {
  case DataField::daChange:
    UpdateDeviceControlVisibility(*(WndProperty *)wf->FindByName(_T("prpComPort2")),
                                  *(WndProperty *)wf->FindByName(_T("prpComSpeed2")),
                                  *(WndProperty *)wf->FindByName(_T("BulkBaudRate2")),
                                  *(WndProperty *)wf->FindByName(_T("prpComDevice2")));
    break;

  case DataField::daSpecial:
    return;
  }
}

static unsigned
AddPort(DataFieldEnum &df, enum DeviceConfig::port_type type,
        const TCHAR *text, const TCHAR *display_string=NULL,
        const TCHAR *help=NULL)
{
  /* the uppper 16 bit is the port type, and the lower 16 bit is a
     serial number to make the enum id unique */

  unsigned id = ((unsigned)type << 16) + df.Count();
  df.AddChoice(id, text, display_string, help);
  return id;
}

#if defined(HAVE_POSIX)

#include <dirent.h>
#include <unistd.h>

static bool
DetectSerialPorts(DataFieldEnum &dfe)
{
  DIR *dir = opendir("/dev");
  if (dir == NULL)
    return false;

  unsigned sort_start = dfe.Count();

  bool found = false;
  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    /* filter "/dev/tty*" */
    if (memcmp(ent->d_name, "tty", 3) == 0) {
      /* filter out "/dev/tty0", ... (valid integer after "tty") */
      char *endptr;
      strtoul(ent->d_name + 3, &endptr, 10);
      if (*endptr == 0)
        continue;
    } else if (memcmp(ent->d_name, "rfcomm", 6) != 0)
      continue;

    char path[64];
    snprintf(path, sizeof(path), "/dev/%s", ent->d_name);
    if (access(path, R_OK|W_OK) == 0 && access(path, X_OK) < 0) {
      AddPort(dfe, DeviceConfig::SERIAL, path);
      found = true;
    }
  }

  closedir(dir);

  if (found)
    dfe.Sort(sort_start);

  return found;
}

#endif

#ifdef GNAV

static bool
DetectSerialPorts(DataFieldEnum &dfe)
{
  AddPort(dfe, DeviceConfig::SERIAL, _T("COM1:"), _T("Vario (COM1)"));
  AddPort(dfe, DeviceConfig::SERIAL, _T("COM2:"), _T("Radio (COM2)"));
  AddPort(dfe, DeviceConfig::SERIAL, _T("COM3:"), _T("Internal (COM3)"));
  return true;
}

#elif defined(_WIN32_WCE)

static bool
DetectSerialPorts(DataFieldEnum &dfe)
{
  PortEnumerator enumerator;
  if (enumerator.Error())
    return false;

  unsigned sort_start = dfe.Count();

  bool found = false;
  while (enumerator.Next()) {
    AddPort(dfe, DeviceConfig::SERIAL, enumerator.GetName(),
            enumerator.GetDisplayName());
    found = true;
  }

  if (found)
    dfe.Sort(sort_start);

  return found;
}

#endif

static void
FillPorts(DataFieldEnum &dfe)
{
#if defined(HAVE_POSIX)
  if (DetectSerialPorts(dfe))
    return;
#elif defined(WIN32)

#ifdef _WIN32_WCE
  if (DetectSerialPorts(dfe))
    return;
#endif

  for (unsigned i = 1; i <= 10; ++i) {
    TCHAR buffer[64];
    _stprintf(buffer, _T("COM%u:"), i);
    AddPort(dfe, DeviceConfig::SERIAL, buffer);
  }
#endif
}

static void
FillBaudRates(DataFieldEnum &dfe)
{
  dfe.addEnumText(_T("1200"), 1200);
  dfe.addEnumText(_T("2400"), 2400);
  dfe.addEnumText(_T("4800"), 4800);
  dfe.addEnumText(_T("9600"), 9600);
  dfe.addEnumText(_T("19200"), 19200);
  dfe.addEnumText(_T("38400"), 38400);
  dfe.addEnumText(_T("57600"), 57600);
  dfe.addEnumText(_T("115200"), 115200);
}

static void
SetupDeviceFields(const DeviceConfig &config,
                  WndProperty *port_field, WndProperty *speed_field,
                  WndProperty &bulk_baud_rate_field,
                  WndProperty *driver_field)
{
  if (port_field != NULL) {
    DataFieldEnum *dfe = (DataFieldEnum *)port_field->GetDataField();
#if defined(ANDROID) && defined (IOIOLIB)
    dfe->EnableItemHelp(true);
#endif

    for (unsigned i = 0; port_types[i].label != NULL; i++) {
      unsigned id = AddPort(*dfe, port_types[i].type,
                            gettext(port_types[i].label));

      if (port_types[i].type == config.port_type)
        dfe->Set(id);
    }

    FillPorts(*dfe);

#ifdef ANDROID
    JNIEnv *env = Java::GetEnv();
    jobjectArray bonded = BluetoothHelper::list(env);
    if (bonded != NULL) {
      jsize n = env->GetArrayLength(bonded) / 2;
      for (jsize i = 0; i < n; ++i) {
        jstring address = (jstring)env->GetObjectArrayElement(bonded, i * 2);
        if (address == NULL)
          continue;

        const char *address2 = env->GetStringUTFChars(address, NULL);
        if (address2 == NULL)
          continue;

        jstring name = (jstring)env->GetObjectArrayElement(bonded, i * 2 + 1);
        const char *name2 = name != NULL
          ? env->GetStringUTFChars(name, NULL)
          : NULL;

        AddPort(*dfe, DeviceConfig::RFCOMM, address2, name2);

        env->ReleaseStringUTFChars(address, address2);
        if (name2 != NULL)
          env->ReleaseStringUTFChars(name, name2);
      }

      env->DeleteLocalRef(bonded);

      if (config.port_type == DeviceConfig::RFCOMM &&
          !config.bluetooth_mac.empty()) {
        if (!dfe->Exists(config.bluetooth_mac))
          AddPort(*dfe, DeviceConfig::RFCOMM, config.bluetooth_mac);

        dfe->SetAsString(config.bluetooth_mac);
      }
    }

#ifdef IOIOLIB
    TCHAR tempID[4];
    TCHAR tempName[15];
    for (unsigned i = 0; i < AndroidIOIOUartPort::getNumberUarts(); i++) {
      _sntprintf(tempID, sizeof(tempID), _T("%d"), i);
      _sntprintf(tempName, sizeof(tempName), _T("IOIO Uart %d"), i);
      AddPort(*dfe, DeviceConfig::IOIOUART, tempID, tempName,
              AndroidIOIOUartPort::getPortHelp(i));
    }

    if (config.port_type == DeviceConfig::IOIOUART &&
        config.ioio_uart_id < AndroidIOIOUartPort::getNumberUarts()) {
      _sntprintf(tempID,  sizeof(tempID), _T("%d"), config.ioio_uart_id);
      dfe->SetAsString(tempID);
    }
#endif
#endif /* Android */

    switch (config.port_type) {
    case DeviceConfig::SERIAL:
      if (!dfe->Exists(config.path))
        AddPort(*dfe, config.port_type, config.path);

      dfe->SetAsString(config.path);
      break;

    case DeviceConfig::DISABLED:
    case DeviceConfig::RFCOMM:
    case DeviceConfig::IOIOUART:
    case DeviceConfig::AUTO:
    case DeviceConfig::INTERNAL:
    case DeviceConfig::TCP_LISTENER:
      break;
    }

    port_field->RefreshDisplay();
  }

  if (speed_field != NULL) {
    DataFieldEnum &dfe = *(DataFieldEnum *)speed_field->GetDataField();
    FillBaudRates(dfe);
    dfe.Set(config.baud_rate);
    speed_field->RefreshDisplay();
  }

  DataFieldEnum &bulk_df = *(DataFieldEnum *)bulk_baud_rate_field.GetDataField();
  bulk_df.addEnumText(_T("Default"), 0u);
  FillBaudRates(bulk_df);
  bulk_df.Set(config.bulk_baud_rate);
  bulk_baud_rate_field.RefreshDisplay();

  if (driver_field) {
    DataFieldEnum *dfe = (DataFieldEnum *)driver_field->GetDataField();

    const TCHAR *DeviceName;
    for (unsigned i = 0; (DeviceName = GetDriverNameByIndex(i)) != NULL; i++)
      dfe->addEnumText(DeviceName, GetDriverDisplayNameByIndex(i));

    dfe->Sort(1);
    dfe->SetAsString(config.driver_name);

    driver_field->RefreshDisplay();
  }

  UpdateDeviceControlVisibility(*port_field, *speed_field,
                                bulk_baud_rate_field,
                                *driver_field);
}


/**
 * @return true if the value has changed
 */
static bool
FinishPortField(DeviceConfig &config, WndProperty &port_field)
{
  const DataFieldEnum &df = *(const DataFieldEnum *)port_field.GetDataField();
  unsigned value = df.GetAsInteger();

  /* decode the port type from the upper 16 bits of the id; we don't
     need the rest, because that's just some serial we don't care
     about */
  const enum DeviceConfig::port_type new_type =
    (enum DeviceConfig::port_type)(value >> 16);
  switch (new_type) {
  case DeviceConfig::DISABLED:
  case DeviceConfig::AUTO:
  case DeviceConfig::INTERNAL:
  case DeviceConfig::TCP_LISTENER:
    if (new_type == config.port_type)
      return false;

    config.port_type = new_type;
    return true;

  case DeviceConfig::SERIAL:
    /* Bluetooth */
    if (new_type == config.port_type &&
        _tcscmp(config.path, df.GetAsString()) == 0)
      return false;

    config.port_type = new_type;
    config.path = df.GetAsString();
    return true;

  case DeviceConfig::RFCOMM:
    /* Bluetooth */
    if (new_type == config.port_type &&
        _tcscmp(config.bluetooth_mac, df.GetAsString()) == 0)
      return false;

    config.port_type = new_type;
    config.bluetooth_mac = df.GetAsString();
    return true;

  case DeviceConfig::IOIOUART:
    /* IOIO UART */
    if (new_type == config.port_type &&
        config.ioio_uart_id == (unsigned)_ttoi(df.GetAsString()))
      return false;

    config.port_type = new_type;
    config.ioio_uart_id = (unsigned)_ttoi(df.GetAsString());
    return true;
  }

  /* unreachable */
  assert(false);
  return false;
}


static bool
FinishDeviceFields(DeviceConfig &config,
                   WndProperty *port_field, WndProperty *speed_field,
                   const WndProperty &bulk_baud_rate_field,
                   WndProperty *driver_field)
{
  bool changed = false;

  if (port_field != NULL && FinishPortField(config, *port_field))
    changed = true;

  if (config.UsesSpeed() && speed_field != NULL &&
      (int)config.baud_rate != speed_field->GetDataField()->GetAsInteger()) {
    config.baud_rate = speed_field->GetDataField()->GetAsInteger();
    changed = true;
  }

  if (config.UsesSpeed() &&
      config.bulk_baud_rate != (unsigned)bulk_baud_rate_field.GetDataField()->GetAsInteger()) {
    config.bulk_baud_rate = (unsigned)bulk_baud_rate_field.GetDataField()->GetAsInteger();
    changed = true;
  }

  if (config.UsesDriver() && driver_field != NULL &&
      !config.driver_name.equals(driver_field->GetDataField()->GetAsString())) {
    config.driver_name = driver_field->GetDataField()->GetAsString();
    changed = true;
  }

  return changed;
}


void
DevicesConfigPanel::Init(WndForm *_wf)
{
  assert(_wf != NULL);
  wf = _wf;
  changed = false;

  for (unsigned i = 0; i < NUMDEV; ++i)
    Profile::GetDeviceConfig(i, device_config[i]);

  SetupDeviceFields(device_config[0],
                    (WndProperty*)wf->FindByName(_T("prpComPort1")),
                    (WndProperty*)wf->FindByName(_T("prpComSpeed1")),
                    *(WndProperty *)wf->FindByName(_T("BulkBaudRate1")),
                    (WndProperty*)wf->FindByName(_T("prpComDevice1")));

  SetupDeviceFields(device_config[1],
                    (WndProperty*)wf->FindByName(_T("prpComPort2")),
                    (WndProperty*)wf->FindByName(_T("prpComSpeed2")),
                    *(WndProperty *)wf->FindByName(_T("BulkBaudRate2")),
                    (WndProperty*)wf->FindByName(_T("prpComDevice2")));

  LoadFormProperty(*wf, _T("prpSetSystemTimeFromGPS"),
                   CommonInterface::SettingsComputer().SetSystemTimeFromGPS);

  LoadFormProperty(*wf, _T("prpIgnoreNMEAChecksum"),
                   NMEAParser::ignore_checksum);
}


bool
DevicesConfigPanel::Save()
{
  DevicePortChanged =
    FinishDeviceFields(device_config[0],
                       (WndProperty*)wf->FindByName(_T("prpComPort1")),
                       (WndProperty*)wf->FindByName(_T("prpComSpeed1")),
                       *(const WndProperty *)wf->FindByName(_T("BulkBaudRate1")),
                       (WndProperty*)wf->FindByName(_T("prpComDevice1")));

  DevicePortChanged =
    FinishDeviceFields(device_config[1],
                       (WndProperty*)wf->FindByName(_T("prpComPort2")),
                       (WndProperty*)wf->FindByName(_T("prpComSpeed2")),
                       *(const WndProperty *)wf->FindByName(_T("BulkBaudRate2")),
                       (WndProperty*)wf->FindByName(_T("prpComDevice2"))) ||
    DevicePortChanged;

  changed |= SaveFormProperty(*wf, _T("prpSetSystemTimeFromGPS"),
                              szProfileSetSystemTimeFromGPS,
                              CommonInterface::SetSettingsComputer().SetSystemTimeFromGPS);

  changed |= SaveFormProperty(*wf, _T("prpIgnoreNMEAChecksum"),
                              szProfileIgnoreNMEAChecksum,
                              NMEAParser::ignore_checksum);

  if (DevicePortChanged) {
    changed = true;
    for (unsigned i = 0; i < NUMDEV; ++i)
      Profile::SetDeviceConfig(i, device_config[i]);
  }

  return changed;
}
