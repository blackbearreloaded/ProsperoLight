/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <RmlUi/Core/FontEngineInterface.h>

// RmlUi font interface backed by build-time bitmap atlases. This keeps RmlUi
// responsible for HTML/RCSS layout while making glyph rasterization fully
// deterministic across desktop and PS5 builds.
class BitmapFontEngine final : public Rml::FontEngineInterface
{
  public:
    void Initialize() override;
    void Shutdown() override;

    bool LoadFontFace(const Rml::String &file_name, int face_index, bool fallback_face,
                      Rml::Style::FontWeight weight) override;
    bool LoadFontFace(Rml::Span<const Rml::byte> data, int face_index, const Rml::String &family,
                      Rml::Style::FontStyle style, Rml::Style::FontWeight weight,
                      bool fallback_face) override;

    Rml::FontFaceHandle GetFontFaceHandle(const Rml::String &family, Rml::Style::FontStyle style,
                                          Rml::Style::FontWeight weight, int size) override;
    Rml::FontEffectsHandle PrepareFontEffects(Rml::FontFaceHandle handle,
                                              const Rml::FontEffectList &font_effects) override;
    const Rml::FontMetrics &GetFontMetrics(Rml::FontFaceHandle handle) override;
    int GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
                       const Rml::TextShapingContext &text_shaping_context,
                       Rml::Character prior_character = Rml::Character::Null) override;
    int GenerateString(Rml::RenderManager &render_manager, Rml::FontFaceHandle face_handle,
                       Rml::FontEffectsHandle font_effects_handle, Rml::StringView string,
                       Rml::Vector2f position, Rml::ColourbPremultiplied colour, float opacity,
                       const Rml::TextShapingContext &text_shaping_context,
                       Rml::TexturedMeshList &mesh_list) override;
    int GetVersion(Rml::FontFaceHandle handle) override;
};
