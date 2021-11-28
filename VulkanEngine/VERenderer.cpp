/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

class VEEngine;

namespace ve
{
	VERenderer *g_pVERendererSingleton = nullptr; ///<Singleton pointer to the only VERenderer instance

	VERenderer::VERenderer()
	{
		g_pVERendererSingleton = this;
	}

	/**
		*
		* \brief Initialize and register a subrenderer
		*
		* \param[in] pSub Pointer to the subrenderer to be initialized and registered
		*
		*/
	void VERenderer::addSubrenderer(VESubrender *pSub)
	{
		pSub->initSubrenderer();
		if (pSub->getClass() == VE_SUBRENDERER_CLASS_SHADOW)
		{
			m_subrenderShadow = pSub;
			return;
		}
		if (pSub->getClass() == VE_SUBRENDERER_CLASS_OVERLAY)
		{
			m_subrenderOverlay = pSub;
			return;
		}
		if (pSub->getClass() == VE_SUBRENDERER_CLASS_RT)
		{
			m_subrenderRT = pSub;
			return;
		}
		if (pSub->getClass() == VE_SUBRENDERER_CLASS_COMPOSER)
		{
			m_subrenderComposer = pSub;
			return;
		}
		m_subrenderers.push_back(pSub);
	}

	/**
		*
		* \brief Find a subrenderer of a certain type
		*
		* \param[in] type The type that is searched for
		* \returns The found subrenderer or nullptr
		*
		*/
	VESubrender *VERenderer::getSubrenderer(veSubrenderType type)
	{
		for (uint32_t i = 0; i < m_subrenderers.size(); i++)
		{
			if (m_subrenderers[i]->getType() == type)
				return m_subrenderers[i];
		}
		return nullptr;
	}

	/**
		* \brief Destroy all subrenderers
		*/
	void VERenderer::destroySubrenderers()
	{
		for (auto pSubrender : m_subrenderers)
		{
			pSubrender->closeSubrenderer();
			delete pSubrender;
		}
		if (m_subrenderShadow != nullptr)
		{
			m_subrenderShadow->closeSubrenderer();
			delete m_subrenderShadow;
		}
		if (m_subrenderOverlay != nullptr)
		{
			m_subrenderOverlay->closeSubrenderer();
			delete m_subrenderOverlay;
		}
		if (m_subrenderRT != nullptr)
		{
			m_subrenderRT->closeSubrenderer();
			delete m_subrenderRT;
		}
		if (m_subrenderComposer != nullptr)
		{
			m_subrenderComposer->closeSubrenderer();
			delete m_subrenderComposer;
		}
	}

	/**
		*
		* \brief Add a new entity to a subrenderer
		*
		* Subrenderers manage resources and drawing of entities. Thus this function determines which subrenderer
		* fits best to the entity. The entity is then added to this subrenderer.
		*
		* \param[in] pEntity Pointer to the entity to be added
		*
		*/
	void VERenderer::addEntityToSubrenderer(VEEntity *pEntity)
	{
		veSubrenderType type = VE_SUBRENDERER_TYPE_NONE;

		switch (pEntity->getEntityType())
		{
		case VEEntity::VE_ENTITY_TYPE_NORMAL:
			if (pEntity->m_pMaterial->mapDiffuse != nullptr)
			{
				if (pEntity->m_pMaterial->mapNormal != nullptr)
				{
					type = VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP;
					break;
				}

				type = VE_SUBRENDERER_TYPE_DIFFUSEMAP;
				break;
			}
			type = VE_SUBRENDERER_TYPE_COLOR1;
			break;
			/*case VEEntity::VE_ENTITY_TYPE_CUBEMAP:
					type = VESubrender::VE_SUBRENDERER_TYPE_CUBEMAP;
					break;
				case VEEntity::VE_ENTITY_TYPE_CUBEMAP2:
					type = VESubrender::VE_SUBRENDERER_TYPE_CUBEMAP2;
					break;*/
		case VEEntity::VE_ENTITY_TYPE_SKYPLANE:
			type = VE_SUBRENDERER_TYPE_SKYPLANE;
			break;
		case VEEntity::VE_ENTITY_TYPE_TERRAIN_HEIGHTMAP:
			break;
		default:
			return;
		}

		for (uint32_t i = 0; i < m_subrenderers.size(); i++)
		{
			if (m_subrenderers[i]->getType() == type)
			{
				m_subrenderers[i]->addEntity(pEntity);
				return;
			}
		}
	}

	/**
		*
		* \brief Remove an entity from all subrenderers
		*
		* Subrenderers manage resources and drawing of entities. This function removes a given entity
		* from all subrenderers that it was associated with
		*
		* \param[in] pEntity Pointer to the entity to be removed
		*
		*/
	void VERenderer::removeEntityFromSubrenderers(VEEntity *pEntity)
	{
		if (pEntity->m_pSubrenderer != nullptr)
		{
			pEntity->m_pSubrenderer->removeEntity(pEntity);
		}
	}
} // namespace ve
