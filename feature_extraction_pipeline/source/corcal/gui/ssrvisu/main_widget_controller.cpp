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


#include <corcal/gui/ssrvisu/main_widget_controller.h>


// Boost
#include <boost/algorithm/string.hpp>

// corcal
#include <corcal/gui/ssrvisu/ui_main_widget.h>


const std::string
corcal::gui::ssrvisu::main_widget_controller::widget_name = "SSR feature visualisation";


corcal::gui::ssrvisu::main_widget_controller::main_widget_controller() :
    widget{new Ui::main_widget{}}
{
    // Initialise widget
    widget->setupUi(getWidget());
}


corcal::gui::ssrvisu::main_widget_controller::~main_widget_controller()
{
    // pass
}


QString
corcal::gui::ssrvisu::main_widget_controller::getWidgetName() const
{
    return corcal::gui::ssrvisu::main_widget_controller::GetWidgetName();
}


QString
corcal::gui::ssrvisu::main_widget_controller::GetWidgetName()
{
    return QString::fromStdString("corcal." + corcal::gui::ssrvisu::main_widget_controller::widget_name);
}


std::string
corcal::gui::ssrvisu::main_widget_controller::getDefaultName() const
{
    return boost::algorithm::replace_all_copy(corcal::gui::ssrvisu::main_widget_controller::widget_name, " ", "_");
}


void
corcal::gui::ssrvisu::main_widget_controller::loadSettings(QSettings*)
{
    // pass
}


void
corcal::gui::ssrvisu::main_widget_controller::saveSettings(QSettings*)
{
    // pass
}


void
corcal::gui::ssrvisu::main_widget_controller::onInitComponent()
{
    usingTopic("SSRFeatures"); // TODO: Make configurable
}


void
corcal::gui::ssrvisu::main_widget_controller::onConnectComponent()
{
    // pass
}


void
corcal::gui::ssrvisu::main_widget_controller::onDisconnectComponent()
{
    // pass
}


void
corcal::gui::ssrvisu::main_widget_controller::ssr_features_detected(
    const std::vector<corcal::core::detected_object>& objects,
    const std::vector<std::vector<int>>& ssr_matrix,
    Ice::Long timestamp,
    const Ice::Current&)
{
    // pass
}
