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
 * @package    corcal::components::ssrfeatex
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#pragma once


// STD/STL
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

// Ice
#include <Ice/Current.h>

// ArmarX
#include <ArmarXCore/core/Component.h>
#include <ArmarXCore/core/services/tasks/RunningTask.h>

// corcal
#include <corcal/core.h>
#include <corcal/interface/ssrfeatex_component_interface.h>
#include <corcal/interface/ssr_feature_listener.h>


namespace corcal::components::ssrfeatex
{


class component :
    virtual public component_interface,
    virtual public armarx::Component
{

    public:

        /**
         * @brief Default name of this component
         */
        static const std::string default_name;

    private:

        /**
         * @brief Listener proxy to publish SSR features
         */
        ssr_feature_listener::ProxyType m_ssr_feature_listener;

        /**
         * @brief Processing mutex used together with m_proc_signal
         */
        std::mutex m_proc_mutex;

        /**
         * @brief Processing condition variable used together with m_proc_mutex
         */
        std::condition_variable m_proc_signal;

        /**
         * @brief Flag indicating whether the worker is currently processing
         */
        bool m_run_feature_extraction_task;

        /**
         * @brief Worker task evaluating the SSR features
         */
        armarx::RunningTask<component>::pointer_type m_ssr_feature_extraction_task;

        std::vector<core::detected_object> m_detected_object_buffer;
        std::chrono::microseconds m_timestamp_detected_objects;

    public:

        virtual std::string getDefaultName() const override;

    protected:

        virtual void onInitComponent() override;

        virtual void onConnectComponent() override;

        virtual void onDisconnectComponent() override;

        virtual void onExitComponent() override;

        virtual void run_ssr_feature_extraction_task();

        void
        virtual object_instances_detected(
            const std::vector<core::detected_object>& objects,
            Ice::Long timestamp_darknet,
            Ice::Long timestamp_openpose,
            const Ice::Current&
        ) override;

        virtual armarx::PropertyDefinitionsPtr createPropertyDefinitions() override;

};


}
