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


#include <corcal/core/ssr/relations.h>


// STD/STL
#include <bitset>


using namespace corcal::core::ssr;


relations::relations() :
    m_bitset{0}
{
    // pass
}


relations::relations(int serialised_relations) :
    m_bitset{static_cast<unsigned long long>(serialised_relations)}
{
    // pass
}


relations::relations(const relations_bitset& relations_bitset) :
    m_bitset{relations_bitset}
{
    // pass
}


relations::operator int() const
{
    // This is safe as long as an int can hold the bitset (generally speaking, <= 32 bit)
    return static_cast<int>(m_bitset.to_ulong());
}


relations&
relations::operator=(int serialised_relations)
{
    m_bitset = relations_bitset{static_cast<unsigned long long>(serialised_relations)};
    return *this;
}


relations
relations::filter(const relations& filter_mask) const
{
    return m_bitset bitand filter_mask.m_bitset;
}


void
relations::contact(bool set_reset)
{
    m_bitset.set(contact_bit, set_reset);
}


bool
relations::contact() const
{
    return m_bitset.test(contact_bit);
}


void
relations::static_above(bool set_reset)
{
    m_bitset.set(static_above_bit, set_reset);
}


bool
relations::static_above() const
{
    return m_bitset.test(static_above_bit);
}


void
relations::static_below(bool set_reset)
{
    m_bitset.set(static_below_bit, set_reset);
}


bool
relations::static_below() const
{
    return m_bitset.test(static_below_bit);
}


void
relations::static_left_of(bool set_reset)
{
    m_bitset.set(static_left_of_bit, set_reset);
}


bool
relations::static_left_of() const
{
    return m_bitset.test(static_left_of_bit);
}


void
relations::static_right_of(bool set_reset)
{
    m_bitset.set(static_right_of_bit, set_reset);
}


bool
relations::static_right_of() const
{
    return m_bitset.test(static_right_of_bit);
}


void
relations::static_behind_of(bool set_reset)
{
    m_bitset.set(static_behind_of_bit, set_reset);
}


bool
relations::static_behind_of() const
{
    return m_bitset.test(static_behind_of_bit);
}


void
relations::static_in_front_of(bool set_reset)
{
    m_bitset.set(static_in_front_of_bit, set_reset);
}


bool
relations::static_in_front_of() const
{
    return m_bitset.test(static_in_front_of_bit);
}


void
relations::static_inside(bool set_reset)
{
    m_bitset.set(static_inside_bit, set_reset);
}


bool
relations::static_inside() const
{
    return m_bitset.test(static_inside_bit);
}


void
relations::static_surround(bool set_reset)
{
    m_bitset.set(static_surround_bit, set_reset);
}


bool
relations::static_surround() const
{
    return m_bitset.test(static_surround_bit);
}


void
relations::dynamic_moving_together(bool set_reset)
{
    m_bitset.set(dynamic_moving_together_bit, set_reset);
}


bool
relations::dynamic_moving_together() const
{
    return m_bitset.test(dynamic_moving_together_bit);
}


void
relations::dynamic_halting_together(bool set_reset)
{
    m_bitset.set(dynamic_halting_together_bit, set_reset);
}


bool
relations::dynamic_halting_together() const
{
    return m_bitset.test(dynamic_halting_together_bit);
}


void
relations::dynamic_fixed_moving_together(bool set_reset)
{
    m_bitset.set(dynamic_fixed_moving_together_bit, set_reset);
}


bool
relations::dynamic_fixed_moving_together() const
{
    return m_bitset.test(dynamic_fixed_moving_together_bit);
}


void
relations::dynamic_getting_close(bool set_reset)
{
    m_bitset.set(dynamic_getting_close_bit, set_reset);
}


bool
relations::dynamic_getting_close() const
{
    return m_bitset.test(dynamic_getting_close_bit);
}


void
relations::dynamic_moving_apart(bool set_reset)
{
    m_bitset.set(dynamic_moving_apart_bit, set_reset);
}


bool
relations::dynamic_moving_apart() const
{
    return m_bitset.test(dynamic_moving_apart_bit);
}


void
relations::dynamic_stable(bool set_reset)
{
    m_bitset.set(dynamic_stable_bit, set_reset);
}


bool
relations::dynamic_stable() const
{
    return m_bitset.test(dynamic_stable_bit);
}


std::vector<std::string>
relations::string_list() const
{
    std::vector<std::string> v;

    if (contact())
        v.push_back("contact");
    if (static_above())
        v.push_back("above");
    if (static_below())
        v.push_back("below");
    if (static_left_of())
        v.push_back("left of");
    if (static_right_of())
        v.push_back("right of");
    if (static_behind_of())
        v.push_back("behind of");
    if (static_in_front_of())
        v.push_back("in front of");
    if (static_surround())
        v.push_back("surround");
    if (dynamic_moving_together())
        v.push_back("moving together");
    if (dynamic_halting_together())
        v.push_back("halting together");
    if (dynamic_fixed_moving_together())
        v.push_back("fixed moving together");
    if (dynamic_getting_close())
        v.push_back("getting close");
    if (dynamic_moving_apart())
        v.push_back("moving apart");
    if (dynamic_stable())
        v.push_back("stable");

    return v;
}
