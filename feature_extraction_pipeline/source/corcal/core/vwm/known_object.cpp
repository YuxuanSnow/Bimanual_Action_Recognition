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


#include <corcal/core/vwm/known_object.h>


// STD/STL
#include <chrono>
#include <cmath>
#include <deque>
#include <limits> // for numeric_limits
#include <memory>
#include <string>
#include <tuple>

// ArmarX
#include <ArmarXCore/core/exceptions/local/ExpressionException.h>


using namespace corcal::core::vwm;


known_object::known_object()
{
    // pass
}


known_object::known_object(observation::ptr initial_observation,unsigned int id)
{
    m_class_name = initial_observation->candidates().at(0).class_name();
    m_id = m_class_name + "_" + std::to_string(id);
    m_observations.push_back(initial_observation);
    m_last_zmin = std::numeric_limits<float>::quiet_NaN();
    m_last_zmax = std::numeric_limits<float>::quiet_NaN();

    ARMARX_CHECK_EQUAL(m_observations.size(), 1);
}


float
known_object::last_zmin() const
{
    return m_last_zmin;
}


void
known_object::last_zmin(float value)
{
    m_last_zmin = value;
}


float
known_object::last_zmax() const
{
    return m_last_zmax;
}


void
known_object::last_zmax(float value)
{
    m_last_zmax = value;
}


const std::string&
known_object::id() const
{
    return m_id;
}


const std::string&
known_object::class_name() const
{
    return m_class_name;
}


observation::ptr
known_object::current_observation() const
{
    ARMARX_CHECK_GREATER(m_observations.size(), 0);

    return m_observations.back();
}


observation::ptr
known_object::past_observation() const
{
    ARMARX_CHECK_GREATER(m_observations.size(), 0);

    const std::chrono::milliseconds limit{333}; // in the Paper, they use 10 frames at 30 fps => ca. 333ms
    const std::chrono::microseconds deadline = current_observation()->seen_at() - limit;

    unsigned int i = 0;
    while (m_observations[i]->seen_at() < deadline)
        ++i;

    return m_observations[i];
}


visionx::BoundingBox3D
known_object::average_bounding_boxes(
        const visionx::BoundingBox3D& current,
        std::chrono::microseconds now,
        std::chrono::milliseconds max_age) const
{
    // Function evaluating the gaussian function to get an appropriate weight
    const auto get_gaussian_weight = [](double x, double sigma) -> double
    {
        // W|A: (1/(sqrt(2π)*σ))*e^(-x²/(2*σ²))
        //      |---- coef ----|   | exp_arg |
        // with σ = sigma
        const constexpr double two_pi = 2 * M_PI;
        const double coef = 1. / (std::sqrt(two_pi) * sigma);
        const double exp_arg = -std::pow(x, 2) / (2 * std::pow(sigma, 2));
        return coef * std::exp(exp_arg);
    };

    // Find possible candidates
    std::vector<std::tuple<double, visionx::BoundingBox3D>> candidates;
    {
        const double sigma = static_cast<double>(max_age.count()) / 3; // 3*sigma = max_age
        const std::chrono::microseconds deadline = now - max_age;
        for (unsigned int i = 0; i < m_observations.size(); ++i)
        {
            observation::ptr observation = m_observations[i];
            if (observation->seen_at() > deadline and observation->has_bounding_box_set())
            {
                const std::chrono::milliseconds diff
                        = std::chrono::duration_cast<std::chrono::milliseconds>(now - observation->seen_at());
                const double weight = get_gaussian_weight((diff).count(), sigma);
                candidates.push_back(std::make_tuple(weight, observation->bounding_box()));
            }
        }

        // Current observation is added as well, with the biggest weight
        candidates.push_back(std::make_tuple(get_gaussian_weight(0, sigma), current));
    }

    // Find normalisation value
    double normalisation;
    {
        double weight_sum = 0;
        for (const auto& t : candidates)
        {
            const double weight = std::get<0>(t);
            weight_sum += weight;
        }
        normalisation = 1. / weight_sum;
    }

    // Calculate mean and return it
    visionx::BoundingBox3D mean;
    mean.x0 = mean.x1 = mean.y0 = mean.y1 = mean.z0 = mean.z1 = 0;
    for (const auto& t : candidates)
    {
        const double weight = std::get<0>(t);
        const visionx::BoundingBox3D& candidate = std::get<1>(t);
        mean.x0 += (weight * normalisation) * candidate.x0;
        mean.x1 += (weight * normalisation) * candidate.x1;
        mean.y0 += (weight * normalisation) * candidate.y0;
        mean.y1 += (weight * normalisation) * candidate.y1;
        mean.z0 += (weight * normalisation) * candidate.z0;
        mean.z1 += (weight * normalisation) * candidate.z1;
    }

    return mean;
}


void
known_object::remember_observation(observation::ptr observation)
{
    m_observations.push_back(observation);
}


void
known_object::forget_observations(std::chrono::microseconds now, std::chrono::microseconds older_than)
{
    ARMARX_CHECK_GREATER(m_observations.size(), 0);

    // Given older_than, calculate absolute deadline
    const std::chrono::microseconds deadline = now - older_than;

    // Forget all observations older than absolute deadline
    while (m_observations.size() > 0 and m_observations.front()->seen_at() < deadline)
        m_observations.pop_front();
}


bool
known_object::all_observations_forgotten() const
{
    return m_observations.size() == 0;
}
