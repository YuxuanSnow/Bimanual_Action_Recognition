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
 * @package    corcal::components::catalyst
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#include <corcal/components/catalyst/component.h>


// STD/STL
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <string>
#include <vector>
namespace ch = std::chrono;
namespace fs = std::filesystem;

// PCL
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/common/common.h>
#include <pcl/filters/crop_box.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>

// JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// IVT
#include <Image/ImageProcessor.h>
#include <Image/PrimitivesDrawer.h>

// Ice
#include <IceUtil/Time.h>
namespace ice
{
    using namespace Ice;
    using namespace IceUtil;
}

// ArmarX
#include <ArmarXCore/core/logging/Logging.h>
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>
namespace ax = armarx;

// RobotAPI
#include <RobotAPI/libraries/core/Pose.h>

// VisionX
#include <VisionX/interface/components/YoloObjectListener.h>
#include <VisionX/tools/ImageUtil.h>
namespace vx = visionx;

// corcal
#include <corcal/components/catalyst/functions.h>
using namespace corcal::components::catalyst;


namespace
{
    /**
     * @brief Constant to a timestamp considered invalid in this context
     */
    const ch::microseconds timestamp_invalid = ch::microseconds::zero();

    /**
     * @brief Helper function to delete a stereo image or RGB-D image
     * @param stereo_image The stereo image to delete
     */
    void delete_stereo_image(::CByteImage** stereo_image)
    {
        delete stereo_image[0];
        delete stereo_image[1];
        delete[] stereo_image;
    }
}


namespace visionx
{
    void from_json(const json& j, visionx::BoundingBox3D bb)
    {
        j.at("x0").get_to(bb.x0);
        j.at("y0").get_to(bb.y0);
        j.at("z0").get_to(bb.z0);
        j.at("x1").get_to(bb.x1);
        j.at("y1").get_to(bb.y1);
        j.at("z1").get_to(bb.z1);
    }
}
namespace corcal::core
{
    void from_json(const json& j, detected_object& det)
    {
        j.at("class_name").get_to(det.class_name);
        j.at("class_index").get_to(det.class_index);
        j.at("instance_name").get_to(det.instance_name);
        j.at("certainty").get_to(det.certainty);
        det.bounding_box.x0 = j["bounding_box"]["x0"];
        det.bounding_box.y0 = j["bounding_box"]["y0"];
        det.bounding_box.z0 = j["bounding_box"]["z0"];
        det.bounding_box.x1 = j["bounding_box"]["x1"];
        det.bounding_box.y1 = j["bounding_box"]["y1"];
        det.bounding_box.z1 = j["bounding_box"]["z1"];
        det.colour = armarx::DrawColor24Bit{j["colour"][0], j["colour"][1], j["colour"][2]};
    }
}


const std::string
component::default_name = "catalyst";


component::~component()
{
    // pass
}


std::string
component::getDefaultName() const
{
    return component::default_name;
}


void
component::onInitImageProcessor()
{
    ARMARX_DEBUG << "Initialising " << getName() << ".";

    // General options.
    m_long_term_image_buffer_max_size =
        static_cast<unsigned int>(getProperty<int>("long_term_image_buffer_size"));
    m_use_manual_timestamps = false;

    const std::string boxes_path = getProperty<std::string>("get_boxes_from");
    if (not boxes_path.empty())
    {
        ARMARX_WARNING << "Dummy mode enabled.";
        m_ignore_cnns = true;
    }

    // Initialise working memory.
    {
        const float initial_certainty = getProperty<float>("memory_initial_certainty");
        const ch::milliseconds remember_duration{
            getProperty<int>("memory_remember_duration").getValue()};
        m_memory.initial_certainty_threshold(initial_certainty);
        m_memory.remember_duration(remember_duration);
    }

    // Signal dependency on the object detection and pose estimation topics.
    if (not m_ignore_cnns)
    {
        const std::string topic_name_2d_body_pos =
            getProperty<std::string>("topic_name_2d_body_pose");
        const std::string topic_name_2d_hand_pose =
            getProperty<std::string>("topic_name_2d_hand_pose");
        const std::string topic_name_objects = getProperty<std::string>("topic_name_objects");
        usingTopic(topic_name_2d_body_pos);
        usingTopic(topic_name_2d_hand_pose);
        usingTopic(topic_name_objects);
    }

    // Signal dependency on image provider.
    m_image_provider_id = getProperty<std::string>("image_provider");
    usingImageProvider(m_image_provider_id);

    // Offer topic name where the inspection debug pointclouds are published.
    {
        const std::string topic_name = getName() + "_pointcloud";
        offeringTopic(topic_name);
    }

    // Offer topic name where the 3D object instances are published.
    {
        const std::string topic_name = getProperty<std::string>("topic_name_3d_object_instances");
        offeringTopic(topic_name);
    }

    ARMARX_DEBUG << "Initialised " << getName() << ".";
}


void
component::onConnectImageProcessor()
{
    ARMARX_DEBUG << "Connecting " << getName() << ".";

    // Initialise image provider.
    m_image_provider_info = getImageProvider(m_image_provider_id);

    // Topic of generated pointclouds for debug inspection.
    {
        const std::string topic_name = getName() + "_pointcloud";
        m_pointcloud_listener = getTopic<pointcloud_listener::ProxyType>(topic_name);
    }

    // Topic of detected objects for consumers.
    {
        const std::string topic_name = getProperty<std::string>("topic_name_3d_object_instances");
        m_object_instance_listener = getTopic<object_instance_listener::ProxyType>(topic_name);
    }

    // Initialise image buffer.
    m_input_image_buffer = new ::CByteImage*[2];
    m_input_image_buffer[0] = vx::tools::createByteImage(m_image_provider_info);
    m_input_image_buffer[1] = vx::tools::createByteImage(m_image_provider_info);

    // Invalidate timestamps.
    m_timestamp_last_input_image = m_timestamp_last_detected_objects = m_timestamp_last_body_pose =
        m_timestamp_last_hand_pose = ::timestamp_invalid;

    // Kick off running task.
    m_input_synchronisation_task = ax::RunningTask<component>::pointer_type(
        new ax::RunningTask<component>(this, &component::run_input_synchronisation));
    m_input_synchronisation_task->start();

    ARMARX_VERBOSE << "Connected " << getName() << ".";
}


void
component::onDisconnectImageProcessor()
{
    ARMARX_DEBUG << "Disconnecting " << getName() << ".";

    // Stop task.
    {
        std::lock_guard<std::mutex> lock(m_input_proc_mutex);
        ARMARX_DEBUG << "Stopping input synchronisation thread...";
        const bool wait_for_join = false;
        m_input_synchronisation_task->stop(wait_for_join);
        m_proc_signal.notify_all();
        ARMARX_DEBUG << "Waiting for input synchronisation thread to stop...";
        m_input_synchronisation_task->waitForStop();
        ARMARX_DEBUG << "Input synchronisation thread stopped.";
    }

    // Free long term image buffer.
    {
        auto it = m_long_term_image_buffer.begin();
        while (it != m_long_term_image_buffer.end())
        {
            ::CByteImage** stereo_image = it->second;
            ::delete_stereo_image(stereo_image);
            std::advance(it, 1);
        }

        m_long_term_image_buffer.erase(m_long_term_image_buffer.begin(),
                                       m_long_term_image_buffer.end());
    }

    // Free input image buffer.
    ::delete_stereo_image(m_input_image_buffer);

    ARMARX_DEBUG << "Disconnected " << getName() << ".";
}


void
component::onExitImageProcessor()
{
    ARMARX_DEBUG << "Exiting " << getName() << ".";
    ARMARX_VERBOSE << "Exited " << getName() << ".";
}


void
component::run_input_synchronisation()
{
    ARMARX_DEBUG << "Started input synchronisation task.";

    // TODO: remove.
    // "201": {"angle": 19, "offset_rl": 60, "offset_h": -810, "offset_d": -1300}
    m_table_angle = 19;
    m_table_offset_rl = 60;
    m_table_offset_h = -810;
    m_table_offset_d = -1300;

    while (not m_input_synchronisation_task->isStopped())
    {
        std::unique_lock<std::mutex> signal_lock{m_input_proc_mutex};
        m_proc_signal.wait(
            signal_lock,
            [&]() -> bool
            {
                ARMARX_DEBUG << "Received notify in input synchronization thread.";

                ARMARX_DEBUG << VAROUT(m_timestamp_last_input_image.count());
                ARMARX_DEBUG << VAROUT(m_timestamp_last_detected_objects.count());
                ARMARX_DEBUG << VAROUT(m_timestamp_last_body_pose.count());
                ARMARX_DEBUG << VAROUT(m_timestamp_last_hand_pose.count());

                // If input synchronisation task has been stopped, exit immediately.
                if (m_input_synchronisation_task->isStopped())
                    return true;

                // If manual bounding box files are replayed.
                if (m_ignore_cnns and m_timestamp_last_input_image != ::timestamp_invalid)
                    return true;

                // Wait until all timestamps are valid.
                return m_timestamp_last_input_image != ::timestamp_invalid
                    and m_timestamp_last_detected_objects != ::timestamp_invalid
                    and m_timestamp_last_body_pose != ::timestamp_invalid
                    and m_timestamp_last_hand_pose != ::timestamp_invalid
                    and m_timestamp_last_body_pose == m_timestamp_last_hand_pose;
            }
        );

        // Break the loop if the input synchronisation task has been stopped.
        if (m_input_synchronisation_task->isStopped()) break;

        ARMARX_DEBUG << "Continuing input synchronization.";

        std::vector<corcal::core::detected_object> objects;
        ch::microseconds timestamp_last_detected_objects;
        ch::microseconds timestamp_last_hand_pose;
        if (not m_ignore_cnns)
        {
            ARMARX_DEBUG << "Creating working copies and releasing the lock.";
            timestamp_last_detected_objects = m_timestamp_last_detected_objects;
            timestamp_last_hand_pose = m_timestamp_last_hand_pose;
            // Invalidate timestamps.
            m_timestamp_last_input_image = m_timestamp_last_detected_objects =
                m_timestamp_last_body_pose = m_timestamp_last_hand_pose = ::timestamp_invalid;
            ::CByteImage** input_image_detected_objects
                = get_closest_image(timestamp_last_detected_objects);
            ::CByteImage** input_image_hand_pose
                = get_closest_image(timestamp_last_hand_pose);
            std::vector<corcal::core::known_object::ptr> preprocessed_objects
                = m_memory.known_objects();

            ARMARX_DEBUG << "Running input processing.";
            const auto proc_start = ch::high_resolution_clock::now();
            objects = process_inputs(
                preprocessed_objects,
                input_image_detected_objects, timestamp_last_detected_objects,
                input_image_hand_pose, timestamp_last_hand_pose,
                ch::milliseconds{std::max(getProperty<int>("bounding_box_smoothing").getValue(), 0)}
            );
            // Don't use any buffer from this point on, they may be in an invalid state by then.
            signal_lock.unlock();

            ARMARX_DEBUG << "Lock released.";
            const ch::milliseconds proc_duration = ch::duration_cast<ch::milliseconds>(
                ch::high_resolution_clock::now() - proc_start);
            ARMARX_DEBUG << "Input processing finished in " << proc_duration << ".";

            ARMARX_DEBUG << "Cleaning up working copies.";
            ::delete_stereo_image(input_image_detected_objects);
            ::delete_stereo_image(input_image_hand_pose);
        }
        else
        {
            objects = get_objects_from_file();
            timestamp_last_detected_objects = m_timestamp_last_input_image;
            timestamp_last_hand_pose = m_timestamp_last_input_image;
            // Invalidate timestamps.
            m_timestamp_last_input_image = m_timestamp_last_detected_objects =
                m_timestamp_last_body_pose = m_timestamp_last_hand_pose = ::timestamp_invalid;

            ::CByteImage** input_image_detected_objects =
                get_closest_image(timestamp_last_detected_objects);
            pcl::PointCloud<pcl::PointXYZ>::Ptr darknet_pointcloud{};

            const unsigned int height =
                static_cast<unsigned int>(input_image_detected_objects[1]->height);
            const unsigned int width =
                static_cast<unsigned int>(input_image_detected_objects[1]->width);

            ARMARX_DEBUG << "Creating pointclouds...";
            {
                std::lock_guard<std::mutex> lock{m_table_hack_mutex};
                const auto start_time = ch::high_resolution_clock::now();
                darknet_pointcloud =
                    functions::cvt_to_point_cloud(*input_image_detected_objects[1], m_table_angle);
                const ch::milliseconds duration = ch::duration_cast<ch::milliseconds>(
                    ch::high_resolution_clock::now() - start_time);
                ARMARX_DEBUG << "Creating pointclouds took " << duration << ".";
            }

            // Sanity checks.
            {
                ARMARX_CHECK_EQUAL(darknet_pointcloud->height, height);
                ARMARX_CHECK_EQUAL(darknet_pointcloud->width, width);
            }

            ARMARX_DEBUG << "Converting to, and publishing debug inspection pointcloud...";
            {
                const auto start_time = ch::high_resolution_clock::now();
                ax::DebugDrawerPointCloud point_cloud{};

                for (unsigned int y = 0; y < height; ++y) for (unsigned int x = 0; x < width; ++x)
                {
                    const unsigned int offset = y * width + x;

                    ax::DebugDrawerPointCloudElement point{
                        darknet_pointcloud->points[offset].x,
                        darknet_pointcloud->points[offset].y,
                        darknet_pointcloud->points[offset].z
                    };
                    point_cloud.points.push_back(std::move(point));
                }

                m_pointcloud_listener->pointcloud_generated(
                    std::move(point_cloud), timestamp_last_detected_objects.count());

                const ch::milliseconds duration = ch::duration_cast<ch::milliseconds>(
                    ch::high_resolution_clock::now() - start_time
                );
                ARMARX_DEBUG << "Converting to, and publishing debug inspection pointcloud took "
                             << duration << ".";
            }

            ARMARX_DEBUG << "Cleaning up working copies.";
            ::delete_stereo_image(input_image_detected_objects);
        }

        ARMARX_VERBOSE << "Publishing processed inputs.";
        m_object_instance_listener->object_instances_detected(
            objects,
            timestamp_last_detected_objects.count(),
            timestamp_last_hand_pose.count()
        );
    }

    ARMARX_DEBUG << "Exiting input synchronisation task.";
}


void
component::process()
{
    // Wait for images.
    {
        const ice::Time timeout = ice::Time::seconds(1);
        if (not waitForImages(m_image_provider_id, timeout))
        {
            std::lock_guard lock(m_input_proc_mutex);
            ARMARX_WARNING << "Timeout while waiting for images (" << timeout << "). "
                           << "Invalidating timestamp.";
            m_timestamp_last_input_image = ::timestamp_invalid;
            return;
        }
    }

    std::lock_guard lock(m_input_proc_mutex);

    ARMARX_DEBUG << "Buffering raw RGB-D input images.";

    ax::MetaInfoSizeBasePtr info;
    const int num_images = getImages(m_image_provider_id, m_input_image_buffer, info);
    m_timestamp_last_input_image = ch::microseconds(info->timeProvided);

    ARMARX_DEBUG << "Creating copies of the input images and saving them in long term buffer.";
    if (num_images != 0)
    {
        ::CByteImage** deep_copy;

        // Print information about relevant long term image buffer states.
        if (m_long_term_image_buffer.size() == 0)
        {
            ARMARX_INFO << "Long term image buffer is empty and will now be filled.";
        }
        else if (m_long_term_image_buffer.size() == m_long_term_image_buffer_max_size - 1)
        {
            ARMARX_IMPORTANT << "Long term image buffer about to reach maximum size.  If you still "
                             << "receive messages about cache misses, you should increase the "
                             << "buffer size to improve accuracy.  Please also be aware of "
                             << "potential frame drops if frames are transmitted over LAN.";
        }

        // Set deep_copy to either an already existing object in the buffer or create a new one.
        if (m_long_term_image_buffer.size() == m_long_term_image_buffer_max_size)
        {
            ARMARX_DEBUG << "Long term image buffer full - overriding oldest image.";
            auto it = m_long_term_image_buffer.begin();
            deep_copy = it->second;
            m_long_term_image_buffer.erase(it);
        }
        else
        {
            ARMARX_DEBUG << "Adding new image to long term image buffer.";
            vx::ImageProviderInfo image_provider_info = getImageProvider(m_image_provider_id);
            deep_copy = new ::CByteImage*[2];
            deep_copy[0] = vx::tools::createByteImage(image_provider_info);
            deep_copy[1] = vx::tools::createByteImage(image_provider_info);
        }

        // Actually write images to buffer.
        ::ImageProcessor::CopyImage(m_input_image_buffer[0], deep_copy[0]);
        ::ImageProcessor::CopyImage(m_input_image_buffer[1], deep_copy[1]);
        m_long_term_image_buffer[m_timestamp_last_input_image] = deep_copy;

        ARMARX_DEBUG << "Notifying input synchronization thread.";
        m_proc_signal.notify_one();
    }
    else
    {
        ARMARX_WARNING << "Didn't receive an image.";
    }
}


void
component::reportDetectedObjects(
    const std::vector<vx::yolo::DetectedObject>& detected_objects,
    ice::Long timestamp,
    const ice::Current&)
{
    std::lock_guard<std::mutex> lock{m_input_proc_mutex};

    ARMARX_DEBUG << "Buffering detected objects from Darknet.";
    m_timestamp_last_detected_objects = ch::microseconds(timestamp);
    if (m_use_manual_timestamps)
    {
        m_memory.now(m_timestamp_last_detected_objects);
    }
    m_memory.make_observations(functions::cvt_to_corcal_observations(
        detected_objects, m_timestamp_last_detected_objects));

    m_proc_signal.notify_one();
}


void
component::report2DHandKeypointsNormalized(
    const ax::Keypoint2DMapList& hand_pose_2d,
    ice::Long timestamp,
    const ice::Current&)
{
    std::lock_guard<std::mutex> lock(m_input_proc_mutex);

    ARMARX_DEBUG << "Buffering 2D hand keypoints from OpenPose.";
    m_timestamp_last_hand_pose = ch::microseconds(timestamp);
    if (m_use_manual_timestamps)
    {
        m_memory.now(m_timestamp_last_hand_pose);
    }
    m_memory.make_observations(
        functions::cvt_to_corcal_observations(hand_pose_2d, m_timestamp_last_hand_pose));
    m_hand_pose_buffer = hand_pose_2d;

    m_proc_signal.notify_one();
}


void
component::report2DKeypointsNormalized(
    const ax::Keypoint2DMapList& body_pose_2d,
    ice::Long timestamp,
    const ice::Current&)
{
    std::lock_guard<std::mutex> lock{m_input_proc_mutex};

    ARMARX_DEBUG << "Buffering 2D pose keypoints from OpenPose.";
    m_timestamp_last_body_pose = ch::microseconds(timestamp);
    m_body_pose_buffer = body_pose_2d;

    m_proc_signal.notify_one();
}


void
component::table_location_hack(
    double angle,
    double offset_rl,
    double offset_h,
    double offset_d,
    const ice::Current&)
{
    std::lock_guard<std::mutex> lock{m_table_hack_mutex};
    ARMARX_DEBUG << "Got table location update";
    m_table_angle = angle;
    m_table_offset_rl = offset_rl;
    m_table_offset_h = offset_h;
    m_table_offset_d = offset_d;
}


std::vector<corcal::core::detected_object>
component::get_objects_from_file()
{
    const fs::path boxes_file{getProperty<std::string>("get_boxes_from")};
    std::ifstream objects_frame_file{boxes_file};
    json objects_frame_json;
    objects_frame_file >> objects_frame_json;
    std::vector<corcal::core::detected_object> detected_objects = objects_frame_json;
    return detected_objects;
}


void
component::reset_memory(const ice::Current&)
{
    ARMARX_DEBUG << "Resetting memory now.";
    m_memory.reset();
    // Invalidate timestamps.
    m_timestamp_last_input_image = m_timestamp_last_detected_objects = m_timestamp_last_body_pose =
        m_timestamp_last_hand_pose = ::timestamp_invalid;
    // Free long term image buffer.
    {
        auto it = m_long_term_image_buffer.begin();
        while (it != m_long_term_image_buffer.end())
        {
            ::CByteImage** stereo_image = it->second;
            ::delete_stereo_image(stereo_image);
            std::advance(it, 1);
        }

        m_long_term_image_buffer.erase(m_long_term_image_buffer.begin(),
                                       m_long_term_image_buffer.end());
    }
}


void
component::use_manual_timestamps(bool enable, const ice::Current&)
{
    m_use_manual_timestamps = enable;
}


::CByteImage**
component::get_closest_image(const ch::microseconds& timestamp) const
{
    ::CByteImage** closest_image;

    // If the timestamp is in the buffer, use it.
    if (m_long_term_image_buffer.count(timestamp) == 1)
    {
        closest_image = m_long_term_image_buffer.at(timestamp);
    }
    // If cache does not contain the given timestamp, find the closest match.
    else
    {
        ARMARX_IMPORTANT << "Cache miss for given timestamp in long term image buffer. Using "
                         << "surrogates instead.";

        // Find two candidates, one which is higher than or equal to the timestamp, one which is
        // lower.
        ARMARX_DEBUG << "Finding two candidates, size is " << m_long_term_image_buffer.size()
                     << ".";
        std::map<ch::microseconds, ::CByteImage**>::const_iterator high_candidate
            = m_long_term_image_buffer.lower_bound(timestamp);
        std::map<ch::microseconds, ::CByteImage**>::const_iterator low_candidate
            = std::prev(high_candidate);  // May be invalid if high_candidate == begin.

        if (high_candidate == m_long_term_image_buffer.end())
        {
            // If there was no element higher than or equal to the timestamp, use the previous
            // element.
            closest_image = low_candidate->second;
        }
        else if (high_candidate == m_long_term_image_buffer.begin())
        {
            // If the first element is higher than or equal to the timestamp, return that one.
            closest_image = high_candidate->second;
        }
        else
        {
            // If some element in between begin and end was found to be greater than or equal the
            // timestamp, calculate the difference of timestamp and key and compare them directly.
            if ((timestamp - low_candidate->first) < (high_candidate->first - timestamp))
                closest_image = low_candidate->second;
            else
                closest_image = high_candidate->second;
        }
    }

    // Create copy of closest match and return.
    ::CByteImage** output_copy = new ::CByteImage*[2];
    output_copy[0] = vx::tools::createByteImage(m_image_provider_info);
    output_copy[1] = vx::tools::createByteImage(m_image_provider_info);
    ::ImageProcessor::CopyImage(closest_image[0], output_copy[0]);
    ::ImageProcessor::CopyImage(closest_image[1], output_copy[1]);
    return output_copy;
}


std::vector<corcal::core::detected_object>
component::process_inputs(
    const std::vector<corcal::core::known_object::ptr>& objects,
    ::CByteImage** input_image_detected_objects,
    const ch::microseconds& detected_objects_timestamp,
    ::CByteImage** input_image_hand_pose,
    const ch::microseconds& hand_pose_timestamp,
    const ch::milliseconds& bounding_box_smoothing) const
{
    pcl::PointCloud<pcl::PointXYZ>::Ptr darknet_pointcloud{}, openpose_pointcloud{};
    float table_orl = 0;
    float table_oh = 0;
    float table_od = 0;

    const unsigned int height = static_cast<unsigned int>(input_image_detected_objects[1]->height);
    const unsigned int width = static_cast<unsigned int>(input_image_detected_objects[1]->width);

    // Sanity checks.
    {
        ARMARX_CHECK_EQUAL(static_cast<unsigned int>(input_image_detected_objects[0]->height),
                           height);
        ARMARX_CHECK_EQUAL(static_cast<unsigned int>(input_image_detected_objects[0]->width),
                           width);
        ARMARX_CHECK_EQUAL(static_cast<unsigned int>(input_image_hand_pose[0]->height), height);
        ARMARX_CHECK_EQUAL(static_cast<unsigned int>(input_image_hand_pose[0]->width), width);
        ARMARX_CHECK_EQUAL(static_cast<unsigned int>(input_image_hand_pose[1]->height), height);
        ARMARX_CHECK_EQUAL(static_cast<unsigned int>(input_image_hand_pose[1]->width), width);
    }

    ARMARX_DEBUG << "Creating pointclouds...";
    {
        std::lock_guard<std::mutex> lock{m_table_hack_mutex};
        table_orl = static_cast<float>(m_table_offset_rl);
        table_oh = static_cast<float>(m_table_offset_h);
        table_od = static_cast<float>(m_table_offset_d);
        const auto start_time = ch::high_resolution_clock::now();
        darknet_pointcloud = functions::cvt_to_point_cloud(*input_image_detected_objects[1],
                                                           m_table_angle);
        openpose_pointcloud = functions::cvt_to_point_cloud(*input_image_hand_pose[1],
                                                            m_table_angle);
        const ch::milliseconds duration = ch::duration_cast<ch::milliseconds>(
            ch::high_resolution_clock::now() - start_time);
        ARMARX_DEBUG << "Creating pointclouds took " << duration << ".";
    }

    // Sanity checks.
    {
        ARMARX_CHECK_EQUAL(darknet_pointcloud->height, height);
        ARMARX_CHECK_EQUAL(darknet_pointcloud->width, width);
        ARMARX_CHECK_EQUAL(openpose_pointcloud->height, height);
        ARMARX_CHECK_EQUAL(openpose_pointcloud->width, width);
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr darknet_pointcloud_filtered{
        new pcl::PointCloud<pcl::PointXYZ>{}};

    //darknet_pointcloud_filtered = darknet_pointcloud;

    corcal::core::detected_object debug_table;
    debug_table.certainty = 1;
    debug_table.class_index = 0;
    debug_table.class_name = "table";
    debug_table.instance_name = "the_table";
    debug_table.colour.r = 128;
    debug_table.colour.g = 255;
    debug_table.colour.b = 0;

    // Find table and crop it from darknet_pc and openpose_pc.
    {
        /* Unreliable code to find table
        // NOTE: RANSAC was too unreliable for the time being, table parameters were derived manually and are loaded
        //       from a config file now

        //        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered(new pcl::PointCloud<pcl::PointXYZ>);
        //        {
        //            pcl::VoxelGrid<pcl::PointXYZ> vg;
        //            vg.setInputCloud(darknet_pointcloud);
        //            const float voxel_size = 10; // in [mm]
        //            vg.setLeafSize(voxel_size, voxel_size, voxel_size);
        //            vg.filter(*cloud_filtered);
        //        }

        //        // Pass filter to get rid of the background
        //        {
        //            pcl::PassThrough<pcl::PointXYZ> pass_x{};
        //            pass_x.setInputCloud(cloud_filtered);
        //            pass_x.setFilterFieldName("z");
        //            pass_x.setFilterLimits(-3500, 0);
        //            pass_x.filter(*cloud_filtered);

        //            pcl::PassThrough<pcl::PointXYZ> pass_y{};
        //            pass_y.setInputCloud(cloud_filtered);
        //            pass_y.setFilterFieldName("y");
        //            pass_y.setFilterLimits(-1000, 3000);
        //            pass_y.filter(*cloud_filtered);
        //        }

        //        // Find indices of points belonging to the table relative to the filtered cloud
        //        pcl::PointIndices::Ptr inliers_indices{new pcl::PointIndices{}};
        //        {
        //            pcl::ModelCoefficients coefficients{};

        //            // Create the segmentation object
        //            pcl::SACSegmentation<pcl::PointXYZ> table_segment{};
        //            table_segment.setOptimizeCoefficients(true);
        //            table_segment.setModelType(pcl::SACMODEL_PLANE);
        //            table_segment.setMethodType(pcl::SAC_RANSAC);
        //            table_segment.setDistanceThreshold(0.5);

        //            // Perform RANSAC segmentation to find the table
        //            table_segment.setInputCloud(cloud_filtered);
        //            table_segment.segment(*inliers_indices, coefficients);
        //        }

        //        // Remove all points which don't belong to the table
        //        {
        //            pcl::ExtractIndices<pcl::PointXYZ> extract{};
        //            extract.setInputCloud(cloud_filtered);
        //            extract.setIndices(inliers_indices);
        //            extract.setNegative(false);
        //            extract.filter(*cloud_filtered);
        //        }
        */

        // Get the minima and maxima of the table (bounding box of the table).
        const pcl::PointXYZ table_extends{350, 20, 350};  // From center (CoM), so x2 for actual
                                                          // extends.
        const pcl::PointXYZ min_p{table_orl - table_extends.x,
                                  table_oh - table_extends.y,
                                  table_od - table_extends.z};
        const pcl::PointXYZ max_p{table_orl + table_extends.x,
                                  table_oh + table_extends.y,
                                  table_od + table_extends.z};
        //pcl::getMinMax3D(*cloud_filtered, min_p, max_p);

        // Set debug_table extends as bounding_box.
        {
            vx::BoundingBox3D bounding_box;
            bounding_box.x0 = min_p.x;
            bounding_box.x1 = max_p.x;
            bounding_box.y0 = min_p.y;
            bounding_box.y1 = max_p.y;
            bounding_box.z0 = min_p.z;
            bounding_box.z1 = max_p.z;
            debug_table.bounding_box = std::move(bounding_box);
        }

        // Remove all points which are inside the table's bounding box.
        {
            pcl::CropBox<pcl::PointXYZ> box_filter{};
            box_filter.setMin(Eigen::Vector4f{min_p.x, min_p.y, min_p.z, 1});
            box_filter.setMax(Eigen::Vector4f{max_p.x, max_p.y, max_p.z, 1});
            box_filter.setNegative(true);
            box_filter.setKeepOrganized(true);
            box_filter.setUserFilterValue(0);
            box_filter.setInputCloud(darknet_pointcloud);
            box_filter.filter(*darknet_pointcloud_filtered);
        }

        // Set dimensions.
        darknet_pointcloud_filtered->width = darknet_pointcloud->width;
        darknet_pointcloud_filtered->height = darknet_pointcloud->height;
    }

    ARMARX_CHECK_EQUAL(darknet_pointcloud->points.size(),
                       darknet_pointcloud_filtered->points.size());

    // TODO: Kick off dedicated thread for this while segmenting the sub-pointclouds.
    ARMARX_DEBUG << "Converting to, and publishing debug inspection pointcloud...";
    {
        const auto start_time = ch::high_resolution_clock::now();
        ax::DebugDrawerPointCloud point_cloud{};

        const unsigned int width = darknet_pointcloud->width;
        const unsigned int height = darknet_pointcloud->height;

        for (unsigned int y = 0; y < height; ++y) for (unsigned int x = 0; x < width; ++x)
        {
            const unsigned int offset = y * width + x;

            ax::DebugDrawerPointCloudElement point{
                darknet_pointcloud->points[offset].x,
                darknet_pointcloud->points[offset].y,
                darknet_pointcloud->points[offset].z
            };
            point_cloud.points.push_back(std::move(point));
        }

        m_pointcloud_listener->pointcloud_generated(std::move(point_cloud),
                                                    detected_objects_timestamp.count());

        const ch::milliseconds duration = ch::duration_cast<ch::milliseconds>(
            ch::high_resolution_clock::now() - start_time
        );
        ARMARX_DEBUG << "Converting to, and publishing debug inspection pointcloud took "
                     << duration << ".";
    }

    std::vector<corcal::core::detected_object> conv_objects;

    ARMARX_DEBUG << "Estimating depth and converting to interface types";
    {
        const auto start_time = ch::high_resolution_clock::now();

        for (corcal::core::known_object::ptr known_object : objects)
        {
            pcl::PointCloud<pcl::PointXYZ>::Ptr pointcloud;
            corcal::core::observation::ptr observation = known_object->current_observation();

            if (observation->seen_at() == detected_objects_timestamp)
                pointcloud = darknet_pointcloud_filtered;
            else if (observation->seen_at() == hand_pose_timestamp)
                pointcloud = openpose_pointcloud;
            else
                continue;

            // Try to estimate bounding box given pointcloud.
            vx::BoundingBox3D bounding_box =
                functions::estimate_bounding_box(pointcloud, observation);

            // Try error correction / recovery, or continue if recovery not possible.
            if (std::isnan(bounding_box.x0) and std::isnan(bounding_box.x1)
                and std::isnan(bounding_box.y0) and std::isnan(bounding_box.y1))
            {
                ARMARX_VERBOSE << "Could not estimate bounding box for object "
                               << observation->candidates().at(0).class_name() << ".";
                continue;
            }
            if (std::isnan(bounding_box.z0) and std::isnan(bounding_box.z1))
            {
                bounding_box.z0 = known_object->last_zmin();
                bounding_box.z1 = known_object->last_zmax();
            }
            else
            {
                known_object->last_zmin(bounding_box.z0);
                known_object->last_zmax(bounding_box.z1);
            }

            // Now that it is certain that the bounding box is valid, save it for later reference.
            if (bounding_box_smoothing != ch::milliseconds::zero())
            {
                ARMARX_DEBUG << "Smoothing bounding boxes now.";
                vx::BoundingBox3D smoothed_bounding_box = known_object->average_bounding_boxes(
                    bounding_box,
                    observation->seen_at(),
                    bounding_box_smoothing
                );
                bounding_box = smoothed_bounding_box;
            }
            else
            {
                ARMARX_DEBUG << "Using raw bounding box, skipping smoothing.";
            }

            observation->bounding_box(bounding_box);

            ARMARX_CHECK_LESS_EQUAL(bounding_box.x0, bounding_box.x1);
            ARMARX_CHECK_LESS_EQUAL(bounding_box.y0, bounding_box.y1);
            ARMARX_CHECK_LESS_EQUAL(bounding_box.z0, bounding_box.z1);

            corcal::core::detected_object conv_object;
            corcal::core::candidate candidate = observation->candidates().at(0);
            conv_object.bounding_box = bounding_box;
            conv_object.past_bounding_box = known_object->past_observation()->bounding_box();
            conv_object.certainty = candidate.certainty();
            conv_object.class_index = candidate.class_index();
            conv_object.class_name = candidate.class_name();
            conv_object.instance_name = known_object->id();
            conv_object.colour = candidate.colour();
            conv_objects.push_back(std::move(conv_object));
        }

        const ch::milliseconds duration = ch::duration_cast<ch::milliseconds>(
            ch::high_resolution_clock::now() - start_time
        );
        ARMARX_DEBUG << "Estimating depth and converting to interface types took " << duration
                     << ".";
    }

    // TODO: Debug code.
    //conv_objects.push_back(debug_table);

    return conv_objects;
}


ax::PropertyDefinitionsPtr
component::createPropertyDefinitions()
{
    ax::PropertyDefinitionsPtr defs{new ax::ComponentPropertyDefinitions{getConfigIdentifier()}};

    defs->defineOptionalProperty<std::string>(
        "topic_name_3d_object_instances",
        "ObjectInstances3D",
        "Topic name under which the 3D object instances are published."
    );
    defs->defineOptionalProperty<std::string>(
        "topic_name_2d_body_pose",
        "OpenPoseEstimation2D",
        "Topic name where 2D human poses are published."
    );
    defs->defineOptionalProperty<std::string>(
        "topic_name_2d_hand_pose",
        "OpenPoseHandEstimation2D",
        "Topic name where 2D hand poses are published."
    );
    defs->defineOptionalProperty<std::string>(
        "get_boxes_from",
        "",
        "Path to a JSON file with bounding boxes");
    defs->defineOptionalProperty<std::string>(
        "topic_name_objects",
        "DarknetObjectDetectionResult",
        "Topic name where detected objects are published."
    );
    defs->defineOptionalProperty<std::string>(
        "image_provider",
        "ImageProvider",
        "Image provider as data source."
    );
    defs->defineOptionalProperty<float>(
        "memory_initial_certainty",
        0.4f,
        "Minimum initial certainty for new objects to be recognised as such (and not be discarded "
        "as noise).  Doesn't affect subsequent observations of known objects."
    ).setMin(0.0).setMax(1.0);
    defs->defineOptionalProperty<int>(
        "memory_remember_duration",
        750,
        "Time in [ms] to pass before an object is completely forgotten if no new observations are "
        "made that match it."
    ).setMin(0);
    defs->defineOptionalProperty<int>(
        "bounding_box_smoothing",
        -1,
        "Time in [ms] for the last bounding boxes considered for smoothing (averaging).  Negative"
        "values: don't smooth."
    );
    defs->defineOptionalProperty<int>(
        "long_term_image_buffer_size",
        30,
        "Size [in frames] of the long term image buffer that are buffered in total."
    ).setMin(0);

    return defs;
}
