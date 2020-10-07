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


#include <corcal/components/catalyst/functions.h>


// STD/STL
#include <chrono>
#include <tuple>
#include <vector>

// PCL
#include <pcl/common/transforms.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

// Eigen
#include <Eigen/Core>

// IVT
#include <Image/ByteImage.h>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>

// VisionX
#include <VisionX/interface/components/OpenPoseEstimationInterface.h>

// corcal
#include <corcal/core/vwm.h>


// TODO: move to core?
using namespace corcal::components::catalyst;


pcl::PointCloud<pcl::PointXYZ>::Ptr
functions::cvt_to_point_cloud(
        const ::CByteImage& depth_image,
        double angle)
{
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud{new pcl::PointCloud<pcl::PointXYZ>};

    struct camera_data
    {
        float field_of_view_x;
        float field_of_view_y;
        float min_distance;
    };

    // PrimeSense Carmine 1.09 data
    const camera_data cam{54, 45, 300}; // FOV_X=54°, FOV_Y=45°, min_dist=300mm

    // Set extends and properties (point cloud is ordered and dense)
    const unsigned int width = static_cast<unsigned int>(depth_image.width);
    const unsigned int height = static_cast<unsigned int>(depth_image.height);
    cloud->height = height;
    cloud->width = width;

    ARMARX_CHECK_EQUAL(cloud->height, height);
    ARMARX_CHECK_EQUAL(cloud->width, width);

    auto fov_to_focal_length = [](float fov, unsigned int absolute)
    {
        float fov_rad = fov * static_cast<float>(M_PI / 180);
        return static_cast<float>(absolute) / (2 * std::tan(fov_rad / 2));
    };

    const float length_scaling = 1;
    float focal_length_x = fov_to_focal_length(cam.field_of_view_x, width);
    float focal_length_y = fov_to_focal_length(cam.field_of_view_y, height);

    // For each possible coordinate X/Y
    for (unsigned int y = 0; y < height; ++y) for (unsigned int x = 0; x < width; ++x)
    {
        const unsigned int offset = (y * width + x) * 3;

        ARMARX_CHECK_LESS(offset + 2, width * height * 3);

        const float z = static_cast<unsigned int>(
               depth_image.pixels[offset + /* R = */ 0]
            + (depth_image.pixels[offset + /* G = */ 1] << /*   sizeof(char) = */  8)
            + (depth_image.pixels[offset + /* B = */ 2] << /* 2*sizeof(char) = */ 16)
        );

        const float xf = static_cast<float>(x);
        const float yf = static_cast<float>(y);

        cloud->points.emplace_back(
            /* pcl::PointXYZ::x = */ (xf - width / 2) * z / focal_length_x * length_scaling,
            /* pcl::PointXYZ::y = */ -(yf - height / 2) * z / focal_length_y * length_scaling,
            /* pcl::PointXYZ::z = */ -z * length_scaling
        );
    }

    // Define rotation
    const double theta = angle * M_PI / 180;
    Eigen::Affine3d rotation = Eigen::Affine3d::Identity();
    rotation.rotate(Eigen::AngleAxisd(-theta, Eigen::Vector3d::UnitX()));

    // Rotate point cloud for normalisation (camera was tilted)
    pcl::transformPointCloud(*cloud, *cloud, rotation);

    ARMARX_CHECK_EQUAL(cloud->height, height);
    ARMARX_CHECK_EQUAL(cloud->width, width);

    return cloud;
}
