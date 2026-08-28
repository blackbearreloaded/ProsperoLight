/*
 * ps5-native-app-boilerplate - ProsperoLight component.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bitmap_font_engine.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/BaseXMLParser.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/StreamMemory.h>
#include <RmlUi/Core/Texture.h>

#include <cstdint>
#include <utility>

namespace
{

struct BitmapGlyph
{
    int advance = 0;
    int page = 0;
    Rml::Vector2f offset{};
    Rml::Vector2f position{};
    Rml::Vector2f dimension{};
};

using FontGlyphs = Rml::UnorderedMap<Rml::Character, BitmapGlyph>;
using FontKerning = Rml::UnorderedMap<std::uint64_t, int>;

struct BitmapPage
{
    BitmapPage(Rml::String texture_name, Rml::String texture_path, Rml::Vector2f dimensions)
        : source(Rml::MakeUnique<Rml::TextureSource>(std::move(texture_name),
                                                     std::move(texture_path))),
          dimensions(dimensions)
    {
    }

    Rml::UniquePtr<Rml::TextureSource> source;
    Rml::Vector2f dimensions{};
};

using FontPages = Rml::Vector<BitmapPage>;

class BitmapFontFace
{
  public:
    BitmapFontFace(Rml::String family, Rml::Style::FontStyle style, Rml::Style::FontWeight weight,
                   Rml::FontMetrics metrics, FontPages &&pages, FontGlyphs &&glyphs,
                   FontKerning &&kerning)
        : family_(std::move(family)), style_(style), weight_(weight), metrics_(metrics),
          pages_(std::move(pages)), glyphs_(std::move(glyphs)), kerning_(std::move(kerning))
    {
    }

    void Merge(FontPages &&pages, FontGlyphs &&glyphs, FontKerning &&kerning)
    {
        const int page_offset = static_cast<int>(pages_.size());
        for (BitmapPage &page : pages)
            pages_.push_back(std::move(page));
        for (auto &pair : glyphs)
        {
            if (glyphs_.find(pair.first) != glyphs_.end())
                continue;
            pair.second.page += page_offset;
            glyphs_.emplace(pair.first, pair.second);
        }
        for (const auto &pair : kerning)
            if (kerning_.find(pair.first) == kerning_.end())
                kerning_.emplace(pair.first, pair.second);
    }

    int GetStringWidth(Rml::StringView string, Rml::Character previous_character,
                       bool kerning_enabled, int letter_spacing) const
    {
        int width = 0;
        for (auto character_it = Rml::StringIteratorU8(string); character_it; ++character_it)
        {
            const Rml::Character character = *character_it;
            const auto glyph_it = glyphs_.find(character);
            if (glyph_it == glyphs_.end())
                continue;
            if (kerning_enabled)
                width += GetKerning(previous_character, character);
            width += glyph_it->second.advance + letter_spacing;
            previous_character = character;
        }
        return width;
    }

    int GenerateString(Rml::RenderManager &render_manager, Rml::StringView string,
                       Rml::Vector2f string_position, Rml::ColourbPremultiplied colour,
                       bool kerning_enabled, int letter_spacing, Rml::TexturedMeshList &mesh_list)
    {
        int width = 0;
        mesh_list.clear();
        Rml::Vector<int> page_meshes(pages_.size(), -1);

        Rml::Vector2f position = string_position.Round();
        Rml::Character previous_character = Rml::Character::Null;
        for (auto character_it = Rml::StringIteratorU8(string); character_it; ++character_it)
        {
            const Rml::Character character = *character_it;
            const auto glyph_it = glyphs_.find(character);
            if (glyph_it == glyphs_.end())
                continue;

            const int pair_kerning =
                kerning_enabled ? GetKerning(previous_character, character) : 0;
            width += pair_kerning;
            position.x += pair_kerning;

            const BitmapGlyph &glyph = glyph_it->second;
            if (glyph.dimension.x > 0 && glyph.dimension.y > 0 && glyph.page >= 0 &&
                static_cast<std::size_t>(glyph.page) < pages_.size())
            {
                int mesh_index = page_meshes[glyph.page];
                if (mesh_index < 0)
                {
                    mesh_index = static_cast<int>(mesh_list.size());
                    page_meshes[glyph.page] = mesh_index;
                    mesh_list.resize(mesh_list.size() + 1);
                    mesh_list[mesh_index].texture =
                        pages_[glyph.page].source->GetTexture(render_manager);
                }
                Rml::Mesh &mesh = mesh_list[mesh_index].mesh;
                const Rml::Vector2f dimensions = pages_[glyph.page].dimensions;
                const Rml::Vector2f uv_top_left = glyph.position / dimensions;
                const Rml::Vector2f uv_bottom_right =
                    (glyph.position + glyph.dimension) / dimensions;
                Rml::MeshUtilities::GenerateQuad(mesh, (position + glyph.offset).Round(),
                                                 glyph.dimension, colour, uv_top_left,
                                                 uv_bottom_right);
            }

            width += glyph.advance + letter_spacing;
            position.x += glyph.advance + letter_spacing;
            previous_character = character;
        }
        return width;
    }

    const Rml::FontMetrics &Metrics() const
    {
        return metrics_;
    }
    const Rml::String &Family() const
    {
        return family_;
    }
    Rml::Style::FontStyle Style() const
    {
        return style_;
    }
    Rml::Style::FontWeight Weight() const
    {
        return weight_;
    }

  private:
    int GetKerning(Rml::Character left, Rml::Character right) const
    {
        const std::uint64_t key = (std::uint64_t(left) << 32) | std::uint64_t(right);
        const auto it = kerning_.find(key);
        return it == kerning_.end() ? 0 : it->second;
    }

    Rml::String family_;
    Rml::Style::FontStyle style_ = Rml::Style::FontStyle::Normal;
    Rml::Style::FontWeight weight_ = Rml::Style::FontWeight::Normal;
    Rml::FontMetrics metrics_{};
    FontPages pages_;
    FontGlyphs glyphs_;
    FontKerning kerning_;
};

class BitmapFontParser final : public Rml::BaseXMLParser
{
  public:
    void HandleElementStart(const Rml::String &name, const Rml::XMLAttributes &attributes) override
    {
        if (name == "info")
        {
            family = Rml::StringUtilities::ToLower(Get(attributes, "face", Rml::String()));
            metrics.size = Get(attributes, "size", 0);
            style = Get(attributes, "italic", 0) == 1 ? Rml::Style::FontStyle::Italic
                                                      : Rml::Style::FontStyle::Normal;
            weight = Get(attributes, "bold", 0) == 1 ? Rml::Style::FontWeight::Bold
                                                     : static_cast<Rml::Style::FontWeight>(500);
        }
        else if (name == "common")
        {
            metrics.line_spacing = Get(attributes, "lineHeight", 0.f);
            metrics.ascent = Get(attributes, "base", 0.f);
            metrics.descent = metrics.line_spacing - metrics.ascent;
            texture_dimensions.x = Get(attributes, "scaleW", 0.f);
            texture_dimensions.y = Get(attributes, "scaleH", 0.f);
        }
        else if (name == "page")
        {
            const int id = Get(attributes, "id", -1);
            if (id >= 0)
                page_names[id] = Get(attributes, "file", Rml::String());
        }
        else if (name == "char")
        {
            const Rml::Character character = static_cast<Rml::Character>(Get(attributes, "id", 0));
            if (character == Rml::Character::Null)
                return;
            BitmapGlyph &glyph = glyphs[character];
            glyph.page = Get(attributes, "page", 0);
            glyph.offset.x = Get(attributes, "xoffset", 0.f);
            glyph.offset.y = Get(attributes, "yoffset", 0.f) - metrics.ascent;
            glyph.advance = Get(attributes, "xadvance", 0);
            glyph.position.x = Get(attributes, "x", 0.f);
            glyph.position.y = Get(attributes, "y", 0.f);
            glyph.dimension.x = Get(attributes, "width", 0.f);
            glyph.dimension.y = Get(attributes, "height", 0.f);
            if (character == static_cast<Rml::Character>('x'))
                metrics.x_height = glyph.dimension.y;
            if (character == static_cast<Rml::Character>(0x2026))
                metrics.has_ellipsis = true;
        }
        else if (name == "kerning")
        {
            const std::uint64_t first = Get(attributes, "first", 0);
            const std::uint64_t second = Get(attributes, "second", 0);
            const int amount = Get(attributes, "amount", 0);
            if (first && second && amount)
                kerning[(first << 32) | second] = amount;
        }
    }

    void HandleElementEnd(const Rml::String &) override
    {
    }
    void HandleData(const Rml::String &, Rml::XMLDataType) override
    {
    }

    Rml::String family;
    Rml::Style::FontStyle style = Rml::Style::FontStyle::Normal;
    Rml::Style::FontWeight weight = static_cast<Rml::Style::FontWeight>(500);
    Rml::UnorderedMap<int, Rml::String> page_names;
    Rml::Vector2f texture_dimensions{};
    Rml::FontMetrics metrics{};
    FontGlyphs glyphs;
    FontKerning kerning;
};

Rml::Vector<Rml::UniquePtr<BitmapFontFace>> fonts;

bool LoadBitmapFont(const Rml::String &file_name)
{
    Rml::FileInterface *files = Rml::GetFileInterface();
    const Rml::FileHandle file = files->Open(file_name);
    if (!file)
        return false;

    const std::size_t length = files->Length(file);
    Rml::UniquePtr<Rml::byte[]> data(new Rml::byte[length]);
    const std::size_t read_length = files->Read(data.get(), length, file);
    files->Close(file);
    if (!data || read_length != length)
        return false;

    BitmapFontParser parser;
    auto stream = Rml::MakeUnique<Rml::StreamMemory>(data.get(), length);
    stream->SetSourceURL(file_name);
    parser.Parse(stream.get());
    if (parser.family.empty() || parser.glyphs.empty() || parser.page_names.empty() ||
        parser.metrics.size == 0)
        return false;

    int maximum_page = -1;
    for (const auto &page : parser.page_names)
        if (page.first > maximum_page)
            maximum_page = page.first;
    FontPages pages;
    pages.reserve(static_cast<std::size_t>(maximum_page + 1));
    for (int page = 0; page <= maximum_page; ++page)
    {
        const auto name = parser.page_names.find(page);
        if (name == parser.page_names.end() || name->second.empty())
            return false;
        pages.emplace_back(name->second, file_name, parser.texture_dimensions);
    }

    parser.metrics.underline_position = 3.f;
    parser.metrics.underline_thickness = 1.f;
    for (const auto &font : fonts)
    {
        if (font->Family() == parser.family && font->Style() == parser.style &&
            font->Weight() == parser.weight && font->Metrics().size == parser.metrics.size)
        {
            font->Merge(std::move(pages), std::move(parser.glyphs), std::move(parser.kerning));
            return true;
        }
    }
    fonts.push_back(Rml::MakeUnique<BitmapFontFace>(
        parser.family, parser.style, parser.weight, parser.metrics, std::move(pages),
        std::move(parser.glyphs), std::move(parser.kerning)));
    return true;
}

BitmapFontFace *FindBitmapFont(const Rml::String &family, Rml::Style::FontStyle style,
                               Rml::Style::FontWeight weight, int size)
{
    const Rml::String normalized_family = Rml::StringUtilities::ToLower(family);
    for (const auto &font : fonts)
    {
        if (font->Family() == normalized_family && font->Style() == style &&
            font->Weight() == weight && font->Metrics().size == size)
            return font.get();
    }
    return nullptr;
}

} // namespace

void BitmapFontEngine::Initialize()
{
}

void BitmapFontEngine::Shutdown()
{
    fonts.clear();
}

bool BitmapFontEngine::LoadFontFace(const Rml::String &file_name, int, bool, Rml::Style::FontWeight)
{
    return LoadBitmapFont(file_name);
}

bool BitmapFontEngine::LoadFontFace(Rml::Span<const Rml::byte>, int, const Rml::String &family,
                                    Rml::Style::FontStyle, Rml::Style::FontWeight, bool)
{
    return family == "rmlui-debugger-font";
}

Rml::FontFaceHandle BitmapFontEngine::GetFontFaceHandle(const Rml::String &family,
                                                        Rml::Style::FontStyle style,
                                                        Rml::Style::FontWeight weight, int size)
{
    return reinterpret_cast<Rml::FontFaceHandle>(FindBitmapFont(family, style, weight, size));
}

Rml::FontEffectsHandle BitmapFontEngine::PrepareFontEffects(Rml::FontFaceHandle,
                                                            const Rml::FontEffectList &)
{
    return 0;
}

const Rml::FontMetrics &BitmapFontEngine::GetFontMetrics(Rml::FontFaceHandle handle)
{
    return reinterpret_cast<BitmapFontFace *>(handle)->Metrics();
}

int BitmapFontEngine::GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
                                     const Rml::TextShapingContext &shaping,
                                     Rml::Character prior_character)
{
    return reinterpret_cast<BitmapFontFace *>(handle)->GetStringWidth(
        string, prior_character, shaping.font_kerning != Rml::Style::FontKerning::None,
        static_cast<int>(shaping.letter_spacing));
}

int BitmapFontEngine::GenerateString(Rml::RenderManager &render_manager, Rml::FontFaceHandle handle,
                                     Rml::FontEffectsHandle, Rml::StringView string,
                                     Rml::Vector2f position, Rml::ColourbPremultiplied colour,
                                     float, const Rml::TextShapingContext &shaping,
                                     Rml::TexturedMeshList &mesh_list)
{
    return reinterpret_cast<BitmapFontFace *>(handle)->GenerateString(
        render_manager, string, position, colour,
        shaping.font_kerning != Rml::Style::FontKerning::None,
        static_cast<int>(shaping.letter_spacing), mesh_list);
}

int BitmapFontEngine::GetVersion(Rml::FontFaceHandle)
{
    return 0;
}
