#include <fstream>
#include <sstream>
#include <vector>

#include <boost/program_options.hpp>

#include "lib/schedule.hpp"
#include "lib/plan_pack.hpp"
#include "lib/point.hpp"

namespace bpo = boost::program_options;

typedef PlanPack<unsigned long long> CPlan;
typedef CPlan::Point CPoint;

CPlan find_plan(const std::vector<CPlan>& plans, const CPoint& point);

CPlan easy_plan(const std::vector<CPlan>& plans, const CPoint& point)
{
    CPlan best_plan;
    auto dimension = point.get_dimension();
    auto density = point.get_density();
    if ( density > 1 )
        return best_plan;
    if ( dimension <= 1)
        return CPlan("A");
    if ( point.get_dimension() == 2 )
    {
        for (size_t denominator = 2 ; denominator <= 100 ; denominator++ )
        {
            Fraction rest = Fraction(1) - point.get_coordinate(0);
            size_t num = ((rest*denominator).numerator() / (rest*denominator).denominator()).convert_to<size_t>();
            Fraction rounded = Fraction(num, denominator);
            if (rounded > 0) {
                auto plan = CPlan::rayleigh_plan(Fraction(1)-rounded);
                if ( plan.simply_satisfies(point) )
                    return plan;
            }
        }
    }
    for (const auto& plan : plans)
        if (plan.simply_satisfies(point))
            return plan;
    return best_plan;
}

CPlan optimal_folded_plan(const std::vector<CPlan>& plans, CPoint& point, Subset subset)
{
    if ( subset.get_size() == 1)
        return CPlan("A");
    auto partition = point.optimal_pack_folds[subset.get_value()].second;
    CPlan best_plan = CPlan::equal_plan(partition.size());
    std::map<CPlan::Job, CPlan> inject_map;
    for (size_t i = 0; i < partition.size(); ++i)
        inject_map[i] = optimal_folded_plan(plans, point, partition[i]);
    best_plan = best_plan.inject(inject_map);
    return best_plan;
}

CPlan optimal_folded_plan(const std::vector<CPlan>& plans, CPoint& point)
{
    CPlan best_plan;
    auto fold = point.optimal_pack_fold();
    if ( fold <= 1 )
        return optimal_folded_plan(plans, point, Subset::complete(point.get_dimension()));
    return best_plan;
}
CPlan easy_optimal_folded_plan(const std::vector<CPlan>& plans, CPoint& point)
{
    auto partitions = Subset::complete(point.get_dimension()).get_proper_partitions(point.get_dimension());
    std::sort(partitions.begin(), partitions.end(), [](const std::vector<Subset>& a, const std::vector<Subset>& b) {
        return a.size() < b.size();
    });
    CPlan best_plan;
    for (const auto& partition : partitions)
    {
        std::vector<std::pair<Fraction, CPlan>> subplans;
        for (size_t i = 0; i < partition.size(); ++i)
        {
            auto fold = point.optimal_pack_fold(partition[i]);
            auto plan = optimal_folded_plan(plans, point, partition[i]);
            subplans.emplace_back(fold, plan);
        }
        Fraction total = 0;
        for (const auto& sp : subplans)
            total += sp.first;
        if ( total > 1 )
            continue;
        std::sort(subplans.begin(), subplans.end(), [](const std::pair<Fraction, CPlan>& a, const std::pair<Fraction, CPlan>& b) {
            return a.first > b.first;
        });
        std::vector<Fraction> fcoord(subplans.size());
        for (size_t i = 0; i < subplans.size(); ++i)
            fcoord[i] = subplans[i].first;
        CPlan plan = easy_plan(plans, CPoint(fcoord));
        if ( plan.get_size() > 0 )
        {
            std::map<CPlan::Job, CPlan> inject_map;
            for (size_t i = 0; i < plan.get_dimension(); ++i)
                inject_map[i] = subplans[i].second;
            return plan.inject(inject_map);
        }
    }
    return best_plan;
}


CPlan folded_plan(const std::vector<CPlan>& plans, const CPoint& point, size_t index, size_t count)
{
    if ( index+count > point.get_dimension() or count < 2 )
        throw std::invalid_argument("Invalid fold");
    CPlan best_plan;
    auto coordinates = point.get_coordinates();
    auto value = coordinates[index] / count;
    coordinates[index] = value;
    for ( size_t i = index+count ; i < point.get_dimension() ; i++ )
        coordinates[i-count+1] = coordinates[i];
    coordinates.resize(point.get_dimension()-count+1);
    std::sort(coordinates.begin(), coordinates.end(), std::greater<Fraction>());
    CPoint folded(coordinates);
    auto plan = find_plan(plans, folded);
    if ( plan.get_size() > 0 )
    {
        auto ind = std::find(coordinates.begin(), coordinates.end(), value) - coordinates.begin();
        std::map<CPlan::Job, CPlan> inject_map;
        inject_map[ind] = CPlan::equal_plan(count);
        best_plan = plan.inject(inject_map);
    }
    return best_plan;
}

CPlan find_plan(const std::vector<CPlan>& plans, const CPoint& _point)
{
    CPlan best_plan;
    CPoint point(_point);
    point.make_symmetric_pack();
    if ( point.get_density() > 1 )
        return best_plan;
    if ( point.get_dimension() == 0 )
        return best_plan;
    if ( point.get_dimension() == 1 )
        return CPlan("A");
    best_plan = easy_plan(plans, point);
    if ( best_plan.get_size() > 0 )
        return best_plan;
    best_plan = optimal_folded_plan(plans, point);
    if ( best_plan.get_size() > 0 )
        return best_plan;
    best_plan = easy_optimal_folded_plan(plans, point);
    if ( best_plan.get_size() > 0 )
        return best_plan;
    return best_plan;
}

int main(int argc, char** argv)
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);
    std::cerr.tie(nullptr);

    std::vector<std::string> libraries;
    std::vector<std::string> coordinates;

    bpo::options_description desc("");
    desc.add_options()
        ("help", "produce help message")
        ("library", bpo::value(&libraries)->composing(), "library file")
        ("coordinate", bpo::value(&coordinates)->multitoken()->required(), "coordinates")
    ;

    bpo::positional_options_description pos;
    pos.add("coordinate", -1);

    bpo::variables_map vm;
    bpo::store(bpo::command_line_parser(argc, argv).options(desc).positional(pos).run(), vm);
    bpo::notify(vm);
    if (vm.contains("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    unsigned int dimension = coordinates.size();

    std::vector<Fraction> _point(dimension);
    for (size_t i = 0; i < dimension; ++i)
        _point[i] = string_cast<Fraction>(coordinates[i]);
    Point point(_point);
    point.make_symmetric_pack();

    std::vector<CPlan> plans;
    for ( const auto& library : libraries) {
        std::ifstream input(library);
        if (!input) {
            std::cerr << "Cannot open library file: " << library << "\n";
            return 1;
        }
        std::string line;
        while (std::getline(input, line)) {
            line = string_strip(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            plans.emplace_back(line);
        }
    }
    CPlan best_plan = find_plan(plans, point);
    if (best_plan.get_size() > 0) {
        /*
        if ( not best_plan.simply_satisfies(point) )
            throw std::logic_error("Best plan does not cover the point");
        */
        std::cout << best_plan << "\n";
        return 0;
    }
    std::cerr << "No plan found for the given point " << point << std::endl;
    return 1;
}
