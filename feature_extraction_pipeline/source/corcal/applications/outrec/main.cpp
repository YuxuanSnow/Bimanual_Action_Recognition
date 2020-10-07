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
 * @package    visionx::applications
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


// STD/STL
#include <string>

// ArmarX
#include <ArmarXCore/core/application/Application.h>

// corcal
#include <corcal/components/outrec/component.h>


int
main(int argc, char* argv[])
{
    const std::string name = corcal::components::outrec::component::default_name;
    return armarx::runSimpleComponentApp<corcal::components::outrec::component>(argc, argv, name);
}
