#pragma once
#ifndef PINWHEEL_SCHEDULING_BOUND_HPP
#define PINWHEEL_SCHEDULING_BOUND_HPP

#include <string>

#include "utils.hpp"

template <typename T>
requires requires(const T& v) { v < v and v <= v;}
class LowerBound {
private:
    T value;
    bool strict;
public:
    LowerBound(const T& value = T(), bool strict = false) :
        value(value),
        strict(strict) {
        }
    LowerBound(const std::string& desc) {
        from_string(desc);
    }

    std::string to_string() const {
        return (strict?">":">=") + string_repr(value);
    }
    void from_string(const std::string& input) {
        auto inp = string_strip(input);
        if ( inp.empty() or inp[0] != '>' )
            throw std::invalid_argument("Invalid lower bound format");
        if (inp.size() > 1 && inp[1] == '=') {
            strict = false;
            inp = string_strip(inp.substr(2));
        } else {
            strict = true;
            inp = string_strip(inp.substr(1));
        }
        if (inp.empty())
            throw std::invalid_argument("Invalid lower bound format");
        value = string_cast<T>(inp);
    }
    bool operator==(const LowerBound& other) const {
        return value == other.value && strict == other.strict;
    }
    bool operator!=(const LowerBound& other) const { return !(*this == other); }
    bool operator<(const LowerBound& other) const {
        if (value != other.value) {
            return value < other.value;
        }
        return !strict && other.strict;
    }
    bool operator>(const LowerBound& other) const { return other < *this; }

    bool get_satisfied(const T& x) const {
        if (strict) {
            return x > value;
        } else {
            return x >= value;
        }
    }

    const T& get_value() const { return value; }
    bool get_strict() const { return strict; }
    void set_value(const T& val) { value = val; }
    void set_strict(bool val) { strict = val; }

    friend std::ostream& operator<<(std::ostream& os, const LowerBound& obj)
    {
        return os << obj.to_string();
    }
    friend std::istream& operator>>(std::istream& is, LowerBound& obj)
    {
        std::string temp;
        is >> temp;
        obj.from_string(temp);
        return is;
    }
};

template <typename T>
requires requires(const T& v) { v < v and v <= v;}
class UpperBound {
private:
    T value;
    bool strict;
public:
    UpperBound(const T& value = T(), bool strict = false) :
        value(value),
        strict(strict) {
        }
    UpperBound(const std::string& desc) {
        from_string(desc);
    }

    std::string to_string() const {
        return (strict?"<":"<=") + string_repr(value);
    }
    void from_string(const std::string& input) {
        auto inp = string_strip(input);
        if ( inp.empty() or inp[0] != '<' )
            throw std::invalid_argument("Invalid upper bound format");
        if (inp.size() > 1 && inp[1] == '=') {
            strict = false;
            inp = string_strip(inp.substr(2));
        } else {
            strict = true;
            inp = string_strip(inp.substr(1));
        }
        if (inp.empty())
            throw std::invalid_argument("Invalid lower bound format");
        value = string_cast<T>(inp);
    }
    bool operator==(const UpperBound& other) const {
        return value == other.value && strict == other.strict;
    }
    bool operator!=(const UpperBound& other) const { return !(*this == other); }
    bool operator<(const UpperBound& other) const {
        if (value != other.value) {
            return value > other.value;
        }
        return !strict && other.strict;
    }
    bool operator>(const UpperBound& other) const { return other < *this; }

    bool get_satisfied(const T& x) const {
        if (strict) {
            return x < value;
        } else {
            return x <= value;
        }
    }

    const T& get_value() const { return value; }
    bool get_strict() const { return strict; }
    void set_value(const T& val) { value = val; }
    void set_strict(bool val) { strict = val; }

    friend std::ostream& operator<<(std::ostream& os, const UpperBound& obj)
    {
        return os << obj.to_string();
    }
    friend std::istream& operator>>(std::istream& is, UpperBound& obj)
    {
        std::string temp;
        is >> temp;
        obj.from_string(temp);
        return is;
    }
};

#endif // PINWHEEL_SCHEDULING_BOUND_HPP
