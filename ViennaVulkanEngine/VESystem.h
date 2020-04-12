#pragma once


namespace vve::sysvul {

	class VeSystem {

	protected:
		std::string m_systemName = "";
		VeHandle m_systemHandle = VE_NULL_HANDLE;

		VeHandle m_subscribePreupdateID = VE_NULL_HANDLE;
		VeHandle m_subscribeUpdateID = VE_NULL_HANDLE;
		VeHandle m_subscribePostupdateID = VE_NULL_HANDLE;
		VeHandle m_subscribeCloseID = VE_NULL_HANDLE;

	public:
		VeSystem( std::string name, bool subscribe = true) {
			m_systemName = name;
			//syseng::registerEntity(m_systemName);
			//m_systemHandle = syseng::getEntityHandle(m_systemName);

			if (subscribe) {

			}
		};

		virtual ~VeSystem() {};
		virtual void preupdate(VeHandle receiverID) {};
		virtual void update(VeHandle receiverID) {};
		virtual void postupdate(VeHandle receiverID) {};
		virtual void close(VeHandle receiverID) {
			if (m_subscribePreupdateID != VE_NULL_HANDLE) {

			}
			if (m_subscribeUpdateID != VE_NULL_HANDLE) {

			}
			if (m_subscribePostupdateID != VE_NULL_HANDLE) {

			}
			if (m_subscribeCloseID != VE_NULL_HANDLE) {

			}
		};
	};

};

