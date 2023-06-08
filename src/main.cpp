#include <xviewmap.hpp>
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <iostream>

int main(int argc, char const* argv[])
{
    XViewMap::ViewMap viewmap{};

    if (argc >= 2) {
        viewmap.readToml(argv[1]);
    } else {
        viewmap.readToml();
    }

    while (!std::cin.eof()) {
        std::string inl;
        std::getline(std::cin, inl);
        std::vector<std::string> in_data;
        std::istringstream ins(inl);
        while (!ins.eof()) {
            std::string ds;
            std::getline(ins, ds, ' ');
            if (!ds.empty()) {
                in_data.push_back(ds);
            }
        }
        if (in_data.size() >= 8 && in_data[1] == "[FieldMap]") {
            try {
                double x = std::stod(in_data[2]);
                double y = std::stod(in_data[3]);
                double th = std::stod(in_data[4]);
                double vx = std::stod(in_data[5]);
                double vy = std::stod(in_data[6]);
                double vth = std::stod(in_data[7]);
                viewmap.updatePos(x, y, th, vx, vy, vth);
            } catch (const std::invalid_argument&) {
            }
        } else if (in_data.size() >= 5 && in_data[1] == "[LocusMap]") {
            try {
                double x = std::stod(in_data[2]);
                double y = std::stod(in_data[3]);
                double th = std::stod(in_data[4]);
                viewmap.updateLocus(x, y, th);
            } catch (const std::invalid_argument&) {
            }
        } else {
            std::cout << inl << std::endl;
        }
    }

    return 0;
}
