#pragma once


namespace vve::sysvul {

	class VeSystem {

	public:
		VeSystem() {

		};

		~VeSystem() {

		};

		virtual void preupdate(VeHandle receiverID) {

		};

		virtual void update(VeHandle receiverID) {

		};

		void postupdate(VeHandle receiverID) {

		};

		void close(VeHandle receiverID) {

		};

	};

};

