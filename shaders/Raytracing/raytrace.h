// Shared functions for raytracing against the scene.

// Definitions for software based raytracing.
#define MAX_STACK_SIZE 32

struct BVHNode
{
    vec3 Center;
    uint TriangleCountOrChildCount;
    vec3 HalfLength;
    uint TriangleStartOrChildStart;
};

struct DecodedBVHNode
{
    vec3 Center;
    uint TriangleCountOrChildCount;
    vec3 HalfLength;
    uint TriangleStartOrChildStart;
    bool IsLeaf;
};

struct BVHTriangle
{
	vec3 A;
	uint TexCoordA;

	vec3 AB;
	uint TexCoordB;

	vec3 AC;
	uint TexCoordC;

	vec3 N;
	uint MaterialIndex;
};

layout(std430, binding = BIND_NODE_BUFFER) buffer NodeBuffer
{
	BVHNode Nodes[];
};

BVHNode GetNode(uint nodeIndex) {
	return Nodes[nodeIndex];
}

layout(std430, binding = BIND_TRIANGLE_BUFFER) buffer TriangleBuffer
{
	BVHTriangle Triangles[];
};

BVHTriangle GetTriangle(uint triangleIndex) {
	return Triangles[triangleIndex];
}

bool TestSegmentAABB(vec3 m, vec3 d, vec3 c, vec3 e)
{
	//vec3 c = (bmin + bmax) * 0.5; // Box center-point
	//vec3 e = bmax - c; // Box halflength extents
	//vec3 m = (p0 + p1) * 0.5; // Segment midpoint
	//vec3 d = p1 - m; // Segment halflength vector
	m = m - c; // Translate box and segment to origin
	// Try world coordinate axes as separating axes
	float adx = abs(d.x);
	if (abs(m.x) > e.x + adx) return false;
	float ady = abs(d.y);
	if (abs(m.y) > e.y + ady) return false;
	float adz = abs(d.z);
	if (abs(m.z) > e.z + adz) return false;
	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis (see text for detail)
	adx += EPSILON; ady += EPSILON; adz += EPSILON;

	// Try cross products of segment direction vector with coordinate axes
	if (abs(m.y * d.z - m.z * d.y) > e.y * adz + e.z * ady) return false;
	if (abs(m.z * d.x - m.x * d.z) > e.x * adz + e.z * adx) return false;
	if (abs(m.x * d.y - m.y * d.x) > e.x * ady + e.y * adx) return false;
	// No separating axis found; segment must be overlapping AABB
	return true;
}

// Given segment pq and triangle abc, returns whether segment intersects
// triangle and if so, also returns the barycentric coordinates (u,v,w)
// of the intersection point
bool IntersectSegmentTriangle(vec3 p, vec3 qp, vec3 a, vec3 ab, vec3 ac, vec3 n, out float u, out float v, out float w, out float t)
{
	u = 0;
	v = 0;
	w = 0;
	t = 0;
	//vec3 ab = b - a;
	//vec3 ac = c - a;
	//vec3 qp = p - q;
	// Compute triangle normal. Can be precalculated or cached if
	// intersecting multiple segments against the same triangle
	//vec3 n = cross(ab, ac);
	// Compute denominator d. If d <= 0, segment is parallel to or points
	// away from triangle, so exit early
	float d = dot(qp, n);
	if (d <= 0.0) return false;
	// Compute intersection t value of pq with plane of triangle. A ray
	// intersects iff 0 <= t. Segment intersects iff 0 <= t <= 1. Delay
	// dividing by d until intersection has been found to pierce triangle
	vec3 ap = p - a;
	t = dot(ap, n);
	if (t < 0.0) return false;
	if (t > d) return false; // For segment; exclude this code line for a ray test
	// Compute barycentric coordinate components and test if within bounds
	vec3 e = cross(qp, ap);
	v = dot(ac, e);
	if (v < 0.0 || v > d) return false;
	w = -dot(ab, e);
	if (w < 0.0 || v + w > d) return false;
	// Segment/ray intersects triangle. Perform delayed division and
	// compute the last barycentric coordinate component
	float ood = 1.0 / d;
	t *= ood;
	v *= ood;
	w *= ood;
	u = 1.0 - v - w;
	return true;
}

// Do some minor decoding of node data.
DecodedBVHNode DecodeNode(BVHNode node) {
	DecodedBVHNode result;
	result.IsLeaf = node.TriangleCountOrChildCount >= 2147483648u; // highest bit set
	if(result.IsLeaf) {
		result.TriangleCountOrChildCount = node.TriangleCountOrChildCount - 2147483648;
	} else {
		result.TriangleCountOrChildCount = node.TriangleCountOrChildCount;
	}
	result.Center = node.Center;
	result.HalfLength = node.HalfLength;
	result.TriangleStartOrChildStart = node.TriangleStartOrChildStart;
	return result;
}

#ifndef USE_HARDWARE_RT
	// Software implementation of raytracing through scene.

	// Cast a ray to only check if it hits anything (e.g. for shadows).
	bool CastVisRay(vec3 origin, vec3 target) {
		// Set up node stack and push root.
		uint nodeStack[MAX_STACK_SIZE];
		int stackSize = 1;
		nodeStack[0] = 0;

		// Precompute ray values.
		vec3 rayMidPoint = (origin + target) * 0.5;
		vec3 rayHalfLength = target - rayMidPoint; 
		vec3 qp = origin - target; 

		// Traverse stack while there are nodes on.
		while(stackSize > 0) {
			DecodedBVHNode node = DecodeNode(GetNode(nodeStack[--stackSize]));
			
			// Do AABB test first and only continue down this branch if we have an intersection.
			if(TestSegmentAABB(rayMidPoint, rayHalfLength, node.Center, node.HalfLength)) {
				if (node.IsLeaf) { 
					// If it is a leaf node, intersect the triangles.
					uint triangleStart = node.TriangleStartOrChildStart;
					uint triangleEnd = triangleStart + node.TriangleCountOrChildCount;

					for(uint i = triangleStart; i < triangleEnd; ++i) {
						BVHTriangle tri = GetTriangle(i);
						float u, v, w, t;
						
						// Check actual triangle intersection.
						if(IntersectSegmentTriangle(origin, qp, tri.A, tri.AB, tri.AC, tri.N, u, v, w, t)) {
							// Check if we have a triangle with a transparent material, if yes sample the texture to see if we actually hit something.
							vec2 uv = unpackHalf2x16_emu(tri.TexCoordA) * u + unpackHalf2x16_emu(tri.TexCoordB) * v + unpackHalf2x16_emu(tri.TexCoordC) * w;
							RendererMaterial mat = GetMaterial(tri.MaterialIndex);
							if(HasFeature(mat, MATERIAL_FEATURE_ALBEDO_MAP) && HasFeature(mat, MATERIAL_FEATURE_OPACITY_CUTOUT)) {
								if (SampleMaterialTextureLod(mat.AlbedoMap, uv, 0).a > mat.AlphaCutoff) {
									// Immediately return if we hit anything.
									return true;
								}
							} else {
								// Immediately return if we hit anything.
								return true;
							}
						}
					}
				} else {
					// If it's not a leaf add the children to the stack.
					if(stackSize < MAX_STACK_SIZE - node.TriangleCountOrChildCount) {
						for(uint i = 0; i < node.TriangleCountOrChildCount; ++i) {
							nodeStack[stackSize++] = node.TriangleStartOrChildStart + i;
						}
					}
				}
			}
		}
		
		return false;
	}

	// Cast a ray and get detailed intersection data about the point in the scene.
	bool CastSurfaceRay(vec3 origin, vec3 target, out SurfacePoint point) {
		// Set up node stack and push root.
		uint nodeStack[MAX_STACK_SIZE];
		int stackSize = 1;
		nodeStack[0] = 0;

		// Precompute ray values.
		vec3 rayMidPoint = (origin + target) * 0.5;
		vec3 rayHalfLength = target - rayMidPoint; 
		vec3 qp = origin - target; 

		// Remember nearest hit values.
		bool hasHit = false;
		float minT = 1000000.0;
		
		// Traverse stack while there are nodes on.
		while(stackSize > 0) {
			DecodedBVHNode node = DecodeNode(GetNode(nodeStack[--stackSize]));

			// Do AABB test first and only continue down this branch if we have an intersection.
			if(TestSegmentAABB(rayMidPoint, rayHalfLength, node.Center, node.HalfLength)) {
				if (node.IsLeaf) { 
					// If it is a leaf node, intersect the triangles.
					uint triangleStart = node.TriangleStartOrChildStart;
					uint triangleEnd = triangleStart + node.TriangleCountOrChildCount;

					for(uint i = triangleStart; i < triangleEnd; ++i) {
						BVHTriangle tri = GetTriangle(i);
						float u, v, w, t;
							
						// Check actual triangle intersection.
						if(IntersectSegmentTriangle(origin, qp, tri.A, tri.AB, tri.AC, tri.N, u, v, w, t)) {
							// Check if the hit is nearer than the old one.
							if(t < minT) {
								// Check if we have a triangle with a transparent material, if yes sample the texture to see if we actually hit something.
								RendererMaterial mat = GetMaterial(tri.MaterialIndex);
								vec2 uv = unpackHalf2x16_emu(tri.TexCoordA) * u + unpackHalf2x16_emu(tri.TexCoordB) * v + unpackHalf2x16_emu(tri.TexCoordC) * w;
								vec4 albedo = mat.AlbedoFactor;
								bool hit = false;
								if(HasFeature(mat, MATERIAL_FEATURE_ALBEDO_MAP)) {
									albedo *= SampleMaterialTextureLod(mat.AlbedoMap, uv, 0);	
									if(HasFeature(mat, MATERIAL_FEATURE_OPACITY_CUTOUT)) {
										if(albedo.a > mat.AlphaCutoff) {
											hit = true;
										}
									} else {
										hit = true;
									}
								} else {
									hit = true;
								}
								if(hit) {
									// Compute surface values for hit. As an optimization this could be move to a later point when we
									// already are sure that this is the nearest hit.
									hasHit = true;
									point.Position = origin - t * qp; 
									point.Normal = normalize(tri.N);
									point.Albedo = albedo;
									float metallic = mat.MetalnessFactor;
									float roughness = mat.RoughnessFactor;
									if(HasFeature(mat, MATERIAL_FEATURE_METALLICROUGHNESS_MAP)) {
										vec2 metallicRoughness = SampleMaterialTextureLod(mat.MetallicRoughnessMap, uv, 0).rg;
										metallic *= metallicRoughness.r;
										roughness *= metallicRoughness.g;	
									}
									point.Metalness = metallic;
									point.Roughness = roughness;
									minT = t;
								}
							}
						}				
					}
				} else {
					// If it's not a leaf add the children to the stack.
					if(stackSize < MAX_STACK_SIZE - node.TriangleCountOrChildCount) {
						for(uint i = 0; i < node.TriangleCountOrChildCount; ++i) {
							nodeStack[stackSize++] = node.TriangleStartOrChildStart + i;
						}
					}
				}
			}
		}

		//traceDepth = traceNodeCount;
		return hasHit;
	}

#else
	// Hardware implementation of scene raycasts.

	// Top level acceleration structure for the scene.
	vk_layout(binding = BIND_TLAS) uniform accelerationStructureEXT TopLevelAS;

	// Cast a ray to only check if it hits anything (e.g. for shadows).
	bool CastVisRay(vec3 origin, vec3 target) {	
		// Compute ray values.
		vec3 pq = target - origin; 
		float tMin = 0.0;
		float tMax = length(pq);	
		vec3 dir = pq / tMax;

		// Set up query to end at the first hit.
		uint cullMask = 0xFF;
		rayQueryEXT query;
		rayQueryInitializeEXT(query, TopLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, cullMask, origin, tMin, dir, tMax);
		
		// Traverse the acceleration structure and store information about the first intersection (if any)
		while(rayQueryProceedEXT(query)) {
			if (rayQueryGetIntersectionTypeEXT(query, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
				// Get necessary values to get the objects vertex data.
				// First we get the instance value (DrawDataOffset in the node).
				uint instanceIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(query, false);

				// Then we get the geometryIndex (group in the mesh).
				uint geometryIndex = rayQueryGetIntersectionGeometryIndexEXT(query, false);

				// We combine it to get the final draw index to get the rendered meshes draw data.
				uint drawIndex = instanceIndex + geometryIndex;
				DrawData drawData = GetDrawData(drawIndex);

				// The primitive index is the index of the triangle that was hit.
				uint primitiveIndex = rayQueryGetIntersectionPrimitiveIndexEXT(query, false);

				// We also get the barycentric coordinates of the hit.
				vec2 barycentric = rayQueryGetIntersectionBarycentricsEXT(query, false);
				
				// Then we compute the indices of the vertices of the triangle and get their data.
				uint baseIndex = drawData.IndexOffset + primitiveIndex * 3;
				IndexBufferReference indices = IndexBufferReference(drawData.Indices);
				VertexBufferReference vertices = VertexBufferReference(drawData.Vertices);

				uint i0 = indices.data[baseIndex + 0];
				uint i1 = indices.data[baseIndex + 1];
				uint i2 = indices.data[baseIndex + 2];

				Vertex v0 = vertices.data[i0];
				Vertex v1 = vertices.data[i1];
				Vertex v2 = vertices.data[i2];
				
				// We use the vertex data and the barycentric coordinates to compute the uv coodinates of the hit.
				vec3 barycentricFull = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
				vec2 uv = v0.TextureCoordinates * barycentricFull.x + v1.TextureCoordinates * barycentricFull.y + v2.TextureCoordinates * barycentricFull.z;

				// Then we check if the hit is on a part of the mesh that is opaque.
				bool opaqueHit = false; 
				RendererMaterial mat = GetMaterial(drawData.MaterialIndex);
				if(HasFeature(mat, MATERIAL_FEATURE_ALBEDO_MAP) && HasFeature(mat, MATERIAL_FEATURE_OPACITY_CUTOUT)) {
					if (SampleMaterialTextureLod(mat.AlbedoMap, uv, 0).a > mat.AlphaCutoff) {
						opaqueHit = true;
					}
				} else {
					opaqueHit = true;
				}

				if (opaqueHit) {
					// If we have a hit we confirm it to end traversal of the BVH.
					rayQueryConfirmIntersectionEXT(query);
				}
			} 
		}
		
		// If we have a confirmed intersection we return true.
		if (rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
			return true;
		}
		
		return false;
	}
	
	// Cast a ray and get detailed intersection data about the point in the scene.
	bool CastSurfaceRay(vec3 origin, vec3 target, out SurfacePoint point) {
		// Compute ray values.
		vec3 pq = target - origin; 
		float tMin = 0.0;
		float tMax = length(pq);	
		vec3 dir = pq / tMax;

		// Set up query to return all possible hits (because of possible transparency).
		uint cullMask = 0xFF;
		rayQueryEXT query;
		rayQueryInitializeEXT(query, TopLevelAS, gl_RayFlagsNoneEXT, cullMask, origin, tMin, dir, tMax);
		
		
		// Traverse the acceleration structure and store information about the first intersection (if any)
		while(rayQueryProceedEXT(query)) {
			if (rayQueryGetIntersectionTypeEXT(query, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
				// Get necessary values to get the objects vertex data.
				// First we get the instance value (DrawDataOffset in the node).
				uint instanceIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(query, false);

				// Then we get the geometryIndex (group in the mesh).
				uint geometryIndex = rayQueryGetIntersectionGeometryIndexEXT(query, false);

				// We combine it to get the final draw index to get the rendered meshes draw data.
				uint drawIndex = instanceIndex + geometryIndex;
				DrawData drawData = GetDrawData(drawIndex);

				// The primitive index is the index of the triangle that was hit.
				uint primitiveIndex = rayQueryGetIntersectionPrimitiveIndexEXT(query, false);

				// We also get the barycentric coordinates of the hit.
				vec2 barycentric = rayQueryGetIntersectionBarycentricsEXT(query, false);
				
				// Then we compute the indices of the vertices of the triangle and get their data.
				uint baseIndex = drawData.IndexOffset + primitiveIndex * 3;
				IndexBufferReference indices = IndexBufferReference(drawData.Indices);
				VertexBufferReference vertices = VertexBufferReference(drawData.Vertices);

				uint i0 = indices.data[baseIndex + 0];
				uint i1 = indices.data[baseIndex + 1];
				uint i2 = indices.data[baseIndex + 2];

				Vertex v0 = vertices.data[i0];
				Vertex v1 = vertices.data[i1];
				Vertex v2 = vertices.data[i2];
				
				// We use the vertex data and the barycentric coordinates to compute the uv coodinates of the hit.
				vec3 barycentricFull = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
				vec2 uv = v0.TextureCoordinates * barycentricFull.x + v1.TextureCoordinates * barycentricFull.y + v2.TextureCoordinates * barycentricFull.z;

				// Then we check if the hit is on a part of the mesh that is opaque.
				bool opaqueHit = false;
				RendererMaterial mat = GetMaterial(drawData.MaterialIndex);
				vec4 albedo = mat.AlbedoFactor;
				if(HasFeature(mat, MATERIAL_FEATURE_ALBEDO_MAP)) {
					albedo *= SampleMaterialTextureLod(mat.AlbedoMap, uv, 0);	
					if(HasFeature(mat, MATERIAL_FEATURE_OPACITY_CUTOUT)) {
						if(albedo.a > mat.AlphaCutoff) {
							opaqueHit = true;
						}
					} else {
						opaqueHit = true;
					}
				} else {
					opaqueHit = true;
				}

				if (opaqueHit) {
					// If we have a hit we confirm it to end traversal of the BVH.
					rayQueryConfirmIntersectionEXT(query);
				}
			} 
		}
		
		if (rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
			// When we have a confirmed hit we do the same computations as above to get the surface data.
			uint instanceIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(query, true);
			uint geometryIndex = rayQueryGetIntersectionGeometryIndexEXT(query, true);
			uint drawIndex = instanceIndex + geometryIndex;
			DrawData drawData = GetDrawData(drawIndex);

			uint primitiveIndex = rayQueryGetIntersectionPrimitiveIndexEXT(query, true);
			vec2 barycentric = rayQueryGetIntersectionBarycentricsEXT(query, true);
			
			uint baseIndex = drawData.IndexOffset + primitiveIndex * 3;
			IndexBufferReference indices = IndexBufferReference(drawData.Indices);
			VertexBufferReference vertices = VertexBufferReference(drawData.Vertices);

			uint i0 = indices.data[baseIndex + 0];
			uint i1 = indices.data[baseIndex + 1];
			uint i2 = indices.data[baseIndex + 2];

			Vertex v0 = vertices.data[i0];
			Vertex v1 = vertices.data[i1];
			Vertex v2 = vertices.data[i2];
			
			vec3 barycentricFull = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
			vec2 uv = v0.TextureCoordinates * barycentricFull.x + v1.TextureCoordinates * barycentricFull.y + v2.TextureCoordinates * barycentricFull.z;

			RendererMaterial mat = GetMaterial(drawData.MaterialIndex);
			vec4 albedo = mat.AlbedoFactor;
			if(HasFeature(mat, MATERIAL_FEATURE_ALBEDO_MAP)) {
				albedo *= SampleMaterialTextureLod(mat.AlbedoMap, uv, 0);	
			}

			// Get the distance of the intersection from the origin to compute the position.
			float t = rayQueryGetIntersectionTEXT(query, true);
			point.Position = origin + t * dir; 

			// Compute the points normal by interpolating the vertex normals.
			vec3 normal = normalize(v0.Normal * barycentricFull.x + v1.Normal * barycentricFull.y + v2.Normal * barycentricFull.z);
			point.Normal = (drawData.WorldInvTrans * vec4(normal, 0)).xyz;

			// Compute other surface values used for shading.
			point.Albedo = albedo;
			float metallic = mat.MetalnessFactor;
			float roughness = mat.RoughnessFactor;
			if(HasFeature(mat, MATERIAL_FEATURE_METALLICROUGHNESS_MAP)) {
				vec2 metallicRoughness = SampleMaterialTextureLod(mat.MetallicRoughnessMap, uv, 0).rg;
				metallic *= metallicRoughness.r;
				roughness *= metallicRoughness.g;	
			}
			point.Metalness = metallic;
			point.Roughness = roughness;

			return true;
		}
		
		return false;
	}
#endif

// Debugging function to count intersections of AABBs for ray interection.
int
CastOverdrawRay(vec3 origin, vec3 target) {
	// Set up node stack and push root.
	uint nodeStack[MAX_STACK_SIZE];
	int stackSize = 1;
	nodeStack[0] = 0;

	// Precompute ray values.
	vec3 rayMidPoint = (origin + target) * 0.5;
	vec3 rayHalfLength = target - rayMidPoint; 
	vec3 qp = origin - target; 

	int aabbCount = 0;
	while(stackSize > 0) {
		DecodedBVHNode node = DecodeNode(GetNode(nodeStack[--stackSize]));
		++aabbCount;

		// Check AABB.
		if(TestSegmentAABB(rayMidPoint, rayHalfLength, node.Center, node.HalfLength)) {
			if (node.IsLeaf) { 
				uint triangleStart = node.TriangleStartOrChildStart;
				uint triangleEnd = triangleStart + node.TriangleCountOrChildCount;

				// Check all leaf triangles.
				for(uint i = triangleStart; i < triangleEnd; ++i) {
					BVHTriangle tri = GetTriangle(i);
					
					// Check if we have a triangle with a transparent material, if yes sample the texture to see if we actually hit something.
					float u, v, w, t;
					if(IntersectSegmentTriangle(origin, qp, tri.A, tri.AB, tri.AC, tri.N, u, v, w, t)) {
						vec2 uv = unpackHalf2x16_emu(tri.TexCoordA) * u + unpackHalf2x16_emu(tri.TexCoordB) * v + unpackHalf2x16_emu(tri.TexCoordC) * w;
						RendererMaterial mat = GetMaterial(tri.MaterialIndex);
						if(HasFeature(mat, MATERIAL_FEATURE_ALBEDO_MAP) && HasFeature(mat, MATERIAL_FEATURE_OPACITY_CUTOUT)) {
							if (SampleMaterialTextureLod(mat.AlbedoMap, uv, 0).a > mat.AlphaCutoff) {
								// Return count of aabb node traversals.
								return aabbCount;
							}
						} else {
							// Return count of aabb node traversals.
							return aabbCount;
						}
					}	
				}
			} else {
				// Put children on stack.
				if(stackSize < MAX_STACK_SIZE - node.TriangleCountOrChildCount) {
					for(uint i = 0; i < node.TriangleCountOrChildCount; ++i) {
						nodeStack[stackSize++] = node.TriangleStartOrChildStart + i;
					}
				} else{
					// Stack overflow.
					aabbCount = 255;
				}
			}
		}
	}
	
	return aabbCount;
}

