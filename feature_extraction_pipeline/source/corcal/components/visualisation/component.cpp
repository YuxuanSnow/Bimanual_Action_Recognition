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
 * @package    corcal::components::visualisation
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#include <corcal/components/visualisation/component.h>


// STD/STL
#include <algorithm>
#include <chrono>
#include <string>
namespace ch = std::chrono;

// IVT
#include <Image/ImageProcessor.h>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>

// RobotAPI
#include <RobotAPI/libraries/core/Pose.h>

// VisionX
#include <VisionX/tools/ImageUtil.h>

// corcal
#include <corcal/core/ssr.h>


using namespace corcal::core;
using namespace corcal::components::visualisation;


const std::string component::default_name = "visualisation";


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
    ARMARX_DEBUG << "Initialising " << getName();

    // General options
    m_rgb_image_buffer_max_size =
        static_cast<unsigned int>(getProperty<int>("rgb_image_buffer_size"));
    m_draw_layer = 0;

    // Signal dependency on image provider
    m_image_provider_id = getProperty<std::string>("image_provider");
    usingImageProvider(m_image_provider_id);

    // Signal dependency on the pointcloud debug inspection topic
    {
        const std::string catalyst = getProperty<std::string>("input_catalyst");
        usingTopic(catalyst + "_pointcloud");
    }

    // Signal dependency where the 3D object instances are published
    {
        const std::string topic_name = getProperty<std::string>("object_instances_3d");
        usingTopic(topic_name);
    }

    // Signal dependency on the topic where the SSR features are published
    {
        const std::string topic_name = getProperty<std::string>("ssr_features_topic");
        usingTopic(topic_name);
    }

    // Offer debug drawer topic
    offeringTopic("DebugDrawerUpdates");

    ARMARX_DEBUG << "Initialised " << getName();
}


void
component::onConnectImageProcessor()
{
    ARMARX_DEBUG << "Connecting " << getName();

    // Initialise image provider
    m_image_provider_info = getImageProvider(m_image_provider_id);

    // Init input image
    m_input_image_buf = new ::CByteImage*[2];
    m_input_image_buf[0] = visionx::tools::createByteImage(m_image_provider_info);
    m_input_image_buf[1] = visionx::tools::createByteImage(m_image_provider_info);

    // Init debug drawer
    m_debug_drawer = getTopic<armarx::DebugDrawerInterfacePrx>("DebugDrawerUpdates");

    ARMARX_VERBOSE << "Connected " << getName();
}


void
component::onDisconnectImageProcessor()
{
    ARMARX_DEBUG << "Disconnecting " << getName();

    // Free input image
    delete m_input_image_buf[0];
    delete m_input_image_buf[1];
    delete[] m_input_image_buf;

    // Free long term image buffer
    {
        auto it = m_rgb_image_buffer.begin();
        while (it != m_rgb_image_buffer.end())
        {
            ::CByteImage* rgb_image = it->second;
            delete rgb_image;
            std::advance(it, 1);
        }

        m_rgb_image_buffer.erase(m_rgb_image_buffer.begin(), m_rgb_image_buffer.end());
    }

    ARMARX_DEBUG << "Disconnected " << getName();
}


void
component::onExitImageProcessor()
{
    ARMARX_DEBUG << "Exiting " << getName() << ".";
    ARMARX_VERBOSE << "Exited " << getName() << ".";
}


void
component::process()
{
    // Wait for images
    {
        const ch::milliseconds timeout(1000);
        if (not waitForImages(m_image_provider_id, timeout.count()))
        {
            ARMARX_WARNING << "Timeout while waiting for images (" << timeout
                           << "). Invalidating timestamp.";
            return;
        }
    }

    std::lock_guard<std::mutex> lock(m_input_image_mutex);

    ARMARX_DEBUG << "Buffering raw RGB-D input images";

    ::CByteImage* rgb_image = nullptr;
    ch::microseconds timestamp;
    int num_images;
    {
        armarx::MetaInfoSizeBasePtr info;
        num_images = getImages(m_image_provider_id, m_input_image_buf, info);
        timestamp = ch::microseconds(info->timeProvided);
        rgb_image = m_input_image_buf[0];

        if (getProperty<bool>("dull_rgb"))
        {
            const unsigned int width = static_cast<unsigned int>(m_image_provider_info.imageFormat.dimension.width);
            const unsigned int height = static_cast<unsigned int>(m_image_provider_info.imageFormat.dimension.height);

            for (unsigned int y = 0; y < height; ++y) for (unsigned int x = 0; x < width; ++x)
            {
                const unsigned int times = 2;

                const unsigned int offset = y * width + x;
                const unsigned int image_offset = offset * 3;
                float r = rgb_image->pixels[image_offset + /* R = */ 0] + (times * 255);
                float g = rgb_image->pixels[image_offset + /* G = */ 1] + (times * 255);
                float b = rgb_image->pixels[image_offset + /* B = */ 2] + (times * 255);
                rgb_image->pixels[image_offset + /* R = */ 0] = static_cast<unsigned char>(r / (times + 1));
                rgb_image->pixels[image_offset + /* G = */ 1] = static_cast<unsigned char>(g / (times + 1));
                rgb_image->pixels[image_offset + /* B = */ 2] = static_cast<unsigned char>(b / (times + 1));
            }
        }
    }

    ARMARX_DEBUG << "Creating copies of the input images and saving them in long term buffer";
    if (num_images != 0)
    {
        ::CByteImage* deep_copy;

        // Print information about relevant long term image buffer states
        if (m_rgb_image_buffer.size() == 0)
        {
            ARMARX_INFO << "Long term image buffer is empty and will now be filled";
        }
        else if (m_rgb_image_buffer.size() == m_rgb_image_buffer_max_size - 1)
        {
            ARMARX_IMPORTANT << "Long term image buffer about to reach maximum size. If you still receive messages "
                "about cache misses, you should increase the buffer size to improve accuracy. Please also be aware of "
                "potential frame drops if frames are transmitted over LAN.";
        }

        // Set deep_copy to either an already existing object in the buffer or create a new one
        if (m_rgb_image_buffer.size() == m_rgb_image_buffer_max_size)
        {
            ARMARX_DEBUG << "Long term image buffer full - overriding oldest image";
            auto it = m_rgb_image_buffer.begin();
            deep_copy = it->second;
            m_rgb_image_buffer.erase(it);
        }
        else
        {
            ARMARX_DEBUG << "Adding new image to long term image buffer";
            visionx::ImageProviderInfo image_provider_info = getImageProvider(m_image_provider_id);
            deep_copy = visionx::tools::createByteImage(image_provider_info);
        }

        // Actually write images to buffer
        ::ImageProcessor::CopyImage(rgb_image, deep_copy);
        m_rgb_image_buffer[timestamp] = deep_copy;
    }
    else
    {
        ARMARX_WARNING << "Didn't receive an image";
    }
}


armarx::PropertyDefinitionsPtr
component::createPropertyDefinitions()
{
    armarx::PropertyDefinitionsPtr defs{new armarx::ComponentPropertyDefinitions{getConfigIdentifier()}};

    defs->defineOptionalProperty<std::string>("image_provider", "ImageProvider",
        "Image provider as data source"
    );
    defs->defineOptionalProperty<std::string>("input_catalyst", "catalyst",
        "Input catalyst as data source"
    );
    defs->defineOptionalProperty<std::string>("object_instances_3d", "ObjectInstances",
        "Topic name where the 3D object instances are published (from catalyst)"
    );
    defs->defineOptionalProperty<std::string>("ssr_features_topic", "ssr_features",
        "Topic name where the SSR features are published"
    );
    defs->defineOptionalProperty<bool>("dull_rgb", false, "Make RGB in debug pointcloud more dull");
    defs->defineOptionalProperty<int>("rgb_image_buffer_size", 60,
        "Size [in frames] of the long term image buffer that are buffered in total"
    ).setMin(0);

    return defs;
}


void
component::pointcloud_generated(
        const armarx::DebugDrawerPointCloud& pointcloud,
        Ice::Long timestamp,
        const Ice::Current&)
{
    armarx::DebugDrawer24BitColoredPointCloud rgb_pointcloud{};

    // Find corresponding RGB image
    const ::CByteImage* rgb_image = nullptr;
    if (m_rgb_image_buffer.find(ch::microseconds(timestamp)) != m_rgb_image_buffer.end())
    {
        rgb_image = m_rgb_image_buffer.at(ch::microseconds(timestamp));
        ARMARX_DEBUG << "Found corresponding RGB image";
    }
    else
    {
        ARMARX_VERBOSE << "Cache miss while searching for corresponding RGB image";
    }

    const unsigned int width = static_cast<unsigned int>(m_image_provider_info.imageFormat.dimension.width);
    const unsigned int height = static_cast<unsigned int>(m_image_provider_info.imageFormat.dimension.height);

    for (unsigned int y = 0; y < height; ++y) for (unsigned int x = 0; x < width; ++x)
    {
        const unsigned int offset = y * width + x;
        const armarx::DebugDrawerPointCloudElement& point = pointcloud.points[offset];
        armarx::DrawColor24Bit colour{0, 0, 0}; // Default: black

        if (rgb_image != nullptr)
        {
            const unsigned int image_offset = offset * 3;
            colour.r = rgb_image->pixels[image_offset + /* R = */ 0];
            colour.g = rgb_image->pixels[image_offset + /* G = */ 1];
            colour.b = rgb_image->pixels[image_offset + /* B = */ 2];
        }

        armarx::DebugDrawer24BitColoredPointCloudElement rgb_point{point.x, point.y, point.z, colour};
        rgb_pointcloud.points.push_back(std::move(rgb_point));
    }

    // Visualise point cloud and camera frame
    {
        const std::string layer_name = "corcal_pc_debug";
        const std::string pc_name = "corcal_pc";
        const std::string rgbd_cam_name = "rgbd_camera";

        const armarx::Vector3BasePtr position{new armarx::Vector3{0, 0, 0}};
        const armarx::QuaternionBasePtr orientation{new armarx::Quaternion{1.f, 0.f, 0.f, 0.f}};
        const armarx::PoseBasePtr pose{new armarx::Pose{position, orientation}};

        const float pose_visu_scale = 2.f;
        m_debug_drawer->setScaledPoseVisu(layer_name, rgbd_cam_name, pose, pose_visu_scale);
        m_debug_drawer->set24BitColoredPointCloudVisu(layer_name, pc_name, std::move(rgb_pointcloud));
    }
}


void
component::object_instances_detected(
        const std::vector<corcal::core::detected_object>& objects,
        Ice::Long /*timestamp_darknet*/,
        Ice::Long /*timestamp_openpose*/,
        const Ice::Current&)
{
    ARMARX_DEBUG << "Received " << objects.size() << " object instances to draw";

    const std::string layer_name_prefix = "visualisation_bounding_boxes";

    // Iterate layer, clear previous
    {
        const unsigned int max_layers = 5;
        m_debug_drawer->clearLayer(layer_name_prefix + std::to_string(m_draw_layer));
        m_draw_layer = (m_draw_layer + 1) % max_layers;
    }

    const std::string layer_name = layer_name_prefix + std::to_string(m_draw_layer);

    // Iterate over each object and draw it
    for (const corcal::core::detected_object& object : objects)
    {
        const std::string debug_element_name_suffix = object.instance_name;
        const armarx::Vector3BasePtr position{new armarx::Vector3{
            (object.bounding_box.x0 + object.bounding_box.x1) / 2,
            (object.bounding_box.y0 + object.bounding_box.y1) / 2,
            (object.bounding_box.z0 + object.bounding_box.z1) / 2
        }};

        // Draw bounding box
        {
            bool is_past = false;
            for (const visionx::BoundingBox3D& bb : {object.bounding_box})
            {
                const armarx::PoseBasePtr pose{new armarx::Pose{
                    position,
                    armarx::QuaternionBasePtr{new armarx::Quaternion{1.f, 0.f, 0.f, 0.f}}
                }};
                const armarx::Vector3BasePtr dimensions{new armarx::Vector3{
                    bb.x1 - bb.x0,
                    bb.y1 - bb.y0,
                    bb.z1 - bb.z0
                }};

                armarx::DrawColor box_colour;
                if (is_past)
                {
                    box_colour.r = 0.25;
                    box_colour.g = 0.25;
                    box_colour.b = 0.25;
                    box_colour.a = 0.50;
                }
                else
                {
                    const float norm_24bit_to_float = 255; // Factor to scale 24 bit values [0, 255] to float [0, 1]
                    box_colour.r = static_cast<float>(object.colour.r) / norm_24bit_to_float;
                    box_colour.g = static_cast<float>(object.colour.g) / norm_24bit_to_float;
                    box_colour.b = static_cast<float>(object.colour.b) / norm_24bit_to_float;
                    box_colour.a = std::max(object.certainty / 2, 0.4f); // Probability capped to threshold
                }

                m_debug_drawer->setBoxVisu(
                    layer_name,
                    "bb_" + debug_element_name_suffix,
                    pose,
                    dimensions,
                    box_colour
                );

                is_past = true;
            }
        }

        // Draw object instance name as label
        {
            const armarx::DrawColor font_colour{0, 0, 0, 1}; // opaque black
            const int font_size = 11;

            m_debug_drawer->setTextVisu(
                layer_name,
                "label_" + debug_element_name_suffix,
                object.instance_name,
                position,
                font_colour,
                font_size
            );
        }
    }
}


void
component::ssr_features_detected(
        const std::vector<corcal::core::detected_object>& objects,
        const std::vector<std::vector<int>>& ssr_matrix_serialised,
        Ice::Long /*timestamp*/,
        const Ice::Current&)
{
    const std::vector<std::vector<relations>> ssr_matrix = unserialise(ssr_matrix_serialised);
    const unsigned long dim = ssr_matrix.size();

    ARMARX_DEBUG << "Received an " << dim << "x" << dim << " SSR features matrix";

    return;

    const std::string layer_name_prefix = "visualisation_relations";

    // Iterate layer, clear previous
    {
        const unsigned int max_layers = 5;
        m_debug_drawer->clearLayer(layer_name_prefix + std::to_string(m_draw_layer2));
        m_draw_layer2 = (m_draw_layer2 + 1) % max_layers; // safe because typeof(m_draw_layer) = unsigned int
    }

    const std::string layer_name = layer_name_prefix + std::to_string(m_draw_layer2);

    for (unsigned int i = 0; i < dim; ++i) for (unsigned int j = 0; j < dim; ++j)
    {
        // Skip if i equals j (no self-relations)
        if (i == j) continue;

        const relations& relations_ij = ssr_matrix.at(i).at(j);

        for (const std::string& rel : relations_ij.string_list())
        {
            armarx::DrawColor colour{0, 0, 0, 1};
            enum class mode
            {
                static_xyz, // left, right, above, below, in front, behind
                dynamic_no_contact, // getting close, moving apart, stable
                dynamic_contact // moving together, fixed moving together, halting together
            };

            float xo = 0;
            float yo = 0;
            float zo = 0;

            const mode m = mode::static_xyz;
            if (m == mode::static_xyz)
            {
                if (rel == "left of" or rel == "right of")
                {
                    colour.r = 1;

                }
                else if (rel == "above" or rel == "below")
                {
                    colour.g = 1;
                    xo = yo = zo = 5;
                }
                else if (rel == "in front of" or rel == "behind of")
                {
                    colour.b = 1;
                    xo = yo = zo = 10;
                }
                else
                {
                    continue;
                }
            }
            else if (m == mode::dynamic_no_contact)
            {
                if (rel == "getting close")
                {
                    colour.r = 1;
                }
                else if (rel == "stable")
                {
                    continue;
                }
                else if (rel == "moving apart")
                {
                    colour.g = 1;
                }
                else
                {
                    continue;
                }
            }
            else if (m == mode::dynamic_contact)
            {
                if (rel == "moving together")
                {
                    colour.r = 1;
                }
                else if (rel == "fixed moving together")
                {
                    colour.b = 1;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            std::string arrow_name = "rel_" + objects[i].instance_name + "_" + objects[j].instance_name + "_" + rel;
            const armarx::Vector3BasePtr origin{new armarx::Vector3{
                xo + (objects[i].bounding_box.x0 + objects[i].bounding_box.x1) / 2,
                yo + (objects[i].bounding_box.y0 + objects[i].bounding_box.y1) / 2,
                zo + (objects[i].bounding_box.z0 + objects[i].bounding_box.z1) / 2
            }};
            const armarx::Vector3BasePtr target{new armarx::Vector3{
                xo + (objects[j].bounding_box.x0 + objects[j].bounding_box.x1) / 2,
                yo + (objects[j].bounding_box.y0 + objects[j].bounding_box.y1) / 2,
                zo + (objects[j].bounding_box.z0 + objects[j].bounding_box.z1) / 2
            }};
            const armarx::Vector3BasePtr dir{new armarx::Vector3{
                target->x - origin->x,
                target->y - origin->y,
                target->z - origin->z
            }};
            const double length = std::sqrt(std::pow(dir->x, 2) + std::pow(dir->y, 2) + std::pow(dir->z, 2));
            const float width = 4;
            //const float f = 0.003921569f;
            //armarx::DrawColor colour{objects[i].colour.r * f, objects[i].colour.g * f, objects[i].colour.b * f, 1.};

            m_debug_drawer->setArrowVisu(layer_name, arrow_name, origin, dir, colour, static_cast<float>(length), width);
        }

        // TODO: Get mask from GUI
        //relations filter_mask;
        //filter_mask.static_left_of(true);
        //filter_mask.static_right_of(true);

        //const relations relations_ij_filtered = relations_ij.filter(filter_mask);

        //const relations::iterator it = relations_ij.begin();

        //if (it == relations_ij.end())
        //{
        //    ARMARX_WARNING << "IS THE END!" << ssr_matrix_serialised.at(i).at(j) << "x" << ssr_matrix.at(i).at(j).to_int();
        //}

        //for (const relation& relation : relations_ij)
        //{
        //    ARMARX_IMPORTANT << "RELATION: " << relation.name();
        //}

        //m_debug_drawer->setArrowVisu(layer_name,
    }
}
