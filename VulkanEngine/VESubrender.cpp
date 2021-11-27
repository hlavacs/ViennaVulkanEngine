/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

namespace ve
{
	/**
		* \brief Initialize the subrenderer
		*/
	void VESubrender::initSubrenderer() {};

	/**
		*
		* \brief Add an entity to the list of associated entities.
		*
		* \param[in] pEntity Pointer to the entity to include into the list.
		*
		*/
	void VESubrender::addEntity(VEEntity *pEntity)
	{
		m_entities.push_back(pEntity);
		pEntity->m_pSubrenderer = this;
	}

	/**
		*
		* \returns the list with subrenderers from the renderer
		*
		*/
	std::vector<VESubrender *> &VESubrender::getSubrenderers()
	{
		return getEnginePointer()->getRenderer()->m_subrenderers;
	};

} // namespace ve
