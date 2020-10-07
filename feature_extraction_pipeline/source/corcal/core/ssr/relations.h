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


// STD/STL
#include <bitset>
#include <vector>


namespace
{
    const unsigned int relations_count = 16;

    /**
     * Typedef for a bitset large enough to hold all relations
     */
    using relations_bitset = std::bitset<relations_count>;
}


namespace corcal { namespace core { namespace ssr
{


class relations
{

    private:

        /**
         * Typedef for a bitset large enough to hold all relations
         */
        using relations_bitset = ::relations_bitset;

        // Constants for relations
        static const unsigned int contact_bit = 0;
        static const unsigned int static_above_bit = 1;
        static const unsigned int static_below_bit = 2;
        static const unsigned int static_left_of_bit = 3;
        static const unsigned int static_right_of_bit = 4;
        static const unsigned int static_behind_of_bit = 5;
        static const unsigned int static_in_front_of_bit = 6;
        //static const unsigned int static_around_bit = 7; // removed because already implicitly encoded:
                                                           // around = above or below or right or left or behind ...
        static const unsigned int static_inside_bit = 8;
        static const unsigned int static_surround_bit = 9;
        static const unsigned int dynamic_moving_together_bit = 10;
        static const unsigned int dynamic_halting_together_bit = 11;
        static const unsigned int dynamic_fixed_moving_together_bit = 12;
        static const unsigned int dynamic_getting_close_bit = 13;
        static const unsigned int dynamic_moving_apart_bit = 14;
        static const unsigned int dynamic_stable_bit = 15;

        /**
         * @brief Active relations represented by a bitset, where HI means that the given relation is active, and LO
         *        that a given relation is not active
         */
        relations_bitset m_bitset;

    public:

        unsigned long to_int() const
        {
            return m_bitset.to_ulong();
        }

        /**
         * @brief Default constructor where no relation is active
         */
        relations();

        /**
         * @brief Constructs a relations object from a serialised relations integer
         */
        relations(int);

        /**
         * @brief Serialises the relations object into an integer
         */
        operator int() const;

        /**
         * @brief Assigns the unserialised relations of the right hand side
         * @return Assignee
         */
        relations& operator=(int);

        /**
         * @brief Filters the relations given the mask
         * @param filter_mask Mask to be applied as filter
         * @return New instance of relations, filtered by filter_mask
         */
        relations filter(const relations& filter_mask) const;

        void contact(bool);
        bool contact() const;
        void static_above(bool);
        bool static_above() const;
        void static_below(bool);
        bool static_below() const;
        void static_left_of(bool);
        bool static_left_of() const;
        void static_right_of(bool);
        bool static_right_of() const;
        void static_behind_of(bool);
        bool static_behind_of() const;
        void static_in_front_of(bool);
        bool static_in_front_of() const;
        void static_inside(bool);
        bool static_inside() const;
        void static_surround(bool);
        bool static_surround() const;
        void dynamic_moving_together(bool);
        bool dynamic_moving_together() const;
        void dynamic_halting_together(bool);
        bool dynamic_halting_together() const;
        void dynamic_fixed_moving_together(bool);
        bool dynamic_fixed_moving_together() const;
        void dynamic_getting_close(bool);
        bool dynamic_getting_close() const;
        void dynamic_moving_apart(bool);
        bool dynamic_moving_apart() const;
        void dynamic_stable(bool);
        bool dynamic_stable() const;

        std::vector<std::string> string_list() const;

    private:

        /**
         * @brief Implementation-relevant constructor to construct a relations object from a bitset
         */
        relations(const relations_bitset&);

};


}}}
