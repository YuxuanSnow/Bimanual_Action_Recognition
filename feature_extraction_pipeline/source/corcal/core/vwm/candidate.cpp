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


#include <corcal/core/vwm/candidate.h>


// STD/STL
#include <string>


using namespace corcal::core::vwm;


float
candidate::certainty() const
{
    return m_certainty;
}


void
candidate::certainty(float value)
{
    m_certainty = value;
}


const std::string&
candidate::class_name() const
{
    return m_class_name;
}


void
candidate::class_name(const std::string& value)
{
    m_class_name = value;
}


int
candidate::class_index() const
{
    return m_class_index;
}


void
candidate::class_index(int value)
{
    m_class_index = value;
}


const armarx::DrawColor24Bit&
candidate::colour() const
{
    return m_colour;
}


void
candidate::colour(const armarx::DrawColor24Bit& value)
{
    m_colour = value;
}
