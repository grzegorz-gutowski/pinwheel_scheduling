#pragma once
#ifndef PINWHEEL_SCHEDULING_STRATEGY_PACK_HPP
#define PINWHEEL_SCHEDULING_STRATEGY_PACK_HPP

#include <list>

#include "plan_pack.hpp"
#include "point.hpp"

template<typename T>
class StrategyPack {
public:
    typedef PlanPack<T> Plan;
    typedef Plan::Schedule Schedule;
    typedef Schedule::Job Job;
    typedef Schedule::LowerBound LowerBound;
    typedef Point<Fraction> Point;

private:
    std::list<Plan> plans;

public:
    void push_back(const Plan& plan)
    {
        bool minimal = true;
        for (auto pi = plans.begin() ; pi != plans.end() ; ++pi )
        {
            if ( *pi == plan or pi->is_better(plan) )
            {
                minimal = false;
                break;
            }
        }
        if (minimal)
        {
            for (auto pi = plans.begin() ; pi != plans.end() ; )
            {
                if ( plan.is_better(*pi) )
                {
                    pi = plans.erase(pi);
                } else {
                    ++pi;
                }
            }
            plans.push_back(plan);
        }
    }

    size_t size() const { return plans.size(); }
    size_t get_size() const { return plans.size(); }
    std::vector<Plan> get_plans() const
    {
        std::vector<Plan> result;
        for (const auto& plan : plans)
            result.push_back(plan);
        return result;
    }
    bool satisfies(const Point& point) const {
        for (const auto& plan : plans) {
            if (plan.satisfies(point))
                return true;
        }
        return false;
    }
    std::vector<Plan> satisfies_plans(const Point& point) const
    {
        std::vector<Plan> result;
        for (const auto& plan : plans) {
            if (plan.satisfies(point))
                result.push_back(plan);
            }
        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const Strategy& obj) {
        for (const auto& plan : obj.plans)
            os << plan << std::endl;
        return os;
    }
};

#endif // PINWHEEL_SCHEDULING_STRATEGY_PACK_HPP
