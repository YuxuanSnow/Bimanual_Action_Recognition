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


#pragma once


// STD/STL
#include <chrono>
#include <tuple>
#include <vector>

// PCL
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

// IVT
#include <Image/ByteImage.h>

// VisionX
#include <VisionX/interface/core/DataTypes.h>
#include <VisionX/interface/components/OpenPoseEstimationInterface.h>
#include <VisionX/interface/components/YoloObjectListener.h>

// corcal
#include <corcal/core/vwm.h>


// TODO: move to core?
namespace corcal::components::catalyst::functions
{


std::vector<corcal::core::observation::ptr>
cvt_to_corcal_observations(
    const std::vector<visionx::yolo::DetectedObject>& dol,
    const std::chrono::microseconds& timestamp);


std::vector<corcal::core::observation::ptr>
cvt_to_corcal_observations(
    const armarx::Keypoint2DMapList& kpml,
    const std::chrono::microseconds& timestamp);


pcl::PointCloud<pcl::PointXYZ>::Ptr
cvt_to_point_cloud(
    const ::CByteImage& image,
    double angle);


visionx::BoundingBox3D
estimate_bounding_box(
    const pcl::PointCloud<pcl::PointXYZ>::Ptr scene,
    corcal::core::observation::ptr object);


}
