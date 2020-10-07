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
 * @package    corcal::core::ssr
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#include <corcal/core/ssr/functions.h>


// STD/STL
#include <cmath>
#include <string>

// ArmarX
#include <ArmarXCore/core/logging/Logging.h>
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>

// VisionX
#include <VisionX/interface/core/DataTypes.h>

// corcal
#include <corcal/core/ssr/relations.h>
#include <corcal/interface/data_structures.h>


using namespace corcal::core;
using namespace corcal::core::ssr;
using bounding_box = visionx::BoundingBox3D;


namespace
{
    /**
     * @brief Helper function to perform a trivial collision check of two axis-aligned bounding boxes a and b
     * @param a First bounding box
     * @param b Second bounding box
     * @return True if the bounding boxes collide, false otherwise
     */
    bool
    is_colliding(const bounding_box& a, const bounding_box& b)
    {
        // This function is equivalent to TNR in the paper
        // Sources:
        //  - (original) https://developer.mozilla.org/en-US/docs/Games/Techniques/3D_collision_detection#AABB_vs._AABB
        //  - (archive)  https://web.archive.org/web/20170325233444/https://developer.mozilla.org/en-US/docs/Games/Techniques/3D_collision_detection

        return (a.x0 <= b.x1 and a.x1 >= b.x0 and
                a.y0 <= b.y1 and a.y1 >= b.y0 and
                a.z0 <= b.z1 and a.z1 >= b.z0);
    }


    /**
     * @brief Helper function to calculate the distance between two bounding boxes a and b
     * @param a First bounding box
     * @param b Secodn bounding box
     * @return Distance from a to b (or b to a) in the same units as a and b
     */
    double
    distance_between(const bounding_box& a, const bounding_box& b)
    {
        // This function is equivalent to delta in the paper

        float cax = (a.x1 + a.x0) / 2;
        float cay = (a.y1 + a.y0) / 2;
        float caz = (a.z1 + a.z0) / 2;
        float cbx = (b.x1 + b.x0) / 2;
        float cby = (b.y1 + b.y0) / 2;
        float cbz = (b.z1 + b.z0) / 2;

        // TODO: C++17: distance = std::hypot(cax - cbx, cay - cby, caz - cbz);
        const double radicand = std::pow(cax - cbx, 2) + std::pow(cay - cby, 2) + std::pow(caz - cbz, 2);
        const double distance = std::sqrt(radicand);

        ARMARX_CHECK_EXPRESSION_W_HINT(distance >= 0, "Calculated distance cannot be less than zero (is "
            + std::to_string(distance) + ")");
        return distance;
    }
}


std::vector<std::vector<relations>>
ssr::evaluate_relations(
    const std::vector<detected_object>& objects,
    const double distance_equality_threshold)
{
    std::vector<std::vector<relations>> ssr_matrix{objects.size(), std::vector<relations>(objects.size())};

    ssr::evaluate_contact_relations(objects, ssr_matrix);
    ssr::evaluate_static_relations(objects, ssr_matrix);
    ssr::evaluate_dynamic_relations(objects, ssr_matrix, distance_equality_threshold);

    return ssr_matrix;
}


void
ssr::evaluate_contact_relations(
    const std::vector<detected_object>& objects,
    std::vector<std::vector<relations>>& ssr_matrix)
{
    const unsigned long dim = objects.size();

    // Iteration scheme, for example with 5 objects (dim = 5)
    //
    // Rows: subject indices (0 <= subject_index < dim)
    // Colums: object indices (0 <= object_index < dim)
    // Cells:
    //    c = considered
    //    x = not considered / iteration skipped (reason: no relations to self)
    //    i = implicitly considered (here: [subject IN CONCTACT WITH object] implies [object IN CONTACT WITH subject])
    //
    //               <--object_index-->
    //
    //                    0 1 2 3 4
    //                    | | | | |
    //         ^       0— x c c c c
    //         |       1— i x c c c
    //  subject_index  2— i i x c c
    //         |       3— i i i x c
    //         v       4— i i i i x
    //
    for (unsigned int subject_index = 0; subject_index < dim; ++subject_index)
    {
        for (unsigned int object_index = subject_index + 1; object_index < dim; ++object_index)
        {
            // 0 <= subject_index < dim
            // 0 <= subject_index is always true (typeof(subject_index) == unsigned int)
            ARMARX_CHECK_LESS(subject_index, dim);
            // subject_index < object_index < dim
            ARMARX_CHECK_LESS(subject_index, object_index);
            ARMARX_CHECK_LESS(object_index, dim);

            const bounding_box& subject_bb = objects.at(subject_index).bounding_box;
            const bounding_box& object_bb = objects.at(object_index).bounding_box;

            if (::is_colliding(subject_bb, object_bb))
            {
                ssr_matrix.at(subject_index).at(object_index).contact(true);
                // <=>
                ssr_matrix.at(object_index).at(subject_index).contact(true);
            }
        }
    }
}


void
ssr::evaluate_static_relations(
    const std::vector<detected_object>& objects,
    std::vector<std::vector<relations>>& ssr_matrix)
{
    const unsigned long dim = objects.size();

    // Iteration scheme, for example with 5 objects (dim = 5)
    //
    // Rows: subject indices (0 <= subject_index < dim)
    // Colums: object indices (0 <= object_index < dim)
    // Cells:
    //    c = considered
    //    x = not considered / iteration skipped (reason: no relations to self)
    //    i = implicitly considered (example: [subject LEFT OF object] implies [object RIGHT OF subject])
    //
    //               <--object_index-->
    //
    //                    0 1 2 3 4
    //                    | | | | |
    //         ^       0— x c c c c
    //         |       1— i x c c c
    //  subject_index  2— i i x c c
    //         |       3— i i i x c
    //         v       4— i i i i x
    //
    for (unsigned int subject_index = 0; subject_index < dim; ++subject_index)
    {
        for (unsigned int object_index = subject_index + 1; object_index < dim; ++object_index)
        {
            // 0 <= subject_index < dim
            // 0 <= subject_index is always true (typeof(subject_index) == unsigned int)
            ARMARX_CHECK_LESS(subject_index, dim);
            // subject_index < object_index < dim
            ARMARX_CHECK_LESS(subject_index, object_index);
            ARMARX_CHECK_LESS(object_index, dim);

            const detected_object& subject = objects.at(subject_index);
            const detected_object& object = objects.at(object_index);
            const bounding_box& subject_bb = subject.bounding_box;
            const bounding_box& object_bb = object.bounding_box;

            // Left
            if (subject_bb.x1 < object_bb.x0)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " is left of " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_left_of(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_right_of(true);
            }
            // Right
            else if (subject_bb.x0 > object_bb.x1)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " is right of " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_right_of(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_left_of(true);
            }

            // Below
            if (subject_bb.y1 < object_bb.y0)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " is below of " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_below(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_above(true);
            }
            // Above
            else if (subject_bb.y0 > object_bb.y1)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " is above of " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_above(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_below(true);
            }

            // Behind
            if (subject_bb.z1 < object_bb.z0)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " is behind of " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_behind_of(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_in_front_of(true);
            }
            // In front
            else if (subject_bb.z0 > object_bb.z1)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " is in front of " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_in_front_of(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_behind_of(true);
            }

            // Inside
            if (object_bb.x0 < subject_bb.x0 and subject_bb.x1 < object_bb.x1 and object_bb.z0 < subject_bb.z0
                and subject_bb.z1 < object_bb.z1 and object_bb.y0 < subject_bb.y0 and subject_bb.y0 <= object_bb.y1)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " is inside of " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_inside(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_surround(true);
            }
            // Surround
            else if (object_bb.x0 > subject_bb.x0 and subject_bb.x1 > object_bb.x1 and object_bb.z0 > subject_bb.z0
                and subject_bb.z1 > object_bb.z1 and object_bb.y0 > subject_bb.y1 and subject_bb.y1 >= object_bb.y1)
            {
                ARMARX_VERBOSE << "Found SSR: " << subject.class_name << " surrounds " << object.class_name;
                ssr_matrix.at(subject_index).at(object_index).static_surround(true);
                // =>
                ssr_matrix.at(object_index).at(subject_index).static_inside(true);
            }
        }
    }
}


void
ssr::evaluate_dynamic_relations(
    const std::vector<detected_object>& objects,
    std::vector<std::vector<relations>>& ssr_matrix,
    const double distance_equality_threshold)
{
    const unsigned long dim = objects.size();

    // Considering dynamic relations, there is no natural way of thinking about subjects and objects since the relations
    // are commutative. This means that a detected relation will always be set the same for both ssr_matrix[i][j] and
    // ssr_matrix[j][i].
    // Iteration scheme, for example with 5 objects (dim = 5)
    //
    // Rows: Row indices (in others: subject) (0 <= i < dim)
    // Colums: Column indices (in others: object) (0 <= j < dim)
    // Cells:
    //    c = considered
    //    x = not considered / iteration skipped (reason: no relations to self)
    //    i = implicitly considered (commutative)
    //
    //              <---j--->
    //
    //              0 1 2 3 4
    //              | | | | |
    //     ^     0— x c c c c
    //     |     1— i x c c c
    //     i     2— i i x c c
    //     |     3— i i i x c
    //     v     4— i i i i x
    //
    for (unsigned int i = 0; i < dim; ++i)
    {
        for (unsigned int j = i + 1; j < dim; ++j)
        {
            // Get present objects
            const detected_object& object_a = objects.at(i);
            const detected_object& object_b = objects.at(j);

            const bounding_box& object_a_bb = object_a.bounding_box;
            const bounding_box& object_b_bb = object_b.bounding_box;
            const bounding_box& object_a_past_bb = object_a.past_bounding_box;
            const bounding_box& object_b_past_bb = object_b.past_bounding_box;

            const double delta_ab = ::distance_between(object_a_bb, object_b_bb);
            const double delta_ab_past = ::distance_between(object_a_past_bb, object_b_past_bb);

            // If the bounding boxes are colliding now and also did so earlier
            const bool p1 = ::is_colliding(object_a_bb, object_b_bb)
                and ::is_colliding(object_a_past_bb, object_b_past_bb);
            // If the bounding boxes are not colliding now and also did not so earlier
            const bool p2 = not ::is_colliding(object_a_bb, object_b_bb)
                and not ::is_colliding(object_a_past_bb, object_b_past_bb);

            // If objects are in contact and were in contact earlier
            if (p1)
            {
                // Instead of checking if the centers of the objects are the very same point (as suggested in the
                // paper), we also compute delta and check for the distance to be below the distance eq. threshold zeta
                const bool p3 = ::distance_between(object_a_bb, object_a_past_bb) < (distance_equality_threshold / 2);
                const bool p4 = ::distance_between(object_b_bb, object_b_past_bb) < (distance_equality_threshold / 2);

                // If both objects moved
                if (p3 and p4)
                {
                    ssr_matrix.at(i).at(j).dynamic_moving_together(true);
                    ssr_matrix.at(j).at(i).dynamic_moving_together(true);
                }
                // If no object moved
                else if (!p3 and !p4)
                {
                    ssr_matrix.at(i).at(j).dynamic_halting_together(true);
                    ssr_matrix.at(j).at(i).dynamic_halting_together(true);
                }
                // If exactly one object moved
                else if (p3 xor p4)
                {
                    ssr_matrix.at(i).at(j).dynamic_fixed_moving_together(true);
                    ssr_matrix.at(j).at(i).dynamic_fixed_moving_together(true);
                }
            }
            // If objects were not in contact and were not in contact earlier
            else if (p2)
            {
                // If the distance now is (considerably) smaller than previously. Note: Added minus to the distance
                // equality threshold (zeta), which is not in the paper, but doesn't make sense otherwise
                if (delta_ab - delta_ab_past < -distance_equality_threshold)
                {
                    ssr_matrix.at(i).at(j).dynamic_getting_close(true);
                    ssr_matrix.at(j).at(i).dynamic_getting_close(true);
                }
                // If the distance now is (considerably) greater than previously
                else if (delta_ab - delta_ab_past > distance_equality_threshold)
                {
                    ssr_matrix.at(i).at(j).dynamic_moving_apart(true);
                    ssr_matrix.at(j).at(i).dynamic_moving_apart(true);
                }
                // This case is basically: | delta_ab - delta_ab_past | < zeta
                // In words: The distance between the objects now and then has not considerably changed
                else
                {
                    ssr_matrix.at(i).at(j).dynamic_stable(true);
                    ssr_matrix.at(j).at(i).dynamic_stable(true);
                }
            }
        }
    }
}
