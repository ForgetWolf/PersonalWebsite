#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "home-svg.h"
#include "home-terminal.h"
#include "mobile-layout.h"

using Document = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>;

struct Timing {
    double begin;
    double duration;
};

struct Greeting {
    const char *language;
    Document document;
    double first;
    double length;
    double fadeIn;
    double fadeOut;
};

static std::string attribute(xmlNodePtr node, const char *name)
{
    xmlChar *value = xmlGetProp(node, BAD_CAST name);
    std::string result = value ? reinterpret_cast<const char *>(value) : "";
    xmlFree(value);
    return result;
}

static double seconds(xmlNodePtr node, const char *name)
{
    const auto value = attribute(node, name);
    char *end;
    const double time = strtod(value.c_str(), &end);
    if (end == value.c_str() || strcmp(end, "s") != 0 || !std::isfinite(time) || time < 0)
        fail("Invalid source animation time", value.c_str());
    return time;
}

static std::string number(double value)
{
    char formatted[64];
    decimal(formatted, value);
    return formatted;
}

static Timing timing(xmlNodePtr animation)
{
    return {seconds(animation, "begin"), named(animation, "set") ? 0 : seconds(animation, "dur")};
}

static Greeting load_greeting(const char *language, const char *path)
{
    std::unique_ptr<char, decltype(&free)> source(read_text(path), free);
    const std::string contents(source.get());
    const auto start = contents.find("<svg");
    const auto end = contents.rfind("</svg>");
    if (start == std::string::npos || end == std::string::npos || end < start)
        fail("Missing source SVG", path);
    const auto svg = contents.substr(start, end + 6 - start);
    Document document(xmlReadMemory(svg.data(), static_cast<int>(svg.size()), path, nullptr,
                                  XML_PARSE_NONET), xmlFreeDoc);
    if (!document)
        fail("Cannot parse source SVG", path);

    const auto ink = child(child(xmlDocGetRootElement(document.get()), "svg"), "g");
    const auto fadeIn = child(ink, "animate");
    const auto fadeOut = xmlNextElementSibling(fadeIn);
    if (!named(fadeOut, "animate") || attribute(fadeIn, "attributeName") != "opacity" ||
        attribute(fadeOut, "attributeName") != "opacity" ||
        attribute(fadeIn, "from") != "0" || attribute(fadeIn, "to") != "1" ||
        attribute(fadeOut, "from") != "1" || attribute(fadeOut, "to") != "0")
        fail("Missing language fades", path);
    const auto in = timing(fadeIn);
    const auto out = timing(fadeOut);
    for (const auto fade : {fadeIn, fadeOut}) {
        xmlUnlinkNode(fade);
        xmlFreeNode(fade);
    }
    xmlUnsetProp(ink, BAD_CAST "opacity");
    return {language, std::move(document), in.begin, out.begin + out.duration - in.begin,
            in.duration, out.duration};
}

static void convert_timeline(xmlNodePtr node, const Greeting &greeting)
{
    std::vector<xmlNodePtr> events;
    for (auto item = node->children; item; item = item->next) {
        if (named(item, "set") || named(item, "animate")) {
            events.push_back(item);
        } else if (item->type == XML_ELEMENT_NODE) {
            convert_timeline(item, greeting);
        }
    }
    if (events.empty())
        return;

    const auto time = timing(events.front());
    if (time.begin < greeting.first - 1e-9 ||
        time.begin + time.duration > greeting.first + greeting.length + 1e-9)
        fail("Ink animation outside language slot", greeting.language);
    double duration = time.duration;
    std::string cssClass;
    bool growsUp = false;
    for (const auto event : events) {
        const auto current = timing(event);
        const auto property = attribute(event, "attributeName");
        const auto initial = attribute(event, "from");
        const auto final = attribute(event, "to");
        if (std::abs(current.begin - time.begin) > 1e-9 ||
            (current.duration > 0 && duration > 0 && std::abs(current.duration - duration) > 1e-9))
            fail("Mismatched animations on one element", greeting.language);
        duration = std::max(duration, current.duration);

        if (named(event, "set") && property == "visibility" && final == "visible") {
            xmlUnsetProp(node, BAD_CAST "visibility");
            if (cssClass.empty())
                cssClass = "reveal";
        } else if (named(event, "animate") && property == "stroke-dashoffset" &&
                   initial == "1" && final == "0" && attribute(event, "calcMode") == "linear") {
            prop(node, "stroke-dashoffset", "0");
            cssClass = "ink";
        } else if (named(event, "animate") && named(node, "rect") &&
                   ((initial == "0" && (property == "width" || property == "height")) ||
                    property == "y")) {
            prop(node, property.c_str(), final.c_str());
            if (property == "y")
                growsUp = true;
            else
                cssClass = "mask-" + property;
        } else {
            fail("Unsupported source animation", property.c_str());
        }
        xmlUnlinkNode(event);
        xmlFreeNode(event);
    }

    if (growsUp) {
        if (cssClass != "mask-height")
            fail("Rising mask lacks matching height animation", greeting.language);
        cssClass = "mask-rise";
    }
    const auto start = std::max(0.0, time.begin - greeting.first);
    auto style = "--start:" + number(start) + "s";
    if (duration > 0)
        style += ";--duration:" + number(duration) + "s";
    prop(node, "class", cssClass.c_str());
    prop(node, "style", style.c_str());
}

static void prefix_ids(xmlNodePtr node, const std::string &prefix)
{
    if (node->type != XML_ELEMENT_NODE)
        return;
    const auto id = attribute(node, "id");
    if (!id.empty())
        prop(node, "id", (prefix + id).c_str());
    for (const auto key : {"mask", "clip-path", "filter", "href"}) {
        auto value = attribute(node, key);
        if (value.rfind("url(#", 0) == 0) {
            value.insert(5, prefix);
            prop(node, key, value.c_str());
        } else if (strcmp(key, "href") == 0 && value.rfind('#', 0) == 0) {
            value.insert(1, prefix);
            prop(node, key, value.c_str());
        }
    }
    for (auto item = node->children; item; item = item->next)
        prefix_ids(item, prefix);
}

static void write_svg(std::ostream &html, xmlNodePtr node)
{
    auto buffer = new_buffer();
    if (xmlNodeDump(buffer, node->doc, node, 0, 0) < 0)
        fail("Cannot serialize SVG", "welcome");
    html << xmlBufferContent(buffer);
    xmlBufferFree(buffer);
}

static void write_terminal(std::ostream &html)
{
    std::ofstream styles("dist/terminal-schedule.css", std::ios::binary);
    TerminalCss css(styles);
    std::unique_ptr<TerminalTrace, decltype(&terminal_free)> trace(terminal_execute(), terminal_free);
    for (const bool mobile : {false, true}) {
        const char *layout = mobile ? "narrow" : "wide";
        const auto path = std::string("builder/templates/home-") + layout + ".svg";
        Document document(read_xml(path.c_str()), xmlFreeDoc);
        const auto svg = xmlDocGetRootElement(document.get());
        prop(svg, "class", (std::string("terminal terminal--") + layout).c_str());
        terminal_render(svg, trace.get(), mobile, css);
        css.write_rules();
        prefix_ids(svg, std::string(layout) + '-');
        write_svg(html, svg);
    }
    styles.close();
    if (!styles)
        fail("Cannot write stylesheet", "dist/terminal-schedule.css");

    for (const auto path : {"builder/templates/menu-wide.html", "builder/templates/menu-narrow.html"}) {
        std::unique_ptr<char, decltype(&free)> menu(read_text(path), free);
        html << menu.get();
    }
}

int main()
{
    const Greeting greetings[] = {
        load_greeting("en", "geometry/greeting.hpp"),
        load_greeting("ru", "geometry/russian.hpp"),
        load_greeting("zh", "geometry/chinese.hpp"),
    };
    const auto &base = greetings[0];
    for (const auto &greeting : greetings) {
        if (std::abs(greeting.length - base.length) > 1e-9 ||
            std::abs(greeting.fadeIn - base.fadeIn) > 1e-9 ||
            std::abs(greeting.fadeOut - base.fadeOut) > 1e-9)
            fail("Language slot lengths or fades differ", greeting.language);
        convert_timeline(xmlDocGetRootElement(greeting.document.get()), greeting);
    }

    std::filesystem::create_directories("dist");
    std::ofstream schedule("dist/schedule.css", std::ios::binary);
    schedule << ".welcome{--slot-length:" << number(base.length) << "s;}\n"
             << "@keyframes greeting{0%{opacity:0}" << number(100 * base.fadeIn / base.length)
             << "%," << number(100 * (1 - base.fadeOut / base.length))
             << "%{opacity:1}100%{opacity:0}}\n";

    const int orders[][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (int language = 0; language < 3; ++language) {
        schedule << ".language." << greetings[language].language << "{--offset:calc(0s";
        for (int order = 0; order < 6; ++order) {
            const int slot = static_cast<int>(std::find(orders[order], orders[order] + 3, language) -
                                              orders[order]);
            if (slot == 0)
                continue;
            // At an integer sample, only its matching weight can be 1.
            schedule << " + " << number(slot * base.length)
                     << "s * clamp(0, 1 - max(var(--order) - " << order << ", " << order
                     << " - var(--order)), 1)";
        }
        schedule << ")}\n";
    }
    schedule.close();
    if (!schedule)
        fail("Cannot write stylesheet", "dist/schedule.css");

    std::ofstream html("dist/index.html", std::ios::binary);
    html << R"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>ForgetWolf</title>
<link rel="icon" href="favicon.ico">
<link rel="stylesheet" href="welcome.css">
<link rel="stylesheet" href="schedule.css">
<link rel="stylesheet" href="terminal.css">
<link rel="stylesheet" href="terminal-schedule.css">
<link rel="stylesheet" href="menu.css">
</head>
<body class="home">
<svg class="home-scene" xmlns="http://www.w3.org/2000/svg" width="100%" height="100%">
)";
    // Both layers switch at the same time. Showing the landing starts its CSS animations.
    const auto handoff = number(3 * base.length) + "s;welcomeHit.click;welcomeKeyboard.click";
    html << "<set href=\"#welcome-screen\" attributeName=\"display\" to=\"none\" begin=\""
         << handoff << "\" fill=\"freeze\" restart=\"never\"/>\n"
            "<set href=\"#landing-screen\" attributeName=\"display\" to=\"inline\" begin=\""
         << handoff << "\" fill=\"freeze\" restart=\"never\"/>\n"
         << R"(
<g id="welcome-screen">
<foreignObject width="100%" height="100%">
<div xmlns="http://www.w3.org/1999/xhtml" class="welcome">
<button id="welcomeKeyboard" type="button" aria-label="Skip welcome animation"></button>
)";
    for (const auto &greeting : greetings) {
        const auto svg = xmlDocGetRootElement(greeting.document.get());
        const std::string classes = std::string("language ") + greeting.language;
        const auto portrait = xmlDocCopyNode(svg, svg->doc, 1);
        if (!portrait)
            fail("Cannot copy SVG", greeting.language);
        prop(svg, "class", (classes + " wide").c_str());
        prefix_ids(svg, "wide-");
        write_svg(html, svg);
        prop(portrait, "class", (classes + " portrait").c_str());
        mobile_layout(portrait, greeting.language);
        prefix_ids(portrait, "portrait-");
        write_svg(html, portrait);
        xmlFreeNode(portrait);
        html << '\n';
    }
    html << "</div>\n</foreignObject>\n"
            "<rect id=\"welcomeHit\" width=\"100%\" height=\"100%\" fill=\"transparent\""
            " pointer-events=\"all\" aria-hidden=\"true\"/>\n</g>\n"
            "<foreignObject id=\"landing-screen\" width=\"100%\" height=\"100%\" display=\"none\">\n"
            "<main xmlns=\"http://www.w3.org/1999/xhtml\" id=\"main\"><h1 aria-label=\"ForgetWolf\">";
    Document heading(read_xml("logo/wordmark.svg"), xmlFreeDoc);
    const auto wordmark = xmlDocGetRootElement(heading.get());
    prop(wordmark, "class", "wordmark");
    prop(wordmark, "aria-hidden", "true");
    write_svg(html, wordmark);
    html << "</h1>";
    write_terminal(html);
    html << "</main>\n</foreignObject>\n</svg>\n</body>\n</html>\n";
    html.close();
    if (!html)
        fail("Cannot write page", "dist/index.html");
}
