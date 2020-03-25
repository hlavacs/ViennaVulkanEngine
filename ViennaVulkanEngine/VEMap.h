#pragma once



namespace vve {


	/**
	*
	* \brief
	*
	*
	*/
	class VeMap {
	protected:
		VeHeapMemory		m_heap;
		VeClock				m_clock;

	public:
		VeMap() : m_heap(), m_clock("Map Clock", 100) {};
		virtual ~VeMap() {};
		virtual void	operator=(const VeMap& map) { assert(false); return; };
		virtual void	clear() { assert(false); return; };
		virtual VeCount size() { assert(false); return 0; };
		bool			empty() { return size() == 0; };
		virtual VeMap*	clone() { assert(false); return nullptr; };
		virtual bool	insert(void* entry, VeIndex dir_index) { assert(false); return false; };
		virtual VeCount	erase(void* entry, VeIndex dir_index) { assert(false); return 0; };

		virtual VeIndex	find(VeHandle key) { assert(false); return VE_NULL_INDEX; };
		virtual VeIndex	find(VeHandlePair key) { assert(false); return VE_NULL_INDEX; };
		virtual VeIndex	find(VeHandleTriple key) { assert(false); return VE_NULL_INDEX; };
		virtual VeIndex	find(std::string key) { assert(false); return VE_NULL_INDEX; };

		virtual VeIndex operator[](const VeHandle &key) { return find(key); };
		virtual VeIndex operator[](const VeHandlePair &key) { return find(key); };
		virtual VeIndex operator[](const VeHandleTriple &key) { return find(key); };
		virtual VeIndex operator[](const std::string &key) { return find(key); };

		virtual VeCount	equal_range(VeHandle key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	equal_range(VeHandlePair key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	equal_range(VeHandleTriple key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	equal_range(std::string key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };

		virtual VeCount	range(VeHandle lower, VeHandle upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	range(VeHandlePair lower, VeHandlePair upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	range(VeHandleTriple lower, VeHandleTriple upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };
		virtual VeCount	range(std::string lower, std::string upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };

		virtual VeCount	getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; };

		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeHandle, VeIndex>, custom_alloc<std::pair<VeHandle, VeIndex>>>& result) { assert(false); return 0; };
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeHandlePair, VeIndex>, custom_alloc<std::pair<VeHandlePair, VeIndex>>>& result) { assert(false); return 0; };
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeHandleTriple, VeIndex>, custom_alloc<std::pair<VeHandleTriple, VeIndex>>>& result) { assert(false); return 0; };
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<std::string, VeIndex>, custom_alloc<std::pair<std::string, VeIndex>>>& result) { assert(false); return 0; };

		virtual VeCount leftJoin(VeHandle key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };
		virtual VeCount leftJoin(VeHandlePair key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };
		virtual VeCount leftJoin(VeHandleTriple key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };
		virtual VeCount leftJoin(std::string key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };
		virtual VeCount leftJoin(VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; };

		virtual void	print() { assert(false); return; };

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		VeHandle getIntFromEntry(void* entry, VeIndex offset, VeIndex num_bytes) {
			uint8_t* ptr = (uint8_t*)entry + offset;

			if (num_bytes == 4) {
				uint32_t* k1 = (uint32_t*)ptr;
				return (VeHandle)*k1;
			}
			uint64_t* k2 = (uint64_t*)ptr;
			return (VeHandle)*k2;
		};

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \param[out] key
		*
		*/
		void getKey(void* entry, VeIndex offset, VeIndex num_bytes, VeHandle& key) {
			key = getIntFromEntry(entry, offset, num_bytes);
		};

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		void getKey(void* entry, VeIndexPair offset, VeIndexPair num_bytes, VeHandlePair& key) {
			key = VeHandlePair(	getIntFromEntry(entry, offset.first, num_bytes.first),
								getIntFromEntry(entry, offset.second, num_bytes.second));
		}

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		void getKey(void* entry, VeIndexTriple offset, VeIndexTriple num_bytes, VeHandleTriple& key) {
			key = VeHandleTriple(	getIntFromEntry(entry, std::get<0>(offset), std::get<0>(num_bytes)),
									getIntFromEntry(entry, std::get<1>(offset), std::get<1>(num_bytes)),
									getIntFromEntry(entry, std::get<2>(offset), std::get<2>(num_bytes)));
		}

		/**
		*
		* \brief
		*
		* \param[in] entry
		* \param[in] offset
		* \param[in] numbytes
		* \returns
		*
		*/
		void getKey(void* entry, VeIndex offset, VeIndex num_bytes, std::string& key) {
			uint8_t* ptr = (uint8_t*)entry + offset;
			std::string* pstring = (std::string*)ptr;
			key = *pstring;
		}

	};


	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeOrderedMultimap : public VeMap {

		struct VeMapEntry {
			K			m_key;
			VeIndex		m_value = VE_NULL_INDEX;
			int32_t		m_height = 1;				///<height of subtree rooting at this node
			VeIndex		m_parent = VE_NULL_INDEX;	///<index of parent
			VeIndex		m_left = VE_NULL_INDEX;		///<index of first child with lower key
			VeIndex		m_right = VE_NULL_INDEX;	///<index of first child with larger key

			VeMapEntry() : m_key() {
				m_value = VE_NULL_INDEX;
				m_height = 0;
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};

			VeMapEntry( K key, VeIndex value ) : m_key(key), m_value(value) {
				m_height = 0;
				m_parent = VE_NULL_INDEX;	///<index of parent
				m_left = VE_NULL_INDEX;		///<index of first child with lower key
				m_right = VE_NULL_INDEX;	///<index of first child with larger key
			};

			void operator=(const VeMapEntry& entry) {
				m_key = entry.m_key;
				m_value = entry.m_value;
				m_height = entry.m_height;
				m_parent = entry.m_parent;
				m_left = entry.m_left;
				m_right = entry.m_right;
			};

			bool operator==( const VeMapEntry& entry) {
				if( m_value != VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX  )
					return m_key == entry.m_key && m_value == entry.m_value;

				return m_key == entry.m_key;
			};

			bool operator<(const VeMapEntry& entry) {
				if (m_value != VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX)
					return m_key < entry.m_key || (m_key == entry.m_key && m_value < entry.m_value);

				if (m_value == VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX) 
					return m_key == entry.m_key || m_key < entry.m_key;

				return m_key < entry.m_key;
			};

			bool operator<=(const VeMapEntry& entry) {
				return *this < entry || *this == entry;
			};

			bool operator>(const VeMapEntry& entry) {
				return !(*this == entry || *this < entry);
			};

			void print(uint32_t node, uint32_t level = 1) {
				std::cout << "L " << level << " IDX " << node;
				std::cout << " KEY " << m_key << " VAL " << m_value << " HEIGHT " << m_height;
				std::cout << " PAR " << m_parent << " LEFT " << m_left << " RIGHT " << m_right << std::endl;
			};
		};

		VeOrderedMultimap<K, I>* clone() {
			VeOrderedMultimap<K, I>* map = new VeOrderedMultimap<K, I>(*this);
			return map;
		};

	protected:
		I						m_offset;					///
		I						m_num_bytes;				///
		VeVector<VeMapEntry>	m_map;						///
		VeIndex					m_root = VE_NULL_INDEX;


		//---------------------------------------------------------------------------
		//subtree properties

		int32_t getHeight(VeIndex node) {
			if (node == VE_NULL_INDEX)
				return 0;

			return m_map[node].m_height;
		}

		int32_t getBalance(VeIndex node) {
			if (node == VE_NULL_INDEX)
				return 0;

			return getHeight(m_map[node].m_left) - getHeight(m_map[node].m_right);
		}

		//---------------------------------------------------------------------------
		//rotate subtrees

		void replaceChild(VeIndex parent, VeIndex old_child, VeIndex new_child) {
			if (parent == VE_NULL_INDEX) {
				m_root = new_child;
				return;
			}

			if (new_child != VE_NULL_INDEX)
				m_map[new_child].m_parent = parent;

			if (old_child == VE_NULL_INDEX) //cannot identify right child so return
				return;

			if (m_map[parent].m_left == old_child) {
				m_map[parent].m_left = new_child;
				return;
			}
			m_map[parent].m_right = new_child;
		};

		void replaceLeftChild(VeIndex parent, VeIndex new_child) {
			if (parent == VE_NULL_INDEX)
				return;
			m_map[parent].m_left = new_child;
			if (new_child == VE_NULL_INDEX)
				return;
			m_map[new_child].m_parent = parent;
		};

		void replaceRightChild(VeIndex parent, VeIndex new_child) {
			if (parent == VE_NULL_INDEX)
				return;
			m_map[parent].m_right = new_child;
			if (new_child == VE_NULL_INDEX)
				return;
			m_map[new_child].m_parent = parent;
		};

		VeIndex rightRotate(VeIndex y) {
			VeIndex x = m_map[y].m_left;
			VeIndex T2 = m_map[x].m_right;
			VeIndex z = m_map[y].m_parent;

			replaceRightChild(x, y);
			replaceLeftChild(y, T2);

			// Update heights  
			m_map[y].m_height = std::max(getHeight(m_map[y].m_left), getHeight(m_map[y].m_right)) + 1;
			m_map[x].m_height = std::max(getHeight(m_map[x].m_left), getHeight(m_map[x].m_right)) + 1;

			// Return new root  
			return x;
		}

		VeIndex leftRotate( VeIndex x ) {
			VeIndex y = m_map[x].m_right;
			VeIndex T2 = m_map[y].m_left;
			VeIndex z = m_map[x].m_parent;

			replaceRightChild(x, T2);
			replaceLeftChild(y, x);

			// Update heights  
			m_map[x].m_height = std::max(getHeight(m_map[x].m_left), getHeight(m_map[x].m_right)) + 1;
			m_map[y].m_height = std::max(getHeight(m_map[y].m_left), getHeight(m_map[y].m_right)) + 1;

			// Return new root  
			return y;
		}

		//---------------------------------------------------------------------------
		//insert

		VeIndex insert( VeIndex node, VeIndex index ) {
			if (node == VE_NULL_INDEX) {
				m_map[index].m_height = 1;
				return index;
			}

			VeIndex& left  = m_map[node].m_left;
			VeIndex& right = m_map[node].m_right;

			if (m_map[index] < m_map[node]) {
				replaceLeftChild(node, insert(left, index) );
			}
			else if (m_map[index] > m_map[node]) {
				replaceRightChild(node, right = insert(right, index) );
			}
			else {
				return node;
			}

			m_map[node].m_height = 1 + std::max(getHeight(left), getHeight(right));

			int32_t balance = getBalance(node);

			// Left Left Case  
			if (balance > 1 && m_map[index] < m_map[left]) {
				return rightRotate(node);
			}

				// Right Right Case  
			if (balance < -1 && m_map[index] > m_map[right]) {
				return leftRotate(node);
			}

			// Left Right Case  
			if (balance > 1 && m_map[index] > m_map[left]) {
				replaceLeftChild( node, leftRotate(left) );
				return rightRotate(node);
			}

			// Right Left Case  
			if (balance < -1 && m_map[node] < m_map[right]) {
				replaceRightChild( node, rightRotate(right) );
				return leftRotate(node);
			}

			return node;
		};


		//---------------------------------------------------------------------------
		//find

		VeIndex findEntry(VeIndex node, VeMapEntry &entry ) {
			if (node == VE_NULL_INDEX)
				return VE_NULL_INDEX;

			if (m_map[node] == entry )
				return node;

			if (entry < m_map[node]) 
				return findEntry(m_map[node].m_left, entry);

			return findEntry(m_map[node].m_right, entry);
		};


		VeIndex findKeyValue(K& key, VeIndex value) {
			VeMapEntry entry{ key, value };
			return findEntry(m_root, entry);
		};


		VeIndex findKey(K& key) {
			return findKeyValue(key, VE_NULL_INDEX);
		};


		//---------------------------------------------------------------------------
		//delete



		void deleteIndex(VeIndex index) {
			VeIndex last = m_map.size() - 1;

			if (index < last) {
				m_map.swap(index, last);

				VeIndex parent = m_map[index].m_parent;
				VeIndex left = m_map[index].m_left;
				VeIndex right = m_map[index].m_right;

				replaceChild(parent, last, index);		//reset child
				replaceChild(index, left, left);		//reset parent
				replaceChild(index, right, right);		//reset parent
			}
			m_map.pop_back();
		};


		VeIndex findMin(VeIndex node) {
			assert(node != VE_NULL_INDEX);
			while (m_map[node].m_left != VE_NULL_INDEX) 
				node = m_map[node].m_left;
			return node;
		};

		void replaceKeyValue(VeIndex dst, VeIndex src) {
			m_map[dst].m_key = m_map[src].m_key;
			m_map[dst].m_value = m_map[src].m_value;
		}

		VeIndex deleteEntry(VeIndex node, VeMapEntry& entry, 
							std::vector<VeIndex, custom_alloc<VeIndex>> &del_indices) {

			if (node == VE_NULL_INDEX)
				return node;

			VeIndex& parent = m_map[node].m_parent;
			VeIndex& left = m_map[node].m_left;
			VeIndex& right = m_map[node].m_right;

			if (entry < m_map[node]) {
				replaceLeftChild(node, deleteEntry(left, entry, del_indices));
			}
			else if (m_map[node] < entry) {
				replaceRightChild(node, deleteEntry(right, entry, del_indices));
			}
			else {	//have found the node
				if (left != VE_NULL_INDEX && right != VE_NULL_INDEX) {
					VeIndex min_idx = findMin(right);
					replaceKeyValue(node, min_idx);
					replaceRightChild(node, deleteEntry(right, m_map[min_idx], del_indices) );
				}
				else {
					VeIndex temp = left != VE_NULL_INDEX ? left : right;
					if (temp == VE_NULL_INDEX) {
						del_indices.push_back(node);
						return VE_NULL_INDEX;
					}
					replaceChild(parent, node, temp);					
					del_indices.push_back(node);
					node = temp;				//return successor
				}
			}

			if (node == VE_NULL_INDEX)
				return node;

			m_map[node].m_height = 1 + std::max(getHeight(left), 
												getHeight(right));

			int balance = getBalance(node);

			// If this node becomes unbalanced, then there are 4 cases  

			// Left Left Case  
			if (balance > 1 && getBalance(left) >= 0)
				return rightRotate(node);

			// Left Right Case  
			if (balance > 1 && getBalance(left) < 0) {
				replaceLeftChild( node, leftRotate(left) );
				return rightRotate(node);
			}

			// Right Right Case  
			if (balance < -1 && getBalance(right) <= 0)
				return leftRotate(node);

			// Right Left Case  
			if (balance < -1 && getBalance(right) > 0) {
				replaceRightChild(node, rightRotate(right));
				return leftRotate(node);
			}

			return node;
		};

		//---------------------------------------------------------------------------
		//get indices

		VeCount getAllIndices(VeIndex node, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (node == VE_NULL_INDEX)
				return 0;

			VeCount num = getAllIndices(m_map[node].m_left, result);
			result.push_back(m_map[node].m_value);
			num += getAllIndices(m_map[node].m_right, result);
			return num + 1;
		};

		VeCount getAllKeyValuePairs(VeIndex node, std::vector<std::pair<K,VeIndex>, custom_alloc<std::pair<K, VeIndex>>>& result) {
			if (node == VE_NULL_INDEX)
				return 0;

			VeCount num = getAllKeyValuePairs(m_map[node].m_left, result);
			result.push_back( std::pair<K,VeIndex>(m_map[node].m_key, m_map[node].m_value) );
			num += getAllKeyValuePairs(m_map[node].m_right, result);
			return num + 1;
		};

		VeCount equal_range(VeIndex node, VeMapEntry &entry, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (node == VE_NULL_INDEX)
				return 0;

			VeCount num = 0;

			if(entry <= m_map[node])
				num += equal_range(m_map[node].m_left, entry, result);

			if (entry.m_key == m_map[node].m_key) {
				result.push_back(node);
				++num;
			}

			if (m_map[node] <= entry)
				num += equal_range(m_map[node].m_right, entry, result);
			return num;
		};


		VeCount range(VeIndex node, VeMapEntry& lower, VeMapEntry upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (node == VE_NULL_INDEX)
				return 0;

			if (m_map[node] < lower || m_map[node] > upper)
				return 0;

			VeCount num = 0;

			if (lower < m_map[node])
				num += range(m_map[node].m_left, lower, upper, result);

			if (lower <= m_map[node] && m_map[node] <= upper) {
				result.push_back(m_map[node].m_value);
				++num;
			}

			if (m_map[node] < upper)
				num += range(m_map[node].m_right, lower, upper, result);

			return num;
		};


		//---------------------------------------------------------------------------
		//debug

		void printTree(VeIndex node, VeIndex level) {
			if (node == VE_NULL_INDEX)
				return;

			printTree(m_map[node].m_left, level + 1);

			std::cout << std::setw(level) << " ";
			m_map[node].print(node, level);
			//std::cout << "IDX " << node << " KEY " << m_map[node].m_key << " VAL " << m_map[node].m_value << std::endl;

			printTree(m_map[node].m_right, level + 1);
		};


	public:

		VeOrderedMultimap(I offset, I num_bytes, bool memcopy = false) : VeMap(), m_map(memcopy), m_offset(offset), m_num_bytes(num_bytes) {};
		VeOrderedMultimap(const VeOrderedMultimap<K, I>& map) : VeMap(), m_map(map.m_map), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {};
		virtual	~VeOrderedMultimap() {};

		virtual void operator=(const VeMap& basemap) override {
			VeOrderedMultimap<K, I>* map = &((VeOrderedMultimap<K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_map = map->m_map;
		};

		virtual void clear() override {
			m_root = VE_NULL_INDEX;
			m_map.clear();
		};

		VeCount size() override {
			return m_map.size();
		};

		std::pair<K, VeIndex> getKeyValuePair(VeIndex num) {
			VeIndex value = find(m_map[num].m_key);
			return { m_map[num].m_key, value};
		};

		void printTree() {
			std::cout << "Root " << m_root << std::endl;
			printTree(m_root, 1);
			std::cout << std::endl;
		};

		void print() override {
			std::cout << "Num " << m_map.size() << " Root " << m_root << std::endl;
			for (uint32_t i = 0; i < m_map.size(); ++i) {
				m_map[i].print(i);
			}
			std::cout << std::endl;
		};

		virtual VeIndex find(K key) override {
			VeIndex index = findKey(key);
			return index == VE_NULL_INDEX ? VE_NULL_INDEX : m_map[index].m_value;
		};

		virtual VeCount equal_range(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeMapEntry entry(key, VE_NULL_INDEX);
			return equal_range(m_root, entry, result);
		};

		virtual VeCount range(K lower, K upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeMapEntry mlower(lower, VE_NULL_INDEX), mupper(upper, VE_NULL_INDEX);
			return range(m_root, mlower, mupper, result);
		};

		virtual VeCount getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			return getAllIndices(m_root, result);
		};

		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<K, VeIndex>, custom_alloc<std::pair<K, VeIndex>>>& result) override { 
			return getAllKeyValuePairs(m_root, result);
		};

		virtual VeCount leftJoin(K key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) override {
			std::vector < VeIndex, custom_alloc<VeIndex> > result1(&m_heap);
			equal_range(key, result1);

			std::vector < VeIndex, custom_alloc<VeIndex> > result2(&m_heap);
			other.equal_range(key, result2);

			for (uint32_t i = 0; i < result1.size(); ++i) {
				for (uint32_t j = 0; j < result2.size(); ++j) {
					result.emplace_back( result1[i], result2[j] );
				}
			}
			return (VeCount)(result1.size()*result2.size());
		};

		virtual VeCount leftJoin(VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) override {
			if (size() == 0 ||other.size()==0)
				return 0;

			VeCount num = 0;
			std::vector<std::pair<K,VeIndex>, custom_alloc<std::pair<K, VeIndex>> > map1(&m_heap);
			map1.reserve(size());
			getAllKeyValuePairs(map1);

			K last = map1[0].first;
			std::vector < VeIndex, custom_alloc<VeIndex> > result2(&m_heap);
			other.equal_range(last, result2);

			for (uint32_t i = 0; i < map1.size(); ++i) {
				K key = map1[i].first;
				if (key != last) {
					result2.clear();
					other.equal_range(key, result2);
					last = key;
				}
				for (uint32_t j = 0; j < result2.size(); ++j) {
					result.emplace_back( map1[i].second, result2[j] );
					++num;
				}
			}

			return num;
		};

		virtual bool insert(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex last = m_map.size();
			m_map.emplace_back({ key, dir_index });
			m_root = insert(m_root, last);
			m_map[m_root].m_parent = VE_NULL_INDEX;
			return true;
		};

		virtual VeCount erase(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeMapEntry mentry(key, dir_index);
			std::vector<VeIndex, custom_alloc<VeIndex>> del_indices(&m_heap);
			VeCount res = m_map.size();
			m_root = deleteEntry(m_root, mentry, del_indices );
			if( m_root != VE_NULL_INDEX)
				m_map[m_root].m_parent = VE_NULL_INDEX;

			for (auto idx : del_indices)
				deleteIndex(idx);

			return res - m_map.size();	//so many elements have been deleted
		};

	};


	//----------------------------------------------------------------------------------

	template<typename S, typename T>
	struct std::hash<std::pair<S, T>>
	{
		inline size_t operator()(const std::pair<S, T>& val) const
		{
			size_t seed = 0;
			seed ^= std::hash<S>()(val.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(val.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};

	template<typename S, typename T, typename U>
	struct std::hash<std::tuple<S, T, U>>
	{
		inline size_t operator()(const std::tuple<S, T, U>& val) const
		{
			size_t seed = 0;
			seed ^= std::hash<S>()(std::get<0>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(std::get<1>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(std::get<2>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};


	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeHashedMultimap : public VeMap {

		struct VeMapEntry {
			K			m_key;
			VeIndex		m_value = VE_NULL_INDEX;
			VeIndex		m_next = VE_NULL_INDEX;		///<next node with same hash value or free

			VeMapEntry() : m_key() {
				m_value = VE_NULL_INDEX;
				m_next = VE_NULL_INDEX;
			};

			VeMapEntry(K key, VeIndex value) : m_key(key), m_value(value) {
				m_next = VE_NULL_INDEX;
			};

			void operator=(const VeMapEntry& entry) {
				m_key = entry.m_key;
				m_value = entry.m_value;
				m_next = entry.m_next;
			};

			bool operator==(const VeMapEntry& entry) {
				if (m_value != VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX)
					return m_key == entry.m_key && m_value == entry.m_value;

				return m_key == entry.m_key;
			};

			void print(uint32_t node, uint32_t level = 1) {
				std::cout << "L " << level << " IDX " << node;
				std::cout << " KEY " << m_key << " VAL " << m_value << " NEXT " << m_next << std::endl;
			};
		};

		VeHashedMultimap<K, I>* clone() {
			VeHashedMultimap<K, I>* map = new VeHashedMultimap<K, I>(*this);
			return map;
		};

	protected:
		I						m_offset;			///
		I						m_num_bytes;		///
		VeCount					m_num_entries = 0;
		VeVector<VeMapEntry>	m_entries;			///
		VeIndex					m_first_free_entry = VE_NULL_INDEX;
		VeVector<VeIndex>		m_map;

	public:

		VeHashedMultimap(I offset, I num_bytes, bool memcopy = false) : 
			VeMap(), m_num_entries(0), m_map(memcopy), m_offset(offset), m_num_bytes(num_bytes) {
			VeIndex size = 1 << 10;
			m_entries.reserve(size);
			m_map.reserve(size);
			for (VeIndex i = 0; i < size; ++i)
				m_map.push_back((VeIndex)VE_NULL_INDEX);
		};

		VeHashedMultimap(const VeHashedMultimap<K, I>& map) : 
			VeMap(), m_num_entries(map.m_num_entries), m_entries(map.m_entries), m_first_free_entry(map.m_first_free_entry),
			m_map(map.m_map), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {
		};

		virtual	~VeHashedMultimap() {};

		virtual void operator=(const VeMap& basemap) {
			VeHashedMultimap<K, I>* map = &((VeHashedMultimap<K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_entries = map->m_entries;
			m_first_free_entry = map->m_first_free_entry;
			m_map = map->m_map;
		};

		void clear() override {
			m_entries.clear();
			m_first_free_entry = VE_NULL_INDEX;
			for (VeIndex i = 0; i < m_map.size(); ++i)
				m_map[i] = VE_NULL_INDEX;
		};

		VeCount size() override {
			return m_num_entries;
		};

		void print() override {
			std::vector<std::pair<K, VeIndex>, custom_alloc<std::pair<K, VeIndex>>> result(&m_heap);
			getAllKeyValuePairs(result);
			for (auto p : result) {
				std::cout << "KEY " << p.first << " VAL " << p.second << std::endl;
			}
			std::cout << std::endl;
		};

		virtual VeIndex find(K key) override {
			VeIndex idx = std::hash<K>()(key) % m_map.size();
			for (VeIndex index = m_map[idx]; index != VE_NULL_INDEX; index = m_entries[index].m_next) {
				if (m_entries[index].m_key == key) {
					return m_entries[index].m_value;
				}
			}
			return VE_NULL_INDEX;
		};

		virtual VeCount equal_range(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeCount num = 0;
			VeIndex idx = std::hash<K>()(key) % m_map.size();
			for (VeIndex index = m_map[idx]; index != VE_NULL_INDEX; index = m_entries[index].m_next) {
				if (m_entries[index].m_key == key) {
					result.push_back(m_entries[index].m_value);
					++num;
				}
			}
			return num;
		};

		virtual VeCount getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeCount num = 0;
			std::for_each(m_map.begin(), m_map.end(), [&]( VeIndex index ) {
				while (index != VE_NULL_INDEX) {
					result.emplace_back( m_entries[index].m_value );
					++num;
					index = m_entries[index].m_next;
				};
			});
			return num;
		};

		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<K, VeIndex>, custom_alloc<std::pair<K, VeIndex>>>& result) override {
			VeCount num = 0;
			std::for_each(m_map.begin(), m_map.end(), [&](VeIndex index) {
				while (index != VE_NULL_INDEX) {
					result.emplace_back(m_entries[index].m_key, m_entries[index].m_value);
					++num;
					index = m_entries[index].m_next;
				};
			});
			return num;;
		};

		virtual VeCount leftJoin(K key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) override {
			std::vector < VeIndex, custom_alloc<VeIndex> > result1(&m_heap);
			equal_range(key, result1);

			std::vector < VeIndex, custom_alloc<VeIndex> > result2(&m_heap);
			other.equal_range(key, result2);

			for (uint32_t i = 0; i < result1.size(); ++i) {
				for (uint32_t j = 0; j < result2.size(); ++j) {
					result.emplace_back( result1[i], result2[j] );
				}
			}
			return (VeCount)(result1.size() * result2.size());
		};

		virtual VeCount leftJoin(VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) override {
			if (size() == 0 || other.size() == 0)
				return 0;

			std::vector<std::pair<K, VeIndex>, custom_alloc<std::pair<K, VeIndex>> > map1(&m_heap);
			map1.reserve(size());
			getAllKeyValuePairs(map1);

			std::set<K, std::less<K>, custom_alloc<K>> keys(&m_heap);
			for (auto kvp : map1) {
				keys.insert(kvp.first);
			}

			VeCount num = 0;
			for (auto key : keys) {
				num += leftJoin(key, other, result);
			}

			return num;
		};

		virtual bool insert(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex idx = std::hash<K>()(key) % m_map.size();

			VeMapEntry nentry(key, dir_index);
			VeIndex new_index = m_first_free_entry;
			if (m_first_free_entry != VE_NULL_INDEX) {
				m_first_free_entry = m_entries[new_index].m_next;
				m_entries[new_index] = nentry;
			} else {
				m_entries.emplace_back(nentry);
				new_index = m_entries.size() - 1;
			}
			
			++m_num_entries;
			if (m_map[idx] == VE_NULL_INDEX) {
				m_map[idx] = new_index;
				return true;
			}
			m_entries[new_index].m_next = m_map[idx];
			m_map[idx] = new_index;
			return true;
		};

		virtual VeCount erase(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex hidx = std::hash<K>()(key) % m_map.size();
			VeIndex index = m_map[hidx];
			VeIndex last = VE_NULL_INDEX;
			while (index != VE_NULL_INDEX) {
				if (m_entries[index].m_key == key && m_entries[index].m_value == dir_index) {
					if (index == m_map[hidx]) {
						m_map[hidx] = m_entries[index].m_next;
					} else {
						m_entries[last].m_next = m_entries[index].m_next;
					}
					m_entries[index].m_next = m_first_free_entry;
					m_first_free_entry = index;
					m_num_entries--;
					return 1;
				}
				last = index;
				index = m_entries[index].m_next;
			};
			return 0;
		};

	};


	namespace map {
		void testMap();
	};

}


