export module VVE:VEMemory;

import std.core;
import std.memory;
import std.threading;

export namespace vve {

	auto g_global_mem = std::pmr::synchronized_pool_resource({ .max_blocks_per_chunk = 20, .largest_required_pool_block = 1 << 20 }, std::pmr::new_delete_resource());

	thread_local auto g_local_mem = std::pmr::unsynchronized_pool_resource({ .max_blocks_per_chunk = 20, .largest_required_pool_block = 1 << 10 }, &g_global_mem);
	thread_local auto g_local_tmp_mem = std::pmr::monotonic_buffer_resource( 1<<14, &g_global_mem);
	thread_local auto g_table_mem = std::pmr::unsynchronized_pool_resource({ .max_blocks_per_chunk = 20, .largest_required_pool_block = 1 << 14 }, &g_global_mem);

};


