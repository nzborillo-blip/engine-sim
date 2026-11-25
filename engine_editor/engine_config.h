#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace engine_editor {

enum class ComponentType {
    Crankshaft, CylinderBank, Piston, ConnectingRod, CylinderHead,
    Camshaft, Fuel, Intake, Exhaust, IgnitionModule, Throttle, Cylinder
};

using ParameterValue = std::variant<double, std::int64_t, bool, std::string>;

struct ComponentConfig {
    std::string id;
    ComponentType type = ComponentType::Cylinder;
    std::string name;
    std::map<std::string, ParameterValue> parameters;
    std::vector<std::string> connections;
};

struct EngineConfig {
    std::string name = "Custom engine";
    std::map<std::string, ParameterValue> parameters;
    std::vector<ComponentConfig> components;
};

struct ValidationIssue {
    std::string path;
    std::string message;
};

EngineConfig makeDefaultEngineConfig();
const char *componentTypeName(ComponentType type);
bool parseComponentType(const std::string &name, ComponentType *type);
bool validate(const EngineConfig &config, std::vector<ValidationIssue> *issues);
bool saveConfig(const EngineConfig &config, const std::string &filePath, std::string *error = nullptr);
bool loadConfig(const std::string &filePath, EngineConfig *config, std::string *error = nullptr);

} // namespace engine_editor
