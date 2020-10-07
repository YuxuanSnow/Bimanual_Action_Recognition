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
 * @package    corcal::components::cnnreplay
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2019
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#include <corcal/components/cnnreplay/component.h>


// STD/STL
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
namespace fs = std::filesystem;

// Boost
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>

// JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// ArmarX
#include <ArmarXCore/core/time/TimeUtil.h>
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>
namespace ax = armarx;
namespace vx = visionx;

// corcal
#include <corcal/core/ssr.h>
using namespace corcal::core;
using namespace corcal::components::cnnreplay;


/**
 * JSON conversion functions
 */
namespace armarx
{
    void from_json(const json& j, armarx::Keypoint2D& kp)
    {
        j.at("label").get_to(kp.label);
        j.at("x").get_to(kp.x);
        j.at("y").get_to(kp.y);
        j.at("confidence").get_to(kp.confidence);
        kp.dominantColor = armarx::DrawColor24Bit{
            j["dominantColor"][0],
            j["dominantColor"][1],
            j["dominantColor"][2]};
    }
}
namespace visionx
{
    void to_json(json& j, const visionx::BoundingBox3D bb)
    {
        j = json
        {
            {"x0", bb.x0},
            {"y0", bb.y0},
            {"z0", bb.z0},
            {"x1", bb.x1},
            {"y1", bb.y1},
            {"z1", bb.z1}
        };
    }

    void from_json(const json& j, visionx::BoundingBox2D& bb)
    {
        j.at("x").get_to(bb.x);
        j.at("y").get_to(bb.y);
        j.at("w").get_to(bb.w);
        j.at("h").get_to(bb.h);
    }
}
namespace visionx::yolo
{
    void from_json(const json& j, visionx::yolo::ClassCandidate& cc)
    {
        j.at("class_name").get_to(cc.className);
        j.at("class_index").get_to(cc.classIndex);
        j.at("certainty").get_to(cc.certainty);
        cc.color = armarx::DrawColor24Bit{j["colour"][0], j["colour"][1], j["colour"][2]};
    }

    void from_json(const json& j, visionx::yolo::DetectedObject& det)
    {
        j.at("candidates").get_to(det.candidates);
        j.at("class_count").get_to(det.classCount);
        j.at("object_name").get_to(det.objectName);
        j.at("bounding_box").get_to(det.boundingBox);
    }
}
namespace corcal::core
{
    void to_json(json& j, const detected_object& det)
    {
        j = json
        {
            {"class_name", det.class_name},
            {"class_index", det.class_index},
            {"instance_name", det.instance_name},
            {"certainty", det.certainty},
            {"bounding_box", det.bounding_box},
            {"past_bounding_box", det.past_bounding_box},
            {"colour", {det.colour.r, det.colour.g, det.colour.b}}
        };
    }
}
namespace
{
    struct table_parameters
    {
        double angle;
        double offset_rl;
        double offset_h;
        double offset_d;
    };

    struct relation
    {
        unsigned int subject_index;
        unsigned int object_index;
        std::string relation_name;

        relation(unsigned int si, unsigned int oi, const std::string& rel) :
            subject_index{si}, object_index{oi}, relation_name{rel}
        {}
    };

    void from_json(const json& j, ::table_parameters& tp)
    {
        j.at("angle").get_to(tp.angle);
        j.at("offset_rl").get_to(tp.offset_rl);
        j.at("offset_h").get_to(tp.offset_h);
        j.at("offset_d").get_to(tp.offset_d);
    }

    void to_json(json& j, const ::relation& r)
    {
        j = json
        {
            {"subject_index", r.subject_index},
            {"object_index", r.object_index},
            {"relation_name", r.relation_name}
        };
    }
}


const std::string
component::default_name = "cnnreplay";


component::component()
{
    m_catalyst_initialised = false;
}


component::~component()
{
    // pass
}


void
component::onInitCapturingImageProvider()
{
    ARMARX_VERBOSE << "Caching paths...";
    {
        for (const fs::path& subject : {"subject_1", "subject_2", "subject_3", "subject_4",
                                        "subject_5", "subject_6"})
        {
            for (const fs::path& task : {"task_1_k_cooking", "task_2_k_cooking_with_bowls",
                                         "task_3_k_pouring", "task_4_k_wiping", "task_5_k_cereals",
                                         "task_6_w_hard_drive", "task_7_w_free_hard_drive",
                                         "task_8_w_hammering", "task_9_w_sawing"})
            {
                for (const fs::path& take : {"take_0", "take_1", "take_2", "take_3", "take_4",
                                             "take_5", "take_6", "take_7", "take_8", "take_9"})
                {
                    m_rec_paths.push_back(subject / task / take);
                }
            }
        }

        ARMARX_DEBUG << "Done caching paths (" << m_rec_paths.size() << " recordings).";
    }

    // Offer topics.
    {
        const std::string topic_name_2d_body_pose =
            getProperty<std::string>("topic_name_2d_body_pose");
        const std::string topic_name_2d_hand_pose =
            getProperty<std::string>("topic_name_2d_hand_pose");
        const std::string topic_name_objects =
            getProperty<std::string>("topic_name_objects");
        offeringTopic(topic_name_2d_body_pose);
        offeringTopic(topic_name_2d_hand_pose);
        offeringTopic(topic_name_objects);
    }

    usingTopic("corcal_catalyst_3dobjects");
    usingTopic("corcal_ssr_features");

    const int fixed_rec_index = getProperty<int>("replay_single_rec");
    if (fixed_rec_index >= 0)
        m_current_rec_index = static_cast<unsigned int>(fixed_rec_index);
    else
        m_current_rec_index = 0;

    // Set up required properties for the image capturer.
    setNumberImages(2);
    setImageFormat(vx::ImageDimension(640, 480), vx::eRgb, vx::eBayerPatternGr);
    setImageSyncMode(vx::eFpsSynchronization);

    ARMARX_VERBOSE << "Initialized.";
}


void
component::onExitCapturingImageProvider()
{
    for (vx::playback::Playback playback : playbacks)
        playback->stopPlayback();

    playbacks.clear();
}


void
component::onStartCapture(float)
{
    const std::string topic_name_2d_body_pose = getProperty<std::string>("topic_name_2d_body_pose");
    const std::string topic_name_2d_hand_pose = getProperty<std::string>("topic_name_2d_hand_pose");
    const std::string topic_name_objects = getProperty<std::string>("topic_name_objects");
    m_objects_topic = getTopic<vx::yolo::ObjectListener::ProxyType>(topic_name_objects);
    m_pose_body_topic = getTopic<ax::OpenPose2DListener::ProxyType>(topic_name_2d_body_pose);
    m_pose_hand_topic =
        getTopic<ax::OpenPoseHand2DListener::ProxyType>(topic_name_2d_hand_pose);
    m_shutdown = false;
    m_received_3d_objects = false;
    m_received_ssr_feats = false;
}


void
component::onStopCapture()
{
    m_shutdown = true;
}


bool
component::capture(void** image_buffer)
{
    bool success = false;  // Result is true, if at least one of the playbacks provided a picture.

    if (m_catalyst_initialised)
    {
        if (getProperty<bool>("sync"))
        {
            std::unique_lock<std::mutex> signal_lock{m_proc_mutex};

            ARMARX_DEBUG << "Waiting until results are received...";
            m_proc_signal.wait(
                signal_lock,
                [&]() -> bool
                {
                    ARMARX_DEBUG << "Received notify.";
                    return m_shutdown or (m_received_3d_objects and m_received_ssr_feats);
                }
            );
            ARMARX_DEBUG << "Done with everything, continuing now.";

            // Reset flags.
            m_received_3d_objects = m_received_ssr_feats = false;

            // Done, prepare replaying next frame.
            ++m_current_frame_index;
        }
        else
        {
            std::lock_guard<std::mutex> lock{m_proc_mutex};
            ++m_current_frame_index;
        }
    }

    // Connectivity checks.
    if (m_catalyst_initialised)
    {
        try
        {
            ARMARX_DEBUG << "Pinging catalyst now to see if it's still responsive.";
            m_catalyst->ice_ping();
        }
        catch (...)
        {
            ARMARX_WARNING << "Lost connection to corcal_catalyst, trying to reconnect now...";
            m_catalyst_initialised = false;
        }
    }
    if (not m_catalyst_initialised)
    {
        try
        {
            const bool was_initialised = m_catalyst;
            ARMARX_VERBOSE << "Trying to connect to catalyst now.";
            m_catalyst = getProxy<catalyst::component_interface::ProxyType>("corcal_catalyst");
            m_catalyst->ice_ping();  // Provocate an exception if the component is not available.
            m_catalyst->use_manual_timestamps(true);
            ARMARX_VERBOSE << "Trying to subscribe to object instances topic now.";
            table_hack();

            if (was_initialised)
            {
                ARMARX_IMPORTANT << "Reconnected to catalyst, continuing normal operation.";
            }

            m_catalyst_initialised = true;
        }
        catch (...)
        {
            return false;
        }
    }

    // Load next recording if appropriate.
    if (playbacks.empty() or playbacks[0]->getFrameCount() <= m_current_frame_index)
    {
        if (not playbacks.empty())
        {
            ARMARX_DEBUG << "Unloading previous recording...";
            for (vx::playback::Playback playback : playbacks)
            {
                playback->stopPlayback();
            }
            playbacks.clear();

            if (getProperty<int>("replay_single_rec") < 0)  // Negative => disabled, load next rec.
            {
                ++m_current_rec_index;
            }
        }

        m_catalyst->reset_memory();

        m_current_frame_index = 0;
        if (m_current_rec_index >= m_rec_paths.size())
        {
            return false;
        }

        // Table hack.
        table_hack();

        const fs::path rgb_path = get_path(path_type::rgb);
        const fs::path depth_path = get_path(path_type::depth);

        ARMARX_INFO << "Loading `" << rgb_path.string() << "`.";
        ARMARX_DEBUG << "Loading `" << depth_path.string() << "`.";

        playbacks.push_back(vx::playback::newPlayback(rgb_path));
        playbacks.push_back(vx::playback::newPlayback(depth_path));

        const fs::path metadata_csv = rgb_path / fs::path{"metadata.csv"};
        std::ifstream metadata_file{metadata_csv.string()};
        std::string line;
        frameRate = -1;
        while (std::getline(metadata_file, line))
        {
            std::vector<std::string> split;
            boost::algorithm::split(split, line, boost::is_any_of(","));
            if (split.size() == 3 and split[0] == "fps")
            {
                frameRate = boost::lexical_cast<float>(split[2]);
                m_frame_offset =
                    static_cast<long long>((1. / static_cast<double>(frameRate)) * 1000 * 1000);
            }
        }
        if (frameRate < 0)
        {
            ARMARX_ERROR << "Could not determine FPS while reading " << rgb_path.string() << ".";
            throw "Could not determine FPS while reading " + rgb_path.string() + ".";
        }

        ARMARX_DEBUG << "Loaded next recording.";
    }

    ARMARX_CHECK_EQUAL_W_HINT(playbacks.size(), 2,
                              "Playback size is expected to be 2 at this point.");

    // Write image_buffer.
    for (unsigned int i = 0; i < 2; ++i)
    {
        unsigned int frame = m_current_frame_index;
        int repeat_from_frame = getProperty<int>("repeat_from_frame");
        if (repeat_from_frame >= 0 and
            m_current_frame_index > static_cast<unsigned int>(repeat_from_frame))
        {
            frame = static_cast<unsigned int>(repeat_from_frame);
        }
        vx::playback::Playback playback = playbacks[i];
        playback->setCurrentFrame(frame);
        success |= playback->getNextFrame(image_buffer[i]);
    }

    // Repeats the last image if there was any error.
    if (success)
    {
        // Set timestamp.
        const long long recording_offset = m_timestamp_offset * m_current_rec_index;
        const long long frame_offset = m_current_frame_index * m_frame_offset;
        const long long timestamp = recording_offset + frame_offset + m_timestamp_offset;
        updateTimestamp(timestamp);

        ARMARX_VERBOSE << "Replaying detected objects and poses.";
        m_objects_topic->reportDetectedObjects(get_objects(), timestamp);
        m_pose_body_topic->report2DKeypointsNormalized(get_pose_body(), timestamp);
        m_pose_hand_topic->report2DHandKeypointsNormalized(get_pose_hand(), timestamp);
        ARMARX_DEBUG << "Replayed detected objects and poses.";
    }

    return success;
}


void
component::object_instances_detected(
    const std::vector<detected_object>& objects,
    Ice::Long timestamp_darknet,
    Ice::Long timestamp_openpose,
    const Ice::Current&)
{
    const std::lock_guard<std::mutex> lock{m_proc_mutex};

    ARMARX_DEBUG << "Received 3D object instances.";

    if (timestamp_darknet != timestamp_openpose)
    {
        ARMARX_ERROR << "TIMESTAMPS DIFFER!";
    }

    const long long frame_rest = timestamp_darknet % m_timestamp_offset;
    const long long recording_number = ((timestamp_darknet - frame_rest) / m_timestamp_offset) - 1;
    const long long frame_number = frame_rest / m_frame_offset;

    ARMARX_DEBUG << "Got object instances for recording " << recording_number << " frame "
                 << frame_number << " (extracted from " << timestamp_darknet << ").";

    for (const detected_object& object : objects)
    {
        ARMARX_CHECK_LESS_EQUAL(object.bounding_box.x0, object.bounding_box.x1);
        ARMARX_CHECK_LESS_EQUAL(object.bounding_box.y0, object.bounding_box.y1);
        ARMARX_CHECK_LESS_EQUAL(object.bounding_box.z0, object.bounding_box.z1);
    }

    if (getProperty<bool>("sync"))
    {
        if (recording_number != m_current_rec_index)
        {
            ARMARX_ERROR << "Expected recording number " << m_current_rec_index << ", but got "
                         << recording_number << ".";
        }
        if (frame_number != m_current_frame_index)
        {
            ARMARX_ERROR << "Expected frame number " << m_current_frame_index << ", but got "
                         << frame_number << ".";
        }
    }

    if (getProperty<bool>("write_to_disk"))
    {
        ARMARX_DEBUG << "Writing object instances to disk.";
        json ser = objects;
        const std::string filename = "frame_" + std::to_string(m_current_frame_index) + ".json";
        const fs::path out = get_path(path_type::objects_3d) / fs::path{filename};
        std::ofstream o{out.string()};
        o << std::setw(4) << ser << std::endl;
        ARMARX_DEBUG << "Done writing object instances to disk to " << out.string() << ".";
    }

    if (getProperty<bool>("sync"))
    {
        ARMARX_DEBUG << "Done handling 3D object instances, notifying now...";
        m_received_3d_objects = true;
        m_proc_signal.notify_one();
    }
}


void
component::ssr_features_detected(
    const std::vector<detected_object>& objects,
    const std::vector<std::vector<int>>& ssr_matrix_serialised,
    Ice::Long /*timestamp*/,
    const Ice::Current&)
{
    const std::lock_guard<std::mutex> lock{m_proc_mutex};

    ARMARX_DEBUG << "Received SSR features.";

    const std::vector<std::vector<relations>> ssr_matrix = unserialise(ssr_matrix_serialised);
    const unsigned long dim = ssr_matrix.size();

    ARMARX_DEBUG << "Received an " << dim << "x" << dim << " SSR features matrix.";

    std::vector<::relation> rels;

    for (unsigned int i = 0; i < dim; ++i) for (unsigned int j = 0; j < dim; ++j)
    {
        // Skip if i equals j (no self-relations).
        if (i == j) continue;

        const relations& relations_ij = ssr_matrix.at(i).at(j);
        for (const std::string& relation : relations_ij.string_list())
        {
            std::string subject = objects[i].instance_name;
            std::string object = objects[j].instance_name;
            ARMARX_DEBUG << "Found relation `" << relation << "` "
                         << "between `" << subject << "` and `" << object << "`.";

            rels.emplace_back(i, j, relation);
        }
    }

    if (getProperty<bool>("write_to_disk"))
    {
        ARMARX_DEBUG << "Writing SSR features to disk.";
        json ser = rels;
        const std::string filename = "frame_" + std::to_string(m_current_frame_index) + ".json";
        const fs::path out = get_path(path_type::spatial_relations) / fs::path{filename};
        std::ofstream o{out.string()};
        o << std::setw(4) << ser << std::endl;
        ARMARX_DEBUG << "Done writing SSR features to disk to " << out.string() << ".";
    }

    if (getProperty<bool>("sync"))
    {
        ARMARX_DEBUG << "Done handling SSR features, notifying now...";
        m_received_ssr_feats = true;
        m_proc_signal.notify_one();
    }
}


fs::path
component::path_type_to_name(path_type kind) const
{
    switch (kind)
    {
        case path_type::depth:
            return "depth";
        case path_type::rgb:
            return "rgb";
        case path_type::objects_2d:
            return "2d_objects";
        case path_type::objects_3d:
            return "3d_objects";
        case path_type::hand_pose:
            return "hand_pose";
        case path_type::body_pose:
            return "body_pose";
        case path_type::spatial_relations:
            return "spatial_relations";
    }

    return "?";
}


fs::path
component::get_path(const path_type kind) const
{
    if (kind == path_type::rgb or kind == path_type::depth)
    {
        const fs::path out_path{getProperty<std::string>("dataset_path")};
        const fs::path location = m_rec_paths[m_current_rec_index];
        return out_path / location / path_type_to_name(kind);
    }
    else
    {
        const fs::path out_path{getProperty<std::string>("derived_data_path")};
        const fs::path location = m_rec_paths[m_current_rec_index];

        const fs::path path = out_path / location / path_type_to_name(kind);

        if (not fs::is_directory(path))
            fs::create_directories(path);

        return path;
    }
}


vx::yolo::DetectedObjectList
component::get_objects() const
{
    unsigned int frame = m_current_frame_index;
    int repeat_from_frame = getProperty<int>("repeat_from_frame");
    if (repeat_from_frame >= 0 and
        m_current_frame_index > static_cast<unsigned int>(repeat_from_frame))
    {
        frame = static_cast<unsigned int>(repeat_from_frame);
    }
    const fs::path filename{"frame_" + std::to_string(frame) + ".json"};
    const fs::path objects_frame_path = get_path(path_type::objects_2d) / filename;
    std::ifstream objects_frame_file{objects_frame_path.string()};
    json objects_frame_json;
    objects_frame_file >> objects_frame_json;
    vx::yolo::DetectedObjectList detected_objects = objects_frame_json;
    return detected_objects;
}


ax::Keypoint2DMapList
component::get_pose_body() const
{
    unsigned int frame = m_current_frame_index;
    int repeat_from_frame = getProperty<int>("repeat_from_frame");
    if (repeat_from_frame >= 0 and
        m_current_frame_index > static_cast<unsigned int>(repeat_from_frame))
    {
        frame = static_cast<unsigned int>(repeat_from_frame);
    }
    const fs::path filename{"frame_" + std::to_string(frame) + ".json"};
    const fs::path pose_frame_path = get_path(path_type::body_pose) / filename;
    std::ifstream pose_frame_file{pose_frame_path.string()};
    json pose_frame_json;
    pose_frame_file >> pose_frame_json;
    ax::Keypoint2DMapList pose = pose_frame_json;
    return pose;
}


ax::Keypoint2DMapList
component::get_pose_hand() const
{
    unsigned int frame = m_current_frame_index;
    int repeat_from_frame = getProperty<int>("repeat_from_frame");
    if (repeat_from_frame >= 0 and
        m_current_frame_index > static_cast<unsigned int>(repeat_from_frame))
    {
        frame = static_cast<unsigned int>(repeat_from_frame);
    }
    const fs::path filename{"frame_" + std::to_string(frame) + ".json"};
    const fs::path pose_frame_path = get_path(path_type::hand_pose) / filename;
    std::ifstream pose_frame_file{pose_frame_path.string()};
    json pose_frame_json;
    pose_frame_file >> pose_frame_json;
    ax::Keypoint2DMapList pose = pose_frame_json;
    return pose;
}


void
component::table_hack()
{
    const fs::path filepath{getProperty<std::string>("camera_normalization")};

    // Read json file.
    std::ifstream file{filepath.string()};
    json file_json;
    file >> file_json;

    std::vector<unsigned int> key_indices = file_json["key_indices"];
    std::map<std::string, ::table_parameters> map = file_json["map"];

    unsigned int relevant_index = 0;
    for (unsigned int i = 0; i < key_indices.size() - 1; ++i)
    {
        if (key_indices[i] <= m_current_rec_index and m_current_rec_index < key_indices[i+1])
        {
            relevant_index = key_indices[i+1];
            break;
        }
    }

    double angle = 0;  // in degree.
    double offset_rl = 0;
    double offset_h = 0;
    double offset_d = 0;

    // Set parameters from map.
    {
        const std::string index = std::to_string(relevant_index);
        angle = map.at(index).angle;
        offset_rl = map.at(index).offset_rl;
        offset_h = map.at(index).offset_h;
        offset_d = map.at(index).offset_d;
    }

    // Publish table parameters.
    ARMARX_VERBOSE << "Publishing table location now.";
    m_catalyst->table_location_hack(angle, offset_rl, offset_h, offset_d);
}


std::string
component::getDefaultName() const
{
    return component::default_name;
}


ax::PropertyDefinitionsPtr
component::createPropertyDefinitions()
{
    ax::PropertyDefinitionsPtr defs{new ax::ComponentPropertyDefinitions{getConfigIdentifier()}};
    defs->defineRequiredProperty<std::string>(
        "camera_normalization",
        "Camera normalization file."
    );
    defs->defineRequiredProperty<std::string>(
        "dataset_path",
        "Path to the dataset."
    );
    defs->defineRequiredProperty<std::string>(
        "derived_data_path",
        "Derived data path."
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
        "topic_name_objects",
        "DarknetObjectDetectionResult",
        "Topic name where detected objects are published."
    );
    defs->defineOptionalProperty<std::string>(
        "ssr_features_topic",
        "ssr_features",
        "Topic name where the SSR features are published."
    );
    defs->defineOptionalProperty<bool>(
        "write_to_disk",
        false,
        "Whether to write to disk or not."
    );
    defs->defineOptionalProperty<bool>(
        "sync",
        false,
        "Whether to synchronise with the output frequency or not."
    );
    defs->defineOptionalProperty<int>(
        "repeat_from_frame",
        -1,
        "After reaching the given frame, it is repeated for the rest of the recording. -1 = "
        "disabled."
    );
    defs->defineOptionalProperty<int>(
        "replay_single_rec",
        -1,
        "Replays the recording with set ID."
    );
    return defs;
}
