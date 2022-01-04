/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VESUBRENDER_H
#define VESUBRENDER_H

class VERenderer;

namespace ve
{
	/**
		*
		* \brief A VESubrender instance manages and draws its entities
		*
		* Subrenderers understand the data that is attached to entities and create and manage the according Vulkan resources.
		* The also create PSOs and use them to draw entities. Subrenderers maintain a list of entities associated with them.
		*
		*/
	class VESubrender
	{
	protected:
		std::vector<VEEntity *> m_entities; ///<List of associated entities
		std::vector<VESubrender *> &
			getSubrenderers(); ///<return a list with the current subrenderers, used for shadows

	public:
		///Constructor of subrender class
		VESubrender() {};

		///Destructor of subrender class
		virtual ~VESubrender() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass() = 0;

		///\returns the type of the subrenderer
		virtual veSubrenderType getType() = 0;

		//------------------------------------------------------------------------------------------------------------------
		virtual void initSubrenderer();

		///\brief close the subrenderer - empty base class function
		virtual void closeSubrenderer() {};

		///\brief recreate subrenderer resources - empty base class function
		virtual void recreateResources() {};

		virtual void addEntity(VEEntity *pEntity);

		///\brief remove an entity from the subrenderer - empty base class function
		virtual void removeEntity(VEEntity *pEntity) {};

		///\returns a reference the list with entities
		std::vector<VEEntity *> &getEntities()
		{
			return m_entities;
		};

		//------------------------------------------------------------------------------------------------------------------
		///\brief Prepare to perform draw operation, e.g. for an overlay
		virtual void prepareDraw() {};

		///\brief Called after all draw calls have been recorded. Used for incremental recording
		virtual void afterDrawFinished() {};

		///\brief Draw all entities that are managed by this subrenderer
		virtual void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow = {}) {};

		///Perform an arbitrary draw operation
		///\returns a semaphore signalling when this draw operations has finished
		virtual VkSemaphore draw(uint32_t imageIndex, VkSemaphore wait_semaphore)
		{
			return VK_NULL_HANDLE;
		};

		///\brief draw a specific entity
		virtual void drawEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {};

		virtual void updateRTDescriptorSets() {};
	};

} // namespace ve

#endif
