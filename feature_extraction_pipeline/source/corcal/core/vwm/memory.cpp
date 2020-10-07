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


#include <corcal/core/vwm/memory.h>


// STD/STL
#include <algorithm> // for begin, end, remove
#include <cmath> // for hypot, pow, sqrt
#include <limits> // for numerical_limits

// PF
#include <sisr_filter.h>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h> // for ARMARX_CHECK_* assertions


using namespace corcal::core::vwm;


memory::memory()
{
    // pass
}


memory::memory(float initial_certainty_threshold, std::chrono::milliseconds remember_duration)
{
    m_initial_certainty_threshold = initial_certainty_threshold;
    m_remember_duration = remember_duration;
    m_now = std::chrono::microseconds::zero();
}


memory::~memory()
{
    // pass
}


void
memory::initial_certainty_threshold(float value)
{
    m_initial_certainty_threshold = value;
}


void
memory::remember_duration(const std::chrono::milliseconds& value)
{
    m_remember_duration = value;
}


void
memory::now(const std::chrono::microseconds& value)
{
    m_now = value;
}


void
memory::make_observations(const std::vector<observation::ptr>& observations)
{
    std::vector<observation::ptr> observations_mutable = observations;

    SISRFilter<1000, > cff;

    // Refresh memory using the new observations.
    {
        // Tries to match obervations to already known objects in first instance. Matched
        // observations will be removed from the obervations list.
        match_observations_to_known_objects(observations_mutable);

        // Add the remaining observations as known object if the certainty is high enough in second
        // instance.
        for (observation::ptr observation : observations_mutable)
        {
            if (observation->candidates().at(0).certainty() >= m_initial_certainty_threshold)
            {
                known_object::ptr known_object =
                    std::make_shared<vwm::known_object>(observation, ++m_id_counter);
                m_known_objects.push_back(known_object);
            }
        }
    }

    // Forget outdated observations.
    {
        auto it = std::begin(m_known_objects);
        while (it != std::end(m_known_objects))
        {
            known_object::ptr known_object = *it;

            std::chrono::microseconds now;
            if (m_now == std::chrono::microseconds::zero())
            {
                auto now_any_unit = std::chrono::system_clock::now().time_since_epoch();
                now = std::chrono::duration_cast<std::chrono::microseconds>(now_any_unit);
            }
            else
            {
                now = m_now;
            }

            known_object->forget_observations(now, m_remember_duration);
            if (known_object->all_observations_forgotten())
                it = m_known_objects.erase(it);
            else
                std::advance(it, 1);
        }
    }
}


void
memory::match_observations_to_known_objects(std::vector<observation::ptr>& observations) const
{
    // Helper to find possible match candidates by target class name
    auto find_possible_matches = [](
        const std::vector<observation::ptr>& observations,
        const std::string& target_class_name
    ) -> std::vector<observation::ptr>
    {
        std::vector<observation::ptr> match_candidates;

        for (observation::ptr observation : observations)
            for (const candidate& candidate : observation->candidates())
                if (candidate.class_name() == target_class_name)
                    match_candidates.push_back(observation);

        ARMARX_CHECK_LESS_EQUAL_W_HINT(match_candidates.size(), observations.size(), "Filtered vector of candidates "
            "cannot have more elements than the input vector");

        return match_candidates;
    };

    // Helper to find the observation that matches best to the last confirmed observation.
    auto find_best_match = [](
        std::vector<observation::ptr>& possible_matches,
        observation::ptr last_confirmed_observation
    ) -> observation::ptr
    {
        ARMARX_CHECK_GREATER_EQUAL_W_HINT(possible_matches.size(), 1, "The input vector of possible matches to pick "
            "the best match from cannot be empty");

        // Pairwise compare possible matches and eliminate the worse in each loop
        while (possible_matches.size() > 1)
        {
            // Ensure that there are at least two elements left in the list, remember list size
            const unsigned long int possible_matches_size_pre = possible_matches.size();
            ARMARX_CHECK_GREATER_EQUAL(possible_matches_size_pre, 2);

            // Considered candidates in this loop
            observation::ptr candiate1 = possible_matches.at(0);
            observation::ptr candidate2 = possible_matches.at(1);

            // Calculate distance to first and second candidate in candidates list
            const double distance1 = last_confirmed_observation->distance_to(candiate1);
            const double distance2 = last_confirmed_observation->distance_to(candidate2);

            // Find the elimination candidate
            observation::ptr elimination_candidate = distance1 <= distance2 ? candidate2 : candiate1;

            // Delete the elimination candidate from the list of possible matches
            possible_matches.erase(
                std::remove(std::begin(possible_matches), std::end(possible_matches), elimination_candidate),
                std::end(possible_matches)
            );

            // Ensure that exactly one element was deleted from the list
            ARMARX_CHECK_EQUAL(possible_matches_size_pre, possible_matches.size() + 1);
        }

        // Ensure that there is exactly one possible match left, that is, the best match
        ARMARX_CHECK_EQUAL(possible_matches.size(), 1);

        // Return the best match
        return possible_matches.at(0);
    };

    // Helper to transfer the best match from the observations list to the known object
    auto transfer_best_match = [](
        observation::ptr best_match,
        std::vector<observation::ptr>& observations,
        known_object::ptr known_object
    ) -> void
    {
        const unsigned long int observations_size_pre = observations.size();
        ARMARX_CHECK_GREATER_EQUAL_W_HINT(observations_size_pre, 1, "Cannot transfer the best match if the source "
            "vector is empty");

        known_object->remember_observation(best_match);

        // Remove the best match from the list of observations
        observations.erase(
            std::remove(std::begin(observations), std::end(observations), best_match),
            std::end(observations)
        );

        // Ensure that exactly one item was deleted from observations list
        ARMARX_CHECK_EQUAL(observations_size_pre, observations.size() + 1);
    };

    for (known_object::ptr known_object : m_known_objects)
    {
        // If there's nothing to match against (anymore), exit early
        if (observations.size() == 0) return;

        // Find possible matches for the known object by class name
        std::vector<observation::ptr> possible_matches = find_possible_matches(observations, known_object->class_name());

        // If there are no candidates, continue
        if (possible_matches.size() == 0) continue;

        // Find the candidate which matches best to the last confirmed observation
        observation::ptr best_match = find_best_match(possible_matches, known_object->current_observation());

        // Transfer the best match from the observations list to the known object
        transfer_best_match(best_match, observations, known_object);
    }
}


void
memory::reset()
{
    m_id_counter = 0;
    m_known_objects.clear();
}


std::vector<known_object::ptr>
memory::known_objects()
{
    return m_known_objects;
}
