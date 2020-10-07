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
#include <limits>
#include <tuple>

// IVT
#include <Image/ByteImage.h>

// PCL
#include <pcl/common/common.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>

// VisionX
#include <VisionX/interface/core/DataTypes.h>


// TODO: move to core?
using namespace corcal::components::catalyst;


visionx::BoundingBox3D
functions::estimate_bounding_box(
        const pcl::PointCloud<pcl::PointXYZ>::Ptr scene,
        corcal::core::observation::ptr object)
{
    // Capped extends of the bounding box in pixel
    const unsigned int top = static_cast<unsigned int>(
        std::min(static_cast<unsigned int>(object->ymax() * scene->height), scene->height - 1));
    const unsigned int bottom = static_cast<unsigned int>(
        std::max(static_cast<int>(object->ymin() * scene->height), 0));
    const unsigned int right = static_cast<unsigned int>(
        std::min(static_cast<unsigned int>(object->xmax() * scene->width), scene->width - 1));
    const unsigned int left = static_cast<unsigned int>(
        std::max(static_cast<int>(object->xmin() * scene->width), 0));

    // Height and width of the bounding box in pixel
    const unsigned int height = top - bottom;
    const unsigned int width = right - left;

    const visionx::BoundingBox3D invalid_bb = []() -> visionx::BoundingBox3D
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan, nan, nan, nan};
    }();

    if (top <= bottom or right <= left)
    {
        return invalid_bb;
    }

    // Consistency checks
    {
        ARMARX_CHECK_GREATER(top, bottom);
        ARMARX_CHECK_GREATER(right, left);
        ARMARX_CHECK_LESS(right, static_cast<unsigned int>(scene->width));
        ARMARX_CHECK_LESS(top, static_cast<unsigned int>(scene->height));
        ARMARX_CHECK_EQUAL(left + width, right);
        ARMARX_CHECK_EQUAL(bottom + height, top);
    }

    // Initialise point cloud
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
    {
        // Set extends and properties (point cloud is ordered and dense)
        cloud->height = height;
        cloud->width = width;

        // Initialise point cloud from the patch of the depth image defined by the objects' bounding box
        for (unsigned int y = 0; y < height; ++y) for (unsigned int x = 0; x < width; ++x)
        {
            const unsigned int offset = ((bottom + y) * scene->width) + (left + x);

            // Set that point in point cloud
            cloud->points.push_back(scene->points[offset]);
        }
    }

    // Downsample point cloud and save to cloud_filtered
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered{new pcl::PointCloud<pcl::PointXYZ>};
    {
        pcl::VoxelGrid<pcl::PointXYZ> vg;
        vg.setInputCloud(cloud);
        vg.setLeafSize(5, 5, 5); // in [mm]
        vg.filter(*cloud_filtered);
    }

    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree{new pcl::search::KdTree<pcl::PointXYZ>};
    tree->setInputCloud(cloud_filtered);

    // Cluster point cloud
    std::vector<pcl::PointIndices> cluster_indices;
    {
        pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
        ec.setClusterTolerance(25); // in [mm]
        ec.setMinClusterSize(5);
        ec.setMaxClusterSize(25000);
        ec.setSearchMethod(tree);
        ec.setInputCloud(cloud_filtered);
        ec.extract(cluster_indices);
    }

    // Clusters are sorted descending by the amount of points, so biggest clusters are on top, but it could be a cluster
    // of uninitialised depth values.
    for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
    {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster{new pcl::PointCloud<pcl::PointXYZ>};
        for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit)
            cloud_cluster->points.push_back(cloud_filtered->points[static_cast<unsigned int>(*pit)]);
        cloud_cluster->width = static_cast<unsigned int>(cloud_cluster->points.size());
        cloud_cluster->height = 1;

        // Find minimum and maximum and populate bounding box
        pcl::PointXYZ min_p, max_p;
        pcl::getMinMax3D(*cloud_cluster, min_p, max_p);

        const float depth_threshold = 0.001f; // TODO: Value arbitrary + const should be declared elsewhere
        const float depth = max_p.z - min_p.z;

        // Ensure that this cluster is not a plane of uninitialised points (in this case, the depth should be zero or
        // very small). If the cluster actually is one, the next cluster should give better results.
        if (depth > depth_threshold)
        {
            visionx::BoundingBox3D bounding_box;
            bounding_box.x0 = min_p.x;
            bounding_box.x1 = max_p.x;
            bounding_box.y0 = min_p.y;
            bounding_box.y1 = max_p.y;
            bounding_box.z0 = min_p.z;
            bounding_box.z1 = max_p.z;
            return bounding_box;
        }
    }

    return invalid_bb;
}
