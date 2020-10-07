/*
 * This file is part of ArmarX.
 *
 * ArmarX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ArmarX is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * @package    corcal::gui::ssrvisu
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#include <corcal/gui/ssrvisu/gui_plugin.h>


// corcal
#include <corcal/gui/ssrvisu/main_widget_controller.h>


corcal::gui::ssrvisu::gui_plugin::gui_plugin()
{
    this->addWidget<corcal::gui::ssrvisu::main_widget_controller>();

    ARMARX_INFO << "SSR feature visualisation ArmarX GUI plugin constructed";
}


corcal::gui::ssrvisu::gui_plugin::~gui_plugin()
{
    ARMARX_INFO << "SSR feature visualisation ArmarX GUI plugin deconstructed";
}


#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN2(corcal_gui_ssrvisu_gui_plugin, corcal::gui::ssrvisu::gui_plugin)
#endif
