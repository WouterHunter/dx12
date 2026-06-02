#include "DX12PCH.h"
#include "ResourceStateTracker.h"
#include "Context.h"
#include "CommandList.h"


// ============================================================================
// ResourceStateTracker
// ============================================================================

void ResourceStateTracker::TransitionTexture(
	ID3D12Resource* resource,
	u32 subresource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter,
	D3D12_BARRIER_LAYOUT layoutAfter,
	bool discard
) {
	SubresourceKey key = { resource, subresource };
	auto it = m_localTextureStates.find(key);

	if (it != m_localTextureStates.end()) {
		// We know the current local state — emit an immediate barrier
		LocalTextureState& localState = it->second;

		D3D12_TEXTURE_BARRIER barrier = {};
		barrier.SyncBefore = localState.lastSync;
		barrier.SyncAfter = syncAfter;
		barrier.AccessBefore = localState.lastAccess;
		barrier.AccessAfter = accessAfter;
		barrier.LayoutBefore = localState.layout;
		barrier.LayoutAfter = layoutAfter;
		barrier.pResource = resource;
		barrier.Subresources.IndexOrFirstMipLevel = subresource;
		barrier.Subresources.NumMipLevels = 0; // Indicates IndexOrFirstMipLevel is a subresource index
		barrier.Flags = discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE;

		// Skip no-op barriers (same layout, no cache flush needed)
		bool layoutChange = localState.layout != layoutAfter;
		bool accessFlush = (localState.lastAccess != D3D12_BARRIER_ACCESS_NO_ACCESS) &&
			(localState.lastAccess != accessAfter || layoutChange);

		if (layoutChange || accessFlush || discard) {
			m_immediateTextureBarriers.push_back(barrier);
		}

		// Update local state
		localState.layout = layoutAfter;
		localState.lastAccess = accessAfter;
		localState.lastSync = syncAfter;
	}
	else {
		// First time seeing this resource in this command list.
		// We don't know its LayoutBefore — defer to submit-time resolution.
		PendingTextureBarrier pending = {};
		pending.resource = resource;
		pending.subresource = subresource;
		pending.syncBefore = D3D12_BARRIER_SYNC_NONE;
		pending.syncAfter = syncAfter;
		pending.accessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS;
		pending.accessAfter = accessAfter;
		pending.layoutAfter = layoutAfter;
		pending.discard = discard;
		m_pendingTextureBarriers.push_back(pending);

		// Track the resource locally going forward
		m_localTextureStates[key] = { layoutAfter, accessAfter, syncAfter };
	}
}

void ResourceStateTracker::TransitionBuffer(
	ID3D12Resource* resource,
	D3D12_BARRIER_SYNC syncAfter,
	D3D12_BARRIER_ACCESS accessAfter
) {
	SubresourceKey key = { resource, 0 };
	auto it = m_localBufferStates.find(key);

	if (it != m_localBufferStates.end()) {
		// Known local state — emit immediate barrier if access changed from a write
		LocalBufferState& localState = it->second;

		// Only need a barrier if the previous access was a write
		bool prevWasWrite = (localState.lastAccess & (
			D3D12_BARRIER_ACCESS_RENDER_TARGET |
			D3D12_BARRIER_ACCESS_UNORDERED_ACCESS |
			D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE |
			D3D12_BARRIER_ACCESS_STREAM_OUTPUT |
			D3D12_BARRIER_ACCESS_COPY_DEST |
			D3D12_BARRIER_ACCESS_RESOLVE_DEST)) != 0;

		if (prevWasWrite) {
			D3D12_BUFFER_BARRIER barrier = {};
			barrier.SyncBefore = localState.lastSync;
			barrier.SyncAfter = syncAfter;
			barrier.AccessBefore = localState.lastAccess;
			barrier.AccessAfter = accessAfter;
			barrier.pResource = resource;
			barrier.Offset = 0;
			barrier.Size = UINT64_MAX;
			m_immediateBufferBarriers.push_back(barrier);
		}

		localState.lastAccess = accessAfter;
		localState.lastSync = syncAfter;
	}
	else {
		m_localBufferStates[key] = { accessAfter, syncAfter };
	}
}

void ResourceStateTracker::UAVBarrier(ID3D12Resource* resource, bool isTexture) {
	if (isTexture) {
		SubresourceKey key = { resource, SUBRESOURCE_ALL };
		auto it = m_localTextureStates.find(key);
		if (it != m_localTextureStates.end()) {
			D3D12_TEXTURE_BARRIER barrier = {};
			barrier.SyncBefore = it->second.lastSync;
			barrier.SyncAfter = it->second.lastSync; // same scope
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			barrier.LayoutBefore = it->second.layout;
			barrier.LayoutAfter = it->second.layout;
			barrier.pResource = resource;
			barrier.Subresources.IndexOrFirstMipLevel = SUBRESOURCE_ALL;
			barrier.Subresources.NumMipLevels = 0;
			barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;
			m_immediateTextureBarriers.push_back(barrier);
		}
	}
	else {
		SubresourceKey key = { resource, 0 };
		auto it = m_localBufferStates.find(key);
		if (it != m_localBufferStates.end()) {
			D3D12_BUFFER_BARRIER barrier = {};
			barrier.SyncBefore = it->second.lastSync;
			barrier.SyncAfter = it->second.lastSync;
			barrier.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			barrier.AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
			barrier.pResource = resource;
			barrier.Offset = 0;
			barrier.Size = UINT64_MAX;
			m_immediateBufferBarriers.push_back(barrier);
		}
	}
}

void ResourceStateTracker::FlushImmediateBarriers(CommandList* commandList) {

	if (m_immediateTextureBarriers.empty() && m_immediateBufferBarriers.empty())
		return;

	std::vector<D3D12_BARRIER_GROUP> groups;

	if (!m_immediateTextureBarriers.empty()) {
		D3D12_BARRIER_GROUP texGroup = {};
		texGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
		texGroup.NumBarriers = (u32)m_immediateTextureBarriers.size();
		texGroup.pTextureBarriers = m_immediateTextureBarriers.data();
		groups.push_back(texGroup);
	}

	if (!m_immediateBufferBarriers.empty()) {
		D3D12_BARRIER_GROUP bufGroup = {};
		bufGroup.Type = D3D12_BARRIER_TYPE_BUFFER;
		bufGroup.NumBarriers = (u32)m_immediateBufferBarriers.size();
		bufGroup.pBufferBarriers = m_immediateBufferBarriers.data();
		groups.push_back(bufGroup);
	}

	commandList->d3dCommandList->Barrier((u32)groups.size(), groups.data());

	// Clear immediate barriers after they've been flushed to the command list
	m_immediateTextureBarriers.clear();
	m_immediateBufferBarriers.clear();
}

void ResourceStateTracker::Reset() {
	m_localTextureStates.clear();
	m_localBufferStates.clear();
	m_pendingTextureBarriers.clear();
	m_immediateTextureBarriers.clear();
	m_immediateBufferBarriers.clear();
}


// ============================================================================
// GlobalLayoutTracker
// ============================================================================

void GlobalLayoutTracker::Init(Context* context) {

	Device& device = context->GetDevice();

	// Create pending command allocator and list
	ThrowIfFailed(device.d3d12Device10->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_pendingAllocator)));

	ComPtr<ID3D12GraphicsCommandList> baseCL;
	ThrowIfFailed(device.d3d12Device10->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pendingAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&baseCL)));
	ThrowIfFailed(baseCL.As(&m_pendingCommandList));
	ThrowIfFailed(m_pendingCommandList->Close());

	ThrowIfFailed(m_pendingAllocator->SetName(L"Pending Command Allocator"));
	ThrowIfFailed(m_pendingCommandList->SetName(L"Pending Command List"));
}

void GlobalLayoutTracker::Register(ID3D12Resource* resource, D3D12_BARRIER_LAYOUT initialLayout, u32 subresourceCount) {
	std::unique_lock lock(m_mutex);
	m_subresourceCounts[resource] = subresourceCount;
	for (u32 i = 0; i < subresourceCount; ++i) {
		m_layouts[{resource, i}] = initialLayout;
	}
}

void GlobalLayoutTracker::Unregister(ID3D12Resource* resource) {
	std::unique_lock lock(m_mutex);
	auto it = m_subresourceCounts.find(resource);
	if (it != m_subresourceCounts.end()) {
		u32 count = it->second;
		for (u32 i = 0; i < count; ++i) {
			m_layouts.erase({resource, i});
		}
		m_subresourceCounts.erase(it);
	}
}

ID3D12GraphicsCommandList7* GlobalLayoutTracker::ResolvePendingBarriers(std::vector<ResourceStateTracker*> trackers) {
	// Collect all pending texture barriers from all trackers
	std::vector<D3D12_TEXTURE_BARRIER> resolvedTextureBarriers;
	std::vector<D3D12_BUFFER_BARRIER> resolvedBufferBarriers;
	{
		std::shared_lock lock(m_mutex);
		for (ResourceStateTracker* tracker : trackers) {
			for (const PendingTextureBarrier& pending : tracker->GetPendingTextureBarriers()) {

				// Resolve the before state of the layout
				D3D12_BARRIER_LAYOUT layoutBefore;
				if (pending.discard) {
					layoutBefore = D3D12_BARRIER_LAYOUT_UNDEFINED;
				}
				else {
					// Find any registered layouts
					auto it = m_layouts.find({
						pending.resource,
						pending.subresource == SUBRESOURCE_ALL ? 0 : pending.subresource
						});
					if (it != m_layouts.end()) {
						layoutBefore = it->second;
					}
					else {
						// Unregistered resource — assume common layout
						layoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
					}
				}

				// Early exit: same layout or first access in ECL (no flush needed)
				// If the discard flag is true, continue anyway.
				if (layoutBefore == pending.layoutAfter && !pending.discard) {
					continue;
				}

				D3D12_TEXTURE_BARRIER barrier = {};
				barrier.SyncBefore = pending.syncBefore;
				barrier.SyncAfter = pending.syncAfter;
				barrier.AccessBefore = pending.accessBefore;
				barrier.AccessAfter = pending.accessAfter;
				barrier.LayoutBefore = layoutBefore;
				barrier.LayoutAfter = pending.layoutAfter;
				barrier.pResource = pending.resource;
				barrier.Subresources.IndexOrFirstMipLevel = pending.subresource;
				barrier.Subresources.NumMipLevels = 0;
				barrier.Flags = pending.discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE;

				resolvedTextureBarriers.push_back(barrier);
			}
		}
	}

	// If no barriers to emit, return nullptr
	if (resolvedTextureBarriers.empty() && resolvedBufferBarriers.empty()) {
		return nullptr;
	}

	// Record resolved barriers into the pending command list
	ThrowIfFailed(m_pendingAllocator->Reset());
	ThrowIfFailed(m_pendingCommandList->Reset(m_pendingAllocator.Get(), nullptr));

	std::vector<D3D12_BARRIER_GROUP> groups;

	if (!resolvedTextureBarriers.empty()) {
		D3D12_BARRIER_GROUP texGroup = {};
		texGroup.Type = D3D12_BARRIER_TYPE_TEXTURE;
		texGroup.NumBarriers = (u32)resolvedTextureBarriers.size();
		texGroup.pTextureBarriers = resolvedTextureBarriers.data();
		groups.push_back(texGroup);
	}

	if (!resolvedBufferBarriers.empty()) {
		D3D12_BARRIER_GROUP bufGroup = {};
		bufGroup.Type = D3D12_BARRIER_TYPE_BUFFER;
		bufGroup.NumBarriers = (u32)resolvedBufferBarriers.size();
		bufGroup.pBufferBarriers = resolvedBufferBarriers.data();
		groups.push_back(bufGroup);
	}

	m_pendingCommandList->Barrier((u32)groups.size(), groups.data());
	ThrowIfFailed(m_pendingCommandList->Close());

	return m_pendingCommandList.Get();
}

void GlobalLayoutTracker::CommitFinalLayoutStates(std::vector<ResourceStateTracker*> trackers) {
	std::unique_lock lock(m_mutex);
	for (ResourceStateTracker* tracker : trackers) {
		// For each tracker, store the final layout states
		for (const auto& [key, state] : tracker->GetFinalTextureStates()) {
			if (key.subresource == SUBRESOURCE_ALL) {
				auto it = m_subresourceCounts.find(key.resource);
				if (it != m_subresourceCounts.end()) {
					u32 count = it->second;
					for (u32 i = 0; i < count; ++i) {
						m_layouts[{key.resource, i}] = state.layout;
					}
				}
			}
			else {
				m_layouts[{key.resource, key.subresource}] = state.layout;
			}
		}
	}
}
