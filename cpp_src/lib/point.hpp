#pragma once
#ifndef PINWHEEL_SCHEDULING_POINT_HPP
#define PINWHEEL_SCHEDULING_POINT_HPP

#include <string>
#include <vector>

#include "utils.hpp"

template<typename T>
class Point {
private:
    std::vector<T> coordinates;
public:
    Point(const std::vector<T>& coordinates) :
        coordinates(coordinates) {
        }
    Point(const std::string& input) {
        from_string(input);
    }
    Point(const char* input) {
        from_string(input);
    }
    std::string to_string() const {
        return string_join(coordinates, ",", "(", ")");
    }
    void from_string(const std::string& input)
    {
        coordinates.clear();
        std::string inp = string_strip(input);
        if (inp.size() < 2 || inp[0] != '(' || inp[inp.size()-1] != ')')
            throw std::invalid_argument("Invalid point format");
        inp = string_strip(inp.substr(1, inp.size()-2));
        if (inp.empty())
            throw std::invalid_argument("Invalid point format");
        auto coords = string_split(inp, ",");
        for (auto coord : coords) {
            coordinates.push_back(string_cast<T>(coord));
        }
    }
    bool operator==(const Point& other) const {
        return coordinates == other.coordinates;
    }
    bool operator!=(const Point& other) const { return !(*this == other); }
    bool is_smaller(const Point& other) const
    {
        for (size_t i = 0; i < std::max(coordinates.size(),other.coordinates.size()); ++i) {
            auto mc = (i<coordinates.size()) ? coordinates[i] : T();
            auto oc = (i<other.coordinates.size()) ? other.coordinates[i] : T();
            if (mc != oc) {
                return mc < oc;
            }
        }
        return false;
    }
    size_t size() const { return coordinates.size(); }
    size_t get_size() const { return coordinates.size(); }
    size_t get_dimension() const { return coordinates.size(); }
    const std::vector<T>& get_coordinates() const { return coordinates; }
    T get_coordinate(size_t index) const {
        if (index >= coordinates.size())
            throw std::out_of_range("Coordinate index out of range");
        return coordinates[index];
    }
    void set_coordinate(size_t index, T val) {
        if (index >= coordinates.size())
            throw std::out_of_range("Coordinate index out of range");
        coordinates[index] = val;
        optimal_pack_folds.resize(0);
        optimal_cover_folds.resize(0);
    }
    void make_symmetric_cover()
    {
        std::sort(coordinates.begin(), coordinates.end(), std::greater<T>());
        optimal_pack_folds.resize(0);
        optimal_cover_folds.resize(0);
    }
    void make_symmetric_pack()
    {
        std::sort(coordinates.begin(), coordinates.end(), std::greater<T>());
        optimal_pack_folds.resize(0);
        optimal_cover_folds.resize(0);
    }
    T get_density() const
    {
        T sum = T(0);
        for (const auto& coord : coordinates) {
            sum += coord;
        }
        return sum;
    }
    // Prefix of the current order: largest densities after symmetrization.
    Point get_subpoint(size_t dimension) {
        dimension = std::min(dimension, get_dimension());
        std::vector<T> coords;
        for (size_t i = 0 ; i < dimension ; i++ )
            coords.push_back(get_coordinate(i));
        coords.resize(dimension);
        return Point(coords);
    }

    friend std::ostream& operator<<(std::ostream& os, const Point& obj)
    {
        return os << obj.to_string();
    }
    friend std::istream& operator>>(std::istream& is, Point& obj)
    {
        std::string temp;
        is >> temp;
        obj.from_string(temp);
        return is;
    }

    std::vector<std::pair<T,std::vector<Subset>>> optimal_pack_folds;
    std::vector<std::pair<T,std::vector<Subset>>> optimal_cover_folds;

    T optimal_pack_fold(const Subset& sub)
    {
        auto value = sub.get_value();
        while (optimal_pack_folds.size() <= value)
        {
            Subset current(get_dimension(), optimal_pack_folds.size());
            auto size = current.get_size();
            T best_value(2*get_density());
            std::vector<Subset> best_fold;
            if ( size == 0 )
                best_value = 0;
            else if ( size == 1 )
                best_value = get_coordinate(current.get_elements().back());
            else
            {
                for (const auto& partition : current.get_proper_partitions(size))
                {
                    T max_val = optimal_pack_fold(partition[0]);
                    for (const auto& part : partition)
                        max_val = std::max(max_val, optimal_pack_fold(part));
                    if (max_val*partition.size() < best_value)
                    {
                        best_value = max_val*partition.size();
                        best_fold = partition;
                    }
                }
            }
            optimal_pack_folds.emplace_back(best_value, best_fold);
        }
        return optimal_pack_folds[value].first;
    }
    T optimal_cover_fold(const Subset& sub)
    {
        auto value = sub.get_value();
        while (optimal_cover_folds.size() <= value)
        {
            Subset current(get_dimension(), optimal_cover_folds.size());
            auto size = current.get_size();
            T best_value(0);
            std::vector<Subset> best_fold;
            if ( size == 0 )
                best_value = 0;
            else if ( size == 1 )
                best_value = get_coordinate(current.get_elements().back());
            else
            {
                for (const auto& partition : current.get_proper_partitions(size))
                {
                    T min_val = optimal_cover_fold(partition[0]);
                    for (const auto& part : partition)
                        min_val = std::min(min_val, optimal_cover_fold(part));
                    if (min_val*partition.size() > best_value)
                    {
                        best_value = min_val*partition.size();
                        best_fold = partition;
                    }
                }
            }
            optimal_cover_folds.emplace_back(best_value, best_fold);
        }
        return optimal_cover_folds[value].first;
    }
 
    T optimal_pack_fold()
    {
        auto result = optimal_pack_fold(Subset::complete(get_dimension()));
        return result;
    }
    T optimal_cover_fold()
    {
        auto result = optimal_cover_fold(Subset::complete(get_dimension()));
        return result;
    }
    std::map<size_t, T> optimal_sum_pack_folds()
    {
        std::map<size_t, T> result;
        result[1] = optimal_pack_fold();
        auto partitions = Subset::complete(get_dimension()).get_proper_partitions(get_dimension());
        for (const auto& partition : partitions)
        {
            T total = 0;
            for (size_t i = 0; i < partition.size(); ++i)
                total += optimal_pack_fold(partition[i]);
            if (result.find(partition.size()) != result.end())
                if (result[partition.size()] < total)
                    continue;
            result[partition.size()] = total;
        }
        return result;
    }
    std::map<size_t, T> optimal_sum_cover_folds()
    {
        std::map<size_t, T> result;
        result[1] = optimal_cover_fold();
        auto partitions = Subset::complete(get_dimension()).get_proper_partitions(get_dimension());
        for (const auto& partition : partitions)
        {
            T total = 0;
            for (size_t i = 0; i < partition.size(); ++i)
                total += optimal_cover_fold(partition[i]);
            if (result.find(partition.size()) != result.end())
                if (result[partition.size()] > total)
                    continue;
            result[partition.size()] = total;
        }
        return result;
    }
};

#endif // PINWHEEL_SCHEDULING_POINT_HPP
