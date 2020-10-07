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


// corcal
#include <corcal/core/ssr.h>


using namespace corcal::core;


std::vector<std::vector<int>>
ssr::serialise(const std::vector<std::vector<relations>>& ssr_matrix)
{
    const unsigned long dim = ssr_matrix.size();
    std::vector<std::vector<int>> serialised_ssr_matrix{dim, std::vector<int>(dim)};

    for (unsigned int i = 0; i < ssr_matrix.size(); ++i) for (unsigned int j = 0; j < ssr_matrix.size(); ++j)
        serialised_ssr_matrix.at(i).at(j) = ssr_matrix.at(i).at(j);

    return serialised_ssr_matrix;
}
