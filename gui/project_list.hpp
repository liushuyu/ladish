/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains interface of the project_list class
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

#ifndef PROJECT_LIST_HPP__D786489B_E400_4E92_85C7_2BAE606DE56D__INCLUDED
#define PROJECT_LIST_HPP__D786489B_E400_4E92_85C7_2BAE606DE56D__INCLUDED

struct project_list_impl;
class Patchage;
class session;

class project_list
{
public:
  project_list(
    Patchage* app,
    session * session_ptr);

  ~project_list();

  void set_lash_availability(bool lash_active);

private:
  project_list_impl * _impl_ptr;
};

#endif // #ifndef PROJECT_LIST_HPP__D786489B_E400_4E92_85C7_2BAE606DE56D__INCLUDED