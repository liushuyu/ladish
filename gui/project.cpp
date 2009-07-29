/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains implementation of the project class
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

#include "common.h"
#include "project.hpp"
#include "lash_proxy.hpp"
#include "lash_client.hpp"
//#include "globals.hpp"

struct project_impl
{
  lash_proxy * lash_ptr;
  std::string name;
  std::string description;
  std::string notes;
  bool modified_status;
  std::list<boost::shared_ptr<lash_client> > clients;
};

project::project(
  lash_proxy * lash_ptr,
  const std::string& name)
{
  lash_loaded_project_properties properties;

  lash_ptr->get_loaded_project_properties(name, properties);

  _impl_ptr = new project_impl;
  _impl_ptr->lash_ptr = lash_ptr;
  _impl_ptr->name = name;

  _impl_ptr->description = properties.description;
  _impl_ptr->notes = properties.notes;
  _impl_ptr->modified_status = properties.modified_status;

  //g_app->info_msg("project created");
}

project::~project()
{
  delete _impl_ptr;
  //g_app->info_msg("project destroyed");
}

void
project::clear()
{
  boost::shared_ptr<lash_client> client_ptr;

  while (!_impl_ptr->clients.empty())
  {
    client_ptr = _impl_ptr->clients.front();
    _impl_ptr->clients.pop_front();
    _signal_client_removed.emit(client_ptr);
  }
}

void
project::get_name(
  std::string& name)
{
  name = _impl_ptr->name;
}

void
project::on_name_changed(
  const std::string& name)
{
  _impl_ptr->name = name;
  _signal_renamed.emit();
}

void
project::get_description(
  std::string& description)
{
  description = _impl_ptr->description;
}

void
project::get_notes(
  std::string& notes)
{
  notes = _impl_ptr->notes;
}

bool
project::get_modified_status()
{
  return _impl_ptr->modified_status;
}

void
project::get_clients(
  std::list<boost::shared_ptr<lash_client> >& clients)
{
  clients = _impl_ptr->clients;
}

void
project::on_client_added(
    boost::shared_ptr<lash_client> client_ptr)
{
  _impl_ptr->clients.push_back(client_ptr);
  _signal_client_added.emit(client_ptr);
}

void
project::on_client_removed(
  const std::string& id)
{
  boost::shared_ptr<lash_client> client_ptr;
  std::string temp_id;

  for (std::list<boost::shared_ptr<lash_client> >::iterator iter = _impl_ptr->clients.begin(); iter != _impl_ptr->clients.end(); iter++)
  {
    client_ptr = *iter;
    client_ptr->get_id(temp_id);

    if (temp_id == id)
    {
      _impl_ptr->clients.erase(iter);
      _signal_client_removed.emit(client_ptr);
      return;
    }
  }
}

void
project::on_modified_status_changed(
  bool modified_status)
{
  _impl_ptr->modified_status = modified_status;
  _signal_modified_status_changed.emit();
}

void
project::on_description_changed(
    const std::string& description)
{
  _impl_ptr->description = description;
  _signal_description_changed.emit();
}

void
project::on_notes_changed(
    const std::string& notes)
{
  _impl_ptr->notes = notes;
  _signal_notes_changed.emit();
}

void
project::do_rename(
  const std::string& name)
{
  if (_impl_ptr->name != name)
  {
    _impl_ptr->lash_ptr->project_rename(_impl_ptr->name, name);
  }
}

void
project::do_change_description(
  const std::string& description)
{
  if (_impl_ptr->description != description)
  {
    _impl_ptr->lash_ptr->project_set_description(_impl_ptr->name, description);
  }
}

void
project::do_change_notes(
  const std::string& notes)
{
  if (_impl_ptr->notes != notes)
  {
    _impl_ptr->lash_ptr->project_set_notes(_impl_ptr->name, notes);
  }
}
