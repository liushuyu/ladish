/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "SubpatchModule.h"
#include <cassert>
#include <iostream>
#include "interface/EngineInterface.h"
#include "client/PatchModel.h"
#include "App.h"
#include "NodeModule.h"
#include "NodeControlWindow.h"
#include "PatchWindow.h"
#include "PatchCanvas.h"
#include "Port.h"
#include "WindowFactory.h"
using std::cerr; using std::cout; using std::endl;

namespace Ingenuity {


SubpatchModule::SubpatchModule(boost::shared_ptr<PatchCanvas> canvas, SharedPtr<PatchModel> patch)
: NodeModule(canvas, patch),
  _patch(patch)
{
	assert(canvas);
	assert(patch);
}


void
SubpatchModule::on_double_click(GdkEventButton* event)
{
	assert(_patch);

	SharedPtr<PatchModel> parent = PtrCast<PatchModel>(_patch->parent());

	PatchWindow* const preferred = ( (parent && (event->state & GDK_SHIFT_MASK))
		? NULL
		: App::instance().window_factory()->patch_window(parent) );

	App::instance().window_factory()->present_patch(_patch, preferred);
}



/** Browse to this patch in current (parent's) window
 * (unless an existing window is displaying it)
 */
void
SubpatchModule::browse_to_patch()
{
	assert(_patch->parent());
	
	SharedPtr<PatchModel> parent = PtrCast<PatchModel>(_patch->parent());

	PatchWindow* const preferred = ( (parent)
		? App::instance().window_factory()->patch_window(parent)
		: NULL );
	
	App::instance().window_factory()->present_patch(_patch, preferred);
}



void
SubpatchModule::show_dialog()
{
	cerr << "FIXME: dialog\n";
	//m_patch->show_control_window();
}


void
SubpatchModule::menu_remove()
{
	App::instance().engine()->destroy(_patch->path());
}

} // namespace Ingenuity
