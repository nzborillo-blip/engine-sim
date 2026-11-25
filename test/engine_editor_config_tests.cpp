#include "engine_config.h"

#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace engine_editor;

TEST(EngineEditorConfig, DefaultConfigurationIsValid) {
    EngineConfig config = makeDefaultEngineConfig();
    std::vector<ValidationIssue> issues;
    EXPECT_TRUE(validate(config, &issues));
    EXPECT_TRUE(issues.empty());
}

TEST(EngineEditorConfig, RejectsDuplicateComponentIds) {
    EngineConfig config = makeDefaultEngineConfig();
    config.components.push_back(config.components.front());
    std::vector<ValidationIssue> issues;
    EXPECT_FALSE(validate(config, &issues));
    ASSERT_FALSE(issues.empty());
    EXPECT_EQ(issues.front().message, "component id must be unique");
}

TEST(EngineEditorConfig, RejectsInvalidDynoRange) {
    EngineConfig config = makeDefaultEngineConfig();
    config.parameters["dyno_min_rpm"] = 7000.0;
    config.parameters["dyno_max_rpm"] = 6500.0;
    std::vector<ValidationIssue> issues;
    EXPECT_FALSE(validate(config, &issues));
}

TEST(EngineEditorConfig, SavesAndLoadsConfiguration) {
    const auto path = std::filesystem::temp_directory_path() / "engine_editor_config_test.esconfig";
    const std::string filePath = path.string();
    const EngineConfig original = makeDefaultEngineConfig();
    std::string error;
    ASSERT_TRUE(saveConfig(original, filePath, &error)) << error;
    EngineConfig loaded;
    ASSERT_TRUE(loadConfig(filePath, &loaded, &error)) << error;
    EXPECT_EQ(loaded.name, original.name);
    EXPECT_EQ(loaded.parameters.size(), original.parameters.size());
    EXPECT_EQ(loaded.components.size(), original.components.size());
    EXPECT_EQ(loaded.components.front().id, original.components.front().id);
    std::remove(filePath.c_str());
}

TEST(EngineEditorConfig, RejectsTrailingDataAfterEndRecord) {
    const auto path = std::filesystem::temp_directory_path() / "engine_editor_config_trailing.esconfig";
    EngineConfig original = makeDefaultEngineConfig();
    std::string error;
    ASSERT_TRUE(saveConfig(original, path.string(), &error)) << error;
    {
        std::ofstream out(path, std::ios::app);
        out << "UNEXPECTED_RECORD\n";
    }

    EngineConfig loaded = makeDefaultEngineConfig();
    loaded.name = "Keep this";
    EXPECT_FALSE(loadConfig(path.string(), &loaded, &error));
    EXPECT_NE(error.find("Unexpected data after END record"), std::string::npos);
    EXPECT_EQ(loaded.name, "Keep this");
    std::remove(path.string().c_str());
}

TEST(EngineEditorConfig, LoadFailureDoesNotReplaceExistingConfig) {
    EngineConfig loaded = makeDefaultEngineConfig();
    loaded.name = "Keep this";
    const auto path = std::filesystem::temp_directory_path() / "engine_editor_config_invalid.esconfig";
    {
        std::ofstream out(path);
        out << "not a config";
    }
    std::string error;
    EXPECT_FALSE(loadConfig(path.string(), &loaded, &error));
    EXPECT_EQ(loaded.name, "Keep this");
    std::remove(path.string().c_str());
}
