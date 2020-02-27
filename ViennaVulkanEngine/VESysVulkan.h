#pragma once



namespace vve::sysvul {


	struct VeStringTableEntry {
		const char* m_string;
	};


	void init();
	void tick();
	void sync();
	void close();


}

