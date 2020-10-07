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
#include <string>

// RobotAPI
#include <RobotAPI/interface/visualization/DebugDrawerInterface.h>


namespace corcal { namespace core { namespace vwm
{


class candidate
{

    private:

        float m_certainty;
        std::string m_class_name;
        int m_class_index;
        armarx::DrawColor24Bit m_colour;

    public:

        float certainty() const;
        void certainty(float value);
        const std::string& class_name() const;
        void class_name(const std::string& value);
        int class_index() const;
        void class_index(int value);
        const armarx::DrawColor24Bit& colour() const;
        void colour(const armarx::DrawColor24Bit& value);

};


}}}
