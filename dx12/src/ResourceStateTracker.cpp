#include "DX12PCH.h"
#include "ResourceStateTracker.h"
#include "Context.h"
#include "CommandList.h"

namespace {
	const wchar_t* str(D3D12_BARRIER_LAYOUT layout) {
		switch (layout) {
		case D3D12_BARRIER_LAYOUT_UNDEFINED: return L"LAYOUT_UNDEFINED";
		case D3D12_BARRIER_LAYOUT_COMMON: return L"LAYOUT_COMMON/PRESENT";
		case D3D12_BARRIER_LAYOUT_GENERIC_READ: return L"LAYOUT_GENERIC_READ";
		case D3D12_BARRIER_LAYOUT_RENDER_TARGET: return L"LAYOUT_RENDER_TARGET";
		case D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS: return L"LAYOUT_UNORDERED_ACCESS";
		case D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE: return L"LAYOUT_DEPTH_STENCIL_WRITE";
		case D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ: return L"LAYOUT_DEPTH_STENCIL_READ";
		case D3D12_BARRIER_LAYOUT_SHADER_RESOURCE: return L"LAYOUT_SHADER_RESOURCE";
		case D3D12_BARRIER_LAYOUT_COPY_SOURCE: return L"LAYOUT_COPY_SOURCE";
		case D3D12_BARRIER_LAYOUT_COPY_DEST: return L"LAYOUT_COPY_DEST";
		case D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE: return L"LAYOUT_RESOLVE_SOURCE";
		case D3D12_BARRIER_LAYOUT_RESOLVE_DEST: return L"LAYOUT_RESOLVE_DEST";
		case D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE: return L"LAYOUT_SHADING_RATE_SOURCE";
		case D3D12_BARRIER_LAYOUT_VIDEO_DECODE_READ: return L"LAYOUT_VIDEO_DECODE_READ";
		case D3D12_BARRIER_LAYOUT_VIDEO_DECODE_WRITE: return L"LAYOUT_VIDEO_DECODE_WRITE";
		case D3D12_BARRIER_LAYOUT_VIDEO_PROCESS_READ: return L"LAYOUT_VIDEO_PROCESS_READ";
		case D3D12_BARRIER_LAYOUT_VIDEO_PROCESS_WRITE: return L"LAYOUT_VIDEO_PROCESS_WRITE";
		case D3D12_BARRIER_LAYOUT_VIDEO_ENCODE_READ: return L"LAYOUT_VIDEO_ENCODE_READ";
		case D3D12_BARRIER_LAYOUT_VIDEO_ENCODE_WRITE: return L"LAYOUT_VIDEO_ENCODE_WRITE";
		case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON: return L"LAYOUT_DIRECT_QUEUE_COMMON";
		case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ: return L"LAYOUT_DIRECT_QUEUE_GENERIC_READ";
		case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS: return L"LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS";
		case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE: return L"LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE";
		case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_SOURCE: return L"LAYOUT_DIRECT_QUEUE_COPY_SOURCE";
		case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_DEST: return L"LAYOUT_DIRECT_QUEUE_COPY_DEST";
		case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COMMON: return L"LAYOUT_COMPUTE_QUEUE_COMMON";
		case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_GENERIC_READ: return L"LAYOUT_COMPUTE_QUEUE_GENERIC_READ";
		case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS: return L"LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS";
		case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE: return L"LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE";
		case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_SOURCE: return L"LAYOUT_COMPUTE_QUEUE_COPY_SOURCE";
		case D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_COPY_DEST: return L"LAYOUT_COMPUTE_QUEUE_COPY_DEST";
		case D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ_COMPUTE_QUEUE_ACCESSIBLE: return L"LAYOUT_DIRECT_QUEUE_GENERIC_READ_COMPUTE_QUEUE_ACCESSIBLE";
		default: return L"INVALID LAYOUT";
		}
	}

	const wchar_t* str(D3D12_BARRIER_SYNC sync) {
		switch (sync) {
		case D3D12_BARRIER_SYNC_NONE: return L"SYNC_NONE";
		case D3D12_BARRIER_SYNC_ALL: return L"SYNC_ALL";
		case D3D12_BARRIER_SYNC_DRAW: return L"SYNC_DRAW";
		case D3D12_BARRIER_SYNC_INDEX_INPUT: return L"SYNC_INDEX_INPUT";
		case D3D12_BARRIER_SYNC_VERTEX_SHADING: return L"SYNC_VERTEX_SHADING";
		case D3D12_BARRIER_SYNC_PIXEL_SHADING: return L"SYNC_PIXEL_SHADING";
		case D3D12_BARRIER_SYNC_DEPTH_STENCIL: return L"SYNC_DEPTH_STENCIL";
		case D3D12_BARRIER_SYNC_RENDER_TARGET: return L"SYNC_RENDER_TARGET";
		case D3D12_BARRIER_SYNC_COMPUTE_SHADING: return L"SYNC_COMPUTE_SHADING";
		case D3D12_BARRIER_SYNC_RAYTRACING: return L"SYNC_RAYTRACING";
		case D3D12_BARRIER_SYNC_COPY: return L"SYNC_COPY";
		case D3D12_BARRIER_SYNC_RESOLVE: return L"SYNC_RESOLVE";
		case D3D12_BARRIER_SYNC_EXECUTE_INDIRECT: return L"SYNC_EXECUTE_INDIRECT/PREDICATION";
		case D3D12_BARRIER_SYNC_ALL_SHADING: return L"SYNC_ALL_SHADING";
		case D3D12_BARRIER_SYNC_NON_PIXEL_SHADING: return L"SYNC_NON_PIXEL_SHADING";
		case D3D12_BARRIER_SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO: return L"SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO";
		case D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW: return L"SYNC_CLEAR_UNORDERED_ACCESS_VIEW";
		case D3D12_BARRIER_SYNC_VIDEO_DECODE: return L"SYNC_VIDEO_DECODE";
		case D3D12_BARRIER_SYNC_VIDEO_PROCESS: return L"SYNC_VIDEO_PROCESS";
		case D3D12_BARRIER_SYNC_VIDEO_ENCODE: return L"SYNC_VIDEO_ENCODE";
		case D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE: return L"SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE";
		case D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE: return L"SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE";
		case D3D12_BARRIER_SYNC_SPLIT: return L"SYNC_SPLIT";
		default: return L"INVALID SYNC";
		}
	}

	const wchar_t* str(D3D12_BARRIER_ACCESS access) {
		switch (access) {
		case D3D12_BARRIER_ACCESS_COMMON: return L"ACCESS_COMMON";
		case D3D12_BARRIER_ACCESS_VERTEX_BUFFER: return L"ACCESS_VERTEX_BUFFER";
		case D3D12_BARRIER_ACCESS_CONSTANT_BUFFER: return L"ACCESS_CONSTANT_BUFFER";
		case D3D12_BARRIER_ACCESS_INDEX_BUFFER: return L"ACCESS_INDEX_BUFFER";
		case D3D12_BARRIER_ACCESS_RENDER_TARGET: return L"ACCESS_RENDER_TARGET";
		case D3D12_BARRIER_ACCESS_UNORDERED_ACCESS: return L"ACCESS_UNORDERED_ACCESS";
		case D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE: return L"ACCESS_DEPTH_STENCIL_WRITE";
		case D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ: return L"ACCESS_DEPTH_STENCIL_READ";
		case D3D12_BARRIER_ACCESS_SHADER_RESOURCE: return L"ACCESS_SHADER_RESOURCE";
		case D3D12_BARRIER_ACCESS_STREAM_OUTPUT: return L"ACCESS_STREAM_OUTPUT";
		case D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT: return L"ACCESS_INDIRECT_ARGUMENT/PREDICATION";
		case D3D12_BARRIER_ACCESS_COPY_DEST: return L"ACCESS_COPY_DEST";
		case D3D12_BARRIER_ACCESS_COPY_SOURCE: return L"ACCESS_COPY_SOURCE";
		case D3D12_BARRIER_ACCESS_RESOLVE_DEST: return L"ACCESS_RESOLVE_DEST";
		case D3D12_BARRIER_ACCESS_RESOLVE_SOURCE: return L"ACCESS_RESOLVE_SOURCE";
		case D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ: return L"ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ";
		case D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE: return L"ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE";
		case D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE: return L"ACCESS_SHADING_RATE_SOURCE";
		case D3D12_BARRIER_ACCESS_VIDEO_DECODE_READ: return L"ACCESS_VIDEO_DECODE_READ";
		case D3D12_BARRIER_ACCESS_VIDEO_DECODE_WRITE: return L"ACCESS_VIDEO_DECODE_WRITE";
		case D3D12_BARRIER_ACCESS_VIDEO_PROCESS_READ: return L"ACCESS_VIDEO_PROCESS_READ";
		case D3D12_BARRIER_ACCESS_VIDEO_PROCESS_WRITE: return L"ACCESS_VIDEO_PROCESS_WRITE";
		case D3D12_BARRIER_ACCESS_VIDEO_ENCODE_READ: return L"ACCESS_VIDEO_ENCODE_READ";
		case D3D12_BARRIER_ACCESS_VIDEO_ENCODE_WRITE: return L"ACCESS_VIDEO_ENCODE_WRITE";
		case D3D12_BARRIER_ACCESS_NO_ACCESS: return L"ACCESS_NO_ACCESS";
		default: return L"INVALID ACCESS";
		}
	}

	void PrintBufferBarrier(const D3D12_BUFFER_BARRIER& barrier) {
		wchar_t name[128] = {};
		UINT size = sizeof(name);
		barrier.pResource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);

		fmt::print(LR"(
		Resource: {}
		Sync Before: {} | Sync After: {}
		Access Before: {} | Access After: {}
			)",
			name,
			str(barrier.SyncBefore),
			str(barrier.SyncAfter),
			str(barrier.AccessBefore),
			str(barrier.AccessAfter));
	}

	void PrintTextureBarrier(const D3D12_TEXTURE_BARRIER& barrier) {
		wchar_t name[128] = {};
		UINT size = sizeof(name);
		barrier.pResource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
		std::wstring subresource = barrier.Subresources.IndexOrFirstMipLevel == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ?
			L"ALL_SUBRESOURCES" : std::to_wstring(barrier.Subresources.IndexOrFirstMipLevel);

		fmt::print(LR"(
		Resource: {} | Subresource: {}
		Sync Before: {} | Sync After: {}
		Access Before: {} | Access After: {}
		Layout Before: {} | Layout After: {}		
			)",
			name, subresource,
			str(barrier.SyncBefore),
			str(barrier.SyncAfter),
			str(barrier.AccessBefore),
			str(barrier.AccessAfter),
			str(barrier.LayoutBefore),
			str(barrier.LayoutAfter));
	};

	u32 GetSubresourceCount(ID3D12Resource* resource) {
		D3D12_RESOURCE_DESC desc = resource->GetDesc();
		u32 arraySize = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : desc.DepthOrArraySize;
		u32 subresourceCount = desc.MipLevels * arraySize;
		return subresourceCount;
	}
}

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
	if (subresource == SUBRESOURCE_ALL) {
		// Transition all subresources one by one
		const u32 count = GetSubresourceCount(resource);
		for (u32 i = 0; i < count; ++i) {
			TransitionTexture(
				resource, i,
				syncAfter, accessAfter, 
				layoutAfter, discard);
		}
	} else {
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
		} else {
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

void ResourceStateTracker::UAVBufferBarrier(ID3D12Resource* resource) {
	SubresourceKey key = { resource, 0 };
	auto it = m_localBufferStates.find(key);
	ASSERT_MSG(it != m_localBufferStates.end(), "Unknown subresource key");
	ASSERT_MSG(it->second.lastSync & D3D12_BARRIER_SYNC_ALL_SHADING, "Invalid sync state");
	ASSERT_MSG(it->second.lastAccess & D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, "Invalid access state");

	D3D12_BUFFER_BARRIER barrier = {};
	barrier.SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
	barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
	barrier.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	barrier.AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	barrier.pResource = resource;
	barrier.Offset = 0;
	barrier.Size = UINT64_MAX;
	m_immediateBufferBarriers.push_back(barrier);
}

void ResourceStateTracker::UAVTextureBarrier(ID3D12Resource* resource, u32 firstSubresource, u32 numSubresources) {
	for (u32 i = 0; i < numSubresources; ++i) {
		SubresourceKey key = { resource, firstSubresource + i };
		auto it = m_localTextureStates.find(key);
		ASSERT_MSG(it != m_localTextureStates.end(), "Unknown subresource key");
		ASSERT_MSG(it->second.lastSync & D3D12_BARRIER_SYNC_ALL_SHADING, "Invalid sync state");
		ASSERT_MSG(it->second.lastAccess & D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, "Invalid access state");
	}

	D3D12_TEXTURE_BARRIER barrier = {};
	barrier.SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
	barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
	barrier.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	barrier.AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	barrier.pResource = resource;
	barrier.Subresources = CD3DX12_BARRIER_SUBRESOURCE_RANGE(firstSubresource, numSubresources, 0, 1);
	barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;
	m_immediateTextureBarriers.push_back(barrier);
}

void ResourceStateTracker::FlushImmediateBarriers(CommandList* commandList) {

	if (m_immediateTextureBarriers.empty() && m_immediateBufferBarriers.empty())
		return;

#ifdef PRINT_IMMEDIATE_BARRIERS
	fmt::print("ResourceStateTracker::FlushImmediateBarriers\n");
	for (const auto& barrier : m_immediateTextureBarriers) {
		PrintTextureBarrier(barrier);
	}
#endif

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

void GlobalLayoutTracker::Register(ID3D12Resource* resource, D3D12_BARRIER_LAYOUT initialLayout) {
	std::unique_lock lock(m_mutex);

	// Find the subresource count
	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	u32 arraySize = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : desc.DepthOrArraySize;
	u32 subresourceCount = desc.MipLevels * arraySize;
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
			m_layouts[{key.resource, key.subresource}] = state.layout;
		}
	}
}
