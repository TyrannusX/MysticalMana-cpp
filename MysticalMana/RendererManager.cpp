/*
* Macros to define the following:
* 
* Force GLFW to expose native window handle.
* Hide Windows Header functionality.
*/
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 1
#define NOMINMAX
#elif _WIN64
#define GLFW_EXPOSE_NATIVE_WIN32 1
#define NOMINMAX
#elif __linux__
#define GLFW_EXPOSE_NATIVE_X11 1
#endif

/*
* Define macros for Diligent Engine
*/
#ifdef _WIN32
#define PLATFORM_WIN32 1
#elif _WIN64
#define PLATFORM_WIN32 1
#elif __linux__
#define PLATFORM_LINUX 1
#endif

#include <iostream>
#include "RendererManager.h"
#include "Vertex.h"
#include "UniformConstants.h"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <fstream>
#include <exception>

int modifier = 1;
float scale_modifier = 1.0f;
bool reverse_scale = false;

RendererManager::RendererManager(MysticalMana::Window *window)
{
	//Get underlying window and details
	GLFWwindow *underlying_window = window->GetUnderlyingWindow();

	int window_width;
	int window_height;
	glfwGetWindowSize(underlying_window, &window_width, &window_height);

//Get underlying OS window
#ifdef _WIN32
	HWND native_os_window = glfwGetWin32Window(underlying_window);
#elif _WIN64
	HWND native_os_window = glfwGetWin32Window(underlying_window);
#elif __linux__
	auto native_os_window = glfwGetX11Window(underlying_window);
	auto native_os_display = glfwGetX11Display();
#endif

	m_view_width_ = (uint16_t)window_width;
	m_view_height_ = (uint16_t)window_height;

	//Initialize Vulkan Device, Context, and Swap Chain
	Diligent::IEngineFactoryVk *engine_factory = Diligent::GetEngineFactoryVk();
	Diligent::EngineVkCreateInfo create_info;
	engine_factory->CreateDeviceAndContextsVk(create_info, &m_render_device_, &m_immediate_context_);
	Diligent::SwapChainDesc swap_chain_description;

//Setup the native window handle for Diligent Engine
#ifdef _WIN32
	Diligent::Win32NativeWindow diligent_native_window_handle{native_os_window};
	engine_factory->CreateSwapChainVk(m_render_device_, m_immediate_context_, swap_chain_description, diligent_native_window_handle, &m_swap_chain_);
#elif _WIN64
	Diligent::Win32NativeWindow diligent_native_window_handle{native_os_window};
	engine_factory->CreateSwapChainVk(m_render_device_, m_immediate_context_, swap_chain_description, diligent_native_window_handle, &m_swap_chain_);
#elif __linux__
	Diligent::LinuxNativeWindow diligent_native_window_handle;
	diligent_native_window_handle.WindowId = native_os_window;
	diligent_native_window_handle.pDisplay = native_os_display;
	engine_factory->CreateSwapChainVk(m_render_device_, m_immediate_context_, swap_chain_description, diligent_native_window_handle, &m_swap_chain_);
#endif

	//Initialize graphics/compute pipeline
	Diligent::GraphicsPipelineStateCreateInfo pipeline_create_info;

	//Metadata for the pipeline
	pipeline_create_info.PSODesc.Name = "Mystical Mana PSO";
	pipeline_create_info.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;

	//Define the output (screen in this case)
	pipeline_create_info.GraphicsPipeline.NumRenderTargets = 1;
	pipeline_create_info.GraphicsPipeline.RTVFormats[0] = m_swap_chain_->GetDesc().ColorBufferFormat;
	pipeline_create_info.GraphicsPipeline.DSVFormat = m_swap_chain_->GetDesc().DepthBufferFormat;

	//Define the amount of multisampling
	pipeline_create_info.GraphicsPipeline.SmplDesc.Count = 1;

	//Define the types of primitives the pipeline will output (triangles in this case)
	pipeline_create_info.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	/*
	* Define the ratzerizer stage's culling method.
	* Culling will trim parts of objects that dont
	* appear in the projection matrix.
	*
	* CULL_MODE_NONE -> draw everything.
	* CULL_MODE_BACK -> dont draw what's behind something else.
	*/
	pipeline_create_info.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_BACK;

	/*
	* Set the depth test flag.
	* If enabled, fragments are discarded if they
	* are drawn behind another fragment.
	*/
	pipeline_create_info.GraphicsPipeline.DepthStencilDesc.DepthEnable = 1;

	//Create a shader source stream factory to read shaders from file
	Diligent::RefCntAutoPtr<Diligent::IShaderSourceInputStreamFactory> stream_factory;
	engine_factory->CreateDefaultShaderSourceStreamFactory(nullptr, &stream_factory);

	//Vertex shader handle
	Diligent::ShaderCreateInfo shader_create_info;
	shader_create_info.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
	shader_create_info.pShaderSourceStreamFactory = stream_factory;
	Diligent::RefCntAutoPtr<Diligent::IShader> vertex_shader_handle;
	shader_create_info.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
	shader_create_info.EntryPoint = "main";
	shader_create_info.Desc.Name = "Mystical Mana Vertex Shader";
	shader_create_info.FilePath = "Debug/vertex.hlsl";
	m_render_device_->CreateShader(shader_create_info, &vertex_shader_handle);

	//Pixel/Fragment shader handle
	Diligent::RefCntAutoPtr<Diligent::IShader> pixel_shader_handle;
	shader_create_info.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
	shader_create_info.EntryPoint = "main";
	shader_create_info.Desc.Name = "Mystical Mana Pixel Shader";
	shader_create_info.FilePath = "Debug/fragment.hlsl";
	m_render_device_->CreateShader(shader_create_info, &pixel_shader_handle);

	//Define uniform buffer.
	Diligent::BufferDesc buffer_description;
	buffer_description.Name = "Mystical Mana Vertex Shader Constants";
	buffer_description.Size = sizeof(Diligent::float4x4);
	buffer_description.Usage = Diligent::USAGE_DYNAMIC;
	buffer_description.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
	buffer_description.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
	m_render_device_->CreateBuffer(buffer_description, nullptr, &m_uniform_buffer_);

	/*
	* Define the layout of the data being passed through
	* the vertex buffer. Basically, define metadata.
	*/
	Diligent::LayoutElement layout_elements[] =
		{
			//Attribute 0 is the vertex position
			Diligent::LayoutElement{
				0,					  //Attribute number that the shader will pull from its struct
				0,					  //Buffer slot (defaults to 0)
				3,					  //Number of components (position is x,y,z
				Diligent::VT_FLOAT32, //component value type
				0					  //Normalized?
			},

			//Attribute 2 is the texture coordinate
			Diligent::LayoutElement{
				1,					  //Attribute number that the shader will pull from its struct
				0,					  //Buffer slot (defaults to 0)
				2,					  //Number of components (position is x,y,z
				Diligent::VT_FLOAT32, //component value type
				0					  //Normalized?
			}};
	pipeline_create_info.GraphicsPipeline.InputLayout.LayoutElements = layout_elements;
	pipeline_create_info.GraphicsPipeline.InputLayout.NumElements = _countof(layout_elements);

	//Bind shaders to pipeline
	pipeline_create_info.pVS = vertex_shader_handle;
	pipeline_create_info.pPS = pixel_shader_handle;

	//Define the default shader variable type
	pipeline_create_info.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

	/*
	* Define shader variables
	*/
	Diligent::ShaderResourceVariableDesc shader_variables[] =
		{
			{Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
	pipeline_create_info.PSODesc.ResourceLayout.Variables = shader_variables;
	pipeline_create_info.PSODesc.ResourceLayout.NumVariables = _countof(shader_variables);

	/*
	* Define the immutable sampler for g_Texture in the shader.
	* This creates the anisotropic filter, which basically smooths the appearance of polygons.
	*/
	Diligent::SamplerDesc linear_sample{
		Diligent::FILTER_TYPE_LINEAR,
		Diligent::FILTER_TYPE_LINEAR,
		Diligent::FILTER_TYPE_LINEAR,
		Diligent::TEXTURE_ADDRESS_CLAMP,
		Diligent::TEXTURE_ADDRESS_CLAMP,
		Diligent::TEXTURE_ADDRESS_CLAMP,
	};
	Diligent::ImmutableSamplerDesc immutable_samplers[] =
		{
			{Diligent::SHADER_TYPE_PIXEL, "g_Texture", linear_sample}};
	pipeline_create_info.PSODesc.ResourceLayout.ImmutableSamplers = immutable_samplers;
	pipeline_create_info.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutable_samplers);

	//Finally, pass in the pipeline create struct to create the pipeline
	m_render_device_->CreateGraphicsPipelineState(pipeline_create_info, &m_pipeline_state_);

	//Bind uniform buffer in thia application to the shader's constant variable (look at vertex shader)
	m_pipeline_state_->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->Set(m_uniform_buffer_);

	//Create the resource binder and bind all static resources to this (in this case the uniform buffer data)
	m_pipeline_state_->CreateShaderResourceBinding(&m_shader_resource_binder_, true);
}

RendererManager::~RendererManager()
{
}

void RendererManager::PaintNextFrame(StaticEntity &static_entity)
{
	//
	//Define the color to use for the render target background
	const float kClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};

	//Get handle to the render target (laptop screen) back buffer (drawing not yet shown to user) from the swap chain
	Diligent::ITextureView *render_target_handle = m_swap_chain_->GetCurrentBackBufferRTV();

	//Get handle to the depth buffer
	Diligent::ITextureView *depth_target_handle = m_swap_chain_->GetDepthBufferDSV();

	//Set the swap chain's render target (screen)
	m_immediate_context_->SetRenderTargets(1, &render_target_handle, depth_target_handle, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	//Clear buffers (think erasing a whiteboard to start next drawing)
	m_immediate_context_->ClearRenderTarget(render_target_handle, kClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	m_immediate_context_->ClearDepthStencil(depth_target_handle, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	/*
	* The MapHelper uses the uniform butter to
	* send data from the C++ application (any data)
	* to a match value found in the shader CBuffer constant data.
	*/
	Diligent::MapHelper<Diligent::float4x4> uniform_buffer_constant_data(m_immediate_context_, m_uniform_buffer_, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
	*uniform_buffer_constant_data = m_world_view_projection_matrix_.Transpose();

	/*
	* Bind the vertex buffer of the static entity to the pipeline.
	* 
	* Diligent allows you to set a collection of vertex buffers.
	* Here we are setting one currently. We start at the beginning.
	* We use the specified transition mode to let Diligent transition 
	* buffers to their proper state automatically.
	* The final flag tells Diligent to reset/drop previous vertex buffers.
	*/
	const Diligent::Uint64 kOffset = 0;
	Diligent::IBuffer *buffers[] = {static_entity.m_vertex_buffer};
	m_immediate_context_->SetVertexBuffers(0, 1, buffers, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);

	/*
	* Bind index buffer
	*/
	/*m_immediate_context_->SetIndexBuffer(static_entity.m_index_buffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);*/

	//Set the immediate rendering context's graphics pipeline to use
	m_immediate_context_->SetPipelineState(m_pipeline_state_);

	/*
	* Commit shader resources using the resource binder.
	* 
	* This takes all shader related setup stuff housed internally in Diligent
	* and flushes it down to the graphics pipeline.
	* Think in terms of a DB commit in SQL.
	* Operations dont take affect until you commit
	*/
	m_immediate_context_->CommitShaderResources(m_shader_resource_binder_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	/*
	* Define the draw attributes
	* 
	* We want to draw using indexes and a specified number of them.
	* We also want the engine to verify the state of the vertex and index buffers.
	*/
	Diligent::DrawAttribs indexed_attributes;
	/*indexed_attributes.IndexType = Diligent::VT_UINT32;*/
	indexed_attributes.NumVertices = static_entity.m_vertices.size() * sizeof(Vertex);
	indexed_attributes.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

	//Use the immediate context to render the next frame (draws on the back buffer)
	m_immediate_context_->Draw(indexed_attributes);

	//Use the swap chain to flip the back buffer to the front buffer (front gets moved to back)
	m_swap_chain_->Present();
}

void RendererManager::UpdateWorld(Diligent::Vector3<float> cameraVector, Diligent::Vector3<float> cameraRotationVector)
{
	/*
	* Matrix calculation "recipe"
	* Final Matrix = Scale * Rotation * Translation
	*/

	//Calculate the world matrix which is where the rendered object will live relative to the origin (object space)
	Diligent::float4x4 world_matrix = Diligent::float4x4::Translation(0.0f, 0.0f, 0.0f);

	//Move the view (Camera/your eye/whatever) to desired spot in world
	Diligent::float4x4 camera_rotation_matrix = Diligent::float4x4::RotationZ(cameraRotationVector.z) * Diligent::float4x4::RotationY(cameraRotationVector.y) * Diligent::float4x4::RotationX(cameraRotationVector.x);
	Diligent::float4x4 camera_translation_matrix = Diligent::float4x4::Translation(cameraVector.x, cameraVector.y, cameraVector.z);
	Diligent::float4x4 camera_matrix = camera_rotation_matrix * camera_translation_matrix;

	//Get the projection matrix (projection is like messing with camera lens settings like zoom)
	Diligent::SwapChainDesc swap_chain_desc = m_swap_chain_->GetDesc();
	float aspectRatio = static_cast<float>(swap_chain_desc.Width) / static_cast<float>(swap_chain_desc.Height);
	float x_scale;
	float y_scale;
	if (swap_chain_desc.PreTransform == Diligent::SURFACE_TRANSFORM_ROTATE_90 ||
		swap_chain_desc.PreTransform == Diligent::SURFACE_TRANSFORM_ROTATE_270 ||
		swap_chain_desc.PreTransform == Diligent::SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90 ||
		swap_chain_desc.PreTransform == Diligent::SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270)
	{
		x_scale = 1.0f / std::tan((Diligent::PI_F / 4.0f) / 2.0f);
		y_scale = x_scale * aspectRatio;
	}
	else
	{
		y_scale = 1.0f / std::tan((Diligent::PI_F / 4.0f) / 2.0f);
		x_scale = y_scale / aspectRatio;
	}

	Diligent::float4x4 projection_matrix;
	projection_matrix._11 = x_scale;
	projection_matrix._22 = y_scale;
	projection_matrix.SetNearFarClipPlanes(0.1f, 100.0f, m_render_device_->GetDeviceInfo().IsGLDevice());

	//Calculate the world_view_projection_matrix
	m_world_view_projection_matrix_ = world_matrix * camera_matrix * projection_matrix;
}

Diligent::RefCntAutoPtr<Diligent::IBuffer> RendererManager::CreateVertexBuffer(StaticEntity &staticEntity)
{
	Diligent::BufferDesc buffer_description;
	buffer_description.Name = "Mystical Mana Vertex Buffer";
	buffer_description.Usage = Diligent::USAGE_IMMUTABLE;
	buffer_description.BindFlags = Diligent::BIND_VERTEX_BUFFER;
	buffer_description.Size = staticEntity.m_vertices.size() * sizeof(Vertex);

	Diligent::BufferData buffer_data;
	buffer_data.pData = staticEntity.m_vertices.data();
	buffer_data.DataSize = staticEntity.m_vertices.size() * sizeof(Vertex);

	Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
	m_render_device_->CreateBuffer(buffer_description, &buffer_data, &buffer);
	return buffer;
}

Diligent::RefCntAutoPtr<Diligent::IBuffer> RendererManager::CreateIndexBuffer(StaticEntity &staticEntity)
{
	Diligent::BufferDesc buffer_description;
	buffer_description.Name = "Mystical Mana Index Buffer";
	buffer_description.Usage = Diligent::USAGE_IMMUTABLE;
	buffer_description.BindFlags = Diligent::BIND_INDEX_BUFFER;
	buffer_description.Size = staticEntity.m_indices.size() * sizeof(Diligent::Uint32);

	Diligent::BufferData buffer_data;
	buffer_data.pData = staticEntity.m_indices.data();
	buffer_data.DataSize = staticEntity.m_indices.size() * sizeof(Diligent::Uint32);

	Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
	m_render_device_->CreateBuffer(buffer_description, &buffer_data, &buffer);
	return buffer;
}

Diligent::ITextureView *RendererManager::CreateTextureFromFile(Diligent::Char *texture_file_path)
{
	//Create textue from file
	Diligent::TextureLoadInfo texture_load_info;
	texture_load_info.IsSRGB = true;
	Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
	Diligent::CreateTextureFromFile(texture_file_path, texture_load_info, m_render_device_, &texture);

	//Get the shader resource view for the shader
	Diligent::ITextureView *texture_resource_view = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);

	//Use the shader resource binder to get and set the g_Texture variable in the pixel shader
	m_shader_resource_binder_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture")->Set(texture_resource_view);

	return texture_resource_view;
}