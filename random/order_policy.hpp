#pragma once
#include <algorithm>
#include <array>
#include <random>
#include <string>

using LanguageOrder = std::array<std::string, 3>;

inline LanguageOrder randomOrder(std::mt19937& rng) {
    LanguageOrder order{"en", "ru", "zh"};
    std::shuffle(order.begin(), order.end(), rng);
    return order;
}

/* Future offline IP lookup integration (disabled; no IP database is loaded).
struct IpLocation {
    bool known;
    std::string countryCode;   // ISO country code; mainland China = CN.
    std::string provinceCode;  // Normalized code: Jiangsu = 32, Jiangxi = 36.
};

inline LanguageOrder orderForIp(const IpLocation& location, std::mt19937& rng) {
    if (!location.known) return randomOrder(rng);
    if (location.countryCode == "CN") {
        if (location.provinceCode == "32" || location.provinceCode == "36")
            return {"ru", "zh", "en"};
        LanguageOrder order{"zh", "en", "ru"};
        std::shuffle(order.begin() + 1, order.end(), rng);
        return order;
    }
    LanguageOrder order{"en", "zh", "ru"};
    std::shuffle(order.begin() + 1, order.end(), rng);
    return order;
}

// At the CGI call site, after a trusted client-IP lookup is implemented:
// const auto order = orderForIp(lookupClientIp(), rng);
*/
