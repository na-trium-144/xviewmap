#include <xviewmap.hpp>
#include <filesystem>
#include <iostream>
#include "../lib/toml.hpp"

namespace XViewMap
{

void applyTomlConfig(toml::table& config, XViewMap::ViewMap& visualizer)
{
    if (auto range = config["field"]["range"]) {
        auto xmin = range[0][0].value<double>();
        auto xmax = range[0][1].value<double>();
        auto ymin = range[1][0].value<double>();
        auto ymax = range[1][1].value<double>();
        if (xmin && xmax && ymin && ymax) {
            visualizer.setField(*xmin, *ymin, *xmax, *ymax);
        } else {
            std::cerr << "[XViewMap] invalid data in field.range" << std::endl;
        }
    }
    if (auto draw = config["field"]["draw"]) {
        for (std::size_t i = 0; i < draw.as_array()->size(); i++) {
            auto d = draw[i];
            auto type = d[0].value<std::string>();
            if (*type == "line") {
                auto x1 = d[1].value<double>();
                auto y1 = d[2].value<double>();
                auto x2 = d[3].value<double>();
                auto y2 = d[4].value<double>();
                if (x1 && y1 && x2 && y2) {
                    visualizer.drawFieldLine(*x1, *y1, *x2, *y2);
                } else {
                    std::cerr << "[XViewMap] invalid data in field.draw" << std::endl;
                }
            } else if (*type == "arc") {
                auto x1 = d[1].value<double>();
                auto y1 = d[2].value<double>();
                auto r = d[3].value<double>();
                auto a1 = d[4].value<double>();
                auto a2 = d[5].value<double>();
                if (x1 && y1 && r && a1 && a2) {
                    visualizer.drawFieldArc(*x1, *y1, *r, *a1, *a2);
                } else if (x1 && y1 && r) {
                    visualizer.drawFieldArc(*x1, *y1, *r);
                } else {
                    std::cerr << "[XViewMap] invalid data in field.draw" << std::endl;
                }
            } else {
                std::cerr << "[XViewMap] invalid data in field.draw" << std::endl;
            }
        }
    }
    if (auto machine = config["robot"]["machine"]) {
        visualizer.machine.clear();
        for (std::size_t i = 0; i < machine.as_array()->size(); i++) {
            auto v = machine[i];
            auto x = v[0].value<double>();
            auto y = v[1].value<double>();
            if (x && y) {
                visualizer.machine.push_back({*x, *y, 0});
            } else {
                std::cerr << "[XViewMap] invalid data in robot.machine" << std::endl;
            }
        }
    }
    if (auto wheel = config["robot"]["wheel"]) {
        visualizer.wheels.clear();
        for (std::size_t i = 0; i < wheel.as_array()->size(); i++) {
            auto v = wheel[i];
            auto x = v[0].value<double>();
            auto y = v[1].value<double>();
            auto th = v[2].value<double>();
            if (x && y && th) {
                visualizer.wheels.push_back({*x, *y, *th * 3.14 / 180});
            } else {
                std::cerr << "[XViewMap] invalid data in robot.wheel" << std::endl;
            }
        }
    }
    if (auto wheel_radius = config["robot"]["wheel_radius"]) {
        auto wheel_radius_v = wheel_radius.value<double>();
        if (wheel_radius_v) {
            visualizer.wheel_radius = *wheel_radius_v;
        } else {
            std::cerr << "[XViewMap] invalid data in robot.wheel_radius" << std::endl;
        }
    }
}

void ViewMap::readToml(const std::string& path)
{
    try {
        toml::table config = toml::parse_file(path);
        applyTomlConfig(config, *this);
    } catch (const toml::parse_error& err) {
        std::cerr << "[XViewMap] Parsing " << path << " failed:\n" << err << std::endl;
    }
}
void ViewMap::readToml()
{
    const std::string config_name = "xviewmap.toml";
    std::string path;
    if (std::filesystem::exists("./" + config_name)) {
        path = "./" + config_name;
    } else if (std::filesystem::exists("../" + config_name)) {
        path = "../" + config_name;
    } else {
        std::cerr << "[XViewMap] " << config_name << " not found." << std::endl;
        return;
    }
    try {
        toml::table config = toml::parse_file(path);
        applyTomlConfig(config, *this);
    } catch (const toml::parse_error& err) {
        std::cerr << "[XViewMap] Parsing " << path << " failed:\n" << err << std::endl;
    }
}
}  // namespace XViewMap