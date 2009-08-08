/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains interface of the session class
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

#ifndef SESSION_HPP__C870E949_EF2A_43E8_8FE8_55AE5A714172__INCLUDED
#define SESSION_HPP__C870E949_EF2A_43E8_8FE8_55AE5A714172__INCLUDED

struct session_impl;
class project;
class lash_client;

class session
{
public:
  session();
  ~session();

  void
  clear();

  void
  project_add(
    boost::shared_ptr<project> project_ptr);

  void
  project_close(
    const std::string& project_name);

  boost::shared_ptr<project> find_project_by_name(const std::string& name);

  void
  client_add(
    boost::shared_ptr<lash_client> client_ptr);

  void
  client_remove(
    const std::string& id);

  boost::shared_ptr<lash_client> find_client_by_id(const std::string& id);

  sigc::signal1<void, boost::shared_ptr<project> > _signal_project_added;
  sigc::signal1<void, boost::shared_ptr<project> > _signal_project_closed;

private:
  session_impl * _impl_ptr;
};

#endif // #ifndef SESSION_HPP__C870E949_EF2A_43E8_8FE8_55AE5A714172__INCLUDED