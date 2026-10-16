#pragma once

#ifndef PINWHEEL_SCHEDULING_UTILS_HPP
#define PINWHEEL_SCHEDULING_UTILS_HPP

#include <cctype>
#include <cstddef>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#include <boost/rational.hpp>
#include <boost/multiprecision/cpp_int.hpp>

typedef boost::rational<boost::multiprecision::cpp_int> Fraction;

inline std::string string_strip(const std::string& input) {
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
    {
        ++start;
    }
    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
    {
        --end;
    }
    return input.substr(start, end - start);
}

inline std::vector<std::string> string_split(const std::string& input, const std::string& delimiter) {
    std::vector<std::string> result;
    if (delimiter.empty()) {
        result.push_back(input);
        return result;
    }
    std::size_t start = 0;
    std::size_t end = input.find(delimiter);
    while (end != std::string::npos) {
        result.push_back(input.substr(start, end - start));
        start = end + delimiter.size();
        end = input.find(delimiter, start);
    }
    result.push_back(input.substr(start));

    return result;
}

template<typename T>
inline std::string string_repr(const T& value);

template <typename T, typename Alloc>
inline std::string string_join(const std::vector<T, Alloc>& input, const std::string& delimiter, const std::string& header="", const std::string& footer="") {
    std::string result;
    result += header;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (i > 0)
            result += delimiter;
        result += string_repr(input[i]);
    }
    result += footer;
    return result;
}

template<typename T>
inline T string_cast(const std::string& input) {
    if constexpr (requires(T result) { result.from_string(input); }) {
        T result;
        result.from_string(input);
        return result;
    } else if constexpr (requires() { T(input); }) {
        return T(input);
    } else if constexpr (requires(std::istringstream& iss, T& v) { iss >> v; }) {
        std::istringstream iss(input);
        T result;
        if (!(iss >> result)) {
            throw std::invalid_argument("Invalid string format for the target type");
        }
        return result;
    } else
        static_assert(sizeof(T) == 0, "No string conversion available for this type");
    return T();
}

template <typename T, typename Alloc>
inline std::string string_repr(const std::vector<T, Alloc>& values) {
    std::vector<std::string> str_values;
    for (const auto& val : values)
        str_values.emplace_back(string_repr(val));
    return string_join(str_values, ",", "[", "]");
}
template <typename K, typename V, typename Compare, typename Alloc>
inline std::string string_repr(const std::map<K, V, Compare, Alloc>& values) {
    std::string header = "{";
    std::string footer = "}";
    std::string delimiter = ",";
    std::string key_value_delimiter = ":";
    std::string result;
    result += header;
    bool first = true;
    for (const auto& [key, value] : values) {
        if (!first) {
            result += delimiter;
        }
        first = false;
        result += string_repr(key) + key_value_delimiter + string_repr(value);
    }
    result += footer;
    return result;
}
template<typename T, typename U>
inline std::string string_repr(const std::pair<T,U>& value)
{
    std::string result = "(";
    result += string_repr(value.first);
    result += ",";
    result += string_repr(value.second);
    result += ")";
    return result;
}
template<typename T>
inline std::string string_repr(const T& value) {
    if constexpr (requires { value.to_string(); }) {
        return value.to_string();
    } else if constexpr (requires { std::to_string(value); }) {
        return std::to_string(value);
    } else if constexpr (std::is_convertible_v<T, std::string>) {
        return std::string(value);
    } else if constexpr (requires(std::ostringstream& oss, const T& v) { oss << v; }) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    } else
        static_assert(sizeof(T) == 0, "No string representation available for this type");
    return "";
}

template <typename T, typename Alloc>
inline void minimum_cyclic_permutation(std::vector<T, Alloc>& values) {
    if (values.empty()) return;
    size_t n = values.size();
    size_t best_start = 0, i = 1, k = 0;
    while (best_start < n && i < n && k < n) {
        const T& a = values[(best_start + k) % n];
        const T& b = values[(i + k) % n];
        if (a == b) {
            k++;
        } else {
            if (a < b)
                i += k+1;
            else {
                best_start = i;
            }
            if (i == best_start)
                i++;
            k = 0;
        }
    }
    std::rotate(values.begin(), values.begin() + best_start, values.end());
}

inline Fraction math_ceil(const Fraction& f)
{
    auto n = f.numerator() + f.denominator() - 1;
    auto& d = f.denominator();
    return Fraction(n / d, 1);
}
inline Fraction math_floor(Fraction f)
{
    auto n = f.numerator();
    auto d = f.denominator();
    return Fraction(n / d, 1);
}

class Subset
{
    uint64_t value;
public:
    Subset(uint8_t dimension = 0, uint64_t value = 0) : value((value << 8) | dimension)
    {
    }
    Subset(const Subset& other) : value(other.value)
    {
    }
    Subset(const std::vector<bool>& bitstring) : value(0)
    {
        uint8_t dimension = bitstring.size();
        for (size_t i = 0; i < dimension; ++i)
        {
            if (bitstring[i])
                value |= (1ULL << i);
        }
        value <<= 8;
        value |= dimension;
    }
    Subset(uint8_t dimension, const std::vector<uint8_t>& elements) : value(0)
    {
        for (const auto& element : elements)
        {
            value |= (1ULL << element);
        }
        value <<= 8;
        value |= dimension;
    }
    Subset(const std::string& s) : value(0)
    {
        from_string(s);
    }
    static Subset empty(uint8_t dimension)
    {
        return Subset(dimension, 0);
    }
    static Subset complete(uint8_t dimension)
    {
        return Subset(dimension, (1ULL << dimension) - 1);
    }
    uint8_t get_dimension() const
    {
        return value & 255;
    }
    uint64_t get_value() const
    {
        return value >> 8;
    }
    bool get_element(const uint8_t i) const
    {
        auto v = get_value();
        return (v & (1ULL << i)) != 0;
    }
    void set_element(const uint8_t i, const bool s)
    {
        auto v = get_value();
        auto d = get_dimension();
        if (s)
            v |= (1ULL << i);
        else
            v &= ~(1ULL << i);
        value = (v << 8) | d;
    }
    void clear()
    {
        value = get_dimension();
    }
    bool operator[](const uint8_t i) const
    {
        return get_element(i);
    }
    std::vector<bool> get_bitstring() const
    {
        auto d = get_dimension();
        std::vector<bool> result(d);
        for (uint8_t i = 0; i < d; ++i)
            result[i] = get_element(i);
        return result;
    }
    std::vector<uint8_t> get_elements() const
    {
        auto d = get_dimension();
        std::vector<uint8_t> result;
        for (uint8_t i = 0; i < d; ++i)
            if (get_element(i))
                result.push_back(i);
        return result;
    }
    uint8_t get_size() const
    {
        return __builtin_popcountll(get_value());
    }
    Subset get_complement() const
    {
        auto d = get_dimension();
        auto v = get_value() ^ ((1ULL << d)-1);
        return Subset(d, v);
    }
    Subset AND(const Subset& other) const
    {
        auto v = get_value() & other.get_value();
        return Subset(get_dimension(), v);
    }
    Subset OR(const Subset& other) const
    {
        auto v = get_value() | other.get_value();
        return Subset(get_dimension(), v);
    }
    Subset NEG() const
    {
        auto d = get_dimension();
        auto v = get_value() ^ ((1ULL << d)-1);
        return Subset(d, v);
    }
    Subset XOR(const Subset& other) const
    {
        auto v = get_value() ^ other.get_value();
        return Subset(get_dimension(), v);
    }
    std::vector<Subset> get_subsets( size_t begin=0, size_t end=std::numeric_limits<size_t>::max()) const
    {
        if ( end ==  std::numeric_limits<size_t>::max() )
            end = 1ULL << get_size();
        std::vector<Subset> result;
        Subset sub(get_dimension(), 0);
        auto elements = get_elements();
        for (size_t i = begin ; i < end ; i++)
        {
            sub.clear();
            for (size_t j = 0 ; j < elements.size() ; j++)
                if (i & (1ULL << j))
                    sub.set_element(elements[j], true);
            result.emplace_back(sub);
        }
        return result;
    }
    std::vector<Subset> get_proper_subsets() const
    {
        return get_subsets(1, (1ULL << get_size())-1);
    }

    void visit_partitions(std::vector<std::vector<Subset>>& result, const std::vector<uint8_t>& elements, std::vector<uint8_t> stack, uint8_t parts, uint8_t choices=0) const
    {
        if ( (stack.size() == elements.size()) )
        {
            if ( choices > 1 )
            {
                std::vector<Subset> partition(choices, Subset(get_dimension(), 0));
                for (size_t i = 0 ; i < stack.size() ; i++)
                    partition[stack[i]].set_element(elements[i], true);
                result.emplace_back(partition);
            }
            return;
        }
        for ( uint8_t l = 0 ; l < std::min<uint8_t>(choices+1, parts) ; l++ )
        {
            stack.push_back(l);
            visit_partitions(result, elements, stack, parts, std::max<uint8_t>(choices, l+1));
            stack.pop_back();
        }
    }

    std::vector<std::vector<Subset>> get_proper_partitions(uint8_t parts, size_t begin=0, size_t end=std::numeric_limits<size_t>::max()) const
    {
        std::vector<std::vector<Subset>> result;
        std::vector<uint8_t> stack;
        visit_partitions(result, get_elements(), stack, parts);
        return result;
    }
    bool operator==(const Subset& other) const
    {
        return value == other.value;
    }
    bool operator!=(const Subset& other) const
    {
        return !(*this == other);
    }
    bool operator<(const Subset& other) const
    {
        return value < other.value;
    }
    bool operator>(const Subset& other) const
    {
        return other < *this;
    }
    bool operator<=(const Subset& other) const
    {
        return !(other < *this);
    }
    bool operator>=(const Subset& other) const
    {
        return !(*this < other);
    }
    std::string to_string() const
    {
        return string_join(get_bitstring(), "", "", "");
    }
    void from_string(const std::string& s)
    {
        uint8_t count = 0;
        value = 0;
        for (const auto& symbol : s)
        {
            if (symbol == '1')
            {
                value |= (1ULL << count);
                count++;
            } else if (symbol == '0')
                count++;
        }
        value <<= 8;
        value |= count;
    }
    friend std::ostream& operator<<(std::ostream& os, const Subset& obj)
    {
        return os << string_repr(obj);
    }
    friend std::istream& operator>>(std::istream& is, Subset& obj)
    {
        std::string temp;
        is >> temp;
        obj.from_string(temp);
        return is;
    }
};

#endif //PINWHEEL_SCHEDULING_UTILS_HPP