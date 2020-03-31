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
		virtual VeCount size() { assert(false); return 0; };					///< \returns number of entries in the map
		bool			empty() { return size() == 0; };						///<  true, if the map is empty
		virtual VeMap*	clone() { assert(false); return nullptr; };				///< create a clone of this map
		virtual bool	insert(void* entry, VeIndex dir_index) { assert(false); return false; };	///< insert a key-value pair into the map
		virtual VeCount	erase(void* entry, VeIndex dir_index) { assert(false); return 0; };	///< delete a (key,value) pair from the map

		virtual VeIndex	find(VeHandle key) { assert(false); return VE_NULL_INDEX; }; ///< find an entry, \returns its handle
		virtual VeIndex	find(VeHandlePair key) { assert(false); return VE_NULL_INDEX; }; ///< find an entry, \returns its handle
		virtual VeIndex	find(VeHandleTriple key) { assert(false); return VE_NULL_INDEX; }; ///< find an entry, \returns its handle
		virtual VeIndex	find(std::string key) { assert(false); return VE_NULL_INDEX; }; ///< find an , \returns its handle

		virtual VeIndex operator[](const VeHandle &key) { return find(key); };  ///< find an entry, \returns its handle
		virtual VeIndex operator[](const VeHandlePair &key) { return find(key); }; ///< find an entry, \returns its handle
		virtual VeIndex operator[](const VeHandleTriple &key) { return find(key); }; ///< find an entry, \returns its handle
		virtual VeIndex operator[](const std::string &key) { return find(key); }; ///< find an, \returns its handle

		virtual VeCount	equal_range(VeHandle key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries
		virtual VeCount	equal_range(VeHandlePair key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries
		virtual VeCount	equal_range(VeHandleTriple key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries
		virtual VeCount	equal_range(std::string key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries

		virtual VeCount	range(VeHandle lower, VeHandle upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries
		virtual VeCount	range(VeHandlePair lower, VeHandlePair upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries
		virtual VeCount	range(VeHandleTriple lower, VeHandleTriple upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries
		virtual VeCount	range(std::string lower, std::string upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///<  find entries

		virtual VeCount	getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) { assert(false); return 0; }; ///< return all indices

		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeHandle, VeIndex>, custom_alloc<std::pair<VeHandle, VeIndex>>>& result) { assert(false); return 0; }; ///< return all key-value pairs
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeHandlePair, VeIndex>, custom_alloc<std::pair<VeHandlePair, VeIndex>>>& result) { assert(false); return 0; }; ///< return all key-value pairs
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<VeHandleTriple, VeIndex>, custom_alloc<std::pair<VeHandleTriple, VeIndex>>>& result) { assert(false); return 0; }; ///< return all key-value pairs
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<std::string, VeIndex>, custom_alloc<std::pair<std::string, VeIndex>>>& result) { assert(false); return 0; }; ///< return all key-value pairs

		virtual VeCount leftJoin(VeHandle key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; }; ///< left join another map
		virtual VeCount leftJoin(VeHandlePair key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; }; ///< left join another map
		virtual VeCount leftJoin(VeHandleTriple key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; }; ///< left join another map
		virtual VeCount leftJoin(std::string key, VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; }; ///< left join another map
		virtual VeCount leftJoin(VeMap& other, std::vector<VeIndexPair, custom_alloc<VeIndexPair>>& result) { assert(false); return 0; }; ///< left join another map

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
		* \brief Get value of a uint key from a table entry. 
		*
		* \param[in] entry Pointer to the entry
		* \param[in] offset Number of bytes the uint is away from the start
		* \param[in] numbytes Size in bytes if the uint. Can be 4 or 8.
		* \param[out] key The value of the uint.
		*
		*/
		void getKey(void* entry, VeIndex offset, VeIndex num_bytes, VeHandle& key) {
			key = getIntFromEntry(entry, offset, num_bytes);
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
		void getKey(void* entry, VeIndexPair offset, VeIndexPair num_bytes, VeHandlePair& key) {
			key = VeHandlePair(	getIntFromEntry(entry, offset.first, num_bytes.first),
								getIntFromEntry(entry, offset.second, num_bytes.second));
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
		void getKey(void* entry, VeIndexTriple offset, VeIndexTriple num_bytes, VeHandleTriple& key) {
			key = VeHandleTriple(	getIntFromEntry(entry, std::get<0>(offset), std::get<0>(num_bytes)),
									getIntFromEntry(entry, std::get<1>(offset), std::get<1>(num_bytes)),
									getIntFromEntry(entry, std::get<2>(offset), std::get<2>(num_bytes)));
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


	//----------------------------------------------------------------------------------

	/**
	* \brief A multi map storing key-value pairs, ordered with an AVL tree
	*
	* An AVL tree is a binary search tree. It starts with a root, and every node
	* can have a left and a right successor (or both or none).
	* AVL tress are always perfectly balanced. When ever e new node is inserted or
	* deleted, it checks whether the balance has been compromised, and then
	* rebalances by one or several subtree rotations.
	* See https://www.geeksforgeeks.org/avl-tree-set-1-insertion/
	*
	*/
	template <typename K, typename I>
	class VeOrderedMultimap : public VeMap {
		/**
		* A node of the AVL tree
		*/
		struct VeMapEntry {
			K			m_key;						///<key stored in map
			VeIndex		m_value = VE_NULL_INDEX;	///<value 
			int32_t		m_height = 1;				///<height of subtree rooting at this node
			VeIndex		m_parent = VE_NULL_INDEX;	///<index of parent
			VeIndex		m_left = VE_NULL_INDEX;		///<index of first child with lower key
			VeIndex		m_right = VE_NULL_INDEX;	///<index of first child with larger key

			/**
			* \brief Constructor ot the struct
			*/
			VeMapEntry() : m_key() {
				m_value = VE_NULL_INDEX;	//no value
				m_height = 0;				//no height
				m_parent = VE_NULL_INDEX;	//index of parent
				m_left = VE_NULL_INDEX;		//index of first child with lower key
				m_right = VE_NULL_INDEX;	//index of first child with larger key
			};

			/**
			*
			* \brief Constructor ot the struct
			*
			* \param[in] key The key of the node
			* \param[in] value The value of the node
			*/
			VeMapEntry( K key, VeIndex value ) : m_key(key), m_value(value) {
				m_height = 0;
				m_parent = VE_NULL_INDEX;	//index of parent
				m_left = VE_NULL_INDEX;		//index of first child with lower key
				m_right = VE_NULL_INDEX;	//index of first child with larger key
			};

			/**
			*
			* \brief Copy operator
			*
			* \param[in] entry Source for the copy
			*/
			void operator=(const VeMapEntry& entry) {
				m_key = entry.m_key;
				m_value = entry.m_value;
				m_height = entry.m_height;
				m_parent = entry.m_parent;
				m_left = entry.m_left;
				m_right = entry.m_right;
			};

			/**
			*
			* \brief Compare is equal operator
			*
			* \param[in] entry The other entry
			* \returns true if the two nodes are equal.
			* 
			*/
			bool operator==( const VeMapEntry& entry) {
				if( m_value != VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX  )
					return m_key == entry.m_key && m_value == entry.m_value;

				return m_key == entry.m_key;
			};

			/**
			*
			* \brief Compare less operator
			*
			* \param[in] entry The other entry
			* \returns true if this entry is less than the other entry
			*
			*/
			bool operator<(const VeMapEntry& entry) {
				if (m_value != VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX)
					return m_key < entry.m_key || (m_key == entry.m_key && m_value < entry.m_value);

				if (m_value == VE_NULL_INDEX && entry.m_value != VE_NULL_INDEX) 
					return m_key == entry.m_key || m_key < entry.m_key;

				return m_key < entry.m_key;
			};

			/**
			*
			* \brief Compare less or equal operator
			*
			* \param[in] entry The other entry
			* \returns true if this entry is less or equal to the other entry
			*
			*/
			bool operator<=(const VeMapEntry& entry) {
				return *this < entry || *this == entry;
			};

			/**
			*
			* \brief Compare larger operator
			*
			* \param[in] entry The other entry
			* \returns true if this entry is larger than the other entry
			*
			*/
			bool operator>(const VeMapEntry& entry) {
				return !(*this == entry || *this < entry);
			};

			/**
			*
			* \brief Print debug information
			*
			* \param[in] node Index of a node
			* \param[in] level Level of the node
			*
			*/
			void print(uint32_t node, uint32_t level = 1) {
				std::cout << "L " << level << " IDX " << node;
				std::cout << " KEY " << m_key << " VAL " << m_value << " HEIGHT " << m_height;
				std::cout << " PAR " << m_parent << " LEFT " << m_left << " RIGHT " << m_right << std::endl;
			};
		};

		/**
		*
		* \brief Clone this map
		*
		* \returns a clone of this map
		*
		*/
		VeOrderedMultimap<K, I>* clone() {
			VeOrderedMultimap<K, I>* map = new VeOrderedMultimap<K, I>(*this);
			return map;
		};

	protected:
		I						m_offset;					///<offsets into table entry
		I						m_num_bytes;				///<size in bytes of the key values
		VeVector<VeMapEntry>	m_map;						///<storage for the AVL nodes
		VeIndex					m_root = VE_NULL_INDEX;		///<index of the tree root


		//---------------------------------------------------------------------------
		//subtree properties

		/**
		*
		* \brief Get the heigth of a sub tree
		*
		* \param[in] node Index of the node whos height is sought
		* \returns the height of the subtree starting with node
		*
		*/
		int32_t getHeight(VeIndex node) {
			if (node == VE_NULL_INDEX)
				return 0;

			return m_map[node].m_height;
		}

		/**
		*
		* \brief Compute the balance of a sub tree
		*
		* \param[in] node Index of the root of the subtree whos balance is sought
		* \returns balance of the subtree rooted at node
		*
		*/
		int32_t getBalance(VeIndex node) {
			if (node == VE_NULL_INDEX)
				return 0;

			return getHeight(m_map[node].m_left) - getHeight(m_map[node].m_right);
		}

		//---------------------------------------------------------------------------
		//rotate subtrees

		/**
		*
		* \brief Replace a child of a parent. This relinks subtrees.
		*
		* \param[in] parent The parent with the child to change.
		* \param[in] old_child The child to change.
		* \param[in] new_child The new child to replace the old child.
		*
		*/
		void replaceChild(VeIndex parent, VeIndex old_child, VeIndex new_child) {
			if (parent == VE_NULL_INDEX) {
				m_root = new_child;
				return;
			}

			if (new_child != VE_NULL_INDEX)
				m_map[new_child].m_parent = parent;

			if (old_child == VE_NULL_INDEX)				//cannot identify right child so return
				return;

			if (m_map[parent].m_left == old_child) {	//if old child is left, replace lelt
				m_map[parent].m_left = new_child;
				return;
			}
			m_map[parent].m_right = new_child;	//else replace right
		};

		/**
		*
		* \brief Replace the left child of a parent
		*
		* \param[in] parent The parent of the child to replace.
		* \param[in] new_child The new left child.
		*
		*/
		void replaceLeftChild(VeIndex parent, VeIndex new_child) {
			if (parent == VE_NULL_INDEX)
				return;
			m_map[parent].m_left = new_child;	//replace child
			if (new_child == VE_NULL_INDEX)
				return;
			m_map[new_child].m_parent = parent; //replace parent of child
		};

		/**
		*
		* \brief Replace the right child of a parent
		*
		* \param[in] parent The parent of the child to replace.
		* \param[in] new_child The new right child.
		*
		*/
		void replaceRightChild(VeIndex parent, VeIndex new_child) {
			if (parent == VE_NULL_INDEX)
				return;
			m_map[parent].m_right = new_child;	//replace child
			if (new_child == VE_NULL_INDEX)
				return;
			m_map[new_child].m_parent = parent; //replace parent of child
		};

		/**
		*
		* \brief AVL right rotation
		*
		* \param[in] y Index node that is the pivot for a right rotation.
		* \returns the new node that replaces y.
		*
		*/
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

		/**
		*
		* \brief AVL left rotation
		*
		* \param[in] y Index node that is the pivot for a left rotation.
		* \returns the new node that replaces y.
		*
		*/
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

		/**
		*
		* \brief Insert a new node into the tree, might cause imbalance
		*
		* \param[in] node The root of the subtree where the new node should be inserted.
		* \param[in] index The index of the new node
		* \returns index to the new node.
		*
		*/
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

		/**
		*
		* \brief Find an entry (key, value), start searching at node
		*
		* \param[in] node Root node where to start the search.
		* \param[in] entry The entry holding key and value of the node to find
		* \returns
		*
		*/
		VeIndex findEntry(VeIndex node, VeMapEntry &entry ) {
			if (node == VE_NULL_INDEX)
				return VE_NULL_INDEX;

			if (m_map[node] == entry )
				return node;

			if (entry < m_map[node]) 
				return findEntry(m_map[node].m_left, entry);

			return findEntry(m_map[node].m_right, entry);
		};


		/**
		*
		* \brief Find an entry (key, value), start searching at root node
		*
		* \param[in] key The key looked for.
		* \param[in] value The value looked ofr
		* \returns the index of the found node or VE_NULL_INDEX
		*
		*/
		VeIndex findKeyValue(K& key, VeIndex value) {
			VeMapEntry entry{ key, value };
			return findEntry(m_root, entry);
		};


		/**
		*
		* \brief Find a key
		*
		* \param[in] key The key that is looked for
		* \returns the index of the found node.
		*
		*/
		VeIndex findKey(K& key) {
			return findKeyValue(key, VE_NULL_INDEX);
		};


		//---------------------------------------------------------------------------
		//delete

		/**
		*
		* \brief Delete an index, i.e. copy last entry over it and relink the tree
		*
		* \param[in] index The index of the entry to delete.
		*
		*/
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


		/**
		*
		* \brief Find the minimum node of a subtree
		*
		* \param[in] node Root of the subtree to start the search.
		* \returns the index of the node with the minimum key-value pair.
		*
		*/
		VeIndex findMin(VeIndex node) {
			assert(node != VE_NULL_INDEX);
			while (m_map[node].m_left != VE_NULL_INDEX) 
				node = m_map[node].m_left;
			return node;
		};

		///copy key and value to enother entry
		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		void replaceKeyValue(VeIndex dst, VeIndex src) {
			m_map[dst].m_key = m_map[src].m_key;
			m_map[dst].m_value = m_map[src].m_value;
		}

		///delete an entry given in entry, start search at node
		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		VeIndex deleteEntry(VeIndex node, VeMapEntry& entry,
							std::vector<VeIndex, custom_alloc<VeIndex>> &del_indices) {

			if (node == VE_NULL_INDEX)
				return node;

			VeIndex& parent = m_map[node].m_parent;
			VeIndex& left = m_map[node].m_left;
			VeIndex& right = m_map[node].m_right;

			if (entry < m_map[node]) {	//go left down the tree
				replaceLeftChild(node, deleteEntry(left, entry, del_indices));
			}
			else if (m_map[node] < entry) { //go right down the tree
				replaceRightChild(node, deleteEntry(right, entry, del_indices));
			}
			else {	//have found the node
				if (left != VE_NULL_INDEX && right != VE_NULL_INDEX) { //has two children?
					VeIndex min_idx = findMin(right);
					replaceKeyValue(node, min_idx);
					replaceRightChild(node, deleteEntry(right, m_map[min_idx], del_indices) );
				}
				else {
					VeIndex temp = left != VE_NULL_INDEX ? left : right; //has one child
					if (temp == VE_NULL_INDEX) {
						del_indices.push_back(node);
						return VE_NULL_INDEX;
					}
					replaceChild(parent, node, temp);	//a leaf node	
					del_indices.push_back(node);		//store for later deletion
					node = temp;						//return successor
				}
			}

			if (node == VE_NULL_INDEX)
				return node;

			m_map[node].m_height = 1 + std::max(getHeight(left),	//recompute subtree height
												getHeight(right));

			int balance = getBalance(node);	//might be imbalanced

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

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		VeCount getAllIndices(VeIndex node, std::vector<VeIndex, custom_alloc<VeIndex>>& result) {
			if (node == VE_NULL_INDEX)
				return 0;

			VeCount num = getAllIndices(m_map[node].m_left, result);
			result.push_back(m_map[node].m_value);
			num += getAllIndices(m_map[node].m_right, result);
			return num + 1;
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		VeCount getAllKeyValuePairs(VeIndex node, std::vector<std::pair<K,VeIndex>, custom_alloc<std::pair<K, VeIndex>>>& result) {
			if (node == VE_NULL_INDEX)
				return 0;

			VeCount num = getAllKeyValuePairs(m_map[node].m_left, result);
			result.push_back( std::pair<K,VeIndex>(m_map[node].m_key, m_map[node].m_value) );
			num += getAllKeyValuePairs(m_map[node].m_right, result);
			return num + 1;
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
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


		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
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

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
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

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		VeOrderedMultimap(I offset, I num_bytes, bool memcopy = false) :
			VeMap(), m_map(memcopy), m_offset(offset), m_num_bytes(num_bytes) {};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		VeOrderedMultimap(const VeOrderedMultimap<K, I>& map) :
			VeMap(), m_map(map.m_map), m_offset(map.m_offset), m_num_bytes(map.m_num_bytes) {};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual	~VeOrderedMultimap() {};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual void operator=(const VeMap& basemap) override {
			VeOrderedMultimap<K, I>* map = &((VeOrderedMultimap<K, I>&)basemap);
			m_offset = map->m_offset;
			m_num_bytes = map->m_num_bytes;
			m_map = map->m_map;
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual void clear() override {
			m_root = VE_NULL_INDEX;
			m_map.clear();
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		VeCount size() override {
			return m_map.size();
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		std::pair<K, VeIndex> getKeyValuePair(VeIndex num) {
			VeIndex value = find(m_map[num].m_key);
			return { m_map[num].m_key, value};
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		void printTree() {
			std::cout << "Root " << m_root << std::endl;
			printTree(m_root, 1);
			std::cout << std::endl;
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		void print() override {
			std::cout << "Num " << m_map.size() << " Root " << m_root << std::endl;
			for (uint32_t i = 0; i < m_map.size(); ++i) {
				m_map[i].print(i);
			}
			std::cout << std::endl;
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual VeIndex find(K key) override {
			VeIndex index = findKey(key);
			return index == VE_NULL_INDEX ? VE_NULL_INDEX : m_map[index].m_value;
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual VeCount equal_range(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeMapEntry entry(key, VE_NULL_INDEX);
			return equal_range(m_root, entry, result);
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual VeCount range(K lower, K upper, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeMapEntry mlower(lower, VE_NULL_INDEX), mupper(upper, VE_NULL_INDEX);
			return range(m_root, mlower, mupper, result);
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual VeCount getAllIndices(std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			return getAllIndices(m_root, result);
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual VeCount getAllKeyValuePairs(std::vector<std::pair<K, VeIndex>, custom_alloc<std::pair<K, VeIndex>>>& result) override {
			return getAllKeyValuePairs(m_root, result);
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
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

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
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

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
		virtual bool insert(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex last = m_map.size();
			m_map.emplace_back({ key, dir_index });
			m_root = insert(m_root, last);
			m_map[m_root].m_parent = VE_NULL_INDEX;
			return true;
		};

		/**
		*
		* \brief
		*
		* \param[in]
		* \returns
		*
		*/
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

	template <typename K, typename I>
	class VeOrderedMap : public VeOrderedMultimap<K,I> {
	public:
		VeOrderedMap(I offset, I num_bytes, bool memcopy = false) : VeOrderedMultimap<K,I>(offset, num_bytes, memcopy) {};
		VeOrderedMap(const VeOrderedMap<K, I>& map) : VeOrderedMultimap<K,I>((const VeOrderedMultimap<K,I>&)map) {};
		virtual	~VeOrderedMap() {};

		virtual bool insert(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, this->m_offset, this->m_num_bytes, key);
			if (find(key) == VE_NULL_INDEX)
				return VeOrderedMultimap<K, I>::insert(entry, dir_index);
			return false;
		}
	};


	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeHashedMultimap : public VeMap {
	protected:

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

		I						m_offset;			///
		I						m_num_bytes;		///
		VeCount					m_num_entries = 0;
		VeVector<VeMapEntry>	m_entries;			///
		VeIndex					m_first_free_entry = VE_NULL_INDEX;
		VeVector<VeIndex>		m_map;


		bool insertIntoMap(VeIndex map_idx, VeIndex entry_idx ) {
			if (m_map[map_idx] == VE_NULL_INDEX) {
				m_map[map_idx] = entry_idx;
				return true;
			}
			m_entries[entry_idx].m_next = m_map[map_idx];
			m_map[map_idx] = entry_idx;
			return true;
		};


		bool resizeMap() {
			std::vector<VeIndex, custom_alloc<VeIndex>> result(&m_heap);
			result.reserve(m_num_entries);

			std::for_each(m_map.begin(), m_map.end(), [&](VeIndex index) {
				while (index != VE_NULL_INDEX) {
					auto next = m_entries[index].m_next;
					m_entries[index].m_next = VE_NULL_INDEX;
					result.emplace_back(index);
					index = next;
				};
				});

			m_map.resize(2 * m_map.size());
			for(uint32_t i=0;i<m_map.size();++i)
				m_map[i] = VE_NULL_INDEX;

			for (auto entry_idx : result) {
				K key;
				getKey(&m_entries[entry_idx], m_offset, m_num_bytes, key);
				VeIndex map_idx = std::hash<K>()(key) % (VeIndex)m_map.size();
				insertIntoMap(map_idx, entry_idx);
			};
			return true;
		};

		float fill() {
			return m_num_entries / (float)m_map.size();
		};

		VeHashedMultimap<K, I>* clone() {
			VeHashedMultimap<K, I>* map = new VeHashedMultimap<K, I>(*this);
			return map;
		};

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
			m_num_entries = map->m_num_entries;
			m_entries = map->m_entries;
			m_first_free_entry = map->m_first_free_entry;
			m_map = map->m_map;
		};

		void clear() override {
			m_entries.clear();
			m_first_free_entry = VE_NULL_INDEX;
			m_num_entries = 0;
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
			VeIndex idx = std::hash<K>()(key) % (VeIndex)m_map.size();
			for (VeIndex index = m_map[idx]; index != VE_NULL_INDEX; index = m_entries[index].m_next) {
				if (m_entries[index].m_key == key) {
					return m_entries[index].m_value;
				}
			}
			return VE_NULL_INDEX;
		};

		virtual VeCount equal_range(K key, std::vector<VeIndex, custom_alloc<VeIndex>>& result) override {
			VeCount num = 0;
			VeIndex idx = std::hash<K>()(key) % (VeIndex)m_map.size();
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
			if (fill() > 0.9f)
				resizeMap();

			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex idx = std::hash<K>()(key) % (VeIndex)m_map.size();

			VeMapEntry nentry(key, dir_index);
			VeIndex new_index = m_first_free_entry;
			if (m_first_free_entry != VE_NULL_INDEX) {
				m_first_free_entry = m_entries[new_index].m_next;
				m_entries[new_index] = nentry;
			} else {
				m_entries.emplace_back(nentry);
				new_index = (VeIndex)m_entries.size() - 1;
			}
			
			++m_num_entries;
			return insertIntoMap(idx, new_index);
		};

		virtual VeCount erase(void* entry, VeIndex dir_index) override {
			K key;
			getKey(entry, m_offset, m_num_bytes, key);
			VeIndex hidx = std::hash<K>()(key) % (VeIndex)m_map.size();
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

	//----------------------------------------------------------------------------------

	template <typename K, typename I>
	class VeHashedMap : public VeHashedMultimap<K, I> {
	public:
		VeHashedMap(I offset, I num_bytes, bool memcopy = false) : VeHashedMultimap<K, I>(offset, num_bytes, memcopy) {};
		VeHashedMap(const VeHashedMap<K, I>& map) : VeHashedMultimap<K, I>((const VeHashedMultimap<K, I>&)map) {};
		virtual	~VeHashedMap() {};

		virtual bool insert(void* entry, VeIndex dir_index) override {
			K key;
			this->getKey(entry, this->m_offset, this->m_num_bytes, key);
			if (this->find(key) == VE_NULL_INDEX)
				return VeHashedMultimap<K, I>::insert(entry, dir_index);
			return false;
		}
	};

	namespace map {
		void testMap();
	};

};



