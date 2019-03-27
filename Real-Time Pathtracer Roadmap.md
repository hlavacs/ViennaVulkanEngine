# Vulkan Real-Time Pathtracer Roadmap
This is just a general plan for now, a lot can change, some things may take longer than expected. I wanted to plan everything with more detail. As it turns out this is really hard and at some point even pointless with my current knowledge level. So I decided to create a basic plan and adapt incrementally as my knowledge increases and tackle problems as they come up. I think this makes a lot more sense and is the right path for me in this position, eventough I am a big fan of planning ahead.

I plan to fill out and adapt this document as I am working on it.

---
## Week 1 until 31.03.2019
### Plan
Create a basic Raytracer with a compute shader for spheres. 

In terms of the engine:
  - Derive new renderer
  - Derive entity for spheres?

### Documentation / Sources used
Started to work on a derived renderer for the pathtracer based on the already implemented forward renderer.
First obstacle comute queue family needed, helper function does not provide an option.
Implemented overloaded helper function to get specific queues with the device creation based on: https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#VkQueueFlagBits

### Conclusion

---
## Week 2 until 07.04.2019
### Plan
Update the Raytracer with Pathtracing supporting reflection, refraction and shadows based on:

* [The Rendering Equation]
* [Physical Based Rendering]

Of course recursions need be changed to fit on a gpu. Right now the plan is to use one shader stage for rendering. Seeing as Nvidia uses multiple shaders this might be smart for this project as well but I am not sure about that yet. Seen in [Ray Tracing Gems] (Figure 3-2)

### Documentation / Sources used

### Conclusion

---
## Week 3 until 14.04.2019
### Plan
Add camera movement to the Raytracer. Make a final decision on how and where rays are generated.

### Documentation / Sources used

### Conclusion
---

## Week 4 until 21.04.2019
### Plan
Add support for triangles and the Viennna Vulkan Engine entities.

### Documentation / Sources used

### Conclusion
---

## Week 5 until 28.04.2019
### Plan
Add support for materials included in the Vienna Vulkan Engine as well as textures.

### Documentation / Sources used

### Conclusion

---
## Week 6,7,8,9 until 26.05.2019
### Plan
This step will probably be split up later.
Implement a second compute shader/pipeline to generate and update a acceleration structure.

This step entails the following:
* Generate acceleration structure on the GPU
* Update it based on movement of the entities
* Traverse the structure efficiently in the Pathtracer
* Look further into performance optimizations like coherency and packet traversal

There are lots of papers on different trees for the gpu. As far as my current understanding goes a bvh is the best joice since we need to update the tree for our dynamic scenes and this is pretty easy with bvh trees. An example paper I could try to implement is: [Fast, effective BVH updates for animated scenes].

### Documentation / Sources used

### Conclusion


   [The Rendering Equation]: <https://dl.acm.org/citation.cfm?id=15902>
   [Physical Based Rendering]: <http://www.pbr-book.org/3ed-2018/contents.html>
   [Ray Tracing Gems]: http://www.realtimerendering.com/raytracinggems/unofficial_RayTracingGems_v1.3.pdf
   [Fast, effective BVH updates for animated scenes]: https://dl.acm.org/citation.cfm?id=2159649
