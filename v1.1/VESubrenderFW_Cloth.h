//----------------------------------Subrender-for-Cloth-Simulation----------------------------------
// by Felix Neumann
// Modification of VESubrenderFW_D.h by Helmut Hlavacs, University of Vienna

#ifndef VESUBRENDERFWCLOTH_H
#define VESUBRENDERFWCLOTH_H

namespace ve
{
	/// <summary>
	/// Manages rendering of cloth entities which simply have one diffuse texture for coloring.
	/// </summary>
	class VESubrenderFW_Cloth : public VESubrenderFW
	{
	public:
		VESubrenderFW_Cloth(VERendererForward& renderer)
			: VESubrenderFW(renderer) {};

		virtual ~VESubrenderFW_Cloth() {};

		/// <summary>
		/// Getter for class type.
		/// </summary>
		/// <returns> The class class of the subrenderer. </returns>
		virtual veSubrenderClass getClass() {
			return VE_SUBRENDERER_CLASS_OBJECT;
		};

		/// <summary>
		/// Getter for class type.
		/// </summary>
		/// <returns> The class class of the subrenderer. </returns>
		virtual veSubrenderType getType() {
			return VE_SUBRENDERER_TYPE_CLOTH;
		};

		/// <summary>
		/// Initialize the subrenderer. Create descriptor set layout, pipeline layout and the PSO.
		/// </summary>
		virtual void initSubrenderer();

		/// <summary>
		/// Set the danymic pipeline stat, i.e. the blend constants to be used.
		/// </summary>
		/// <param name="commandBuffer"> The currently used command buffer. </param>
		/// <param name="numPass"> The current pass number - in the forst pass, write over pixel
		/// colors, after this add pixel colors </param>
		virtual void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass);

		/// <summary>
		/// Add an entity to the subrenderer. Create a UBO for this entity, a descriptor set per
		/// swapchain image, and update the descriptor sets.
		/// </summary>
		/// <param name="pEntity"> The entitiy to add. </param>
		virtual void addEntity(VEEntity* pEntity);
	};

} // namespace ve

#endif