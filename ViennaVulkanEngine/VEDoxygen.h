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
* only functionality. Some parts are designed to be used independent from the engine. 
* Interested persons can simply copy and paste them and use them in their own engines.
* 
* You can compile the engine for single-threaded or multithreaded operation. You can switch this by
* commenting or enabling VE_ENABLE_MULTITHREADING in VEDefines.h. Simply recompile the engine if you change this.
* One important design feature is its job-only architecture. 
* This means that if the engine is running in multithreaded mode, there is no main thread. 
* VEGameJobSystem.h provides simple macros that let users make use of submitting jobs.
*
*
* \section label_util Utility Layer
*
* \section label_helper Vulkan Helper Layer
*
* \section label_engine Engine Layer
*
* \subsection label_mem Memory Part
*
* \subsection label_systemspart Systems Part
*
* \section label_systems Engine Systems
*
* \subsection label_assets Asset Management
*
* \subsection label_scene Scene Management
*
* \subsection label_scene Scene Management
*
* \subsection label_job The Job System
*
*
* \section label_dynamics The Game Loop
*
* \section label_usage Using the Engine
*
*
*
*/


