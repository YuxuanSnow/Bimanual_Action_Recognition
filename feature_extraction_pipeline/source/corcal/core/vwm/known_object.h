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
 * @package    corcal::core::vwm
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#pragma once


// STD/STL
#include <deque>
#include <memory>

// VisionX
#include <VisionX/interface/core/DataTypes.h>

// corcal
#include <corcal/core/vwm/observation.h>


namespace corcal { namespace core { namespace vwm
{


/**
 * @brief An object of this class represents an identified object along with several observations
 */
class known_object
{

    public:

        using ptr = std::shared_ptr<known_object>;

    private:

        std::string m_id = "";
        std::string m_class_name = "";
        std::deque<observation::ptr> m_observations;
        float m_last_zmin;
        float m_last_zmax;

    public:

        known_object();
        known_object(observation::ptr initial_observation, unsigned int m_id);

        float last_zmin() const;
        void last_zmin(float value);

        float last_zmax() const;
        void last_zmax(float value);

        const std::string& id() const;

        const std::string& class_name() const;

        observation::ptr current_observation() const;

        observation::ptr past_observation() const;

        /**
         * @brief Averages the last verified bounding boxes, with the current observation weighted double, to smooth
         *        the results
         * @param current
         * @param now
         * @param max_age
         * @return
         */
        visionx::BoundingBox3D average_bounding_boxes(
            const visionx::BoundingBox3D& current,
            std::chrono::microseconds now,
            std::chrono::milliseconds max_age
        ) const;

        void remember_observation(observation::ptr o);

        void forget_observations(std::chrono::microseconds now, std::chrono::microseconds older_than);

        bool all_observations_forgotten() const;

};


}}}
