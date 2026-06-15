#include "DX12PCH.h"
#include "RenderTarget.h"
#include "Context.h"
#include "Texture.h"


RenderTarget RenderTarget::CreateDefaultMultiSampled(Context* context, ivec2 size, DXGI_FORMAT format) {
	RenderTarget renderTarget;
	DXGI_SAMPLE_DESC sampleDesc = context->GetDevice().GetMultiSampleDesc(format);
	CD3DX12_RESOURCE_DESC1 resourceDesc = CD3DX12_RESOURCE_DESC1::Tex2D(
		DEFAULT_BACK_BUFFER_FORMAT, UINT64(size.x), (UINT)size.y, 1, 1,
		sampleDesc.Count, sampleDesc.Quality, 
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	Ref<Texture> texture = context->CreateTexture(
		resourceDesc,
		D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		D3D12_HEAP_TYPE_DEFAULT,
		&DEFAULT_BACK_BUFFER_CLEAR_VALUE,
		L"Render Target Attachment[0]"
	);
	renderTarget.AttachTexture(Color0, texture);

	return renderTarget;
}

void RenderTarget::AttachTexture(Attachment attachment, const Ref<Texture>& texture) {
	ASSERT_MSG(texture, "RenderTarget attachment requires a valid texture.");
	m_attachedTextures[attachment] = texture;
}

void RenderTarget::Reset() {
	m_attachedTextures.clear();
}

void RenderTarget::Resize(ivec2 size) {

	for (Ref<Texture>& texture : m_attachedTextures | std::views::values) {
		texture->Resize(size);
	}
}

Ref<Texture> RenderTarget::GetAttachedTexture(Attachment attachment) const {
	auto it = m_attachedTextures.find(attachment);
	if (it != m_attachedTextures.end())
		return it->second;
	return {};
}

const RenderTarget::TextureMap& RenderTarget::GetAttachedTextures() const {
	return m_attachedTextures;
}

D3D12_RT_FORMAT_ARRAY RenderTarget::GetRTFormats() const {
	D3D12_RT_FORMAT_ARRAY array = {};
	for (const Ref<Texture>& texture : m_attachedTextures | std::views::values) {
		array.RTFormats[array.NumRenderTargets++] = texture->GetD3D12ResourceDesc().Format;
	}
	return array;
}

DXGI_SAMPLE_DESC RenderTarget::GetSampleDesc() const {
	DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };
	for (const Ref<Texture>& texture : m_attachedTextures | std::views::values) {
		sampleDesc = texture->GetD3D12ResourceDesc().SampleDesc;
	}
	return sampleDesc;
}


DepthStencil DepthStencil::CreateDefault(Context* context, ivec2 size, DXGI_FORMAT format) {
	DepthStencil depthStencil;
	CD3DX12_RESOURCE_DESC1 resourceDesc = CD3DX12_RESOURCE_DESC1::Tex2D(
		format, UINT64(size.x), (UINT)size.y, 1, 1,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
		D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

	depthStencil.m_texture = context->CreateTexture(
		resourceDesc,
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
		D3D12_HEAP_TYPE_DEFAULT,
		&DEFAULT_DEPTH_BUFFER_CLEAR_VALUE,
		L"Depth Stencil Attachment"
	);

	return depthStencil;
}

DepthStencil DepthStencil::CreateDefaultMultiSampled(Context* context, ivec2 size, DXGI_FORMAT format) {
	DepthStencil depthStencil;
	DXGI_SAMPLE_DESC sampleDesc = context->GetDevice().GetMultiSampleDesc(format);
	CD3DX12_RESOURCE_DESC1 resourceDesc = CD3DX12_RESOURCE_DESC1::Tex2D(
		format, UINT64(size.x), (UINT)size.y, 1, 1,
		sampleDesc.Count, sampleDesc.Quality,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | 
		D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

	depthStencil.m_texture = context->CreateTexture(
		resourceDesc,
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
		D3D12_HEAP_TYPE_DEFAULT,
		&DEFAULT_DEPTH_BUFFER_CLEAR_VALUE,
		L"Depth Stencil Attachment"
	);

	return depthStencil;
}
void DepthStencil::SetTexture(const Ref<
	Texture>& texture) {
	ASSERT_MSG(texture, "DepthStencil attachment requires a valid texture.");
	m_texture = texture;
}

void DepthStencil::Reset() {
	m_texture.reset();
}

void DepthStencil::Resize(ivec2 size) {
	m_texture->Resize(size);
}

CPUHandle DepthStencil::GetDsv() const {
	if (m_texture)
		return m_texture->GetDepthStencilView();
	return CD3DX12_CPU_DESCRIPTOR_HANDLE{ CD3DX12_DEFAULT{} };
}

DXGI_FORMAT DepthStencil::GetDsFormat() const {
	return m_texture->GetD3D12ResourceDesc().Format;
}
