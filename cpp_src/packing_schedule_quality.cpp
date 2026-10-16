#include <boost/program_options.hpp>

#include "lib/schedule.hpp"
#include "lib/plan_pack.hpp"

namespace bpo = boost::program_options;

typedef PlanPack<unsigned> CPlanPack;

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);
    std::cerr.tie(nullptr);

    std::vector<std::string> schedules;

    bpo::options_description desc("");
    desc.add_options()
        ("help", "produce help message")
        ("schedule", bpo::value(&schedules)->multitoken()->required(), "schedules")
    ;

    bpo::positional_options_description pos;
    pos.add("schedule", -1);

    bpo::variables_map vm;
    bpo::store(bpo::command_line_parser(argc, argv).options(desc).positional(pos).run(), vm);
    bpo::notify(vm);
    if (vm.contains("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    if ( schedules.size() == 1 and schedules[0] == "-" ) {
        for ( std::string line ; std::getline(std::cin, line) ; ) {
            CPlanPack plan(line);
            std::cout << plan << std::endl;
        }
    } else {
        for ( const auto& schedule : schedules ) {
            CPlanPack plan(schedule);
            std::cout << plan << std::endl;
        }
    }
    return 0;
}
