#ifndef VESYSTEMPHYSICS_H
#define VESYSTEMPHYSICS_H

#include "VGJS.h"
#include "VEECS.h"

namespace vve {

	//VeComponentPosition, VeComponentOrientation, VeComponentTransform, VeComponentCollisionShape, VeComponentBody
	template<typename... Ts>
	class VePhysicsSystem : public VeSystem<VePhysicsSystem<Ts...>> {

	protected:

	public:
		VePhysicsSystem();
	};

}

#endif

