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
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#pragma once


// VisionX
#include <VisionX/interface/core/ImageProviderInterface.ice>
#include <VisionX/interface/components/YoloObjectListener.ice>
#include <VisionX/interface/components/OpenPoseEstimationInterface.ice>


module corcal { module components { module outrec
{


interface component_interface extends
    visionx::CompressedImageProviderInterface,
    visionx::yolo::ObjectListener,
    armarx::OpenPoseHand2DListener,
    armarx::OpenPose2DListener
{
    // pass
};


};};};
