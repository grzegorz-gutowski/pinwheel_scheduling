#pragma once
#ifndef PINWHEEL_SCHEDULING_PLAN_PACK_HPP
#define PINWHEEL_SCHEDULING_PLAN_PACK_HPP

#include <algorithm>
#include <numeric>
#include "schedule.hpp"
#include "point.hpp"

template <typename T>
class PlanPack
{
public:
    typedef Schedule<T> Schedule;
    typedef Schedule::Job Job;
    typedef Schedule::UpperBound UpperBound;
    typedef Point<Fraction> Point;

    static const PlanPack& equal_plan(size_t dimension)
    {
        static std::vector<PlanPack> plans(1, PlanPack());
        while (plans.size() <= dimension)
        {
            size_t dim = plans.size();
            std::vector<Job> tasks(dim);
            std::iota(tasks.begin(), tasks.end(), 0);
            std::vector<UpperBound> bounds(dim, UpperBound(Fraction(1,dim), false));
            plans.emplace_back(Schedule(tasks), bounds);
        }
        return plans[dimension];
    }
    static const PlanPack& one_nth_plan(size_t n)
    {
        static std::vector<PlanPack> plans(1, PlanPack());
        while (plans.size() <= n)
        {
            size_t dim = plans.size();
            std::vector<Job> tasks(dim, 0);
            tasks.back() = 1;
            std::vector<UpperBound> bounds(2);
            bounds[0].set_value(Fraction(dim-1,dim));
            bounds[1].set_value(1,dim);
            plans.emplace_back(Schedule(tasks), bounds);
        }
        return plans[n];
    }
    static const PlanPack& rayleigh_plan(Fraction f)
    {
        if ( f >= 1 or f < Fraction(1,2) )
            throw std::invalid_argument("Invalid rayleigh plan");
        static std::map<Fraction, PlanPack> plans;
        if (!plans.count(f))
        {
            auto numerator = f.numerator().convert_to<size_t>();
            auto denominator = f.denominator().convert_to<size_t>();
            std::vector<Job> tasks(denominator, 1);
            for (size_t i = 0 ; i < numerator; i++)
                tasks[i*denominator/numerator] = 0;
            std::vector<UpperBound> bounds(2);
            bounds[0].set_value(f);
            bounds[1].set_value(Fraction(1)-f);
            plans[f] = PlanPack(Schedule(tasks), bounds);
        }
        return plans[f];
    }
    void repair()
    {
        if (job_bounds.empty()) return;

        std::vector<size_t> p(job_bounds.size());
        std::iota(p.begin(), p.end(), 0);
        std::sort(p.begin(), p.end(), [&](size_t i, size_t j)
        {
            return job_bounds[i] < job_bounds[j];
        });

        std::vector<UpperBound> sorted_bounds(job_bounds.size());
        std::vector<Job> inv_p(job_bounds.size());
        for (size_t i = 0; i < p.size(); ++i)
        {
            sorted_bounds[i] = job_bounds[p[i]];
            inv_p[p[i]] = static_cast<Job>(i);
        }
        job_bounds = std::move(sorted_bounds);

        std::vector<Job> tasks = schedule.get_tasks();
        for (Job& task : tasks)
        {
            task = inv_p[task];
        }
        minimum_cyclic_permutation(tasks);

        schedule = Schedule(tasks);
    }

private:
    Schedule schedule;
    std::vector<UpperBound> job_bounds;

public:
    PlanPack(const Schedule& schedule, const std::vector<UpperBound>& job_bounds) :
        schedule(schedule),
        job_bounds(job_bounds)
    {
        if (schedule.get_dimension() != job_bounds.size())
            throw std::invalid_argument("Schedule dimension and job bounds size must match");
        repair();
    }

    PlanPack(const Schedule& schedule = Schedule()) :
        PlanPack(schedule, schedule.get_upper_bounds())
    {
    }

    PlanPack(const std::string& input)
    {
        from_string(input);
    }

    std::string to_string() const
    {
        std::string result = string_repr(schedule);
        for (const auto& bound : job_bounds)
            result += "," + string_repr(bound);
        return result;
    }

    void from_string(const std::string& input)
    {
        std::string inp = string_strip(input);
        if (inp.empty())
            throw std::invalid_argument("Invalid plan format");
        auto parts = string_split(inp, ",");
        if (parts.size() == 1)
        {
            schedule = string_cast<Schedule>(parts[0]);
            job_bounds = schedule.get_upper_bounds();
        }
        else if (parts.size() > 1)
        {
            schedule = string_cast<Schedule>(parts[0]);
            if (schedule.get_dimension() != parts.size() - 1)
                throw std::invalid_argument("Schedule dimension and job bounds size must match");
            job_bounds.clear();
            for (size_t i = 1; i < parts.size(); ++i)
                job_bounds.push_back(string_cast<UpperBound>(parts[i]));
        }
        else
            throw std::invalid_argument("Invalid plan format");

        repair();
    }

    size_t size() const { return schedule.size(); }
    size_t get_size() const { return schedule.size(); }
    Job get_dimension() const { return schedule.get_dimension(); }

    Fraction get_density() const
    {
        Fraction result = 0;
        Fraction one = 1;
        for (const auto& bound : job_bounds)
            result = result + bound.get_value();
        return result;
    }

    bool is_better(const PlanPack& other) const
    {
        if (get_dimension() != other.get_dimension())
            return false;
        auto dim = get_dimension();
        bool worse = false;
        bool better = false;
        for (size_t i = 0; i < dim; ++i)
        {
            if (job_bounds[i] < other.job_bounds[i])
            {
                better = true;
            }
            else if (other.job_bounds[i] < job_bounds[i])
            {
                worse = true;
                return false;
            }
        }
        if (worse)
            return false;
        if (better)
            return true;
        if (get_size() != other.get_size())
            return get_size() < other.get_size();
        return get_schedule() < other.get_schedule();
    }

    bool operator<(const PlanPack& other) const
    {
        if (get_dimension() != other.get_dimension())
            return get_dimension() < other.get_dimension();
        if (get_size() != other.get_size())
            return get_size() < other.get_size();
        if (get_density() != other.get_density())
            return get_density() > other.get_density();
        return schedule < other.schedule;
    }

    bool operator>(const PlanPack& other) const { return other < *this; }

    bool operator==(const PlanPack& other) const
    {
        return schedule == other.schedule;
    }

    bool operator!=(const PlanPack& other) const { return !(*this == other); }

    bool simply_satisfies(const Point& point, Fraction& slack) const
    {
        if (point.get_dimension() != get_dimension())
            return false;
        const auto& coordinates = point.get_coordinates();
        slack = 0;
        for (size_t i = 0; i < coordinates.size(); ++i)
        {
            if (!job_bounds[i].get_satisfied(coordinates[i]))
                return false;
            Fraction cslack = job_bounds[i].get_value() / coordinates[i];
            if (i == 0 or cslack < slack)
                slack = cslack;
        }
        return true;
    }
    bool simply_satisfies(const Point& point) const
    {
        Fraction slack;
        return simply_satisfies(point, slack);
    }

    static void inject(std::vector<Job>& tasks, Job job, const std::vector<Job>& other_tasks)
    {
        Job dimension = 0;
        size_t count = 0;
        for (const auto& j : tasks)
        {
            if (j == job)
                count++;
            dimension = std::max(dimension, j+1);
        }
        std::vector<Job> new_tasks(tasks.size()*std::lcm(count, other_tasks.size())/count);
        size_t j = 0;
        for (size_t i = 0; i < new_tasks.size(); i++)
        {
            new_tasks[i] = tasks[i % tasks.size()];
            if (new_tasks[i] == job)
            {
                new_tasks[i] = other_tasks[j % other_tasks.size()] + dimension;
                j++;
            }
        }
        tasks = std::move(new_tasks);
    }

    PlanPack inject(const std::map<Job, PlanPack>& subplans) const
    {
        std::vector<Job> tasks = schedule.get_tasks();
        for (const auto& subplan : subplans)
            inject(tasks, subplan.first, subplan.second.get_tasks());
        return PlanPack(Schedule(tasks));
    }

    const Schedule& get_schedule() const { return schedule; }
    const std::vector<Job>& get_tasks() const { return schedule.get_tasks(); }
    const std::vector<UpperBound>& get_job_bounds() const { return job_bounds; }

    friend std::ostream& operator<<(std::ostream& os, const PlanPack& obj)
    {
        return os << obj.to_string();
    }

    friend std::istream& operator>>(std::istream& is, PlanPack& obj)
    {
        std::string temp;
        is >> temp;
        obj.from_string(temp);
        return is;
    }
};

#endif // PINWHEEL_SCHEDULING_PLAN_PACK_HPP
