#include "engine_config.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <utility>

namespace engine_editor {
namespace {

bool readParameter(std::istream &in, std::string *name, ParameterValue *value) {
    std::string token;
    char type = 0;
    if (!(in >> token >> std::quoted(*name) >> type) || token != "PARAM") return false;
    switch (type) {
    case 'D': { double v = 0.0; if (!(in >> v)) return false; *value = v; return true; }
    case 'I': { std::int64_t v = 0; if (!(in >> v)) return false; *value = v; return true; }
    case 'B': { int v = 0; if (!(in >> v) || (v != 0 && v != 1)) return false; *value = (v != 0); return true; }
    case 'S': { std::string v; if (!(in >> std::quoted(v))) return false; *value = std::move(v); return true; }
    default: return false;
    }
}

void writeParameter(std::ostream &out, const std::string &name, const ParameterValue &value) {
    out << "PARAM " << std::quoted(name) << ' ';
    if (const auto *v = std::get_if<double>(&value)) {
        out << "D " << std::setprecision(std::numeric_limits<double>::max_digits10) << *v;
    }
    else if (const auto *v = std::get_if<std::int64_t>(&value)) out << "I " << *v;
    else if (const auto *v = std::get_if<bool>(&value)) out << "B " << (*v ? 1 : 0);
    else out << "S " << std::quoted(std::get<std::string>(value));
    out << '\n';
}

bool numericValue(const ParameterValue &value, double *number) {
    if (const auto *v = std::get_if<double>(&value)) { *number = *v; return true; }
    if (const auto *v = std::get_if<std::int64_t>(&value)) { *number = static_cast<double>(*v); return true; }
    return false;
}

bool checkPositive(const ComponentConfig &component, const char *key,
    const std::string &path, std::vector<ValidationIssue> *issues) {
    const auto it = component.parameters.find(key);
    if (it == component.parameters.end()) return true;
    double value = 0.0;
    if (!numericValue(it->second, &value) || !std::isfinite(value) || value <= 0.0) {
        issues->push_back({path + "." + key, "must be a finite number greater than zero"});
        return false;
    }
    return true;
}

bool readRootNumber(const EngineConfig &config, const char *key, double *value,
    std::vector<ValidationIssue> *issues) {
    const auto it = config.parameters.find(key);
    if (it == config.parameters.end()) {
        issues->push_back({std::string("engine.") + key, "required parameter is missing"});
        return false;
    }
    if (!numericValue(it->second, value) || !std::isfinite(*value)) {
        issues->push_back({std::string("engine.") + key, "must be a finite number"});
        return false;
    }
    return true;
}

} // namespace

const char *componentTypeName(ComponentType type) {
    switch (type) {
    case ComponentType::Crankshaft: return "CRANKSHAFT";
    case ComponentType::CylinderBank: return "CYLINDER_BANK";
    case ComponentType::Piston: return "PISTON";
    case ComponentType::ConnectingRod: return "CONNECTING_ROD";
    case ComponentType::CylinderHead: return "CYLINDER_HEAD";
    case ComponentType::Camshaft: return "CAMSHAFT";
    case ComponentType::Fuel: return "FUEL";
    case ComponentType::Intake: return "INTAKE";
    case ComponentType::Exhaust: return "EXHAUST";
    case ComponentType::IgnitionModule: return "IGNITION_MODULE";
    case ComponentType::Throttle: return "THROTTLE";
    case ComponentType::Cylinder: return "CYLINDER";
    }
    return "CYLINDER";
}

bool parseComponentType(const std::string &name, ComponentType *type) {
    if (type == nullptr) return false;
    const ComponentType all[] = {
        ComponentType::Crankshaft, ComponentType::CylinderBank, ComponentType::Piston,
        ComponentType::ConnectingRod, ComponentType::CylinderHead, ComponentType::Camshaft,
        ComponentType::Fuel, ComponentType::Intake, ComponentType::Exhaust,
        ComponentType::IgnitionModule, ComponentType::Throttle, ComponentType::Cylinder
    };
    for (const auto candidate : all) {
        if (name == componentTypeName(candidate)) { *type = candidate; return true; }
    }
    return false;
}

EngineConfig makeDefaultEngineConfig() {
    EngineConfig config;
    config.name = "Custom engine";
    config.parameters = {
        {"starter_torque_nm", 120.0}, {"starter_speed_rpm", 200.0},
        {"redline_rpm", 6500.0}, {"dyno_min_rpm", 1000.0},
        {"dyno_max_rpm", 6500.0}, {"dyno_hold_rpm", 100.0},
        {"simulation_frequency_hz", 20000.0}, {"initial_high_frequency_gain", 0.01},
        {"initial_noise", 1.0}, {"initial_jitter", 0.5}
    };

    ComponentConfig crankshaft;
    crankshaft.id = "crankshaft_1";
    crankshaft.type = ComponentType::Crankshaft;
    crankshaft.name = "Crankshaft 1";
    crankshaft.parameters = {
        {"throw_mm", 45.0}, {"flywheel_mass_kg", 6.0}, {"mass_kg", 12.0},
        {"friction_torque_nm", 5.0}, {"moment_of_inertia_kgm2", 0.16},
        {"position_x_mm", 0.0}, {"position_y_mm", 0.0}, {"tdc_deg", 0.0}
    };

    ComponentConfig bank;
    bank.id = "cylinder_bank_1";
    bank.type = ComponentType::CylinderBank;
    bank.name = "Cylinder bank 1";
    bank.parameters = {
        {"angle_deg", 0.0}, {"bore_mm", 86.0}, {"deck_height_mm", 210.0},
        {"position_x_mm", 0.0}, {"position_y_mm", 0.0}, {"display_depth", 1.0}
    };

    config.components.push_back(std::move(crankshaft));
    config.components.push_back(std::move(bank));
    return config;
}

bool validate(const EngineConfig &config, std::vector<ValidationIssue> *issues) {
    if (issues == nullptr) return false;
    issues->clear();
    if (config.name.empty()) issues->push_back({"engine.name", "must not be empty"});

    for (const auto &parameter : config.parameters) {
        double value = 0.0;
        if (numericValue(parameter.second, &value) && !std::isfinite(value))
            issues->push_back({"engine." + parameter.first, "must be finite"});
    }

    double redline = 0.0, dynoMin = 0.0, dynoMax = 0.0;
    const bool hasRedline = readRootNumber(config, "redline_rpm", &redline, issues);
    const bool hasMin = readRootNumber(config, "dyno_min_rpm", &dynoMin, issues);
    const bool hasMax = readRootNumber(config, "dyno_max_rpm", &dynoMax, issues);
    if (hasRedline && redline <= 0.0)
        issues->push_back({"engine.redline_rpm", "must be greater than zero"});
    if (hasMin && dynoMin < 0.0)
        issues->push_back({"engine.dyno_min_rpm", "must not be negative"});
    if (hasMax && dynoMax <= 0.0)
        issues->push_back({"engine.dyno_max_rpm", "must be greater than zero"});
    if (hasMin && hasMax && dynoMin >= dynoMax)
        issues->push_back({"engine.dyno_max_rpm", "must be greater than dyno_min_rpm"});

    const auto checkRange = [&](const char *key, double minimum, double maximum) {
        double value = 0.0;
        if (readRootNumber(config, key, &value, issues)
            && (value < minimum || value > maximum)) {
            issues->push_back({std::string("engine.") + key, "value is outside the supported range"});
        }
    };
    checkRange("starter_torque_nm", 0.0, 1200.0);
    checkRange("starter_speed_rpm", 50.0, 1500.0);
    checkRange("dyno_hold_rpm", 10.0, 1000.0);
    checkRange("simulation_frequency_hz", 400.0, 400000.0);
    checkRange("initial_high_frequency_gain", 0.0, 1.0);
    checkRange("initial_noise", 0.0, 4.0);
    checkRange("initial_jitter", 0.0, 2.0);

    std::set<std::string> ids;
    std::size_t crankshafts = 0, banks = 0;
    for (const auto &component : config.components) {
        const std::string path = "components[" + component.id + "]";
        if (component.id.empty()) issues->push_back({"components", "component id must not be empty"});
        else if (!ids.insert(component.id).second)
            issues->push_back({path, "component id must be unique"});
        if (component.name.empty()) issues->push_back({path + ".name", "must not be empty"});

        switch (component.type) {
        case ComponentType::Crankshaft:
            ++crankshafts;
            checkPositive(component, "throw_mm", path, issues);
            checkPositive(component, "mass_kg", path, issues);
            checkPositive(component, "moment_of_inertia_kgm2", path, issues);
            break;
        case ComponentType::CylinderBank:
            ++banks;
            checkPositive(component, "bore_mm", path, issues);
            break;
        case ComponentType::Piston:
            checkPositive(component, "mass_kg", path, issues);
            break;
        case ComponentType::ConnectingRod:
            checkPositive(component, "length_mm", path, issues);
            checkPositive(component, "mass_kg", path, issues);
            break;
        default: break;
        }

        for (const auto &parameter : component.parameters) {
            double value = 0.0;
            if (numericValue(parameter.second, &value) && !std::isfinite(value))
                issues->push_back({path + "." + parameter.first, "must be finite"});
        }
    }
    if (crankshafts == 0) issues->push_back({"components", "at least one crankshaft is required"});
    if (banks == 0) issues->push_back({"components", "at least one cylinder bank is required"});

    for (const auto &component : config.components) {
        for (const auto &connection : component.connections) {
            if (ids.find(connection) == ids.end())
                issues->push_back({"components[" + component.id + "].connections",
                    "unknown component id: " + connection});
        }
    }
    return issues->empty();
}

bool saveConfig(const EngineConfig &config, const std::string &filePath, std::string *error) {
    std::vector<ValidationIssue> issues;
    if (!validate(config, &issues)) {
        if (error) *error = "Configuration is invalid: " + issues.front().path + ": " + issues.front().message;
        return false;
    }
    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out) {
        if (error) *error = "Could not open file for writing: " + filePath;
        return false;
    }
    out << "ENGINE_EDITOR_CONFIG 1\n";
    out << "ENGINE " << std::quoted(config.name) << '\n';
    out << "ENGINE_PARAMETERS " << config.parameters.size() << '\n';
    for (const auto &p : config.parameters) writeParameter(out, p.first, p.second);
    out << "COMPONENTS " << config.components.size() << '\n';
    for (const auto &component : config.components) {
        out << "COMPONENT " << componentTypeName(component.type) << ' '
            << std::quoted(component.id) << ' ' << std::quoted(component.name) << '\n';
        out << "PARAMETERS " << component.parameters.size() << '\n';
        for (const auto &p : component.parameters) writeParameter(out, p.first, p.second);
        out << "CONNECTIONS " << component.connections.size() << '\n';
        for (const auto &connection : component.connections)
            out << "CONNECTION " << std::quoted(connection) << '\n';
        out << "END_COMPONENT\n";
    }
    out << "END\n";
    if (!out.good()) {
        if (error) *error = "Failed while writing configuration: " + filePath;
        return false;
    }
    return true;
}

bool loadConfig(const std::string &filePath, EngineConfig *config, std::string *error) {
    if (config == nullptr) {
        if (error) *error = "Output configuration pointer is null";
        return false;
    }
    std::ifstream in(filePath);
    if (!in) {
        if (error) *error = "Could not open configuration file: " + filePath;
        return false;
    }

    EngineConfig loaded;
    std::string token;
    int version = 0;
    if (!(in >> token >> version) || token != "ENGINE_EDITOR_CONFIG" || version != 1) {
        if (error) *error = "Unsupported or invalid configuration header";
        return false;
    }
    if (!(in >> token >> std::quoted(loaded.name)) || token != "ENGINE") {
        if (error) *error = "Missing ENGINE record";
        return false;
    }

    std::size_t count = 0;
    if (!(in >> token >> count) || token != "ENGINE_PARAMETERS") {
        if (error) *error = "Missing ENGINE_PARAMETERS record";
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        std::string name;
        ParameterValue value;
        if (!readParameter(in, &name, &value) || !loaded.parameters.emplace(name, std::move(value)).second) {
            if (error) *error = "Invalid or duplicate engine parameter";
            return false;
        }
    }

    if (!(in >> token >> count) || token != "COMPONENTS") {
        if (error) *error = "Missing COMPONENTS record";
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        ComponentConfig component;
        std::string typeName;
        if (!(in >> token >> typeName >> std::quoted(component.id) >> std::quoted(component.name))
            || token != "COMPONENT" || !parseComponentType(typeName, &component.type)) {
            if (error) *error = "Invalid COMPONENT record";
            return false;
        }
        std::size_t parameterCount = 0;
        if (!(in >> token >> parameterCount) || token != "PARAMETERS") {
            if (error) *error = "Missing PARAMETERS record";
            return false;
        }
        for (std::size_t j = 0; j < parameterCount; ++j) {
            std::string name;
            ParameterValue value;
            if (!readParameter(in, &name, &value) || !component.parameters.emplace(name, std::move(value)).second) {
                if (error) *error = "Invalid or duplicate component parameter";
                return false;
            }
        }
        if (!(in >> token >> parameterCount) || token != "CONNECTIONS") {
            if (error) *error = "Missing CONNECTIONS record";
            return false;
        }
        for (std::size_t j = 0; j < parameterCount; ++j) {
            std::string connection;
            if (!(in >> token >> std::quoted(connection)) || token != "CONNECTION") {
                if (error) *error = "Invalid CONNECTION record";
                return false;
            }
            component.connections.push_back(std::move(connection));
        }
        if (!(in >> token) || token != "END_COMPONENT") {
            if (error) *error = "Missing END_COMPONENT record";
            return false;
        }
        loaded.components.push_back(std::move(component));
    }
    if (!(in >> token) || token != "END") {
        if (error) *error = "Missing END record";
        return false;
    }

    std::vector<ValidationIssue> issues;
    if (!validate(loaded, &issues)) {
        if (error) *error = "Loaded configuration is invalid: " + issues.front().path + ": " + issues.front().message;
        return false;
    }
    *config = std::move(loaded);
    return true;
}

} // namespace engine_editor
