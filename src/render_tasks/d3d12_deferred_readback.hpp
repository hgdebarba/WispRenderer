#pragma once

#include "../d3d12/d3d12_structs.hpp"
#include "../d3d12/d3d12_renderer.hpp"
#include "../d3d12/d3d12_functions.hpp"
#include "../frame_graph/frame_graph.hpp"

namespace wr
{
	struct RenderTargetReadBackTaskData
	{
		// Render target of the previous render task (should be the output from the composition task
		d3d12::RenderTarget* predecessor_render_target;

		// Read back buffer used to retrieve the pixel data on the GPU
		d3d12::ReadbackBufferResource* readback_buffer;

		d3d12::desc::ReadbackDesc readback_buffer_desc;
	};

	namespace internal
	{
		template<typename T>
		inline void SetupReadBackTask(RenderSystem& rs, FrameGraph& fg, RenderTaskHandle handle, void* out_buffer_cpu_location, std::uint64_t& out_buffer_size)
		{
			auto& data = fg.GetData<RenderTargetReadBackTaskData>(handle);
			auto& dx12_render_system = static_cast<D3D12RenderSystem&>(rs);

			// Save the previous render target for use in the execute function
			data.predecessor_render_target = static_cast<d3d12::RenderTarget*>(fg.GetPredecessorRenderTarget<T>());

			// Information about the previous render target
			wr::d3d12::desc::RenderTargetDesc predecessor_rt_desc = data.predecessor_render_target->m_create_info;

			// Bytes per pixel of the previous render target
			unsigned int bytesPerPixel = BytesPerPixel(predecessor_rt_desc.m_rtv_formats[0]);

			// Information for creating the read back buffer object
			data.readback_buffer_desc = {};
			data.readback_buffer_desc.m_buffer_size = dx12_render_system.m_viewport.m_viewport.Width * dx12_render_system.m_viewport.m_viewport.Height * bytesPerPixel;

			// Allows external code to retrieve the read back buffer size
			out_buffer_size = data.readback_buffer_desc.m_buffer_size;
			
			// Create the actual read back buffer
			data.readback_buffer = d3d12::CreateReadbackBuffer(dx12_render_system.m_device, &data.readback_buffer_desc);

			// Keep it mapped for the lifetime of the render task
			out_buffer_cpu_location = MapReadbackBuffer(data.readback_buffer, data.readback_buffer_desc.m_buffer_size);
 		}

		inline void ExecuteReadBackTask(RenderSystem& render_system, FrameGraph& frame_graph, SceneGraph& scene_graph, RenderTaskHandle handle, void* out_buffer_cpu_location)
		{
			auto& dx12_render_system = static_cast<D3D12RenderSystem&>(render_system);
			auto& data = frame_graph.GetData<RenderTargetReadBackTaskData>(handle);
			auto command_list = frame_graph.GetCommandList<d3d12::CommandList>(handle);

			// The render task sets the execution resource state to COPY_SOURCE, so the resource copy can take place without any additional transitions
			//command_list->m_native->CopyResource(data.readback_buffer->m_resource, data.predecessor_render_target->m_render_targets[0]);
			
			D3D12_SUBRESOURCE_FOOTPRINT footprint = {};
			footprint.Width = 1280;
			footprint.Height = 720;
			footprint.Depth = 1;
			footprint.RowPitch = 5120;
			footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_res_footprint = {};
			placed_res_footprint.Offset = 0;
			placed_res_footprint.Footprint = footprint;

			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = data.readback_buffer->m_resource;
			dst.Type = D3D12_TEXTURE_COPY_TYPE::D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			dst.SubresourceIndex = 0;
			dst.PlacedFootprint = placed_res_footprint;

			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = data.predecessor_render_target->m_render_targets[0];
			src.Type = D3D12_TEXTURE_COPY_TYPE::D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			src.SubresourceIndex = 0;

			command_list->m_native->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

			// Ensure the cache is coherent by calling map again, this is free on systems that do not need it, but
			// ensures correctness on systems which do need the map call.
			// Section 3.4:
			// https://software.intel.com/en-us/articles/tutorial-migrating-your-apps-to-directx-12-part-3
			out_buffer_cpu_location = MapReadbackBuffer(data.readback_buffer, data.readback_buffer_desc.m_buffer_size);
		}
		
		inline void DestroyReadBackTask(FrameGraph& fg, RenderTaskHandle handle)
		{
			// Data used for this render task
			auto& data = fg.GetData<RenderTargetReadBackTaskData>(handle);

			// Clean up the mapping
			UnmapReadbackBuffer(data.readback_buffer);

			// Clean up the read back buffer
			Destroy(data.readback_buffer);
		}

	} /* internal */

	template<typename T>
	inline void AddRenderTargetReadBackTask(FrameGraph& frame_graph, std::optional<unsigned int> target_width, std::optional<unsigned int> target_height, void* out_buffer_cpu_location, std::uint64_t& out_buffer_size)
	{
		// This is the same as the composition task, as this task should not change anything of the buffer that comes
		// into the task. It just copies the data to the read back buffer and leaves the render target be.
		RenderTargetProperties rt_properties{
			false,							// This pass does not use the render window
			target_width,					// Width of the frame
			target_height,					// Height of the frame
			ResourceState::COPY_SOURCE,		// Execution state
			ResourceState::RENDER_TARGET,	// Finished state
			false,							// Do not create a depth-stencil-view
			Format::UNKNOWN,				// Depth-stencil-view format
			{ Format::R8G8B8A8_UNORM },		// Render target array containing all formats for this render target
			1,								// Number of render target formats
			true,							// Clear flag
			true							// Clear depth flag
		};

		// Render task information
		RenderTaskDesc readback_task_description;

		// Set-up
		readback_task_description.m_setup_func = [&](RenderSystem& render_system, FrameGraph& frame_graph, RenderTaskHandle render_task_handle, bool) {
			internal::SetupReadBackTask<T>(render_system, frame_graph, render_task_handle, out_buffer_cpu_location, out_buffer_size);
		};

		// Execution
		readback_task_description.m_execute_func = [&](RenderSystem& render_system, FrameGraph& frame_graph, SceneGraph& scene_graph, RenderTaskHandle handle) {
			internal::ExecuteReadBackTask(render_system, frame_graph, scene_graph, handle, out_buffer_cpu_location);
		};

		// Destruction and clean-up
		readback_task_description.m_destroy_func = [](FrameGraph& frame_graph, RenderTaskHandle handle, bool) {
			internal::DestroyReadBackTask(frame_graph, handle);
		};

		readback_task_description.m_name = std::string(std::string("Render Target (") + std::string(typeid(T).name()) + std::string(") read-back task")).c_str();
		readback_task_description.m_properties = rt_properties;
		readback_task_description.m_type = RenderTaskType::COPY;
		readback_task_description.m_allow_multithreading = false;

		// Save this task to the frame graph system
		frame_graph.AddTask<RenderTargetReadBackTaskData>(readback_task_description);
	}

} /* wr */
