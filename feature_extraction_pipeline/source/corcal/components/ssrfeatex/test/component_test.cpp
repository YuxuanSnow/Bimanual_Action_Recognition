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
 * @package    corcal::test::components::ssrfeatex
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#define BOOST_TEST_MODULE corcal::test::components::ssrfeatex::component
#define ARMARX_BOOST_TEST


#include <iostream>

#include <corcal/Test.h>
#include <corcal/components/ssrfeatex/component.h>


BOOST_AUTO_TEST_CASE(testExample)
{
    corcal::components::ssrfeatex::component instance;

    BOOST_CHECK_EQUAL(true, true);
}
