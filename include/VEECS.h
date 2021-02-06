#ifndef VEENGINE_H
#define VEENGINE_H

namespace vve {


	template<typename T>
	struct VeComponent {

	};

	struct VePosition : public VeComponent<VePosition>{

	};

	struct VeOrientation : public VeComponent<VeOrientation> {

	};

	template<typename C>
	struct VeComponentPool {

	};


	class VeSystem {

	};



}

#endif
