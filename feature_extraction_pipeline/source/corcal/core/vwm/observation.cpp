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


#include <corcal/core/vwm/observation.h>


// STD/STL
#include <chrono>
#include <cmath> // hypot, pow, sqrt
#include <memory>
#include <string>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>

// VisionX
#include <VisionX/interface/core/DataTypes.h>


using namespace corcal::core::vwm;


const std::vector<candidate>&
observation::candidates() const
{
    return m_candidates;
}


void
observation::candidates(const std::vector<candidate>& value)
{
    m_candidates = value;
}


const std::chrono::microseconds&
observation::seen_at() const
{
    return m_seen_at;
}


void
observation::seen_at(const std::chrono::microseconds& value)
{
    m_seen_at = value;
}


float
observation::cx() const
{
    return m_cx;
}


void
observation::cx(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_cx = value;
}


float
observation::cy() const
{
    return m_cy;
}


void
observation::cy(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_cy = value;
}


float
observation::w() const
{
    return m_w;
}


void
observation::w(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_w = value;
}


float
observation::h() const
{
    return m_h;
}


void
observation::h(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_h = value;
}


float
observation::xmin() const
{
    return m_xmin;
}


void
observation::xmin(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_xmin = value;
}


float
observation::xmax() const
{
    return m_xmax;
}


void
observation::xmax(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_xmax = value;
}


float
observation::ymin() const
{
    return m_ymin;
}


void
observation::ymin(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_ymin = value;
}


float
observation::ymax() const
{
    return m_ymax;
}


void
observation::ymax(float value)
{
    if (value >= 1) value = 1;
    if (value < 0) value = 0;
    m_ymax = value;
}


int
observation::class_count() const
{
    return m_class_count;
}


void
observation::class_count(int value)
{
    m_class_count = value;
}


visionx::BoundingBox3D
observation::bounding_box() const
{
    return m_bounding_box;
}


void
observation::bounding_box(const visionx::BoundingBox3D& bounding_box)
{
    m_has_bounding_box_set = true;
    m_bounding_box = bounding_box;
}


bool
observation::has_bounding_box_set() const
{
    return m_has_bounding_box_set;
}


double
observation::distance_to(observation::ptr other) const
{
    return std::hypot(static_cast<double>(m_cx) - static_cast<double>(other->m_cx),
                      static_cast<double>(m_cy) - static_cast<double>(other->m_cy));
}
