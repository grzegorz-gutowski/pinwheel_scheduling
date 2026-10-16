#pragma once
#ifndef PINWHEEL_SCHEDULING_SCHEDULE_HPP
#define PINWHEEL_SCHEDULING_SCHEDULE_HPP

#include <vector>
#include <iostream>
#include <set>
#include <map>

#include "bound.hpp"
#include "point.hpp"
#include "utils.hpp"

template<typename T>
class Schedule {
public:
    typedef T Job;
    using UpperBound = ::UpperBound<Fraction>;
    using LowerBound = ::LowerBound<Fraction>;
private:
    size_t dimension;
    std::vector<Job> tasks;
    void repair() {
        if (tasks.empty()) {
            dimension = 0;
            return;
        }

        std::set<Job> jobs;
        for (Job job : tasks) {
            jobs.insert(job);
        }
        dimension = jobs.size();

        std::map<Job, Job> mapping;
        Job next = 0;
        for (Job job : jobs) {
            mapping[job] = next++;
        }
        for (Job& task : tasks) {
            task = mapping[task];
        }
    }
    std::vector<size_t> get_intervals(Job job) const {
        if (job >= dimension)
            throw std::invalid_argument("Invalid job index");
        std::vector<size_t> result;
        size_t f = tasks.size();
        size_t p = tasks.size();
        for (size_t i = 0; i < tasks.size(); i++) {
            if (tasks[i] == job) {
                if (f > i)
                    f = i;
                if (p < i) {
                    result.push_back(i - p);
                }
                p = i;
            }
        }
        if (p < tasks.size()) {
            result.push_back(tasks.size() - p + f);
        }
        return result;
    }
    static UpperBound get_upper_bound(const std::vector<size_t>& intervals) {
        std::vector<size_t> prefix_sum(2*intervals.size(), 0);
        prefix_sum[0] = intervals[0];
        for (size_t i = 1; i < 2*intervals.size(); ++i) {
            prefix_sum[i] = prefix_sum[i-1] + intervals[i % intervals.size()];
        }
        auto average = Fraction(intervals.size(), prefix_sum[intervals.size()-1]);
        UpperBound result = UpperBound(average, false);
        for (size_t i = 0; i < prefix_sum.size(); ++i) {
            for (size_t j = 1; i+j < prefix_sum.size(); ++j) {
                size_t sum = prefix_sum[i+j] - prefix_sum[i];
                if (sum > 1)
                    result = std::max(result, UpperBound(Fraction(j, sum-1), true));
            }
        }
        return result;
    }
    static LowerBound get_lower_bound(const std::vector<size_t>& intervals) {
        std::vector<size_t> prefix_sum(2*intervals.size(), 0);
        prefix_sum[0] = intervals[0];
        for (size_t i = 1; i < 2*intervals.size(); ++i) {
            prefix_sum[i] = prefix_sum[i-1] + intervals[i % intervals.size()];
        }
        auto average = Fraction(intervals.size(), prefix_sum[intervals.size()-1]);
        LowerBound result = LowerBound(average, false);
        for (size_t i = 0; i < prefix_sum.size(); ++i) {
            for (size_t j = 1; i+j < prefix_sum.size(); ++j) {
                size_t sum = prefix_sum[i+j] - prefix_sum[i];
                result = std::max(result, LowerBound(Fraction(j, sum+1), true));
            }
        }
        return result;
    }
 
    UpperBound get_upper_bound(Job job) const {
        return get_upper_bound(get_intervals(job));
    }
    LowerBound get_lower_bound(Job job) const {
        return get_lower_bound(get_intervals(job));
    }
public:
    Schedule(const std::vector<Job>& tasks = std::vector<Job>()) {
        this->tasks = tasks;
        repair();
    }
    Schedule(const std::string& schedule_str) {
        from_string(schedule_str);
    }

    std::string to_string() const {
        std::string result;
        for (Job task : tasks) {
            result += static_cast<char>('A' + task);
        }
        return result;
    }
    void from_string(const std::string& schedule_str) {
        tasks.clear();
        for (char c : schedule_str) {
            if (c >= 'A' && c <= 'Z') {
                tasks.push_back(c - 'A');
            }
        }
        repair();
    }

    bool operator<(const Schedule& other) const {
        if (get_dimension() != other.get_dimension())
            return get_dimension() < other.get_dimension();
        if (tasks.size() != other.tasks.size())
            return tasks.size() < other.tasks.size();
        for (size_t i = 0; i < tasks.size(); i++)
            if (tasks[i] != other.tasks[i])
                return tasks[i] < other.tasks[i];
        return false;
    }
    bool operator==(const Schedule& other) const {
        return tasks == other.tasks;
    }
    bool operator!=(const Schedule& other) const { return !(*this == other); }
    bool operator>(const Schedule& other) const { return other < *this; }
    bool operator<=(const Schedule& other) const { return !(*this > other); }
    bool operator>=(const Schedule& other) const { return !(*this < other); }

    size_t size() const { return tasks.size(); }
    size_t get_size() const { return tasks.size(); }
    Job get_dimension() const { return dimension; }
    const std::vector<Job>& get_tasks() const { return tasks; }

    std::vector<UpperBound> get_upper_bounds() const {
        std::vector<UpperBound> result(dimension);
        for (size_t job = 0; job < dimension; job++) {
            result[job] = get_upper_bound(job);
        }
        return result;
    }

    std::vector<LowerBound> get_lower_bounds() const {
        std::vector<LowerBound> result(dimension);
        for (size_t job = 0; job < dimension; job++) {
            result[job] = get_lower_bound(job);
        }
        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const Schedule& obj) {
        return os << obj.to_string();
    }

    friend std::istream& operator>>(std::istream& is, Schedule& obj) {
        std::string temp;
        is >> temp;
        obj.from_string(temp);
        return is;
    }
};

#endif // PINWHEEL_SCHEDULING_SCHEDULE_HPP
