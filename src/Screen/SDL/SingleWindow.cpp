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

#include "Screen/SingleWindow.hpp"

#include <cassert>

gcc_pure
static const ContainerWindow *
IsAncestor(const Window *maybe_ancestor, const Window *w)
{
  while (true) {
    const ContainerWindow *parent = w->GetParent();
    if (parent == NULL)
      return NULL;

    if (parent == maybe_ancestor)
      return parent;

    w = parent;
  }
}

bool
SingleWindow::FilterMouseEvent(int x, int y, Window *allowed) const
{
  const ContainerWindow *container = this;
  while (true) {
    const Window *child =
      const_cast<ContainerWindow *>(container)->event_child_at(x, y);
    if (child == NULL)
      /* no receiver for the event */
      return false;

    if (child == allowed)
      /* the event reaches an allowed window: success */
      return true;

    const ContainerWindow *next = IsAncestor(allowed, child);
    if (next == NULL)
      return false;

    container = next;
  }
}

#ifndef ANDROID

bool
SingleWindow::FilterEvent(const SDL_Event &event, Window *allowed) const
{
  assert(allowed != NULL);

  switch (event.type) {
  case SDL_MOUSEMOTION:
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    return FilterMouseEvent(event.button.x, event.button.y, allowed);

  default:
    return true;
  }
}

#endif /* !ANDROID */
