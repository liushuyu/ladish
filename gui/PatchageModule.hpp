/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 *
 **************************************************************************
 * This file implements the canvas module class
 **************************************************************************
 *
 * LADI Session Handler is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * LADI Session Handler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LADI Session Handler. If not, see <http://www.gnu.org/licenses/>
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef PATCHAGE_PATCHAGEMODULE_HPP
#define PATCHAGE_PATCHAGEMODULE_HPP

#include "common.h"
#include "PatchageCanvas.hpp"

class PatchageModule
{
public:
  PatchageModule(Patchage* app, const std::string& name, ModuleType type, double x=0, double y=0);

  virtual ~PatchageModule();

  bool
  identify(const std::string& name, ModuleType type)
  {
    return _name == name && (_type == type || _type == InputOutput);
  }
#if 0
  void create_menu() {
    _menu = new Gtk::Menu();
    Gtk::Menu::MenuList& items = _menu->items();
    if (_type == InputOutput) {
      items.push_back(Gtk::Menu_Helpers::MenuElem("Split",
        sigc::mem_fun(this, &PatchageModule::split)));
    } else {
      items.push_back(Gtk::Menu_Helpers::MenuElem("Join",
        sigc::mem_fun(this, &PatchageModule::join)));
    }
    items.push_back(Gtk::Menu_Helpers::MenuElem("Disconnect All",
      sigc::mem_fun(this, &PatchageModule::menu_disconnect_all)));
  }
  void move(double dx, double dy) {
    FlowCanvas::Module::move(dx, dy);
    store_location();
  }
  
  void load_location() {
    boost::shared_ptr<Canvas> canvas = _canvas.lock();
    if (!canvas)
      return;

    Coord loc;

    if (_app->state_manager()->get_module_location(_name, _type, loc))
      move_to(loc.x, loc.y);
    else
      move_to((canvas->width()/2) - 100 + rand() % 400,
               (canvas->height()/2) - 100 + rand() % 400);
  }
  
  virtual void store_location() {
    Coord loc(property_x().get_value(), property_y().get_value());
    _app->state_manager()->set_module_location(_name, _type, loc);
  }
  
  void split() {
    assert(_type == InputOutput);
    _app->state_manager()->set_module_split(_name, true);
    _app->refresh();
  }

  void join() {
    assert(_type != InputOutput);
    _app->state_manager()->set_module_split(_name, false);
    _app->refresh();
  }

  virtual void show_dialog() {}
  
  virtual void menu_disconnect_all() {
    for (PortVector::iterator p = _ports.begin(); p != _ports.end(); ++p)
      (*p)->disconnect_all();
  }
#endif

protected:
  Patchage*  _app;
  ModuleType _type;
  std::string _name;
};


#endif // PATCHAGE_PATCHAGEMODULE_HPP