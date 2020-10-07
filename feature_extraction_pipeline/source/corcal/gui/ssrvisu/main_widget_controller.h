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


#pragma once


// STD/STL
#include <memory>
#include <string>

// Qt
#include <QtCore/QString>

// ArmarX
#include <ArmarXGui/libraries/ArmarXGuiBase/ArmarXGuiPlugin.h>
#include <ArmarXGui/libraries/ArmarXGuiBase/ArmarXComponentWidgetController.h>

// corcal
#include <corcal/components/ssrfeatex/ssr_feature_listener.h>


namespace Ui
{
    class main_widget;
}


namespace corcal
{
    namespace gui
    {
        namespace ssrvisu
        {
            class main_widget_controller;
        }
    }
}


class corcal::gui::ssrvisu::main_widget_controller:
    public armarx::ArmarXComponentWidgetController,
    public corcal::components::ssrfeatex::ssr_feature_listener
{

    Q_OBJECT

    public:

        static const std::string widget_name;

    private:

        std::unique_ptr<Ui::main_widget> widget;

    public:

        main_widget_controller();

        virtual ~main_widget_controller() override;

        virtual QString getWidgetName() const override;

        static QString GetWidgetName();

        virtual std::string getDefaultName() const override;

        virtual void loadSettings(QSettings* settings) override;

        virtual void saveSettings(QSettings* settings) override;

        virtual void onInitComponent() override;

        virtual void onConnectComponent() override;

        virtual void onDisconnectComponent() override;

        virtual void ssr_features_detected(
            const std::vector<corcal::core::detected_object>& objects,
            const std::vector<std::vector<int>>& ssr_matrix,
            Ice::Long timestamp,
            const Ice::Current&
        ) override;

    protected:

        // pass

    public slots:

        // pass

    signals:

        // pass

};
