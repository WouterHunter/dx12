#pragma once

class Context;
class Texture;


class RenderTarget {
public:
    enum Attachment {
        Color0,
        Color1,
        Color2,
        Color3,
        Color4,
        Color5,
        Color6,
        Color7,
        NumAttachments
    };
    using TextureMap = std::map<Attachment, Ref<Texture>>;

    static RenderTarget CreateDefaultMultiSampled(Context* context, ivec2 size, DXGI_FORMAT format = DEFAULT_BACK_BUFFER_FORMAT);

    void AttachTexture(Attachment attachment, const Ref<Texture>& texture);
    void Reset();
    void Resize(ivec2 size);

    [[nodiscard]] const TextureMap& GetAttachedTextures() const;
    [[nodiscard]] Ref<Texture> GetAttachedTexture(Attachment attachment) const;
    [[nodiscard]] D3D12_RT_FORMAT_ARRAY GetRTFormats() const;
    [[nodiscard]] DXGI_SAMPLE_DESC GetSampleDesc() const;
    
private:
    TextureMap m_attachedTextures{};
};

class DepthStencil {
public:

    static DepthStencil CreateDefault(Context* context, ivec2 size, DXGI_FORMAT format = DEFAULT_DEPTH_BUFFER_FORMAT);
    static DepthStencil CreateDefaultMultiSampled(Context* context, ivec2 size, DXGI_FORMAT format = DEFAULT_DEPTH_BUFFER_FORMAT);

    void SetTexture(const Ref<Texture>& texture);
    void Reset();
    void Resize(ivec2 size);

    [[nodiscard]] Ref<Texture> GetTexture() const { return m_texture; }
    [[nodiscard]] CPUHandle GetDsv() const;
    [[nodiscard]] DXGI_FORMAT GetDsFormat() const;

private:
    Ref<Texture> m_texture{};
};