#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "home-svg.h"

// SVG keeps the geometry. Generated styles carry the playback schedule.
class TerminalCss {
    struct Style {
        std::string base;
        std::vector<std::string> animations;
    };
    std::ostream &output;
    std::unordered_map<xmlNodePtr, Style> styles;
    std::vector<xmlNodePtr> nodes;
    unsigned track = 0;
    unsigned rule = 0;

public:
    explicit TerminalCss(std::ostream &stream) : output(stream) {}

    std::string keyframes(const std::string &frames)
    {
        const std::string name = "terminal-track-" + std::to_string(++track);
        output << "@keyframes " << name << '{' << frames << "}\n";
        return name;
    }

    void animate(xmlNodePtr node, const std::string &base, const std::string &animation)
    {
        auto entry = styles.try_emplace(node);
        if (entry.second)
            nodes.push_back(node);
        entry.first->second.base += base;
        entry.first->second.animations.push_back(animation);
    }

    void write_rules()
    {
        for (const auto node : nodes) {
            const std::string name = "terminal-motion-" + std::to_string(++rule);
            prop(node, "class", name.c_str());
            const auto &style = styles.at(node);
            output << '.' << name << '{' << style.base << "animation:";
            const char *separator = "";
            for (const auto &animation : style.animations) {
                output << separator << animation;
                separator = ",";
            }
            output << "}\n";
        }
        styles.clear();
        nodes.clear();
    }
};
