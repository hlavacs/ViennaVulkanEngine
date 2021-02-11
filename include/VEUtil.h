#ifndef VEUTIL_H
#define VEUTIL_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>


namespace vve {

	using index_t = vgjs::int_type<size_t, struct P0, std::numeric_limits<size_t>::max()>;
	using counter_t = vgjs::int_type<size_t, struct P1, std::numeric_limits<size_t>::max()>;


	class VeMonostate {
	protected:
		static inline std::atomic<uint32_t> m_init_counter = 0;

		bool init() {
			if (m_init_counter > 0) return false;
			auto cnt = m_init_counter.fetch_add(1);
			if (cnt > 0) return false;
			return true;
		};

	};


	//https://www.fluentcpp.com/2017/05/19/crtp-helper/

	template <typename T, template<typename...> class crtpType>
	struct VeCRTP {
		T& underlying() { return static_cast<T&>(*this); }
		T const& underlying() const { return static_cast<T const&>(*this); }

	protected:
		VeCRTP() {};
		friend crtpType<T>;
	};



	class VeSpinLockRead;
	class VeSpinLockWrite;

	class VeReadWriteMutex {
		friend class VeSpinLockRead;
		friend class VeSpinLockWrite;

	protected:
		std::atomic<uint32_t> m_read = 0;
		std::atomic<uint32_t> m_write = 0;
	public:
		VeReadWriteMutex() = default;
	};


	class VeSpinLockWrite {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		VeReadWriteMutex& m_spin_mutex;

	public:
		VeSpinLockWrite(VeReadWriteMutex& spin_mutex) : m_spin_mutex(spin_mutex) {
			uint32_t cnt = 0;
			uint32_t w;

			do {
				w = m_spin_mutex.m_write.fetch_add(1);			//prohibit other readers and writers
				if (w > 1) {									//someone has the write lock?
					m_spin_mutex.m_write--;						//undo and try again
				}
				else
					break;	//you got the ticket, so go on. new readers and writers are blocked now

				if (++cnt > m_max_cnt) {					//spin or sleep?
					cnt = 0;
					std::this_thread::sleep_for(100ns);		//might sleep a little to take stress from CPU
				}
			} while (true);	//go back and try again

			//a new reader might have slipped into here, or old readers exist
			cnt = 0;
			while (m_spin_mutex.m_read.load() > 0) {		//wait for existing readers to finish
				if (++cnt > m_max_cnt) {
					cnt = 0;
					std::this_thread::sleep_for(100ns);
				}
			}
		}

		~VeSpinLockWrite() {
			m_spin_mutex.m_write--;
		}
	};


	class VeSpinLockRead {
	protected:
		static const uint32_t m_max_cnt = 1 << 10;
		VeReadWriteMutex& m_spin_mutex;

	public:
		VeSpinLockRead(VeReadWriteMutex& spin_mutex) : m_spin_mutex(spin_mutex) {
			uint32_t cnt = 0;

			do {
				while (m_spin_mutex.m_write.load() > 0) {	//wait for writers to finish
					if (++cnt > m_max_cnt) {
						cnt = 0;
						std::this_thread::sleep_for(100ns);//might sleep a little to take stress from CPU
					}
				}
				//writer might have joined until here
				m_spin_mutex.m_read++;	//announce yourself as reader

				if (m_spin_mutex.m_write.load() == 0) { //still no writer?
					break;
				}
				else {
					m_spin_mutex.m_read--; //undo reading and try again
					if (++cnt > m_max_cnt) {
						cnt = 0;
						std::this_thread::sleep_for(100ns);//might sleep a little to take stress from CPU
					}
				}
			} while (true);
		}

		~VeSpinLockRead() {
			m_spin_mutex.m_read--;
		}
	};


};


#endif

