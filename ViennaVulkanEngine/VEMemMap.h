#pragma once



namespace vve {


	/**
	*
	* \brief Base class of all VeMaps
	*
	* VeMap is the base class of all maps used in tables. Maps are used for sorting and quickly finding items
	* based on their keys. The base class offers the whole interface for all possible key types, but does not
	* implement them. This interface is similar to STL containers, but VeMaps are not STL compatible - as of yet.
	*
	* VeMaps are different to STL containers because in principle not the key, but the values must be unique.
	* The values store indices of directory enntries in tables, and these are unique. Keys do not have to be unique,
	* though some derived classes enforce this. Since values must be unique, so are (key,value) tuples.
	* Thus, when deleting an entry, this mmust be done via the (key,value) tuple, at least in Multimaps.
	*
	*/
	class VeMap {
	protected:
		VeHeapMemory		m_heap;		///< scrap memory for temporary sorting, merging containers
		VeClock				m_clock;	///< a clock for measuring timings

	public:
		VeMap() : m_heap(), m_clock("Map Clock", 100) {};	///< VeMap class constructor
		virtual ~VeMap() {};								///< VeMap class destructor
		virtual void	operator=(const VeMap& map) { assert(false); return; }; ///< assignment operator
		virtual void	clear() { assert(false); return; };						///< delete all entries in the map
		virtual VeCount size() { assert(false); return VeCount(0); };					///< \returns number of entries in the map
		bool			empty() { return size() == VeCount(0); };						///<  true, if the map is empty
		virtual VeMap*	clone() { assert(false); return nullptr; };				///< create a clone of this map
		virtual bool	insert(void* entry, VeValue value) { assert(false); return false; };	///< insert a key-value pair into the map
		virtual VeCount	erase(void* entry, VeValue value) { assert(false); return VeCount(0); };	///< delete a (key,value) pair from the map

		virtual VeValue	find(VeKey key) { assert(false); return VE_NULL_VALUE; }; ///< find an entry, \returns its handle
		virtual VeValue	find(VeKeyPair key) { assert(false); return VE_NULL_VALUE; }; ///< find an entry, \returns its handle
		virtual VeValue	find(VeKeyTriple key) { assert(false); return VE_NULL_VALUE; }; ///< find an entry, \returns its handle
		virtual VeValue	find(std::string key) { assert(false); return VE_NULL_VALUE; }; ///< find an , \returns its handle

		virtual VeValue operator[](const VeKey &key) { return find(key); };  ///< find an entry, \returns its handle
		virtual VeValue operator[](const VeKeyPair &key) { return find(key); }; ///< find an entry, \returns its handle
		virtual VeValue operator[](const VeKeyTriple &key) { return find(key); }; ///< find an entry, \returns its handle
		virtual VeValue operator[](const std::string &key) { return find(key); }; ///< find an, \returns its handle

		virtual VeCount	equal_range(VeKey key, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries
		virtual VeCount	equal_range(VeKeyPair key, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries
		virtual VeCount	equal_range(VeKeyTriple key, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries
		virtual VeCount	equal_range(std::string key, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries

		virtual VeCount	range(VeKey lower, VeKey upper, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries
		virtual VeCount	range(VeKeyPair lower, VeKeyPair upper, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries
		virtual VeCount	range(VeKeyTriple lower, VeKeyTriple upper, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries
		virtual VeCount	range(std::string lower, std::string upper, std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///<  find entries

		virtual VeCount	getAllValues(std::vector<VeValue, custom_alloc<VeValue>>& result) { assert(false); return VeCount(0); }; ///< return all indices

		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeKey, VeValue>, custom_alloc<std::pair<VeKey, VeValue>>>& result) { assert(false); return VeCount(0); }; ///< return all key-value pairs
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeKeyPair, VeValue>, custom_alloc<std::pair<VeKeyPair, VeValue>>>& result) { assert(false); return VeCount(0); }; ///< return all key-value pairs
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeKeyTriple, VeValue>, custom_alloc<std::pair<VeKeyTriple, VeValue>>>& result) { assert(false); return VeCount(0); }; ///< return all key-value pairs
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<std::string, VeValue>, custom_alloc<std::pair<std::string, VeValue>>>& result) { assert(false); return VeCount(0); }; ///< return all key-value pairs

		//virtual VeCount leftJoin(VeKey key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return VeCount(0); }; ///< left join another map
		//virtual VeCount leftJoin(VeKeyPair key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return VeCount(0); }; ///< left join another map
		//virtual VeCount leftJoin(VeKeyTriple key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return VeCount(0); }; ///< left join another map
		//virtual VeCount leftJoin(std::string key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return VeCount(0); }; ///< left join another map
		//virtual VeCount leftJoin(VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return VeCount(0); }; ///< left join another map

		virtual void	print() { assert(false); return; }; ///< print debug information

		/**
		*
		* \brief Extract an uint value from an entry
		*
		* \param[in] entry Pointer to the entry
		* \param[in] offset Number of bytes the uint is away from the start
		* \param[in] numbytes Size in bytes if the uint. Can be 4 or 8.
		* \returns the value of the uint.
		*
		*/
		VeKey getKeyFromEntry(void* entry, VeIndex offset, VeIndex num_bytes) {
			uint8_t* ptr = (uint8_t*)entry + offset;

			if (num_bytes == 4) {
				uint32_t* k1 = (uint32_t*)ptr;
				return (VeKey)*k1;
			}
			uint64_t* k2 = (uint64_t*)ptr;
			return (VeKey)*k2;
		};

		/**
		*
		* \brief Get value of a uint key from a table entry. 
		*
		* \param[in] entry Pointer to the entry
		* \param[in] offset Number of bytes the uint is away from the start
		* \param[in] numbytes Size in bytes if the uint. Can be 4 or 8.
		* \param[out] key The value of the uint.
		*
		*/
		void getKey(void* entry, VeIndex offset, VeIndex num_bytes, VeKey& key) {
			key = getKeyFromEntry(entry, offset, num_bytes);
		};

		/**
		*
		* \brief Get key value of a VeHandle pair from a table entry
		*
		* \param[in] entry Pointer to the entry
		* \param[in] offset Number of bytes the uints are away from the start
		* \param[in] numbytes Sizes in bytes if the uints. Can be 4 or 8.
		* \param[out] key The values of the uints.
		*
		*/
		void getKey(void* entry, VeIndexPair offset, VeIndexPair num_bytes, VeKeyPair& key) {
			key = VeKeyPair( getKeyFromEntry(entry, offset.first, num_bytes.first),
							 getKeyFromEntry(entry, offset.second, num_bytes.second) );
		}

		/**
		*
		* \brief Get key value of a VeHandle triple from a table entry
		*
		* \param[in] entry Pointer to the entry
		* \param[in] offset Number of bytes the uints are away from the start
		* \param[in] numbytes Sizes in bytes if the uints. Can be 4 or 8.
		* \param[out] key The values of the uints.
		*
		*/
		void getKey(void* entry, VeIndexTriple offset, VeIndexTriple num_bytes, VeKeyTriple& key) {
			key = VeKeyTriple(	getKeyFromEntry(entry, std::get<0>(offset), std::get<0>(num_bytes)),
								getKeyFromEntry(entry, std::get<1>(offset), std::get<1>(num_bytes)),
								getKeyFromEntry(entry, std::get<2>(offset), std::get<2>(num_bytes)));
		}

		/**
		*
		* \brief Get the string value of a key from a table entry
		*
		* \param[in] entry Pointer to the table entry.
		* \param[in] offset Offset from start of the string.
		* \param[in] numbytes Must be 0 for a string.
		* \returns the string found there.
		*
		*/
		void getKey(void* entry, VeIndex offset, VeIndex num_bytes, std::string& key) {
			uint8_t* ptr = (uint8_t*)entry + offset;
			std::string* pstring = (std::string*)ptr;
			key = *pstring;
		}

	};



};



#include "VEMemMapSTL.h"
#include "VEMemMapOrdered.h"
#include "VEMemMapHashed.h"
#include "VEMemMapSlot.h"




