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
#include <string>

// VisionX
#include <VisionX/interface/core/DataTypes.h>

// corcal
#include <corcal/core/vwm/candidate.h>


namespace corcal { namespace core { namespace vwm
{


/**
 * @brief An object of this class represents an individual general observation
 */
class observation
{

    public:

        using ptr = std::shared_ptr<observation>;

    private:

        int m_class_count;
        std::vector<candidate> m_candidates;
        std::chrono::microseconds m_seen_at;
        float m_cx;
        float m_cy;
        float m_w;
        float m_h;
        float m_xmin;
        float m_xmax;
        float m_ymin;
        float m_ymax;

        /**
         * @brief 3D bounding box associated with this observation
         */
        bool m_has_bounding_box_set;
        visionx::BoundingBox3D m_bounding_box;


    public:

        const std::vector<candidate>& candidates() const;
        void candidates(const std::vector<candidate>& value);
        const std::chrono::microseconds& seen_at() const;
        void seen_at(const std::chrono::microseconds& value);
        float cx() const;
        void cx(float value);
        float cy() const;
        void cy(float value);
        float w() const;
        void w(float value);
        float h() const;
        void h(float value);
        float xmin() const;
        void xmin(float value);
        float xmax() const;
        void xmax(float value);
        float ymin() const;
        void ymin(float value);
        float ymax() const;
        void ymax(float value);
        int class_count() const;
        void class_count(int value);
        visionx::BoundingBox3D bounding_box() const;
        void bounding_box(const visionx::BoundingBox3D& bounding_box);
        bool has_bounding_box_set() const;

        double distance_to(observation::ptr other) const;

};


}}}
