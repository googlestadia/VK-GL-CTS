/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 ARM Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief PushConstant Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelinePushConstantTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deMemory.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

enum
{
	TRIANGLE_COUNT	= 2,
	MAX_RANGE_COUNT	= 5
};

enum RangeSizeCase
{
	SIZE_CASE_4	= 0,
	SIZE_CASE_8,
	SIZE_CASE_12,
	SIZE_CASE_16,
	SIZE_CASE_32,
	SIZE_CASE_36,
	SIZE_CASE_48,
	SIZE_CASE_128,
	SIZE_CASE_UNSUPPORTED
};

struct PushConstantData
{
	struct PushConstantRange
	{
		VkShaderStageFlags		shaderStage;
		deUint32				offset;
		deUint32				size;
	} range;
	struct PushConstantUpdate
	{
		deUint32				offset;
		deUint32				size;
	} update;
};

// These values will be loaded from push constants and used as an index
static const deUint32 DYNAMIC_VEC_INDEX = 2u;
static const deUint32 DYNAMIC_MAT_INDEX = 0u;
static const deUint32 DYNAMIC_ARR_INDEX = 3u;

// These reference values will be compared in the shader to ensure the correct index was read
static const float DYNAMIC_VEC_CONSTANT = 0.25f;
static const float DYNAMIC_MAT_CONSTANT = 0.50f;
static const float DYNAMIC_ARR_CONSTANT = 0.75f;

enum IndexType
{
	INDEX_TYPE_CONST_LITERAL = 0,
	INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR,

	INDEX_TYPE_LAST
};

std::string getShaderStageNameStr (VkShaderStageFlags stageFlags)
{
	const VkShaderStageFlags	shaderStages[]		=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT
	};

	const char*					shaderStageNames[]	=
	{
		"VK_SHADER_STAGE_VERTEX_BIT",
		"VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT",
		"VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT",
		"VK_SHADER_STAGE_GEOMETRY_BIT",
		"VK_SHADER_STAGE_FRAGMENT_BIT",
	};

	std::stringstream			shaderStageStr;

	for (deUint32 stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(shaderStages); stageNdx++)
	{
		if (stageFlags & shaderStages[stageNdx])
		{
			if (!(shaderStageStr.str().empty()))
				shaderStageStr << " | ";

			shaderStageStr << shaderStageNames[stageNdx];
		}
	}

	return shaderStageStr.str();
}

class PushConstantGraphicsTestInstance : public vkt::TestInstance
{
public:
												PushConstantGraphicsTestInstance	(Context&					context,
																					 const deUint32				rangeCount,
																					 const PushConstantData		pushConstantRange[MAX_RANGE_COUNT],
																					 const deBool				multipleUpdate,
																					 const IndexType			indexType);
	virtual										~PushConstantGraphicsTestInstance	(void);
	void										init								(void);
	virtual tcu::TestStatus						iterate								(void);
	virtual std::vector<VkPushConstantRange>	getPushConstantRanges				(void) = 0;
	virtual void								updatePushConstants					(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) = 0;
	virtual void								setReferenceColor					(tcu::Vec4 initColor) = 0;
	void										createShaderModule					(const DeviceInterface&		vk,
																					 VkDevice					device,
																					 const BinaryCollection&	programCollection,
																					 const char*				name,
																					 Move<VkShaderModule>*		module);
	std::vector<Vertex4RGBA>					createQuad							(const float size);
	tcu::TestStatus								verifyImage							(void);

protected:
	std::vector<Vertex4RGBA>						m_vertices;
	const deUint32									m_rangeCount;
	PushConstantData								m_pushConstantRange[MAX_RANGE_COUNT];
	const IndexType									m_indexType;

private:
	const tcu::UVec2								m_renderSize;
	const VkFormat									m_colorFormat;
	const deBool									m_multipleUpdate;

	VkImageCreateInfo								m_colorImageCreateInfo;
	Move<VkImage>									m_colorImage;
	de::MovePtr<Allocation>							m_colorImageAlloc;
	Move<VkImageView>								m_colorAttachmentView;
	Move<VkRenderPass>								m_renderPass;
	Move<VkFramebuffer>								m_framebuffer;

	Move<VkShaderModule>							m_vertexShaderModule;
	Move<VkShaderModule>							m_fragmentShaderModule;
	Move<VkShaderModule>							m_geometryShaderModule;
	Move<VkShaderModule>							m_tessControlShaderModule;
	Move<VkShaderModule>							m_tessEvaluationShaderModule;

	VkShaderStageFlags								m_shaderFlags;
	std::vector<VkPipelineShaderStageCreateInfo>	m_shaderStage;

	Move<VkBuffer>									m_vertexBuffer;
	de::MovePtr<Allocation>							m_vertexBufferAlloc;

	Move<VkBuffer>									m_uniformBuffer;
	de::MovePtr<Allocation>							m_uniformBufferAlloc;
	Move<VkDescriptorPool>							m_descriptorPool;
	Move<VkDescriptorSetLayout>						m_descriptorSetLayout;
	Move<VkDescriptorSet>							m_descriptorSet;

	Move<VkPipelineLayout>							m_pipelineLayout;
	Move<VkPipeline>								m_graphicsPipelines;

	Move<VkCommandPool>								m_cmdPool;
	Move<VkCommandBuffer>							m_cmdBuffer;
};

void PushConstantGraphicsTestInstance::createShaderModule (const DeviceInterface&	vk,
														   VkDevice					device,
														   const BinaryCollection&	programCollection,
														   const char*				name,
														   Move<VkShaderModule>*	module)
{
	*module = vk::createShaderModule(vk, device, programCollection.get(name), 0);
}

std::vector<Vertex4RGBA> PushConstantGraphicsTestInstance::createQuad(const float size)
{
	std::vector<Vertex4RGBA>	vertices;

	const tcu::Vec4				color				= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	const Vertex4RGBA			lowerLeftVertex		= {tcu::Vec4(-size, -size, 0.0f, 1.0f), color};
	const Vertex4RGBA			lowerRightVertex	= {tcu::Vec4(size, -size, 0.0f, 1.0f), color};
	const Vertex4RGBA			UpperLeftVertex		= {tcu::Vec4(-size, size, 0.0f, 1.0f), color};
	const Vertex4RGBA			UpperRightVertex	= {tcu::Vec4(size, size, 0.0f, 1.0f), color};

	vertices.push_back(lowerLeftVertex);
	vertices.push_back(lowerRightVertex);
	vertices.push_back(UpperLeftVertex);
	vertices.push_back(UpperLeftVertex);
	vertices.push_back(lowerRightVertex);
	vertices.push_back(UpperRightVertex);

	return vertices;
}

PushConstantGraphicsTestInstance::PushConstantGraphicsTestInstance (Context&				context,
																	const deUint32			rangeCount,
																	const PushConstantData	pushConstantRange[MAX_RANGE_COUNT],
																	deBool					multipleUpdate,
																	IndexType				indexType)
	: vkt::TestInstance	(context)
	, m_rangeCount		(rangeCount)
	, m_indexType		(indexType)
	, m_renderSize		(32, 32)
	, m_colorFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_multipleUpdate	(multipleUpdate)
	, m_shaderFlags		(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
{
	deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

void PushConstantGraphicsTestInstance::init (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							vkDevice				= m_context.getDevice();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator							memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping				componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const std::vector<VkPushConstantRange>	pushConstantRanges		= getPushConstantRanges();
	bool									useTessellation			= false;
	bool									useGeometry				= false;

	// Create color image
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			m_colorFormat,															// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout			initialLayout;
		};

		m_colorImageCreateInfo	= colorImageParams;
		m_colorImage			= createImage(vk, vkDevice, &m_colorImageCreateInfo);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkImageViewCreateFlags		flags;
			*m_colorImage,									// VkImage						image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType				viewType;
			m_colorFormat,									// VkFormat						format;
			componentMappingRGBA,							// VkChannelMapping				channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange		subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, m_colorFormat);

	// Create framebuffer
	{
		const VkImageView attachmentBindInfos[1] =
		{
		  *m_colorAttachmentView
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkFramebufferCreateFlags		flags;
			*m_renderPass,								// VkRenderPass					renderPass;
			1u,											// deUint32						attachmentCount;
			attachmentBindInfos,						// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),					// deUint32						width;
			(deUint32)m_renderSize.y(),					// deUint32						height;
			1u											// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		// create descriptor set layout
		m_descriptorSetLayout = DescriptorSetLayoutBuilder().addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build(vk, vkDevice);

		// create descriptor pool
		m_descriptorPool = DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u).build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		// create uniform buffer
		const VkBufferCreateInfo			uniformBufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags
			16u,									// VkDeviceSize			size;
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyCount;
			&queueFamilyIndex						// const deUint32*		pQueueFamilyIndices;
		};

		m_uniformBuffer			= createBuffer(vk, vkDevice, &uniformBufferCreateInfo);
		m_uniformBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_uniformBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_uniformBuffer, m_uniformBufferAlloc->getMemory(), m_uniformBufferAlloc->getOffset()));

		const tcu::Vec4						value					= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
		deMemcpy(m_uniformBufferAlloc->getHostPtr(), &value, 16u);
		invalidateAlloc(vk, vkDevice, *m_uniformBufferAlloc);

		// create and update descriptor set
		const VkDescriptorSetAllocateInfo	allocInfo				=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                             sType;
			DE_NULL,										// const void*                                 pNext;
			*m_descriptorPool,								// VkDescriptorPool                            descriptorPool;
			1u,												// deUint32                                    setLayoutCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*                pSetLayouts;
		};
		m_descriptorSet	= allocateDescriptorSet(vk, vkDevice, &allocInfo);

		const VkDescriptorBufferInfo		descriptorInfo			= makeDescriptorBufferInfo(*m_uniformBuffer, (VkDeviceSize)0u, (VkDeviceSize)16u);

		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorInfo)
			.update(vk, vkDevice);

		// create pipeline layout
		const VkPipelineLayoutCreateInfo	pipelineLayoutParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkPipelineLayoutCreateFlags	flags;
			1u,												// deUint32						descriptorSetCount;
			&(*m_descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
			(deUint32)pushConstantRanges.size(),			// deUint32						pushConstantRangeCount;
			&pushConstantRanges.front()						// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
		{
			if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_GEOMETRY_BIT)
			{
				m_shaderFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
			}
			if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			{
				m_shaderFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			}
			if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			{
				m_shaderFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			}
		}

		VkPhysicalDeviceFeatures features = m_context.getDeviceFeatures();

		createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_vert", &m_vertexShaderModule);
		if (m_shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || m_shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		{
			if (features.tessellationShader == VK_FALSE)
			{
				TCU_THROW(NotSupportedError, "Tessellation Not Supported");
			}
			createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_tesc", &m_tessControlShaderModule);
			createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_tese", &m_tessEvaluationShaderModule);
			useTessellation = true;
		}
		if (m_shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			if (features.geometryShader == VK_FALSE)
			{
				TCU_THROW(NotSupportedError, "Geometry Not Supported");
			}
			createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_geom", &m_geometryShaderModule);
			useGeometry = true;
		}
		createShaderModule(vk, vkDevice, m_context.getBinaryCollection(), "color_frag", &m_fragmentShaderModule);
	}

	// Create pipeline
	{
		const VkVertexInputBindingDescription			vertexInputBindingDescription		=
		{
			0u,							// deUint32					binding;
			sizeof(Vertex4RGBA),		// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	stepRate;
		};

		const VkVertexInputAttributeDescription			vertexInputAttributeDescriptions[]	=
		{
			{
				0u,								// deUint32	location;
				0u,								// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat	format;
				0u								// deUint32	offsetInBytes;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// vkPipelineVertexInputStateCreateFlags	flags;
			1u,															// deUint32									bindingCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,															// deUint32									attributeCount;
			vertexInputAttributeDescriptions							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPrimitiveTopology						topology							= (m_shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		const std::vector<VkViewport>					viewports							(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>						scissors							(1, makeRect2D(m_renderSize));

		m_graphicsPipelines = makeGraphicsPipeline(vk,															// const DeviceInterface&                        vk
												   vkDevice,													// const VkDevice                                device
												   *m_pipelineLayout,											// const VkPipelineLayout                        pipelineLayout
												   *m_vertexShaderModule,										// const VkShaderModule                          vertexShaderModule
												   useTessellation ? *m_tessControlShaderModule : DE_NULL,		// const VkShaderModule                          tessellationControlShaderModule
												   useTessellation ? *m_tessEvaluationShaderModule : DE_NULL,	// const VkShaderModule                          tessellationEvalShaderModule
												   useGeometry ? *m_geometryShaderModule : DE_NULL,				// const VkShaderModule                          geometryShaderModule
												   *m_fragmentShaderModule,										// const VkShaderModule                          fragmentShaderModule
												   *m_renderPass,												// const VkRenderPass                            renderPass
												   viewports,													// const std::vector<VkViewport>&                viewports
												   scissors,													// const std::vector<VkRect2D>&                  scissors
												   topology,													// const VkPrimitiveTopology                     topology
												   0u,															// const deUint32                                subpass
												   3u,															// const deUint32                                patchControlPoints
												   &vertexInputStateParams);									// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
	}

	// Create vertex buffer
	{
		m_vertices			= createQuad(1.0f);

		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			0u,															// VkBufferCreateFlags	flags;
			(VkDeviceSize)(sizeof(Vertex4RGBA) * m_vertices.size()),	// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,							// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode;
			1u,															// deUint32				queueFamilyCount;
			&queueFamilyIndex											// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		invalidateAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);

		// Update push constant values
		updatePushConstants(*m_cmdBuffer, *m_pipelineLayout);

		// draw quad
		const VkDeviceSize				triangleOffset			= (m_vertices.size() / TRIANGLE_COUNT) * sizeof(Vertex4RGBA);

		for (int triangleNdx = 0; triangleNdx < TRIANGLE_COUNT; triangleNdx++)
		{
			VkDeviceSize vertexBufferOffset = triangleOffset * triangleNdx;

			if (m_multipleUpdate)
				vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, m_pushConstantRange[0].range.shaderStage, m_pushConstantRange[0].range.offset, m_pushConstantRange[0].range.size, &triangleNdx);

			vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines);
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &(*m_descriptorSet), 0, DE_NULL);

			vk.cmdDraw(*m_cmdBuffer, (deUint32)(m_vertices.size() / TRIANGLE_COUNT), 1, 0, 0);
		}

		endRenderPass(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

PushConstantGraphicsTestInstance::~PushConstantGraphicsTestInstance (void)
{
}

tcu::TestStatus PushConstantGraphicsTestInstance::iterate (void)
{
	init();

	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus PushConstantGraphicsTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat	= tcu::TextureFormat();
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk		= false;

	// Render reference image
	{
		if (m_shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			m_vertices = createQuad(0.5f);
		}

		setReferenceColor(m_vertices[0].color);

		if (m_multipleUpdate)
		{
			for (size_t vertexNdx = 0; vertexNdx < 3; vertexNdx++)
			{
				m_vertices[vertexNdx].color.xyz() = tcu::Vec3(0.0f, 1.0f, 0.0f);
			}
			for (size_t vertexNdx = 3; vertexNdx < m_vertices.size(); vertexNdx++)
			{
				m_vertices[vertexNdx].color.xyz() = tcu::Vec3(0.0f, 0.0f, 1.0f);
			}
		}

		for (int triangleNdx = 0; triangleNdx < TRIANGLE_COUNT; triangleNdx++)
		{
			rr::RenderState renderState(refRenderer.getViewportState());

			refRenderer.draw(renderState,
							 rr::PRIMITIVETYPE_TRIANGLES,
							 std::vector<Vertex4RGBA>(m_vertices.begin() + triangleNdx * 3,
													  m_vertices.begin() + (triangleNdx + 1) * 3));
		}
	}

	// Compare result with reference image
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					vkDevice			= m_context.getDevice();
		const VkQueue					queue				= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator					allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::MovePtr<tcu::TextureLevel>	result				= readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

class PushConstantGraphicsDisjointInstance : public PushConstantGraphicsTestInstance
{
public:
										PushConstantGraphicsDisjointInstance	(Context&					context,
																				 const deUint32				rangeCount,
																				 const PushConstantData		pushConstantRange[MAX_RANGE_COUNT],
																				 const deBool				multipleUpdate,
																				 const IndexType			indexType);
	virtual								~PushConstantGraphicsDisjointInstance	(void);
	std::vector<VkPushConstantRange>	getPushConstantRanges					(void);
	void								updatePushConstants						(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
	void								setReferenceColor						(tcu::Vec4 initColor);
};


PushConstantGraphicsDisjointInstance::PushConstantGraphicsDisjointInstance (Context&					context,
																			const deUint32				rangeCount,
																			const PushConstantData		pushConstantRange[MAX_RANGE_COUNT],
																			deBool						multipleUpdate,
																			IndexType					indexType)
	: PushConstantGraphicsTestInstance (context, rangeCount, pushConstantRange, multipleUpdate, indexType)
{
	deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

PushConstantGraphicsDisjointInstance::~PushConstantGraphicsDisjointInstance(void)
{
}

std::vector<VkPushConstantRange> PushConstantGraphicsDisjointInstance::getPushConstantRanges (void)
{
	std::vector<VkPushConstantRange> pushConstantRanges;

	for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
	{
		const VkPushConstantRange pushConstantRange =
		{
			m_pushConstantRange[rangeNdx].range.shaderStage,
			m_pushConstantRange[rangeNdx].range.offset,
			m_pushConstantRange[rangeNdx].range.size
		};

		pushConstantRanges.push_back(pushConstantRange);
	}

	return pushConstantRanges;
}

void PushConstantGraphicsDisjointInstance::updatePushConstants (VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	std::vector<tcu::Vec4>	color	(8, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	std::vector<tcu::Vec4>	allOnes	(8, tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	switch (m_indexType)
	{
		case INDEX_TYPE_CONST_LITERAL:
			// Do nothing
			break;
		case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
			// Stick our dynamic index at the beginning of a vector
			color[0] = tcu::Vec4(	float(DYNAMIC_VEC_INDEX),
									float(DYNAMIC_MAT_INDEX),
									float(DYNAMIC_ARR_INDEX),
									1.0f);

			// Place our reference values at each type offset

			// vec4[i]
			DE_ASSERT(DYNAMIC_VEC_INDEX <= 3);
			color[1] = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			color[1][DYNAMIC_VEC_INDEX] = DYNAMIC_VEC_CONSTANT;

			// mat2[i][0]
			DE_ASSERT(DYNAMIC_MAT_INDEX <= 1);
			color[2] = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			color[2][DYNAMIC_MAT_INDEX * 2] = DYNAMIC_MAT_CONSTANT;

			// float[i]
			DE_ASSERT(DYNAMIC_ARR_INDEX <= 3);
			color[3] = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			color[3][DYNAMIC_ARR_INDEX] = DYNAMIC_ARR_CONSTANT;
			break;
		default:
			DE_FATAL("Unhandled IndexType");
			break;
	}

	const deUint32			kind	= 2u;
	const void*				value	= DE_NULL;

	for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
	{
		value = (m_pushConstantRange[rangeNdx].range.size == 4u) ? (void*)(&kind) : (void*)(&color[0]);

		vk.cmdPushConstants(cmdBuffer, pipelineLayout, m_pushConstantRange[rangeNdx].range.shaderStage, m_pushConstantRange[rangeNdx].range.offset, m_pushConstantRange[rangeNdx].range.size, value);

		if (m_pushConstantRange[rangeNdx].update.size < m_pushConstantRange[rangeNdx].range.size)
		{
			value = (void*)(&allOnes[0]);
			vk.cmdPushConstants(cmdBuffer, pipelineLayout, m_pushConstantRange[rangeNdx].range.shaderStage, m_pushConstantRange[rangeNdx].update.offset, m_pushConstantRange[rangeNdx].update.size, value);
		}
	}
}

void PushConstantGraphicsDisjointInstance::setReferenceColor (tcu::Vec4 initColor)
{
	DE_UNREF(initColor);

	const tcu::Vec4 color = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
	{
		if (m_pushConstantRange[rangeNdx].update.size < m_pushConstantRange[rangeNdx].range.size)
		{
			for (size_t vertexNdx = 0; vertexNdx < m_vertices.size(); vertexNdx++)
			{
				m_vertices[vertexNdx].color.xyzw() = color;
			}
		}
	}
}

class PushConstantGraphicsOverlapTestInstance : public PushConstantGraphicsTestInstance
{
public:
										PushConstantGraphicsOverlapTestInstance		(Context&					context,
																				const deUint32				rangeCount,
																				const PushConstantData		pushConstantRange[MAX_RANGE_COUNT],
																				const deBool				multipleUpdate,
																				const IndexType			indexType);
	virtual								~PushConstantGraphicsOverlapTestInstance	(void);
	std::vector<VkPushConstantRange>	getPushConstantRanges					(void);
	std::vector<VkPushConstantRange>	getPushConstantUpdates					(void);
	void								updatePushConstants						(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
	void								setReferenceColor						(tcu::Vec4 initColor);

private:
	const std::vector<float>			m_colorData;
	std::vector<float>					m_referenceData;
};

std::vector<float> generateColorData (deUint32 numBytes)
{
	DE_ASSERT(numBytes % 4u == 0u);

	std::vector<float> colorData;

	deRandom random;
	deRandom_init(&random, numBytes);

	for (deUint32 elementNdx = 0u; elementNdx < numBytes / 4u; elementNdx++)
		colorData.push_back(deRandom_getFloat(&random));

	return colorData;
}

PushConstantGraphicsOverlapTestInstance::PushConstantGraphicsOverlapTestInstance (Context&					context,
																				  const deUint32			rangeCount,
																				  const PushConstantData	pushConstantRange[MAX_RANGE_COUNT],
																				  deBool					multipleUpdate,
																				  IndexType					indexType)
	: PushConstantGraphicsTestInstance	(context, rangeCount, pushConstantRange, multipleUpdate, indexType)
	, m_colorData						(generateColorData(256u))
{
	deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

PushConstantGraphicsOverlapTestInstance::~PushConstantGraphicsOverlapTestInstance(void)
{
}

std::vector<VkPushConstantRange> PushConstantGraphicsOverlapTestInstance::getPushConstantRanges (void)
{
	// Find push constant ranges for each shader stage
	const VkShaderStageFlags			shaderStages[]		=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	std::vector<VkPushConstantRange>	pushConstantRanges;

	m_context.getTestContext().getLog() << tcu::TestLog::Section("Ranges", "Push constant ranges");

	for (deUint32 stageNdx = 0u; stageNdx < DE_LENGTH_OF_ARRAY(shaderStages); stageNdx++)
	{
		deUint32 firstByte	= ~0u;
		deUint32 lastByte	= 0u;

		for (deUint32 rangeNdx = 0u; rangeNdx < m_rangeCount; rangeNdx++)
		{
			if (m_pushConstantRange[rangeNdx].range.shaderStage & shaderStages[stageNdx])
			{
				firstByte	= deMinu32(firstByte, m_pushConstantRange[rangeNdx].range.offset);
				lastByte	= deMaxu32(lastByte, m_pushConstantRange[rangeNdx].range.offset + m_pushConstantRange[rangeNdx].range.size);
			}
		}

		if (firstByte != ~0u)
		{
			const VkPushConstantRange pushConstantRange =
			{
				shaderStages[stageNdx],	// VkShaderStageFlags    stageFlags
				firstByte,				// deUint32              offset
				lastByte - firstByte	// deUint32              size
			};

			pushConstantRanges.push_back(pushConstantRange);

			m_context.getTestContext().getLog()
				<< tcu::TestLog::Message
				<< "VkShaderStageFlags    stageFlags    " << getShaderStageNameStr(shaderStages[stageNdx]) << ",\n"
				<< "deUint32              offset        " << pushConstantRange.offset << ",\n"
				<< "deUint32              size          " << pushConstantRange.size << "\n"
				<< tcu::TestLog::EndMessage;
		}
	}

	m_context.getTestContext().getLog() << tcu::TestLog::EndSection;

	return pushConstantRanges;
}

std::vector<VkPushConstantRange> PushConstantGraphicsOverlapTestInstance::getPushConstantUpdates (void)
{
	VkShaderStageFlags					lastStageFlags		= (VkShaderStageFlags)~0u;
	std::vector<VkPushConstantRange>	pushConstantUpdates;

	// Find matching shader stages for every 4 byte chunk
	for (deUint32 offset = 0u; offset < 128u; offset += 4u)
	{
		VkShaderStageFlags	stageFlags	= (VkShaderStageFlags)0u;
		bool				updateRange	= false;

		// For each byte in the range specified by offset and size and for each push constant range that overlaps that byte,
		// stageFlags must include all stages in that push constant range's VkPushConstantRange::stageFlags
		for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
		{
			const deUint32 rangeStart	= m_pushConstantRange[rangeNdx].range.offset;
			const deUint32 rangeEnd		= rangeStart + m_pushConstantRange[rangeNdx].range.size;

			const deUint32 updateStart	= m_pushConstantRange[rangeNdx].update.offset;
			const deUint32 updateEnd	= updateStart + m_pushConstantRange[rangeNdx].update.size;

			updateRange |= (updateStart <= offset && updateEnd >= offset + 4u);

			DE_ASSERT(rangeEnd <= 128u);

			if (rangeStart <= offset && rangeEnd >= offset + 4u)
				stageFlags |= m_pushConstantRange[rangeNdx].range.shaderStage;
		}

		// Skip chunks with no updates
		if (!stageFlags || !updateRange)
			continue;

		// Add new update entry
		if (stageFlags != lastStageFlags)
		{
			const VkPushConstantRange update =
			{
				stageFlags,	// VkShaderStageFlags    stageFlags;
				offset,		// deUint32              offset;
				4u			// deUint32              size;
			};

			pushConstantUpdates.push_back(update);
			lastStageFlags = stageFlags;
		}
		// Increase current update entry size
		else
		{
			DE_ASSERT(pushConstantUpdates.size() > 0u);
			pushConstantUpdates.back().size += 4u;
		}
	}

	return pushConstantUpdates;
}

void PushConstantGraphicsOverlapTestInstance::updatePushConstants (VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
	const DeviceInterface&				vk = m_context.getDeviceInterface();
	const std::vector<VkPushConstantRange> pushConstantUpdates = getPushConstantUpdates();

	m_referenceData.resize(m_colorData.size(), 0.0f);

	m_context.getTestContext().getLog() << tcu::TestLog::Section("Updates", "Push constant updates");

	for (deUint32 pushNdx = 0u; pushNdx < pushConstantUpdates.size(); pushNdx++)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "VkShaderStageFlags    stageFlags    " << getShaderStageNameStr(pushConstantUpdates[pushNdx].stageFlags) << ",\n"
			<< "deUint32              offset        " << pushConstantUpdates[pushNdx].offset << ",\n"
			<< "deUint32              size          " << pushConstantUpdates[pushNdx].size << ",\n"
			<< "const void*           pValues       " << &m_colorData[pushConstantUpdates[pushNdx].offset / 2u] << "\n"
			<< tcu::TestLog::EndMessage;

		vk.cmdPushConstants(cmdBuffer, pipelineLayout, pushConstantUpdates[pushNdx].stageFlags, pushConstantUpdates[pushNdx].offset, pushConstantUpdates[pushNdx].size, &m_colorData[pushConstantUpdates[pushNdx].offset / 2u]);

		// Copy push constant values to reference buffer
		DE_ASSERT((pushConstantUpdates[pushNdx].offset / 2u + pushConstantUpdates[pushNdx].size) < 4u * m_colorData.size());
		deMemcpy(&m_referenceData.at(pushConstantUpdates[pushNdx].offset / 4u), &m_colorData.at(pushConstantUpdates[pushNdx].offset / 2u), pushConstantUpdates[pushNdx].size);
	}

	m_context.getTestContext().getLog() << tcu::TestLog::EndSection;
}

void PushConstantGraphicsOverlapTestInstance::setReferenceColor (tcu::Vec4 initColor)
{
	tcu::Vec4 expectedColor = initColor;

	// Calculate reference color
	for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
	{
		const deUint32	offset = m_pushConstantRange[rangeNdx].range.offset / 4u;
		const deUint32	size = m_pushConstantRange[rangeNdx].range.size / 4u;
		const deUint32	numComponents = (size < 4u) ? size : 4u;
		const deUint32	colorNdx = (offset + size - numComponents);

		for (deUint32 componentNdx = 0u; componentNdx < numComponents; componentNdx++)
			expectedColor[componentNdx] += m_referenceData[colorNdx + componentNdx];
	}

	expectedColor = tcu::min(tcu::mod(expectedColor, tcu::Vec4(2.0f)), 2.0f - tcu::mod(expectedColor, tcu::Vec4(2.0f)));

	for (size_t vertexNdx = 0; vertexNdx < m_vertices.size(); vertexNdx++)
	{
		m_vertices[vertexNdx].color.xyzw() = expectedColor;
	}
}

class PushConstantGraphicsTest : public vkt::TestCase
{
public:
							PushConstantGraphicsTest	(tcu::TestContext&			testContext,
														 const std::string&			name,
														 const std::string&			description,
														 const deUint32				rangeCount,
														 const PushConstantData		pushConstantRange[MAX_RANGE_COUNT],
														 const deBool				multipleUpdate,
														 const IndexType			indexType);
	virtual					~PushConstantGraphicsTest	(void);
	virtual void			initPrograms				(SourceCollections& sourceCollections) const = 0;
	virtual TestInstance*	createInstance				(Context& context) const = 0;
	RangeSizeCase			getRangeSizeCase			(deUint32 rangeSize) const;

protected:
	const deUint32			m_rangeCount;
	PushConstantData		m_pushConstantRange[MAX_RANGE_COUNT];
	const deBool			m_multipleUpdate;
	const IndexType			m_indexType;
};

PushConstantGraphicsTest::PushConstantGraphicsTest (tcu::TestContext&			testContext,
													const std::string&			name,
													const std::string&			description,
													const deUint32				rangeCount,
													const PushConstantData		pushConstantRange[MAX_RANGE_COUNT],
													const deBool				multipleUpdate,
													const IndexType				indexType)
	: vkt::TestCase		(testContext, name, description)
	, m_rangeCount		(rangeCount)
	, m_multipleUpdate	(multipleUpdate)
	, m_indexType		(indexType)
{
	deMemcpy(m_pushConstantRange, pushConstantRange, sizeof(PushConstantData) * MAX_RANGE_COUNT);
}

PushConstantGraphicsTest::~PushConstantGraphicsTest (void)
{
}

RangeSizeCase PushConstantGraphicsTest::getRangeSizeCase (deUint32 rangeSize) const
{
	switch (rangeSize)
	{
		case 8:
			return SIZE_CASE_8;
		case 4:
			return SIZE_CASE_4;
		case 12:
			return SIZE_CASE_12;
		case 16:
			return SIZE_CASE_16;
		case 32:
			return SIZE_CASE_32;
		case 36:
			return SIZE_CASE_36;
		case 48:
			return SIZE_CASE_48;
		case 128:
			return SIZE_CASE_128;
		default:
			DE_FATAL("Range size unsupported yet");
			return SIZE_CASE_UNSUPPORTED;
	}
}

class PushConstantGraphicsDisjointTest : public PushConstantGraphicsTest
{
public:
							PushConstantGraphicsDisjointTest	(tcu::TestContext&			testContext,
																 const std::string&			name,
																 const std::string&			description,
																 const deUint32				rangeCount,
																 const PushConstantData		pushConstantRange[MAX_RANGE_COUNT],
																 const deBool				multipleUpdate,
																 const IndexType			indexType);
	virtual					~PushConstantGraphicsDisjointTest	(void);
	virtual void			initPrograms						(SourceCollections& sourceCollections) const;
	virtual TestInstance*	createInstance						(Context& context) const;
};

PushConstantGraphicsDisjointTest::PushConstantGraphicsDisjointTest (tcu::TestContext&		testContext,
																	const std::string&		name,
																	const std::string&		description,
																	const deUint32			rangeCount,
																	const PushConstantData	pushConstantRange[MAX_RANGE_COUNT],
																	const deBool			multipleUpdate,
																	const IndexType			indexType)
	: PushConstantGraphicsTest (testContext, name, description, rangeCount, pushConstantRange, multipleUpdate, indexType)
{
}

PushConstantGraphicsDisjointTest::~PushConstantGraphicsDisjointTest (void)
{
}

void PushConstantGraphicsDisjointTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream	vertexSrc;
	std::ostringstream	fragmentSrc;
	std::ostringstream	geometrySrc;
	std::ostringstream	tessControlSrc;
	std::ostringstream	tessEvaluationSrc;

	for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
	{
		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_VERTEX_BIT)
		{
			vertexSrc << "#version 450\n"
					  << "layout(location = 0) in highp vec4 position;\n"
					  << "layout(location = 1) in highp vec4 color;\n"
					  << "layout(location = 0) out highp vec4 vtxColor;\n"
					  << "out gl_PerVertex { vec4 gl_Position; };\n"
					  << "layout(push_constant) uniform Material {\n";

			switch (m_indexType)
			{
				case INDEX_TYPE_CONST_LITERAL:
					switch (getRangeSizeCase(m_pushConstantRange[rangeNdx].range.size))
					{
						case SIZE_CASE_4:
							vertexSrc << "int kind;\n"
									  << "} matInst;\n";
							break;
						case SIZE_CASE_16:
							vertexSrc << "vec4 color;\n"
									  << "} matInst;\n"
									  << "layout(std140, binding = 0) uniform UniformBuf {\n"
									  << "vec4 element;\n"
									  << "} uniformBuf;\n";
							break;
						case SIZE_CASE_32:
							vertexSrc << "vec4 color[2];\n"
									  << "} matInst;\n";
							break;
						case SIZE_CASE_48:
							vertexSrc << "int dummy1;\n"
									  << "vec4 dummy2;\n"
									  << "vec4 color;\n"
									  << "} matInst;\n";
							break;
						case SIZE_CASE_128:
							vertexSrc << "vec4 color[8];\n"
									  << "} matInst;\n";
							break;
						default:
							DE_FATAL("Not implemented yet");
							break;
					}
					break;
				case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
					vertexSrc << "    layout(offset = 0)  vec4 index; \n"
							  << "    layout(offset = 16) vec4 vecType; \n"
							  << "    layout(offset = 32) mat2 matType; \n"
							  << "    layout(offset = 48) float[4] arrType; \n"
							  << "} matInst;\n";
					break;
				default:
					DE_FATAL("Unhandled IndexType");
					break;
			}

			vertexSrc << "void main()\n"
					  << "{\n"
					  << "	gl_Position = position;\n";

			switch (m_indexType)
			{
				case INDEX_TYPE_CONST_LITERAL:
					switch (getRangeSizeCase(m_pushConstantRange[rangeNdx].range.size))
					{
						case SIZE_CASE_4:
							vertexSrc << "switch (matInst.kind) {\n"
									  << "case 0: vtxColor = vec4(0.0, 1.0, 0, 1.0); break;\n"
									  << "case 1: vtxColor = vec4(0.0, 0.0, 1.0, 1.0); break;\n"
									  << "case 2: vtxColor = vec4(1.0, 0.0, 0, 1.0); break;\n"
									  << "default: vtxColor = color; break;}\n"
									  << "}\n";
							break;
						case SIZE_CASE_16:
							vertexSrc << "vtxColor = (matInst.color + uniformBuf.element) * 0.5;\n"
									  << "}\n";
							break;
						case SIZE_CASE_32:
							vertexSrc << "vtxColor = (matInst.color[0] + matInst.color[1]) * 0.5;\n"
									  << "}\n";
							break;
						case SIZE_CASE_48:
							vertexSrc << "vtxColor = matInst.color;\n"
									  << "}\n";
							break;
						case SIZE_CASE_128:
							vertexSrc << "vec4 color = vec4(0.0, 0, 0, 0.0);\n"
									  << "for (int i = 0; i < 8; i++)\n"
									  << "{\n"
									  << "  color = color + matInst.color[i];\n"
									  << "}\n"
									  << "vtxColor = color * 0.125;\n"
									  << "}\n";
							break;
						default:
							DE_FATAL("Not implemented yet");
							break;
					}
					break;
				case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
					{
						vertexSrc << "    vtxColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
								  // Mix in gl_Position to (hopefully) prevent optimizing our index away
								  << "    int vec_selector = int(abs(gl_Position.x) * 0.0000001 + 0);\n"
								  << "    int mat_selector = int(abs(gl_Position.x) * 0.0000001 + 1);\n"
								  << "    int arr_selector = int(abs(gl_Position.x) * 0.0000001 + 2);\n";

						// Use the dynamic index to pull our real index value from push constants
						// Then use that value to index into three variable types
						std::string vecValue = "matInst.vecType[int(matInst.index[vec_selector])]";
						std::string matValue = "matInst.matType[int(matInst.index[mat_selector])][0]";
						std::string arrValue = "matInst.arrType[int(matInst.index[arr_selector])]";

						// Test vector indexing
						vertexSrc << "    if (" << vecValue << " != " << DYNAMIC_VEC_CONSTANT << ")\n"
								  << "        vtxColor += vec4(0.0, 0.5, 0.0, 1.0);\n";

						// Test matrix indexing
						vertexSrc << "    if (" << matValue << " != " << DYNAMIC_MAT_CONSTANT << ")\n"
								  << "        vtxColor += vec4(0.0, 0.0, 0.5, 1.0);\n";

						// Test array indexing
						vertexSrc << "    if (" << arrValue << " != " << DYNAMIC_ARR_CONSTANT << ")\n"
								  << "        vtxColor = vec4(0.0, 0.5, 0.5, 1.0);\n";

						vertexSrc << "}\n";
					}
					break;
				default:
					DE_FATAL("Unhandled IndexType");
					break;
			}

			sourceCollections.glslSources.add("color_vert") << glu::VertexSource(vertexSrc.str());
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		{
			tessControlSrc << "#version 450\n"
						   << "layout (vertices = 3) out;\n"
						   << "layout(push_constant) uniform TessLevel {\n"
						   << "    layout(offset = 24) int level;\n"
						   << "} tessLevel;\n"
						   << "layout(location = 0) in highp vec4 color[];\n"
						   << "layout(location = 0) out highp vec4 vtxColor[];\n"
						   << "in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
						   << "out gl_PerVertex { vec4 gl_Position; } gl_out[];\n"
						   << "void main()\n"
						   << "{\n"
						   << "  gl_TessLevelInner[0] = tessLevel.level;\n"
						   << "  gl_TessLevelOuter[0] = tessLevel.level;\n"
						   << "  gl_TessLevelOuter[1] = tessLevel.level;\n"
						   << "  gl_TessLevelOuter[2] = tessLevel.level;\n"
						   << "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						   << "  vtxColor[gl_InvocationID] = color[gl_InvocationID];\n"
						   << "}\n";

			sourceCollections.glslSources.add("color_tesc") << glu::TessellationControlSource(tessControlSrc.str());
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		{
			tessEvaluationSrc << "#version 450\n"
							  << "layout (triangles) in;\n"
							  << "layout(push_constant) uniform Material {\n"
							  << "    layout(offset = 32) vec4 color;\n"
							  << "} matInst;\n"
							  << "layout(location = 0) in highp vec4 color[];\n"
							  << "layout(location = 0) out highp vec4 vtxColor;\n"
							  << "in gl_PerVertex { vec4 gl_Position; } gl_in[gl_MaxPatchVertices];\n"
							  << "out gl_PerVertex { vec4 gl_Position; };\n"
							  << "void main()\n"
							  << "{\n"
							  << "  gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position;\n"
							  << "  vtxColor = matInst.color;\n"
							  << "}\n";

			sourceCollections.glslSources.add("color_tese") << glu::TessellationEvaluationSource(tessEvaluationSrc.str());
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			geometrySrc << "#version 450\n"
						<< "layout(triangles) in;\n"
						<< "layout(triangle_strip, max_vertices=3) out;\n"
						<< "layout(push_constant) uniform Material {\n"
						<< "    layout(offset = 20) int kind;\n"
						<< "} matInst;\n"
						<< "layout(location = 0) in highp vec4 color[];\n"
						<< "layout(location = 0) out highp vec4 vtxColor;\n"
						<< "in gl_PerVertex { vec4 gl_Position; } gl_in[];\n"
						<< "out gl_PerVertex { vec4 gl_Position; };\n"
						<< "void main()\n"
						<< "{\n"
						<< "  for(int i=0; i<3; i++)\n"
						<< "  {\n"
						<< "    gl_Position.xyz = gl_in[i].gl_Position.xyz / matInst.kind;\n"
						<< "    gl_Position.w = gl_in[i].gl_Position.w;\n"
						<< "    vtxColor = color[i];\n"
						<< "    EmitVertex();\n"
						<< "  }\n"
						<< "  EndPrimitive();\n"
						<< "}\n";

			sourceCollections.glslSources.add("color_geom") << glu::GeometrySource(geometrySrc.str());
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			fragmentSrc << "#version 450\n"
						<< "layout(location = 0) in highp vec4 vtxColor;\n"
						<< "layout(location = 0) out highp vec4 fragColor;\n"
						<< "layout(push_constant) uniform Material {\n";

			switch (m_indexType)
			{
				case INDEX_TYPE_CONST_LITERAL:
					if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_VERTEX_BIT)
					{
						fragmentSrc << "    layout(offset = 0) int kind; \n"
									<< "} matInst;\n";
					}
					else
					{
						fragmentSrc << "    layout(offset = 16) int kind;\n"
									<< "} matInst;\n";
					}

					fragmentSrc << "void main (void)\n"
						<< "{\n"
						<< "    switch (matInst.kind) {\n"
						<< "    case 0: fragColor = vec4(0, 1.0, 0, 1.0); break;\n"
						<< "    case 1: fragColor = vec4(0, 0.0, 1.0, 1.0); break;\n"
						<< "    case 2: fragColor = vtxColor; break;\n"
						<< "    default: fragColor = vec4(1.0, 1.0, 1.0, 1.0); break;}\n"
						<< "}\n";
					break;
				case INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR:
					{
						fragmentSrc << "    layout(offset = 0)  vec4 index; \n"
									<< "    layout(offset = 16) vec4 vecType; \n"
									<< "    layout(offset = 32) mat2 matType; \n"
									<< "    layout(offset = 48) float[4] arrType; \n"
									<< "} matInst;\n";

						fragmentSrc << "void main (void)\n"
									<< "{\n"
									<< "    fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"

									// Mix in gl_FragCoord to (hopefully) prevent optimizing our index away
									<< "    int vec_selector = int(gl_FragCoord.x * 0.0000001 + 0);\n"
									<< "    int mat_selector = int(gl_FragCoord.x * 0.0000001 + 1);\n"
									<< "    int arr_selector = int(gl_FragCoord.x * 0.0000001 + 2);\n";

						// Use the dynamic index to pull our real index value from push constants
						// Then use that value to index into three variable types
						std::string vecValue = "matInst.vecType[int(matInst.index[vec_selector])]";
						std::string matValue = "matInst.matType[int(matInst.index[mat_selector])][0]";
						std::string arrValue = "matInst.arrType[int(matInst.index[arr_selector])]";

						// Test vector indexing
						fragmentSrc << "    if (" << vecValue << " != " << DYNAMIC_VEC_CONSTANT << ")\n"
									<< "        fragColor += vec4(0.0, 0.5, 0.0, 1.0);\n";

						// Test matrix indexing
						fragmentSrc << "    if (" << matValue << " != " << DYNAMIC_MAT_CONSTANT << ")\n"
									<< "        fragColor += vec4(0.0, 0.0, 0.5, 1.0);\n";

						// Test array indexing
						fragmentSrc << "    if (" << arrValue << " != " << DYNAMIC_ARR_CONSTANT << ")\n"
									<< "        fragColor = vec4(0.0, 0.5, 0.5, 1.0);\n";

						fragmentSrc << "}\n";
					}
					break;
				default:
					DE_FATAL("Unhandled IndexType");
					break;
			}

			sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(fragmentSrc.str());
		}
	}

	// add a pass through fragment shader if it's not activated in push constant ranges
	if (fragmentSrc.str().empty())
	{
		fragmentSrc << "#version 450\n"
					<< "layout(location = 0) in highp vec4 vtxColor;\n"
					<< "layout(location = 0) out highp vec4 fragColor;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	fragColor = vtxColor;\n"
					<< "}\n";

		sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(fragmentSrc.str());
	}
}

TestInstance* PushConstantGraphicsDisjointTest::createInstance (Context& context) const
{
	return new PushConstantGraphicsDisjointInstance(context, m_rangeCount, m_pushConstantRange, m_multipleUpdate, m_indexType);
}

class PushConstantGraphicsOverlapTest : public PushConstantGraphicsTest
{
public:
							PushConstantGraphicsOverlapTest		(tcu::TestContext&			testContext,
																 const std::string&			name,
																 const std::string&			description,
																 const deUint32				rangeCount,
																 const PushConstantData		pushConstantRange[MAX_RANGE_COUNT]);
	virtual					~PushConstantGraphicsOverlapTest	(void);
	std::string				getPushConstantDeclarationStr		(VkShaderStageFlags shaderStage) const;
	virtual void			initPrograms						(SourceCollections& sourceCollections) const;
	virtual TestInstance*	createInstance						(Context& context) const;
};

PushConstantGraphicsOverlapTest::PushConstantGraphicsOverlapTest (tcu::TestContext&			testContext,
																  const std::string&		name,
																  const std::string&		description,
																  const deUint32			rangeCount,
																  const PushConstantData	pushConstantRange[MAX_RANGE_COUNT])
	: PushConstantGraphicsTest (testContext, name, description, rangeCount, pushConstantRange, false, INDEX_TYPE_CONST_LITERAL)
{
}

PushConstantGraphicsOverlapTest::~PushConstantGraphicsOverlapTest (void)
{
}

std::string PushConstantGraphicsOverlapTest::getPushConstantDeclarationStr (VkShaderStageFlags shaderStage) const
{
	std::stringstream src;

	src	<< "layout(push_constant) uniform Material\n"
		<< "{\n";

	for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
	{
		if (m_pushConstantRange[rangeNdx].range.shaderStage & shaderStage)
		{
			switch (getRangeSizeCase(m_pushConstantRange[rangeNdx].range.size))
			{
				case SIZE_CASE_4:
					src	<< "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") float color;\n";
					break;
				case SIZE_CASE_8:
					src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec2 color;\n";
					break;
				case SIZE_CASE_12:
					src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec3 color;\n";
					break;
				case SIZE_CASE_16:
					src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec4 color;\n";
					break;
				case SIZE_CASE_32:
					src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec4 color[2];\n";
					break;
				case SIZE_CASE_36:
					src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") int dummy1;\n"
						<< "    layout(offset = " << (m_pushConstantRange[rangeNdx].range.offset + 4) << ") vec4 dummy2;\n"
						<< "    layout(offset = " << (m_pushConstantRange[rangeNdx].range.offset + 20) << ") vec4 color;\n";
					break;
				case SIZE_CASE_128:
					src << "    layout(offset = " << m_pushConstantRange[rangeNdx].range.offset << ") vec4 color[8];\n";
					break;
				default:
					DE_FATAL("Not implemented");
					break;
			}
		}
	}

	src	<< "} matInst;\n";

	return src.str();
}

std::string getSwizzleStr (deUint32 size)
{
	switch (size)
	{
		case 4:		return ".x";
		case 8:		return ".xy";
		case 12:	return ".xyz";
		case 16:
		case 32:
		case 36:
		case 128:	return "";
		default:	DE_FATAL("Not implemented");
					return "";
	}
}

std::string getColorReadStr (deUint32 size)
{
	// Always read the last element from array types
	const std::string	arrayNdx		= (size == 128u)	? "[7]"
										: (size == 32u)		? "[1]"
										: "";
	const std::string	colorReadStr	= getSwizzleStr(size) + " += matInst.color" + arrayNdx + ";\n";;

	return colorReadStr;
}

void PushConstantGraphicsOverlapTest::initPrograms (SourceCollections& sourceCollections) const
{
	for (size_t rangeNdx = 0; rangeNdx < m_rangeCount; rangeNdx++)
	{
		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_VERTEX_BIT)
		{
			const std::string source =
				"#version 450\n"
				"layout(location = 0) in highp vec4 position;\n"
				"layout(location = 1) in highp vec4 inColor;\n"
				"layout(location = 0) out highp vec4 vtxColor;\n"
				"out gl_PerVertex\n"
				"{\n"
				"    vec4 gl_Position;\n"
				"};\n"
				+ getPushConstantDeclarationStr(VK_SHADER_STAGE_VERTEX_BIT) +
				"void main()\n"
				"{\n"
				"    gl_Position = position;\n"
				"    vec4 color = inColor;\n"
				"    color" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) +
				"    vtxColor = color;\n"
				"}\n";

			sourceCollections.glslSources.add("color_vert") << glu::VertexSource(source);
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		{
			const std::string source =
				"#version 450\n"
				"layout (vertices = 3) out;\n"
				+ getPushConstantDeclarationStr(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) +
				"layout(location = 0) in highp vec4 color[];\n"
				"layout(location = 0) out highp vec4 vtxColor[];\n"
				"in gl_PerVertex\n"
				"{\n"
				"    vec4 gl_Position;\n"
				"} gl_in[gl_MaxPatchVertices];\n"
				"out gl_PerVertex\n"
				"{\n"
				"    vec4 gl_Position;\n"
				"} gl_out[];\n"
				"void main()\n"
				"{\n"
				"    gl_TessLevelInner[0] = 2.0;\n"
				"    gl_TessLevelOuter[0] = 2.0;\n"
				"    gl_TessLevelOuter[1] = 2.0;\n"
				"    gl_TessLevelOuter[2] = 2.0;\n"
				"    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"    vec4 outColor = color[gl_InvocationID];\n"
				"    outColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) +
				"    vtxColor[gl_InvocationID] = outColor;\n"
				"}\n";

			sourceCollections.glslSources.add("color_tesc") << glu::TessellationControlSource(source);
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		{
			const std::string source =
				"#version 450\n"
				"layout (triangles) in;\n"
				+ getPushConstantDeclarationStr(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) +
				"layout(location = 0) in highp vec4 color[];\n"
				"layout(location = 0) out highp vec4 vtxColor;\n"
				"in gl_PerVertex\n"
				"{\n"
				"    vec4 gl_Position;\n"
				"} gl_in[gl_MaxPatchVertices];\n"
				"out gl_PerVertex\n"
				"{\n"
				"    vec4 gl_Position;\n"
				"};\n"
				"void main()\n"
				"{\n"
				"    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position;\n"
				"    vtxColor = gl_TessCoord.x * color[0] + gl_TessCoord.y * color[1] + gl_TessCoord.z * color[2];\n"
				"    vtxColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) +
				"}\n";

			sourceCollections.glslSources.add("color_tese") << glu::TessellationEvaluationSource(source);
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			const std::string source =
				"#version 450\n"
				"layout(triangles) in;\n"
				"layout(triangle_strip, max_vertices=3) out;\n"
				+ getPushConstantDeclarationStr(VK_SHADER_STAGE_GEOMETRY_BIT) +
				"layout(location = 0) in highp vec4 color[];\n"
				"layout(location = 0) out highp vec4 vtxColor;\n"
				"in gl_PerVertex\n"
				"{\n"
				"    vec4 gl_Position;\n"
				"} gl_in[];\n"
				"out gl_PerVertex\n"
				"{\n"
				"    vec4 gl_Position;\n"
				"};\n"
				"void main()\n"
				"{\n"
				"    for(int i = 0; i < 3; i++)\n"
				"    {\n"
				"        gl_Position.xyz = gl_in[i].gl_Position.xyz / 2.0;\n"
				"        gl_Position.w = gl_in[i].gl_Position.w;\n"
				"        vtxColor = color[i];\n"
				"        vtxColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) +
				"        EmitVertex();\n"
				"    }\n"
				"    EndPrimitive();\n"
				"}\n";

			sourceCollections.glslSources.add("color_geom") << glu::GeometrySource(source);
		}

		if (m_pushConstantRange[rangeNdx].range.shaderStage & VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			const std::string source =
				"#version 450\n"
				"layout(location = 0) in highp vec4 vtxColor;\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				+ getPushConstantDeclarationStr(VK_SHADER_STAGE_FRAGMENT_BIT) +
				"void main (void)\n"
				"{\n"
				"    fragColor = vtxColor;\n"
				"    fragColor" + getColorReadStr(m_pushConstantRange[rangeNdx].range.size) +
				"    fragColor = min(mod(fragColor, 2.0), 2.0 - mod(fragColor, 2.0));\n"
				"}\n";

			sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(source);
		}
	}
}

TestInstance* PushConstantGraphicsOverlapTest::createInstance (Context& context) const
{
	return new PushConstantGraphicsOverlapTestInstance(context, m_rangeCount, m_pushConstantRange, false, INDEX_TYPE_CONST_LITERAL);
}

class PushConstantComputeTest : public vkt::TestCase
{
public:
							PushConstantComputeTest		(tcu::TestContext&		testContext,
														 const std::string&		name,
														 const std::string&		description,
														 const PushConstantData	pushConstantRange);
	virtual					~PushConstantComputeTest	(void);
	virtual void			initPrograms				(SourceCollections& sourceCollections) const;
	virtual TestInstance*	createInstance				(Context& context) const;

private:
	const PushConstantData	m_pushConstantRange;
};

class PushConstantComputeTestInstance : public vkt::TestInstance
{
public:
							PushConstantComputeTestInstance		(Context&				context,
																 const PushConstantData	pushConstantRange);
	virtual					~PushConstantComputeTestInstance	(void);
	virtual tcu::TestStatus	iterate								(void);

private:
	const PushConstantData			m_pushConstantRange;

	Move<VkBuffer>					m_outBuffer;
	de::MovePtr<Allocation>			m_outBufferAlloc;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorSet>			m_descriptorSet;

	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkPipeline>				m_computePipelines;

	Move<VkShaderModule>			m_computeShaderModule;

	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
};

PushConstantComputeTest::PushConstantComputeTest (tcu::TestContext&			testContext,
												  const std::string&		name,
												  const std::string&		description,
												  const PushConstantData	pushConstantRange)
	: vkt::TestCase			(testContext, name, description)
	, m_pushConstantRange	(pushConstantRange)
{
}

PushConstantComputeTest::~PushConstantComputeTest (void)
{
}

TestInstance* PushConstantComputeTest::createInstance (Context& context) const
{
	return new PushConstantComputeTestInstance(context, m_pushConstantRange);
}

void PushConstantComputeTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream	computeSrc;

	computeSrc << "#version 450\n"
			   << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			   << "layout(std140, set = 0, binding = 0) writeonly buffer Output {\n"
			   << "  vec4 elements[];\n"
			   << "} outData;\n"
			   << "layout(push_constant) uniform Material{\n"
			   << "  vec4 element;\n"
			   << "} matInst;\n"
			   << "void main (void)\n"
			   << "{\n"
			   << "  outData.elements[gl_GlobalInvocationID.x] = matInst.element;\n"
			   << "}\n";

	sourceCollections.glslSources.add("compute") << glu::ComputeSource(computeSrc.str());
}

PushConstantComputeTestInstance::PushConstantComputeTestInstance (Context&					context,
																  const PushConstantData	pushConstantRange)
	: vkt::TestInstance		(context)
	, m_pushConstantRange	(pushConstantRange)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	// Create pipeline layout
	{
		// create push constant range
		VkPushConstantRange	pushConstantRanges;
		pushConstantRanges.stageFlags	= m_pushConstantRange.range.shaderStage;
		pushConstantRanges.offset		= m_pushConstantRange.range.offset;
		pushConstantRanges.size			= m_pushConstantRange.range.size;

		// create descriptor set layout
		m_descriptorSetLayout = DescriptorSetLayoutBuilder().addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT).build(vk, vkDevice);

		// create descriptor pool
		m_descriptorPool = DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u).build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		// create uniform buffer
		const VkDeviceSize			bufferSize			= sizeof(tcu::Vec4) * 8;
		const VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType;
			DE_NULL,													// const void*			pNext;
			0u,															// VkBufferCreateFlags	flags
			bufferSize,													// VkDeviceSize			size;
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,							// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode;
			1u,															// deUint32				queueFamilyCount;
			&queueFamilyIndex											// const deUint32*		pQueueFamilyIndices;
		};

		m_outBuffer			= createBuffer(vk, vkDevice, &bufferCreateInfo);
		m_outBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_outBuffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_outBuffer, m_outBufferAlloc->getMemory(), m_outBufferAlloc->getOffset()));

		// create and update descriptor set
		const VkDescriptorSetAllocateInfo allocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,				// VkStructureType                             sType;
			DE_NULL,													// const void*                                 pNext;
			*m_descriptorPool,											// VkDescriptorPool                            descriptorPool;
			1u,															// uint32_t                                    setLayoutCount;
			&(*m_descriptorSetLayout),									// const VkDescriptorSetLayout*                pSetLayouts;
		};
		m_descriptorSet	= allocateDescriptorSet(vk, vkDevice, &allocInfo);

		const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*m_outBuffer, (VkDeviceSize)0u, bufferSize);

		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
			.update(vk, vkDevice);

		// create pipeline layout
		const VkPipelineLayoutCreateInfo	pipelineLayoutParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						descriptorSetCount;
			&(*m_descriptorSetLayout),							// const VkDescriptorSetLayout*	pSetLayouts;
			1u,													// deUint32						pushConstantRangeCount;
			&pushConstantRanges									// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// create pipeline
	{
		m_computeShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("compute"), 0);

		const VkPipelineShaderStageCreateInfo	stageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			0u,														// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			*m_computeShaderModule,									// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		createInfo	=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType                             sType;
			DE_NULL,												// const void*                                 pNext;
			0u,														// VkPipelineCreateFlags                       flags;
			stageCreateInfo,										// VkPipelineShaderStageCreateInfo             stage;
			*m_pipelineLayout,										// VkPipelineLayout                            layout;
			(VkPipeline)0,											// VkPipeline                                  basePipelineHandle;
			0u,														// int32_t                                     basePipelineIndex;
		};

		m_computePipelines = createComputePipeline(vk, vkDevice, (vk::VkPipelineCache)0u, &createInfo);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipelines);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 1, &(*m_descriptorSet), 0, DE_NULL);

		// update push constant
		tcu::Vec4	value	= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
		vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, m_pushConstantRange.range.shaderStage, m_pushConstantRange.range.offset, m_pushConstantRange.range.size, &value);

		vk.cmdDispatch(*m_cmdBuffer, 8, 1, 1);

		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

PushConstantComputeTestInstance::~PushConstantComputeTestInstance (void)
{
}

tcu::TestStatus PushConstantComputeTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	invalidateAlloc(vk, vkDevice, *m_outBufferAlloc);

	// verify result
	std::vector<tcu::Vec4>	expectValue(8, tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	if (deMemCmp((void*)(&expectValue[0]), m_outBufferAlloc->getHostPtr(), (size_t)(sizeof(tcu::Vec4) * 8)))
	{
		return tcu::TestStatus::fail("Image mismatch");
	}
	return tcu::TestStatus::pass("result image matches with reference");
}

} // anonymous

tcu::TestCaseGroup* createPushConstantTests (tcu::TestContext& testCtx)
{
	static const struct
	{
		const char*			name;
		const char*			description;
		deUint32			count;
		PushConstantData	range[MAX_RANGE_COUNT];
		deBool				hasMultipleUpdates;
		IndexType			indexType;
	} graphicsParams[] =
	{
		// test range size from minimum valid size to maximum
		{
			"range_size_4",
			"test range size is 4 bytes(minimum valid size)",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT, 0, 4 } , { 0, 4 } } },
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"range_size_16",
			"test range size is 16 bytes, and together with a normal uniform",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT, 0, 16 }, { 0, 16 } } },
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"range_size_128",
			"test range size is 128 bytes(maximum valid size)",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT, 0, 128 }, { 0, 128 } } },
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		// test range count, including all valid shader stage in graphics pipeline, and also multiple shader stages share one single range
		{
			"count_2_shaders_vert_frag",
			"test range count is 2, use vertex and fragment shaders",
			2u,
			{
				{ { VK_SHADER_STAGE_VERTEX_BIT, 0, 16 }, { 0, 16 } },
				{ { VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4 }, { 16, 4 } },
			},
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"count_3_shaders_vert_geom_frag",
			"test range count is 3, use vertex, geometry and fragment shaders",
			3u,
			{
				{ { VK_SHADER_STAGE_VERTEX_BIT, 0, 16 }, { 0, 16 } },
				{ { VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4 }, { 16, 4 } },
				{ { VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4 }, { 20, 4 } },
			},
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"count_5_shaders_vert_tess_geom_frag",
			"test range count is 5, use vertex, tessellation, geometry and fragment shaders",
			5u,
			{
				{ { VK_SHADER_STAGE_VERTEX_BIT, 0, 16 }, { 0, 16 } },
				{ { VK_SHADER_STAGE_FRAGMENT_BIT, 16, 4 }, { 16, 4 } },
				{ { VK_SHADER_STAGE_GEOMETRY_BIT, 20, 4 }, { 20, 4 } },
				{ { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 24, 4 }, { 24, 4 } },
				{ { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 32, 16 }, { 32, 16 } },
			},
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"count_1_shader_vert_frag",
			"test range count is 1, vertex and fragment shaders share one range",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4 }, { 0, 4 } } },
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		// test data partial update and multiple times update
		{
			"data_update_partial_1",
			"test partial update of the values",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT, 0, 32 }, { 4, 24 } } },
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"data_update_partial_2",
			"test partial update of the values",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT, 0, 48 }, { 32, 16 } } },
			false,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"data_update_multiple",
			"test multiple times update of the values",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT, 0, 4 }, { 0, 4 } } },
			true,
			INDEX_TYPE_CONST_LITERAL
		},
		{
			"dynamic_index_vert",
			"dynamically uniform indexing of vertex, matrix, and array in vertex shader",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT, 0, 64 }, { 0, 64 } } },
			false,
			INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR
		},
		{
			"dynamic_index_frag",
			"dynamically uniform indexing of vertex, matrix, and array in fragment shader",
			1u,
			{ { { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 64 }, { 0, 64 } } },
			false,
			INDEX_TYPE_DYNAMICALLY_UNIFORM_EXPR
		}
	};

	static const struct
	{
		const char*			name;
		const char*			description;
		deUint32			count;
		PushConstantData	range[MAX_RANGE_COUNT];
	} overlapGraphicsParams[] =
	{
		// test ranges with multiple overlapping stages
		{
			"overlap_2_shaders_vert_frag",
			"overlapping range count is 2, use vertex and fragment shaders",
			2u,
			{
				{ { VK_SHADER_STAGE_VERTEX_BIT, 0, 16 }, { 0, 16 } },
				{ { VK_SHADER_STAGE_FRAGMENT_BIT, 12, 36 }, { 12, 36 } },
			}
		},
		{
			"overlap_3_shaders_vert_geom_frag",
			"overlapping range count is 3, use vertex, geometry and fragment shaders",
			3u,
			{
				{ { VK_SHADER_STAGE_VERTEX_BIT, 12, 36 }, { 12, 36 } },
				{ { VK_SHADER_STAGE_GEOMETRY_BIT, 0, 32 }, { 16, 16 } },
				{ { VK_SHADER_STAGE_FRAGMENT_BIT, 20, 4 }, { 20, 4 } }
			}
		},
		{
			"overlap_4_shaders_vert_tess_frag",
			"overlapping range count is 4, use vertex, tessellation and fragment shaders",
			4u,
			{
				{ { VK_SHADER_STAGE_VERTEX_BIT, 8, 4 }, { 8, 4 } },
				{ { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, 128 }, { 52, 76 } },
				{ { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 56, 8 }, { 56, 8 } },
				{ { VK_SHADER_STAGE_FRAGMENT_BIT, 60, 36 }, { 60, 36 } }
			}
		},
		{
			"overlap_5_shaders_vert_tess_geom_frag",
			"overlapping range count is 5, use vertex, tessellation, geometry and fragment shaders",
			5u,
			{
				{ { VK_SHADER_STAGE_VERTEX_BIT, 40, 8 }, { 40, 8 } },
				{ { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 32, 12 }, { 32, 12 } },
				{ { VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 48, 16 }, { 48, 16 } },
				{ { VK_SHADER_STAGE_GEOMETRY_BIT, 28, 36 }, { 28, 36 } },
				{ { VK_SHADER_STAGE_FRAGMENT_BIT, 56, 8 }, { 60, 4 } }
			}
		}
	};

	static const struct
	{
		const char*			name;
		const char*			description;
		PushConstantData	range;
	} computeParams[] =
	{
		{
			"simple_test",
			"test compute pipeline",
			{ { VK_SHADER_STAGE_COMPUTE_BIT, 0, 16 }, { 0, 16 } },
		},
	};

	de::MovePtr<tcu::TestCaseGroup>	pushConstantTests	(new tcu::TestCaseGroup(testCtx, "push_constant", "PushConstant tests"));

	de::MovePtr<tcu::TestCaseGroup>	graphicsTests	(new tcu::TestCaseGroup(testCtx, "graphics_pipeline", "graphics pipeline"));
	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(graphicsParams); ndx++)
	{
		graphicsTests->addChild(new PushConstantGraphicsDisjointTest(testCtx, graphicsParams[ndx].name, graphicsParams[ndx].description, graphicsParams[ndx].count, graphicsParams[ndx].range, graphicsParams[ndx].hasMultipleUpdates, graphicsParams[ndx].indexType));
	}

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(overlapGraphicsParams); ndx++)
	{
		graphicsTests->addChild(new PushConstantGraphicsOverlapTest(testCtx, overlapGraphicsParams[ndx].name, overlapGraphicsParams[ndx].description, overlapGraphicsParams[ndx].count, overlapGraphicsParams[ndx].range));
	}
	pushConstantTests->addChild(graphicsTests.release());

	de::MovePtr<tcu::TestCaseGroup>	computeTests	(new tcu::TestCaseGroup(testCtx, "compute_pipeline", "compute pipeline"));
	computeTests->addChild(new PushConstantComputeTest(testCtx, computeParams[0].name, computeParams[0].description, computeParams[0].range));
	pushConstantTests->addChild(computeTests.release());

	return pushConstantTests.release();
}

} // pipeline
} // vkt
