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
#include <chrono>
#include <memory>
#include <vector>

// corcal
#include <corcal/core/vwm/known_object.h>
#include <corcal/core/vwm/observation.h>


namespace corcal::core::vwm
{


/**
 * @brief Utility class and primary data structure to memorise and track objects
 */
class memory
{

    protected:

        /**
         * @brief Number used once for each new object to give them a unique ID
         */
        unsigned long int m_id_counter = 0;

        /**
         * @brief List of all known objects
         */
        std::vector<known_object::ptr> m_known_objects;

        /**
         * @brief Time after which an observation is forgotten
         */
        std::chrono::microseconds m_remember_duration;

        /**
         * @brief Current time. If zero, this will be evaluated from system time
         */
        std::chrono::microseconds m_now;

        /**
         * @brief Minimum certainty an observation must have to be considered a new object. This is
         *        ignored if an observation is matched against a known object
         */
        float m_initial_certainty_threshold;

    public:

        memory();

        memory(float m_initial_certainty_threshold, std::chrono::milliseconds m_remember_duration);

        virtual ~memory();

        void initial_certainty_threshold(float value);
        void remember_duration(const std::chrono::milliseconds& value);
        void now(const std::chrono::microseconds& value);

        virtual void make_observations(const std::vector<observation::ptr>& observations);

        virtual void reset();

        std::vector<known_object::ptr> known_objects();

    protected:

        virtual void match_observations_to_known_objects(std::vector<observation::ptr>& observations) const;

};


}
