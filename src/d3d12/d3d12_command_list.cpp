#include "d3d12_functions.hpp"

#include "../util/log.hpp"
#include "d3d12_defines.hpp"

namespace wr::d3d12
{
	CommandList* CreateCommandList(Device* device, unsigned int num_allocators, CmdListType type)
	{
		auto* cmd_list = new CommandList();
		const auto n_device = device->m_native;
		const auto n_cmd_list = cmd_list->m_native;

		cmd_list->m_allocators.resize(num_allocators);

		// Create the allocators
		for (auto& allocator : cmd_list->m_allocators)
		{
			TRY_M(n_device->CreateCommandAllocator((D3D12_COMMAND_LIST_TYPE)type, IID_PPV_ARGS(&allocator)),
				"Failed to create command allocator");
			NAME_D3D12RESOURCE(allocator);
		}

		// Create the command lists
		TRY_M(device->m_native->CreateCommandList(
			0,
			(D3D12_COMMAND_LIST_TYPE)type,
			cmd_list->m_allocators[0],
			NULL,
			IID_PPV_ARGS(&cmd_list->m_native)
		), "Failed to create command list");
		NAME_D3D12RESOURCE(cmd_list->m_native);
		cmd_list->m_native->Close(); // TODO: Can be optimized away.

		if (GetRaytracingType(device) == RaytracingType::FALLBACK)
		{
			device->m_fallback_native->QueryRaytracingCommandList(cmd_list->m_native, IID_PPV_ARGS(&cmd_list->m_native_fallback));
		}

		return cmd_list;
	}

	void SetName(CommandList* cmd_list, std::string const& name)
	{
		SetName(cmd_list, std::wstring(name.begin(), name.end()));
	}

	void SetName(CommandList* cmd_list, std::wstring const& name)
	{
		cmd_list->m_native->SetName(name.c_str());

		for (auto& allocator : cmd_list->m_allocators)
		{
			allocator->SetName((name + L" Allocator").c_str());
		}
	}

	void Begin(CommandList* cmd_list, unsigned int frame_idx)
	{
		// TODO: move resetting to when the command list is executed. This is how vulkan does it.
		TRY_M(cmd_list->m_allocators[frame_idx]->Reset(), 
			"Failed to reset cmd allocators");

		// Only reset with pipeline state if using bundles since only then this will impact fps.
		// Otherwise its just easier to pass NULL and suffer the insignificant performance loss.
		TRY_M(cmd_list->m_native->Reset(cmd_list->m_allocators[frame_idx], NULL),
			"Failed to reset command list.");
	}

	void End(CommandList* cmd_list)
	{
		cmd_list->m_native->Close();
	}

	void ExecuteBundle(CommandList* cmd_list, CommandList* bundle)
	{
		cmd_list->m_native->ExecuteBundle(bundle->m_native);
	}

	void ExecuteIndirect(CommandList* cmd_list, CommandSignature* cmd_signature, IndirectCommandBuffer* buffer)
	{
		cmd_list->m_native->ExecuteIndirect(cmd_signature->m_native, buffer->m_num_buffers, buffer->m_native, 0, nullptr, 0);
	}

	void BindRenderTarget(CommandList* cmd_list, RenderTarget* render_target, bool clear, bool clear_depth)
	{
		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> handles;
		handles.resize(render_target->m_render_targets.size());

		for (auto i = 0; i < handles.size(); i++)
		{
			handles[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(render_target->m_rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), i, render_target->m_rtv_descriptor_increment_size);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle;
		if (render_target->m_create_info.m_create_dsv_buffer) dsv_handle = render_target->m_depth_stencil_resource_heap->GetCPUDescriptorHandleForHeapStart();

		cmd_list->m_native->OMSetRenderTargets(handles.size(), handles.data(), false, render_target->m_create_info.m_create_dsv_buffer ? &dsv_handle : nullptr);
		if (clear)
		{
			for (auto& handle : handles)
			{
				cmd_list->m_native->ClearRenderTargetView(handle, render_target->m_create_info.m_clear_color, 0, nullptr);
			}
		}
		if (clear_depth && render_target->m_create_info.m_create_dsv_buffer)
		{
			cmd_list->m_native->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}
	}

	void BindRenderTargetVersioned(CommandList* cmd_list, RenderTarget* render_target, unsigned int frame_idx, bool clear, bool clear_depth)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(render_target->m_rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
			frame_idx % render_target->m_render_targets.size(), render_target->m_rtv_descriptor_increment_size);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle;
		if (render_target->m_create_info.m_create_dsv_buffer) dsv_handle = render_target->m_depth_stencil_resource_heap->GetCPUDescriptorHandleForHeapStart();

		cmd_list->m_native->OMSetRenderTargets(1, &rtv_handle, false, render_target->m_create_info.m_create_dsv_buffer ? &dsv_handle : nullptr);

		if (clear)
		{
			cmd_list->m_native->ClearRenderTargetView(rtv_handle, render_target->m_create_info.m_clear_color, 0, nullptr);
		}
		if (clear_depth && render_target->m_create_info.m_create_dsv_buffer)
		{
			cmd_list->m_native->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}
	}

	void BindRenderTargetOnlyDepth(CommandList* cmd_list, RenderTarget* render_target, unsigned int frame_idx, bool clear)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle;
		if (render_target->m_create_info.m_create_dsv_buffer) dsv_handle = render_target->m_depth_stencil_resource_heap->GetCPUDescriptorHandleForHeapStart();

		cmd_list->m_native->OMSetRenderTargets(0, nullptr, false, render_target->m_create_info.m_create_dsv_buffer ? &dsv_handle : nullptr);

		if (clear)
		{
			cmd_list->m_native->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}
	}

	void BindPipeline(CommandList* cmd_list, PipelineState* pipeline_state) // TODO: Binding the root signature seperatly can improve perf if done right.
	{
		cmd_list->m_native->SetPipelineState(pipeline_state->m_native);
		cmd_list->m_native->SetGraphicsRootSignature(pipeline_state->m_root_signature->m_native);
	}

	void BindDescriptorHeaps(CommandList* cmd_list, std::vector<DescriptorHeap*> heaps, unsigned int frame_idx, bool fallback)
	{
		auto num = heaps.size();
		std::vector<ID3D12DescriptorHeap*> n_heaps(num);

		for (decltype(num) i = 0; i < num; i++)
		{
			n_heaps[i] = heaps[i]->m_native[frame_idx % heaps[i]->m_create_info.m_versions];
		}

		if (fallback)
		{
			cmd_list->m_native_fallback->SetDescriptorHeaps(num, n_heaps.data());
		}
		else
		{
			cmd_list->m_native->SetDescriptorHeaps(num, n_heaps.data());
		}
	}

	void BindComputePipeline(CommandList* cmd_list, PipelineState * pipeline_state)
	{
		if (pipeline_state->m_type != PipelineType::COMPUTE_PIPELINE)
			LOGW("Tried to bind a graphics pipeline as a compute pipeline");
		cmd_list->m_native->SetPipelineState(pipeline_state->m_native);
		cmd_list->m_native->SetComputeRootSignature(pipeline_state->m_root_signature->m_native);
	}

	void BindViewport(CommandList* cmd_list, Viewport const & viewport)
	{
		cmd_list->m_native->RSSetViewports(1, &viewport.m_viewport);
		cmd_list->m_native->RSSetScissorRects(1, &viewport.m_scissor_rect);
	}

	void BindVertexBuffer(CommandList* cmd_list, StagingBuffer* buffer, std::size_t offset, std::size_t size, std::size_t stride)
	{
		if (!buffer->m_gpu_address)
		{
			LOGC("No GPU address found. Has the buffer beens staged?");
		}

		D3D12_VERTEX_BUFFER_VIEW view;
		view.BufferLocation = buffer->m_gpu_address + offset;
		view.StrideInBytes = stride;
		view.SizeInBytes = size;

		cmd_list->m_native->IASetVertexBuffers(0, 1, &view);
	}

	void BindIndexBuffer(CommandList* cmd_list, StagingBuffer* buffer, unsigned int offset, unsigned int size)
	{
		if (!buffer->m_gpu_address)
		{
			throw "No GPU address found. Has the buffer beens staged?";
		}

		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = buffer->m_gpu_address + offset;
		view.Format = DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = size;

		cmd_list->m_native->IASetIndexBuffer(&view);
	}

	void BindConstantBuffer(CommandList* cmd_list, HeapResource* buffer, unsigned int root_parameter_idx, unsigned int frame_idx)
	{
		cmd_list->m_native->SetGraphicsRootConstantBufferView(root_parameter_idx, buffer->m_gpu_addresses[frame_idx]);
	}

	void BindComputeConstantBuffer(CommandList * cmd_list, HeapResource* buffer, unsigned int root_parameter_idx, unsigned int frame_idx)
	{
		cmd_list->m_native->SetComputeRootConstantBufferView(root_parameter_idx, 
			buffer->m_gpu_addresses[frame_idx]);
	}

	void BindDescriptorTable(CommandList* cmd_list, DescHeapGPUHandle& handle, unsigned int root_param_index)
	{
		cmd_list->m_native->SetGraphicsRootDescriptorTable(root_param_index, handle.m_native);
	}

	void BindComputeDescriptorTable(CommandList * cmd_list, DescHeapGPUHandle & handle, unsigned int root_param_index)
	{
		cmd_list->m_native->SetComputeRootDescriptorTable(root_param_index, handle.m_native);
	}

	void SetPrimitiveTopology(CommandList* cmd_list, D3D12_PRIMITIVE_TOPOLOGY topology)
	{
		cmd_list->m_native->IASetPrimitiveTopology(topology);
	}

	void Draw(CommandList* cmd_list, unsigned int vertex_count, unsigned int inst_count, unsigned int vertex_start)
	{
		cmd_list->m_native->DrawInstanced(vertex_count, inst_count, vertex_start, 0);
	}

	void DrawIndexed(CommandList* cmd_list, unsigned int idx_count, unsigned int inst_count, unsigned int idx_start, unsigned int vertex_start)
	{
		cmd_list->m_native->DrawIndexedInstanced(idx_count, inst_count, idx_start, vertex_start, 0);
	}

	void Dispatch(CommandList * cmd_list, unsigned int thread_group_count_x, unsigned int thread_group_count_y, unsigned int thread_group_count_z)
	{
		cmd_list->m_native->Dispatch(thread_group_count_x, thread_group_count_y, thread_group_count_z);
	}

	void Transition(CommandList* cmd_list, RenderTarget* render_target, unsigned int frame_index, ResourceState from, ResourceState to)
	{
		CD3DX12_RESOURCE_BARRIER end_transition = CD3DX12_RESOURCE_BARRIER::Transition(
			render_target->m_render_targets[frame_index % render_target->m_render_targets.size()],
			(D3D12_RESOURCE_STATES)from,
			(D3D12_RESOURCE_STATES)to
		);

		cmd_list->m_native->ResourceBarrier(1, &end_transition);
	}

	void Transition(CommandList* cmd_list, RenderTarget* render_target, ResourceState from, ResourceState to)
	{
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
		barriers.resize(render_target->m_num_render_targets);
		for (auto i = 0; i < render_target->m_num_render_targets; i++)
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				render_target->m_render_targets[i],
				(D3D12_RESOURCE_STATES)from,
				(D3D12_RESOURCE_STATES)to
			);

			barriers[i] = barrier;
		}
		cmd_list->m_native->ResourceBarrier(barriers.size(), barriers.data());
	}

	void Transition(CommandList* cmd_list, std::vector<TextureResource*> const & textures, ResourceState from, ResourceState to)
	{
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

		for (auto i = 0; i < textures.size(); i++)
		{
			if (textures[i]->m_current_state != to)
			{
				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					textures[i]->m_resource,
					(D3D12_RESOURCE_STATES)from,
					(D3D12_RESOURCE_STATES)to
				);

				textures[i]->m_current_state = to;

				barriers.push_back(barrier);
			}
		}
		cmd_list->m_native->ResourceBarrier(barriers.size(), barriers.data());
	}

	void TransitionDepth(CommandList* cmd_list, RenderTarget* render_target, ResourceState from, ResourceState to)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			render_target->m_depth_stencil_buffer,
			(D3D12_RESOURCE_STATES)from,
			(D3D12_RESOURCE_STATES)to
		);
		cmd_list->m_native->ResourceBarrier(1, &barrier);
	}

	void Transition(CommandList* cmd_list, IndirectCommandBuffer* buffer, ResourceState from, ResourceState to)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			buffer->m_native,
			(D3D12_RESOURCE_STATES)from,
			(D3D12_RESOURCE_STATES)to
		);
		cmd_list->m_native->ResourceBarrier(1, &barrier);
	}

	void Transition(CommandList* cmd_list, StagingBuffer* buffer, ResourceState from, ResourceState to)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			buffer->m_buffer,
			(D3D12_RESOURCE_STATES)from,
			(D3D12_RESOURCE_STATES)to
		);
		cmd_list->m_native->ResourceBarrier(1, &barrier);
	}

	void Destroy(CommandList* cmd_list)
	{
		SAFE_RELEASE(cmd_list->m_native);
		SAFE_RELEASE(cmd_list->m_native_fallback)
		for (auto& allocator : cmd_list->m_allocators)
		{
			SAFE_RELEASE(allocator);
		}
		delete cmd_list;
	}

	CommandSignature* CreateCommandSignature(Device* device, RootSignature* root_signature, std::vector<D3D12_INDIRECT_ARGUMENT_DESC> arg_descs, size_t byte_stride)
	{
		auto cmd_sig = new CommandSignature();

		D3D12_COMMAND_SIGNATURE_DESC cmd_signature_desc = {};
		cmd_signature_desc.pArgumentDescs = arg_descs.data();
		cmd_signature_desc.NumArgumentDescs = arg_descs.size();
		cmd_signature_desc.ByteStride = byte_stride;

		TRY_M(device->m_native->CreateCommandSignature(&cmd_signature_desc, root_signature->m_native, IID_PPV_ARGS(&cmd_sig->m_native))
			, "Failed to create command signature");

		return cmd_sig;
	}

	void DispatchRays(CommandList* cmd_list)
	{
		//cmd_list->
	}

	void Destroy(CommandSignature* cmd_signature)
	{
		SAFE_RELEASE(cmd_signature->m_native);
	}

} /* wr::d3d12 */