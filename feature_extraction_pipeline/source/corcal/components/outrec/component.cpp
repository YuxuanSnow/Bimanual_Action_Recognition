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
 * @package    corcal::components::outrec
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2019
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#include <corcal/components/outrec/component.h>


// STD/STL
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>

// Boost
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

// JSON
#include <nlohmann/json.hpp>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>

// VisionX
#include <VisionX/tools/ImageUtil.h>


using json = nlohmann::json;
using namespace corcal::components::outrec;


const std::string
component::default_name = "outrec";


/**
 * JSON conversion functions
 */
namespace armarx
{
    void to_json(json& j, const armarx::Keypoint2D& kp)
    {
        j = json
        {
            {"label", kp.label},
            {"x", kp.x},
            {"y", kp.y},
            {"confidence", kp.confidence},
            {"dominantColor", {kp.dominantColor.r, kp.dominantColor.g, kp.dominantColor.b}}
        };
    }
}
namespace visionx
{
    void to_json(json& j, const visionx::BoundingBox2D& bb)
    {
        j = json
        {
            {"x", bb.x},
            {"y", bb.y},
            {"w", bb.w},
            {"h", bb.h}
        };
    }

    namespace yolo
    {
        void to_json(json& j, const visionx::yolo::ClassCandidate& cc)
        {
            j = json
            {
                {"class_name", cc.className},
                {"class_index", cc.classIndex},
                {"certainty", cc.certainty},
                {"colour", {cc.color.r, cc.color.g, cc.color.b}}
            };
        }

        void to_json(json& j, const visionx::yolo::DetectedObject& det)
        {
            j = json
            {
                {"candidates", det.candidates},
                {"class_count", det.classCount},
                {"object_name", det.objectName},
                {"bounding_box", det.boundingBox}
            };
        }
    }
}


component::component()
{
    // pass
}


component::~component()
{
    // pass
}


void
component::onInitImageProvider()
{
    const std::filesystem::path paths_cache = std::filesystem::path(getProperty<std::string>("paths_cache").getValue()) / std::filesystem::path{"paths.cache"};

    ARMARX_VERBOSE << "Reading paths cache file...";
    {
        std::ifstream paths_cache_file{paths_cache.string()};
        std::string line;
        while (std::getline(paths_cache_file, line))
        {
            m_rec_paths.emplace_back(line);
        }

        ARMARX_DEBUG << "Done reading paths cache file (" << m_rec_paths.size() << " recordings)";
    }

    // Kick off running task
    m_input_synchronisation_task = armarx::RunningTask<component>::pointer_type(new armarx::RunningTask<component>(
        this, &component::provide_images));
    m_input_synchronisation_task->start();

    // Signal dependency on the object detection and pose estimation topics
    {
        const std::string topic_name_2d_body_pos = getProperty<std::string>("topic_name_2d_body_pose");
        const std::string topic_name_2d_hand_pose = getProperty<std::string>("topic_name_2d_hand_pose");
        const std::string topic_name_objects = getProperty<std::string>("topic_name_objects");
        usingTopic(topic_name_2d_body_pos);
        usingTopic(topic_name_2d_hand_pose);
        usingTopic(topic_name_objects);
    }

    m_current_rec_index = static_cast<unsigned int>(getProperty<int>("start_at_rec"));
    m_received_darknet_objects = false;
    m_received_openpose_hand_pose = false;
    m_received_openpose_body_pose = false;

    setNumberImages(2);
    setImageFormat(visionx::ImageDimension(640, 480), visionx::eRgb, visionx::eBayerPatternGr);

    ARMARX_VERBOSE << "Initialised";

    /*
    //setImageSyncMode(visionx::eFpsSynchronization);

//    const std::vector<std::filesystem::path> recordingFiles = [](const std::string & filesStr)
//    {
//        std::vector<std::filesystem::path> files;
//        boost::split(files, filesStr, boost::is_any_of(";"));
//        return files;
//    }(this->getProperty<std::string>("recording_files"));

//    unsigned int refFrameHeight = 0;
//    unsigned int refFrameWidth = 0;
//    unsigned int maxFps = 0;

//    // Instantiate playbacks
//    for (const std::filesystem::path& recordingFile : recordingFiles)
//    {
//        const std::filesystem::path fullPath = basePath != "" ? basePath / recordingFile : recordingFile;
//        visionx::playback::Playback playback = visionx::playback::newPlayback(fullPath);
//        this->playbacks.push_back(playback);

//        if (not playback)
//        {
//            ARMARX_ERROR << "Could not find a playback strategy for '" << fullPath.string() << "'";
//            continue;
//        }

//        // Sanity checks for image heights and widths
//        {
//            // If uninitialised, use first playback as reference
//            if (refFrameHeight == 0 and refFrameWidth == 0)
//            {
//                refFrameHeight = playback->getFrameHeight();
//                refFrameWidth = playback->getFrameWidth();
//            }

//            // Check that neither hight nor width are 0 pixels
//            ARMARX_CHECK_GREATER_W_HINT(playback->getFrameHeight(), 0, "Image source frame height cannot be 0 pixel");
//            ARMARX_CHECK_GREATER_W_HINT(playback->getFrameWidth(), 0, "Image source frame width cannot be 0 pixel");

//            // Check that the frame heights and widths from all image sources are equal
//            ARMARX_CHECK_EQUAL_W_HINT(playback->getFrameHeight(), refFrameHeight, "Image source frames must have the same dimensions");
//            ARMARX_CHECK_EQUAL_W_HINT(playback->getFrameWidth(), refFrameWidth, "Image source frames must have the same dimensions");
//        }

//        // Determine playback with the highest FPS
//        if (playback->getFps() > maxFps)
//        {
//            maxFps = playback->getFps();
//        }
//    }

//    // Determine FPS
//    unsigned int fps = static_cast<unsigned int>(this->getProperty<int>("fps_lock"));
//    if (fps == 0)
//    {
//        fps = maxFps;
//    }

//    // Set up required properties for the image capturer
//    this->setImageFormat(visionx::ImageDimension(static_cast<int>(refFrameWidth), static_cast<int>(refFrameHeight)), visionx::eRgb, visionx::eBayerPatternGr);
//    this->setImageSyncMode(visionx::eFpsSynchronization);
//    this->frameRate = static_cast<float>(this->getProperty<double>("fps_multiplier") * fps);*/
}


void
component::onConnectImageProvider()
{
    // pass
}


void
component::onDisconnectImageProvider()
{
    // Stop task
    {
        std::lock_guard<std::mutex> lock(m_proc_mutex);
        ARMARX_DEBUG << "Stopping input synchronisation thread...";
        const bool wait_for_join = false;
        m_input_synchronisation_task->stop(wait_for_join);
        m_proc_signal.notify_all();
        ARMARX_DEBUG << "Waiting for input synchronisation thread to stop...";
        m_input_synchronisation_task->waitForStop();
        ARMARX_DEBUG << "Input synchronisation thread stopped";
    }
}


void
component::onExitImageProvider()
{
    for (visionx::playback::Playback playback : playbacks)
    {
        playback->stopPlayback();
    }
    playbacks.clear();
}


void
component::provide_images()
{
    ::CByteImage** image_buffer = new ::CByteImage*[2];
    image_buffer[0] = visionx::tools::createByteImage(getImageFormat(), visionx::ImageType::eRgb);
    image_buffer[1] = visionx::tools::createByteImage(getImageFormat(), visionx::ImageType::eRgb);

    ARMARX_IMPORTANT << "Generic 20 seconds sleep, waiting for OpenPose";
    for (unsigned int i = 19; i > 0; --i)
    {
        std::this_thread::sleep_for(std::chrono::seconds{1});
        ARMARX_VERBOSE << "Continuing in " << i << " second(s)";
    }
    std::this_thread::sleep_for(std::chrono::seconds{1});
    ARMARX_INFO << "Continuing, OpenPose should be ready by now";

    while (not m_input_synchronisation_task->isStopped())
    {
        ARMARX_VERBOSE << "Preparing capture...";

        //component::render_output(input_image, detected_objects, output_image[0]);
        // Provide the images with the original timestamp
        //resultImageProvider->provideResultImages(output_image, timestamp_last_image.count());

        // Load next recording if appropriate
        if (playbacks.empty() or !playbacks[0]->hasNextFrame())
        {
            if (not playbacks.empty())
            {
                ARMARX_DEBUG << "Unloading previous recording...";
                for (visionx::playback::Playback playback : playbacks)
                {
                    playback->stopPlayback();
                }
                playbacks.clear();

                ++m_current_rec_index;
            }

            m_current_frame_index = 0;
            if (m_current_rec_index == m_rec_paths.size())
            {
                break;
            }

            const std::filesystem::path rgb_path = m_rec_paths[m_current_rec_index];
            const std::filesystem::path depth_path = get_path("depth");

            ARMARX_INFO << "Loading (" << m_current_rec_index << ") `" << rgb_path.string() << "`";
            ARMARX_DEBUG << "Loading `" << depth_path.string() << "`";

            playbacks.push_back(visionx::playback::newPlayback(rgb_path));
            playbacks.push_back(visionx::playback::newPlayback(depth_path));

            ARMARX_DEBUG << "Loaded next recording";
        }

        ARMARX_CHECK_EQUAL_W_HINT(playbacks.size(), 2, "Playback size is expected to be 2 at this point");

        ARMARX_VERBOSE << "Capturing frame now...";
        {
            for (unsigned int i = 0; i < 2; ++i)
            {
                visionx::playback::Playback playback = playbacks[i];
                playback->setCurrentFrame(m_current_frame_index);
                playback->getNextFrame(*image_buffer[i]);
            }

            provideImages(image_buffer);

            ARMARX_DEBUG << "Captured frame, returning";
        }

        ARMARX_VERBOSE << "Waiting until the previous frame was processed and saved to disk";
        {
            std::unique_lock<std::mutex> signal_lock{m_proc_mutex};
            m_proc_signal.wait(
                signal_lock,
                [&]() -> bool
                {
                    ARMARX_DEBUG << "Received notify";
                    // If task has been stopped, exit immediately
                    if (m_input_synchronisation_task->isStopped())
                        return true;

                    return m_received_darknet_objects and m_received_openpose_body_pose
                        and m_received_openpose_hand_pose;
                }
            );

            // Reset flags
            m_received_darknet_objects = m_received_openpose_body_pose = m_received_openpose_hand_pose = false;

            // Set next frame
            ++m_current_frame_index;
        }
    }

    delete image_buffer[0];
    delete image_buffer[1];
    delete[] image_buffer;
}


void
component::reportDetectedObjects(
    const std::vector<visionx::yolo::DetectedObject>& dol,
    Ice::Long,
    const Ice::Current&)
{
    std::lock_guard<std::mutex> lock{m_proc_mutex};

    ARMARX_DEBUG << "Received detected objects from Darknet";
    json ser = dol;
    const std::string filename = "frame_" + std::to_string(m_current_frame_index) + ".json";
    const std::filesystem::path out = get_path("out_darknet") / std::filesystem::path{filename};
    std::ofstream o{out.string()};
    o << std::setw(4) << ser << std::endl;

    m_received_darknet_objects = true;
    m_proc_signal.notify_one();
}


void
component::report2DHandKeypointsNormalized(
    const armarx::Keypoint2DMapList& kpml,
    Ice::Long,
    const Ice::Current&)
{
    std::lock_guard<std::mutex> lock{m_proc_mutex};

    ARMARX_DEBUG << "Received 2D hand keypoints from OpenPose";
    json ser = kpml;
    const std::string filename = "frame_" + std::to_string(m_current_frame_index) + ".json";
    const std::filesystem::path out = get_path("out_openpose_hand") / std::filesystem::path{filename};
    std::ofstream o{out.string()};
    o << std::setw(4) << ser << std::endl;

    m_received_openpose_hand_pose = true;
    m_proc_signal.notify_one();
}


void
component::report2DKeypointsNormalized(
    const armarx::Keypoint2DMapList& kpml,
    Ice::Long timestamp,
    const Ice::Current&)
{
    std::lock_guard<std::mutex> lock{m_proc_mutex};

    ARMARX_DEBUG << "Received 2D pose keypoints from OpenPose";
    json ser = kpml;
    const std::string filename = "frame_" + std::to_string(m_current_frame_index) + ".json";
    const std::filesystem::path out = get_path("out_openpose_body") / std::filesystem::path{filename};
    std::ofstream o{out.string()};
    o << std::setw(4) << ser << std::endl;

    m_received_openpose_body_pose = true;
    m_proc_signal.notify_one();
}


std::filesystem::path
component::get_path(const std::string& kind)
{
    const std::filesystem::path rgb_path = m_rec_paths[m_current_rec_index];
    const std::filesystem::path alt_path = [&]()
    {
        std::string recname = rgb_path.filename().string();
        boost::algorithm::replace_all(recname, "_rgb", "_" + kind);
        return rgb_path.parent_path() / std::filesystem::path{recname};
    }();

    if (not std::filesystem::is_directory(alt_path))
    {
        std::filesystem::create_directory(alt_path);
    }

    return alt_path;
}


std::string
component::getDefaultName() const
{
    return component::default_name;
}


armarx::PropertyDefinitionsPtr
component::createPropertyDefinitions()
{
    armarx::PropertyDefinitionsPtr defs = armarx::PropertyDefinitionsPtr{new armarx::ComponentPropertyDefinitions{getConfigIdentifier()}};
    defs->defineRequiredProperty<std::string>("paths_cache", "Path to the paths_cache file (one path to a recording per line)");
    defs->defineOptionalProperty<int>("start_at_rec", 0, "Start at recording ID");
    defs->defineOptionalProperty<std::string>("topic_name_2d_body_pose", "OpenPoseEstimation2D",
        "Topic name where 2D human poses are published"
    );
    defs->defineOptionalProperty<std::string>("topic_name_2d_hand_pose", "OpenPoseHandEstimation2D",
        "Topic name where 2D hand poses are published"
    );
    defs->defineOptionalProperty<std::string>("topic_name_objects", "DarknetObjectDetectionResult",
        "Topic name where detected objects are published"
    );
    return defs;
}
