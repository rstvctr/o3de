// dear imgui: wrappers for O3DE standard library (AZStd) types (AZStd::string, etc.)

#pragma once

#include <AZCore/std/string/string.h>

namespace ImGui
{
    // ImGui::InputText() with AZStd::string
    // Because text input needs dynamic resizing, we need to setup a callback to grow the capacity
    bool InputText(const char* label, AZStd::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    bool InputTextMultiline(const char* label, AZStd::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    bool InputTextWithHint(const char* label, const char* hint, AZStd::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
}
