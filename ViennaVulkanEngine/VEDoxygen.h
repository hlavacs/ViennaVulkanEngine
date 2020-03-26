#pragma once

/**
*
* \file
* \brief Contains the engine manual 
*
* Contains the engine manual, formatted for Doxygen. 
*
*/



/**
*
* \mainpage
* The Vienna Vulkan Engine is an experimental, multithreaded game engine.
* Its purpose is teaching and researching new, out-of-the-box approaches. You are free to use
* the engine or parts of it. Please cite the project, the author or some project publication if you do.
* Documentation is based on Doxygen. If you discover any error, typo etc., please send them to the 
* author.
*
* \section label_struct Engine Structure
* Following the design pattern of separation of concerns, the engine is made up of several different parts or layers. 
* Engine state is stored on tables only, forming up a relational database at the engine core. 
* This way, there is only one unified way of storing state, and data access can be done in parallel.
* Tables are always owned by a system. Thus, only systems store engine state, all other modules provide
* only functionality. 
*
* An important decision made during design is not to use the singleton pattern for storing state in the systems, 
* but use global variables instead. Use of global variables is dicouraged generally, because during the
* life time of an engine, new programmers might refer to or change them in unexpected ways. Arcgitecture is always
* about enabling easy learning and making changes in the future. 
* The VVE tackles this problem by keeping globals "local" in the sense that they are only to be used within the same file,
* not from other files using the extern decoration.
*
* Some parts are designed to be used independent from the engine. 
* Interested persons can simply copy and paste them and use them in their own engines.
* 
* You can compile the engine for single-threaded or multithreaded operation. You can switch this by
* commenting or enabling VE_ENABLE_MULTITHREADING in VEDefines.h. Simply recompile the engine if you change this.
* One important design feature is its job-only architecture. 
* This means that if the engine is running in multithreaded mode, there is no main thread. 
* VEGameJobSystem.h provides simple macros that let users make use of submitting jobs.
*
*
* \subsection label_util Utility Layer
* This layer includes files that do not belong to any of the below described categories.
* These files provide some basic functionality that does not fit anywehere else. 
* This includes VEUtilClock.h, VEUtilFile.h, or VEUtilCollision.h. VEUtilCollision.h provides data structures
* and functionality for testing collisions between game objects. This can be used for physics, but also
* camera culling during the rendering process.
*
* \subsection label_helper Vulkan Helper Layer
* This is a collection of C-functions that build an abstraction layer above Vulkan. Goal is to reduce the
* number of lines to program. Essentially, helper functions act as macros that transform input variables 
* into one or several Vulkan calls. Function names begin with "VH" and do not save any state, i.e. they are 
* "true functions". Ideally, there are only very few calls from the engine directly to Vulkan 
* (e.g. when destroying objects). Most interactions between the engine and Vulkan should go over the helper layer.
*
* \subsection label_engine Engine Layer
* The engine layer holds the whole engine state in a collection of related tables. The main function of the engine
* is to orchestrate all processes without dealing too much with details. The goal of the engine structure
* is to produce a small number of code lines using calls to a thin but powerful abstraction layer.
*
* \section label_mem Memory Part
* The memory part of the engine provides functionality for managing data and memory. 
* VEMemHeap.h provides a heap class that is used by other classes to obtain memory for storing
* transient data, e.g. for storing handles in a vector. Heaps are mainly used in a custom allocator for std::vector.
* VeMemVector.h offers a VeVector class that is similar to std::vector, but is designed to offer fast copying,
* arbitrarily aligned object allocation, and optional use without constructors/destructors. VeVectors are used to store
* data in VeTables and VeMaps.
* VEMemMap.h provides ordered and hashed maps to be used as search and ordering indices in tables.
* VeOrderedMap implements a self-balancing AVL tree, VeHashedMap uses a growable hash map for fast lookups.
* VeTable is the main data structure of the engine. In its core it implements a slot map, storing arbitrary objects.
* Additionally, it can be assigned arbitrary VeMaps to create search and ordering indices for single, pairs, or triples
* of integers (32 and 64 bits), and std::string. 
* VeTable offers a left join operation to merge its data to the data of another table, via two VeMaps, one in each table.
*
* \section label_systems Systems Part
* A system is a part of the engine managing a certain aspect. It is located in a certain file, and holds all
* global state for its part. Systems can manage entity components, the scene, etc., and are essentially a collection
* of functions that can be run as jobs. Data is persisted in tables. System names start with VESys.
*
* \subsection label_assets Assets
* TODO
*
* \subsection label_scene Scene 
* TODO
*
* \subsection label_scene Physics
* TODO
*
*
* \section label_loop The Game Loop
*
*
* \section label_usage Using the Engine
*
*
*
*/


