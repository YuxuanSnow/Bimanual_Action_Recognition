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


// VisionX
#include <VisionX/interface/core/ImageProcessorInterface.ice>
#include <VisionX/interface/components/OpenPoseEstimationInterface.ice>

// corcal
#include <corcal/interface/data_structures.ice>


module corcal { module components { module ssrfeatex
{


interface ssr_feature_listener
{
    void
    ssr_features_detected(
        corcal::core::detected_object_list objects,
        corcal::core::ssr_matrix ssr_matrix,
        long timestamp
    );
};


};};};
