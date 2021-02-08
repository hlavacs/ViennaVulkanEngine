#ifndef VESYSTEMPHYSICS_H
#define VESYSTEMPHYSICS_H

#include "VGJS.h"
#include "VEECS.h"

namespace vve {

	using VePhysicsSystemComponentList = tl::type_list<VeComponentPosition, VeComponentOrientation, VeComponentTransform, VeComponentCollisionShape, VeComponentBody>;
	
	class VePhysicsSystem : public VeSystem<VePhysicsSystem, VePhysicsSystemComponentList> {
	protected:

	public:
		VePhysicsSystem();
	};

}

#endif

