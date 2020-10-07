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


#pragma once


// corcal
#include <corcal/core/ssr/relations.h>
#include <corcal/interface/data_structures.h>


namespace corcal::core::ssr
{


std::vector<std::vector<relations>>
evaluate_relations(
    const std::vector<detected_object>& objects,
    const double distance_equality_threshold
);


void
evaluate_contact_relations(
    const std::vector<detected_object>& objects,
    std::vector<std::vector<relations>>& ssr_matrix
);


void
evaluate_static_relations(
    const std::vector<detected_object>& objects,
    std::vector<std::vector<relations>>& ssr_matrix
);


void
evaluate_dynamic_relations(
    const std::vector<detected_object>& objects,
    std::vector<std::vector<relations>>& ssr_matrix,
    const double distance_equality_threshold
);


std::vector<std::vector<int>>
serialise(const std::vector<std::vector<relations>>& ssr_matrix);


std::vector<std::vector<relations>>
unserialise(const std::vector<std::vector<int>>& ssr_matrix);


}
