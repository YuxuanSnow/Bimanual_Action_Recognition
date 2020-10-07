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


#include <corcal/components/ssrfeatex/component.h>


// STD/STL
#include <string>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>


using namespace corcal::core;
using namespace corcal::components::ssrfeatex;


const std::string
component::default_name = "ssrfeatex";


std::string
component::getDefaultName() const
{
    return component::default_name;
}


void
component::onInitComponent()
{
    ARMARX_DEBUG << "Initialising " << getName();

    // Members
    m_run_feature_extraction_task = false;

    // Signal dependency where the 3D object instances are published
    {
        const std::string topic_name = getProperty<std::string>("object_instances_topic");
        usingTopic(topic_name);
    }

    // Topic name under which the detection results are published
    {
        const std::string topic_name = getProperty<std::string>("ssr_features_topic").getValue();
        offeringTopic(topic_name);
    }

    ARMARX_DEBUG << "Initialised " << getName();
}


void
component::onConnectComponent()
{
    ARMARX_DEBUG << "Connecting " << getName();

    // Topic of SSR features
    {
        const std::string topic_name = getProperty<std::string>("ssr_features_topic");
        m_ssr_feature_listener = getTopic<ssr_feature_listener::ProxyType>(topic_name);
    }

    // Kick off feature extraction task
    m_ssr_feature_extraction_task = new armarx::RunningTask<component>{
        this,
        &component::run_ssr_feature_extraction_task
    };
    m_ssr_feature_extraction_task->start();

    ARMARX_VERBOSE << "Connected " << getName();
}


void
component::onDisconnectComponent()
{
    ARMARX_DEBUG << "Disconnecting " << getName();

    // Stop feature extraction task
    {
        std::lock_guard<std::mutex> lock{m_proc_mutex};
        ARMARX_DEBUG << "Stopping input synchronisation thread...";
        const bool wait_for_join = false;
        m_ssr_feature_extraction_task->stop(wait_for_join);
        m_proc_signal.notify_all();
        ARMARX_DEBUG << "Waiting for input synchronisation thread to stop...";
        m_ssr_feature_extraction_task->waitForStop();
        ARMARX_DEBUG << "Input synchronisation thread stopped";
    }

    ARMARX_DEBUG << "Disconnected " << getName();
}


void
component::onExitComponent()
{
    ARMARX_DEBUG << "Exiting " << getName();
    ARMARX_VERBOSE << "Exited " << getName();
}


void
component::run_ssr_feature_extraction_task()
{
    ARMARX_DEBUG << "Starting feature extraction task";

    while (not m_ssr_feature_extraction_task->isStopped())
    {
        std::unique_lock<std::mutex> signal_lock{m_proc_mutex};
        m_proc_signal.wait(
            signal_lock,
            [&]() -> bool
            {
                return m_ssr_feature_extraction_task->isStopped() or m_run_feature_extraction_task;
            }
        );

        // Break the loop if the input synchronisation task has been stopped
        if (m_ssr_feature_extraction_task->isStopped()) break;

        // Assess time
        const auto proc_start = std::chrono::high_resolution_clock::now();

        ARMARX_DEBUG << "Creating local copies of objects and timestamp";
        const std::vector<detected_object> objects = m_detected_object_buffer;
        const std::chrono::microseconds timestamp = m_timestamp_detected_objects;

        ARMARX_DEBUG << "Releasing lock and continue working with local copies";
        m_run_feature_extraction_task = false;
        signal_lock.unlock();

        // Do not use member variables prone to racing conditions from this point on

        ARMARX_DEBUG << "Processing inputs and calculating SSRs";
        const double dist_eq_thresh = getProperty<double>("ssr.distance_equality_threshold");
        const std::vector<std::vector<relations>> ssr_matrix = evaluate_relations(objects, dist_eq_thresh);

        ARMARX_DEBUG << "Serialising SSR matrix";
        const std::vector<std::vector<int>> ssr_matrix_serialised = serialise(ssr_matrix);

        ARMARX_DEBUG << "Publishing SSR evaluation results";
        m_ssr_feature_listener->ssr_features_detected(
            objects,
            ssr_matrix_serialised,
            timestamp.count()
        );

        const std::chrono::microseconds proc_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - proc_start);
        ARMARX_DEBUG << "Calculating SSRs took " << proc_duration.count() << " µs";
    }

    ARMARX_VERBOSE << "Exiting feature extraction task";
}


void
component::object_instances_detected(
    const std::vector<detected_object>& objects,
    Ice::Long timestamp_darknet,
    Ice::Long /*timestamp_openpose*/,
    const Ice::Current&)
{
    const std::lock_guard<std::mutex> lock(m_proc_mutex);

    ARMARX_DEBUG << "Buffering detected 3D object instances";
    m_detected_object_buffer = objects;
    m_timestamp_detected_objects = std::chrono::microseconds{timestamp_darknet};

    m_run_feature_extraction_task = true;
    m_proc_signal.notify_one();
}


armarx::PropertyDefinitionsPtr
component::createPropertyDefinitions()
{
    armarx::PropertyDefinitionsPtr defs{new armarx::ComponentPropertyDefinitions{this->getConfigIdentifier()}};

    // Result topic
    defs->defineOptionalProperty<std::string>("ssr_features_topic", "ssr_features",
        "Output topic name under which the spatial symbolic relation features are published"
    );

    // Input topic
    defs->defineOptionalProperty<std::string>("object_instances_topic", "ObjectInstances3D",
        "Input topic name under which the 3D object instances are published "
    );

    defs->defineOptionalProperty<double>("ssr.distance_equality_threshold", 30,
        "Distance in [mm] under which distance variations are considered noise and thus discarded"
    );

    return defs;
}
