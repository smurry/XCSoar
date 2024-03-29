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

#ifndef XCSOAR_DATA_FIELD_BOOLEAN_HPP
#define XCSOAR_DATA_FIELD_BOOLEAN_HPP

#include "Util/StaticString.hpp"
#include "DataField/Base.hpp"

class DataFieldBoolean: public DataField
{
private:
  bool mValue;

  StaticString<32> true_text;
  StaticString<32> false_text;

public:
  DataFieldBoolean(bool Default, const TCHAR *TextTrue, const TCHAR *TextFalse,
                   DataAccessCallback_t OnDataAccess)
    :DataField(TYPE_BOOLEAN, OnDataAccess),
     mValue(Default),
     true_text(TextTrue), false_text(TextFalse) {
    SupportCombo = true;
  }

  void Inc(void);
  void Dec(void);
  virtual ComboList *CreateComboList() const;

  bool GetAsBoolean(void) const;
  virtual int GetAsInteger(void) const;
  virtual const TCHAR *GetAsString(void) const;

  virtual void
  Set(int Value)
  {
    if (Value > 0)
      Set(true);
    else
      Set(false);
  }

  #if defined(__BORLANDC__)
  #pragma warn -hid
  #endif

  void Set(bool Value);

  #if defined(__BORLANDC__)
  #pragma warn +hid
  #endif

  void SetAsBoolean(bool Value);
  virtual void SetAsInteger(int Value);
  virtual void SetAsString(const TCHAR *Value);
};

#endif
