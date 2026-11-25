#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <functional>
#include <string>
#include <vector>
#include "engine_config.h"

namespace engine_editor {
class EngineEditorWindow {
public:
    using ApplyHandler = std::function<bool(const EngineConfig &, std::string &)>;
    using CurrentConfigHandler = std::function<EngineConfig()>;
    EngineEditorWindow() = default;
    ~EngineEditorWindow();
    bool initialize(HINSTANCE instance, const EngineConfig &initialConfig,
        ApplyHandler applyHandler, CurrentConfigHandler currentConfigHandler);
    void destroy();
    void pumpMessages();
    bool isOpen() const { return m_window != nullptr; }
private:
    static constexpr int ParameterCount = 10;
    static constexpr int SliderFirstId = 3000;
    static constexpr int EditFirstId = 3100;
    static constexpr int ApplyButtonId = 3200;
    static constexpr int ReloadButtonId = 3201;
    static constexpr int LoadButtonId = 3202;
    static constexpr int SaveButtonId = 3203;
    static LRESULT CALLBACK windowProcedure(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(UINT, WPARAM, LPARAM);
    void createControls();
    void updateControls();
    void updateParameterFromSlider(int index);
    void updateEditFromParameter(int index);
    void applyDraft();
    void reloadDraft();
    void loadConfigFile();
    void saveConfigFile();
    void setStatus(const std::wstring &message);
    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
    HWND m_status = nullptr;
    std::vector<HWND> m_sliders;
    std::vector<HWND> m_edits;
    EngineConfig m_draft;
    ApplyHandler m_applyHandler;
    CurrentConfigHandler m_currentConfigHandler;
};
} // namespace engine_editor
