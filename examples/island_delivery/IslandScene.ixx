/**
 * @file
 * @brief Island Air Delivery scenery and textured aircraft, authored entirely through VVE's public facade.
 * Photo materials and the Cesium aircraft are downloaded at CMake configuration into the supplied asset root.
 */
export module Island.Scene;
import std;
import VVEngine;
export import Island.Logic;

export namespace island {
	[[nodiscard]] auto groundMeshBatches(vve::Vec3, bool) -> std::array<vve::MeshDescriptor,18>;
	[[nodiscard]] auto buildEnvironment(vve::RenderSystem &, vve::AssetSystem &, const std::filesystem::path &)
		-> std::expected<void, vve::Error>;
	inline constexpr std::array<std::string_view, 4> weatherNames{"clear", "hazy", "overcast", "fog"}; ///< Presets of --weather and the V key.
	[[nodiscard]] auto setWeather(vve::RenderSystem &, std::size_t) -> std::expected<void, vve::Error>;
	[[nodiscard]] auto updateEnvironment(vve::RenderSystem &, const std::filesystem::path &, vve::Vec3,
		std::map<std::size_t,vve::Vector<vve::RenderObjectHandle>> &, vve::Vector<vve::RenderObjectHandle> &)
		-> std::expected<void, vve::Error>;
	[[nodiscard]] auto buildAircraft(vve::RenderSystem &, vve::AssetSystem &, vve::SceneNodeHandle, const std::filesystem::path &)
		-> std::expected<void, vve::Error>;
	[[nodiscard]] auto updateAircraft(vve::RenderSystem &, vve::SceneNodeHandle, const Flight &)
		-> std::expected<void, vve::Error>;
}

namespace island::scenery {
	namespace vm = vve::math;
	constexpr float pi{std::numbers::pi_v<float>}; ///< Circle constant for roofs, palms and aircraft rotations.
	constexpr std::array<std::array<float,4>,5> pavedAreas{{{-18,18,-350,350},{25,77,-265,-135},
		{42,72,-154,-130},{17,47,-240,-213},{-25,25,-300,-130}}}; ///< Local min/max X and Z for strip, apron, forecourt, taxiway and unloading.
	/// @brief Selects the exposed paving material; intersecting areas never produce stacked surface polygons.
	unsigned pavementMaterial(float x,float z,bool detailed) {
		constexpr std::array materials{4U,5U,5U,4U,4U}; // Taxiways and unloading remain asphalt where they join concrete.
		for (std::size_t i=detailed?pavedAreas.size():1U; i>0U; --i) {
			const auto &area=pavedAreas[i-1U];
			if (x>area[0] && x<area[1] && z>area[2] && z<area[3]) { return materials[i-1U]; }
		}
		return 0U;
	}
	/// @brief Splits an optional terrain grid at every paving edge, so terrain and paving share exact boundaries.
	auto surfaceCoordinates(int cells,bool detailed,unsigned axis) -> std::vector<float> {
		std::vector<float> values{};
		if (cells>0) { for (int i=0; i<=cells; ++i) { values.push_back(-1000.0F+static_cast<float>(i)*2000.0F/static_cast<float>(cells)); } }
		for (std::size_t i{}; i<(detailed?pavedAreas.size():1U); ++i) {
			values.push_back(pavedAreas[i][axis]); values.push_back(pavedAreas[i][axis+1U]);
		}
		std::ranges::sort(values); values.erase(std::unique(values.begin(),values.end()),values.end()); return values;
	}

	/// @brief Places an object without introducing engine-specific placement data.
	[[nodiscard]] vve::Transform at(vve::Vec3 position, float scale = 1.0F) {
		return {.translation = {.value = position}, .scale = {.value = vve::Vec3{scale, scale, scale}}};
	}
	/// @brief Appends a textured convex face, correcting winding to face the supplied outward direction.
	void face(vve::MeshDescriptor &mesh, std::span<const vve::Vec3> points, vve::Vec3 outward, float tile = 1.0F) {
		auto normal = vm::cross(vm::subtract(points[1], points[0]), vm::subtract(points[2], points[0])); // Geometric face direction.
		if (vm::lengthSquared(normal)<1.0e-10F) { return; } // A contour touching one corner contributes no visible face.
		const bool flip = vm::dot(normal, outward) < 0.0F; // Preserve visible fronts on either side of a shape.
		normal = vm::normalize(flip ? vm::scale(normal, -1.0F) : normal); // Unit normal for PBR illumination.
		const auto base = static_cast<std::uint32_t>(mesh.positions.size()); // Local indices remain valid as the batch grows.
		const bool horizontal = std::abs(normal.y) > 0.7F; // Flat surfaces use a ground-plane photo projection.
		const auto across = horizontal ? vve::Vec3{1, 0, 0} : vm::normalize(vve::Vec3{-normal.z, 0, normal.x});
		// Map photos in metres, so boxes and terrain retain consistent surface detail.
		for (const auto point : points) {
			mesh.positions.push_back(point); mesh.normals.push_back(normal); // Faceted structures retain their sharp corners.
			mesh.uvs.push_back(vm::scale(vve::Vec2{vm::dot(point, across), horizontal ? point.z : point.y}, 1.0F/tile));
		}
		// Convex faces share one fan, including triangular caps and four-sided panels.
		for (std::uint32_t index{1}; index + 1U < points.size(); ++index) {
			mesh.indices.insert(mesh.indices.end(), {base, base + (flip ? index + 1U : index), base + (flip ? index : index + 1U)});
		}
	}
	/// @brief Appends an axis-aligned box to a shared material batch.
	void box(vve::MeshDescriptor &mesh, vve::Vec3 low, vve::Vec3 high, float tile = 1.0F) {
		const auto p = [&](bool x, bool y, bool z) { return vve::Vec3{x ? high.x : low.x, y ? high.y : low.y, z ? high.z : low.z}; }; // Shared corners.
		const std::array<std::array<vve::Vec3, 4>, 6> faces{{
			{p(1,0,0),p(1,1,0),p(1,1,1),p(1,0,1)}, {p(0,0,0),p(0,0,1),p(0,1,1),p(0,1,0)},
			{p(0,1,0),p(0,1,1),p(1,1,1),p(1,1,0)}, {p(0,0,0),p(1,0,0),p(1,0,1),p(0,0,1)},
			{p(0,0,1),p(1,0,1),p(1,1,1),p(0,1,1)}, {p(0,0,0),p(0,1,0),p(1,1,0),p(1,0,0)}}};
		const std::array<vve::Vec3, 6> normals{vve::Vec3{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
		// Append all six faces to the existing material instead of creating six render objects.
		for (const auto index : std::views::iota(0U, 6U)) { face(mesh, faces[index], normals[index], tile); }
	}
	/// @brief Appends a tapered round strut, trunk or mast between two points.
	void tube(vve::MeshDescriptor &mesh, vve::Vec3 from, vve::Vec3 to, float radius, float endRadius, unsigned sides = 8U) {
		const auto axis = vm::normalize(vm::subtract(to, from)); // Round pieces may point along any world direction.
		const auto side = vm::normalize(vm::cross(axis, std::abs(axis.y) < 0.9F ? vve::Vec3{0,1,0} : vve::Vec3{1,0,0}));
		const auto up = vm::cross(axis, side); // Perpendicular plane containing the tube's circular cross-section.
		// The caps are hidden inside neighbouring pieces; the visible round sides retain photographic grain.
		for (const auto index : std::views::iota(0U, sides)) {
			const auto radial = [&](unsigned i) { const float a = 2.0F * pi * static_cast<float>(i) / static_cast<float>(sides);
				return vm::add(vm::scale(side, std::cos(a)), vm::scale(up, std::sin(a))); };
			const auto a = radial(index), b = radial(index + 1U); // Neighbouring directions on the same circular ring.
			face(mesh, std::array{vm::add(from, vm::scale(a, radius)), vm::add(to, vm::scale(a, endRadius)),
				vm::add(to, vm::scale(b, endRadius)), vm::add(from, vm::scale(b, radius))}, a, 0.8F);
		}
	}
	/// @brief Produces stable scenery variation without mutable random state.
	[[nodiscard]] float noise(unsigned seed) {
		seed ^= seed >> 16U; seed *= 0x7feb352dU; seed ^= seed >> 15U; seed *= 0x846ca68bU; seed ^= seed >> 16U;
		return static_cast<float>(seed & 0xffffU) / 65535.0F; // Reproducible variation in the closed interval [0,1].
	}
	/// @brief Appends an irregular weathered coastal boulder, embedded below its base for a natural shoreline contact.
	void boulder(vve::MeshDescriptor &mesh, vve::Vec3 base, float size, unsigned seed) {
		constexpr std::array<vve::Vec2,5> profile{vve::Vec2{0.85F,-0.25F},{1.0F,0.12F},{0.83F,0.60F},{0.40F,0.92F},{0.02F,0.99F}}; // Width and elevation.
		const auto point=[&](unsigned ring,unsigned side) {
			const float a=2*pi*static_cast<float>(side)/12.0F, radius=profile[ring].x*(0.85F+noise(seed+side%12U)*0.3F);
			return vm::add(base,vve::Vec3{std::cos(a)*radius*size,profile[ring].y*size,std::sin(a)*radius*size*0.76F});
		};
		// Broad facets and asymmetric ridges keep repeated rocks from reading as identical spheres.
		for (const auto ring : std::views::iota(0U,4U)) {
			// Keep the same radial irregularity through successive height rings.
			for (const auto side : std::views::iota(0U,12U)) {
				const auto a=point(ring,side), b=point(ring,side+1U);
				face(mesh,std::array{a,b,point(ring+1U,side+1U),point(ring+1U,side)},vm::subtract(a,base),3.0F);
			}
		}
	}
	/// @brief Appends a naturally bent palm with narrow, individually tapered leaflets.
	void palm(vve::MeshDescriptor &bark, vve::MeshDescriptor &leaves, vve::Vec3 base, float height, float angle) {
		const vve::Vec3 bend{std::cos(angle) * height * 0.13F, 0, std::sin(angle) * height * 0.13F}; // Tree-specific lean.
		const auto stem = [&](float t) { return vm::add(base, vm::add(vve::Vec3{0, t * height, 0}, vm::scale(bend, t * t))); };
		// Short tapered sections produce a curved trunk rather than a straight pole.
		for (const auto section : std::views::iota(0U, 5U)) {
			const float t = static_cast<float>(section) / 5.0F; // Fractional height along the curved trunk.
			tube(bark, stem(t), stem(t + 0.2F), 0.25F - t * 0.12F, 0.25F - (t + 0.2F) * 0.12F, 7U);
		}
		// The golden-angle spacing avoids a rigid star-shaped crown.
		for (const auto frond : std::views::iota(0U, 9U)) {
			const float a = angle + static_cast<float>(frond) * 2.39996F;
			const vve::Vec3 along{std::cos(a), 0, std::sin(a)}, across{-std::sin(a), 0, std::cos(a)};
			const float reach = height * (0.37F + 0.09F * std::sin(a * 3.0F)); // Fronds have visibly different lengths.
			const auto spine = [&](float t) { return vm::add(stem(1), vm::add(vm::scale(along, reach * t), vve::Vec3{0, reach * (0.6F * t - t * t), 0})); };
			// Paired leaflets make the feathery silhouette visible during low approaches.
			for (const auto leaflet : std::views::iota(1U, 11U)) {
				const float t = static_cast<float>(leaflet) / 12.0F, width = reach * 0.20F * std::sin(pi * t);
				// Each rib carries one tapering leaflet on each side.
				for (const float sideSign : {-1.0F, 1.0F}) {
					const auto tip = vm::add(spine(t + 0.13F), vm::add(vm::scale(across, sideSign * width), vve::Vec3{0,-0.18F,0}));
					const std::array points{spine(t - 0.035F), tip, spine(t + 0.045F)}; // A thin leaf with a pointed end.
					face(leaves, points, vve::Vec3{0,1,0}, 2.0F); face(leaves, points, vve::Vec3{0,-1,0}, 2.0F);
				}
			}
		}
	}
	/// @brief Adds a branching coastal tree or low shrub to the same bark and foliage batches as the palms.
	void broadleaf(vve::MeshDescriptor &bark, vve::MeshDescriptor &leaves, vve::Vec3 base, float height, unsigned seed) {
		const auto crown=vm::add(base,vve::Vec3{0,height*0.66F,0});
		tube(bark,base,crown,height*0.045F,height*0.022F,6U);
		// Overlapping irregular crowns form a leafy silhouette rather than a single geometric cone.
		for (unsigned branch{}; branch<3U; ++branch) {
			const float angle=static_cast<float>(branch)*2.39996F+noise(seed)*6.28F;
			const auto tip=vm::add(crown,vve::Vec3{std::cos(angle)*height*0.23F,height*0.12F,std::sin(angle)*height*0.23F});
			tube(bark,vm::add(base,vve::Vec3{0,height*0.4F,0}),tip,height*0.018F,height*0.007F,5U);
			boulder(leaves,tip,height*0.36F,seed+branch*17U);
		}
	}
	/// @brief Adds a tuft of broad tropical leaves, with two visible sides and no extra render objects.
	void plant(vve::MeshDescriptor &leaves, vve::Vec3 base, float size, float angle) {
		for (unsigned leaf{}; leaf<7U; ++leaf) {
			const float a=angle+static_cast<float>(leaf)*2.39996F;
			const vve::Vec3 along{std::cos(a),0,std::sin(a)}, across{-std::sin(a)*size*0.16F,0,std::cos(a)*size*0.16F};
			const auto middle=vm::add(base,vm::add(vm::scale(along,size*0.52F),vve::Vec3{0,size*0.75F,0}));
			const auto tip=vm::add(base,vm::add(vm::scale(along,size),vve::Vec3{0,size*0.5F,0}));
			const std::array points{base,vm::subtract(middle,across),tip,vm::add(middle,across)};
			face(leaves,points,{0,1,0},1.5F); face(leaves,points,{0,-1,0},1.5F);
		}
	}
	/// @brief Appends smooth terrain and shoreline to material batches, using the physics height function.
	void terrain(std::array<vve::MeshDescriptor, 18> &meshes, vve::Vec3 centre, int cells,bool detailed) {
		const auto xs=surfaceCoordinates(cells,detailed,0), zs=surfaceCoordinates(cells,detailed,2); // Grid lines meet all pavement edges exactly.
		// A scattered neighbour may touch this square's corners; restrict sampling to this island's enclosing disc.
		const auto height=[centre](float x,float z) { return x*x+z*z>1000000.0F?0.0F:terrainHeight(x+centre.x,z+centre.z); };
		const auto normal = [&height](float x, float z) { return vm::normalize(vve::Vec3{
			height(x - 1.0F, z) - height(x + 1.0F, z), 2.0F,height(x, z - 1.0F) - height(x, z + 1.0F)}); };
		// A shared height field guarantees that visible shores and wheels agree with collision handling.
		for (std::size_t row{}; row+1U<zs.size(); ++row) {
			// Ground cells outside the finite island footprint are omitted, exposing the ocean.
			for (std::size_t column{}; column+1U<xs.size(); ++column) {
				const float x=xs[column], z=zs[row], nextX=xs[column+1U], nextZ=zs[row+1U];
				if (pavementMaterial((x+nextX)*0.5F,(z+nextZ)*0.5F,detailed)!=0U) { continue; } // No hidden earth remains beneath paving.
				const auto point = [&height](float px, float pz) {
					return vve::Vec3{px,height(px,pz),pz}; // Unpaved ground retains the analytic collision height.
				};
				const std::array points{point(x,z),point(x,nextZ),point(nextX,nextZ),point(nextX,z)};
				const float height = (points[0].y + points[1].y + points[2].y + points[3].y) * 0.25F; // Cell material elevation.
				if (height < 0.02F) { continue; } // Zero-height water cells must not become sand-covered squares.
				const auto n = normal((x+nextX)*0.5F,(z+nextZ)*0.5F); // Slope differentiates exposed rock from vegetation.
				const unsigned material = height < 2.8F ? 2U : n.y < 0.86F ? 3U :
					(std::sin(x * 0.021F) + std::cos(z * 0.018F) + std::sin((x + z) * 0.009F) > 0.0F ? 0U : 1U);
				auto &mesh = meshes[material]; const auto base = static_cast<std::uint32_t>(mesh.positions.size()); // One material per cell.
				// Smooth normals span cells even when the adjoining cells use different photo materials.
				for (const auto p : points) {
					mesh.positions.push_back(p); mesh.normals.push_back(normal(p.x,p.z));
					mesh.uvs.push_back(vm::scale(vve::Vec2{p.x,p.z},1.0F/(material == 2U ? 12.0F : 8.0F)));
				}
				mesh.indices.insert(mesh.indices.end(), {base,base+1U,base+2U,base,base+2U,base+3U}); // Upward-facing ground triangles.
				// Follow the actual shore contour with a narrow translucent line of breaking water.
				std::vector<vve::Vec3> shore{}; // Two crossings locate a contour segment through the cell.
				for (const auto edge : std::views::iota(0U, 4U)) {
					const auto a = points[edge], b = points[(edge + 1U) % 4U];
					if ((a.y < 0.45F) == (b.y < 0.45F)) { continue; } // This edge does not cross the surf contour.
					const float t = (0.45F - a.y) / (b.y - a.y); // Shore position between neighbouring terrain samples.
					shore.push_back(vve::Vec3{a.x + t * (b.x-a.x), 0.28F, a.z + t * (b.z-a.z)});
				}
				// The surf strip extends toward the sea from the contour rather than across the beach.
				if (shore.size() == 2U) {
					// A coarse cell's interpolated shore may lie outside the analytic coast; use that cell's own slope.
					auto across=vve::Vec3{points[0].y+points[1].y-points[2].y-points[3].y,0,points[0].y+points[3].y-points[1].y-points[2].y};
					if (vm::lengthSquared(across)<1.0e-10F) { continue; }
					across = vm::scale(vm::normalize(across), 2.3F); // Horizontal strip width in metres.
					face(meshes[14], std::array{shore[0],shore[1],vm::add(shore[1],across),vm::add(shore[0],across)}, {0,1,0});
				}
			}
		}
	}
	/// @brief Describes paint footprints for the shared paving tessellation; paint replaces asphalt instead of floating above it.
	auto runwayPaint(bool detailed) -> std::vector<std::pair<std::array<float,4>,unsigned>> {
		std::vector<std::pair<std::array<float,4>,unsigned>> paint{};
		for (const float side : {-1.0F,1.0F}) {
			paint.push_back({{side*17-0.18F,side*17+0.18F,-345,345},8U});
			if (!detailed) { continue; }
			for (unsigned bar{}; bar<8U; ++bar) {
				const float x=-14+static_cast<float>(bar)*4;
				paint.push_back({{x,x+1.6F,side*327-6,side*327+6},8U});
			}
		}
		if (!detailed) { return paint; } // Distant strips retain only their edge lines.
		for (unsigned dash{}; dash<23U; ++dash) {
			const float z=-286+static_cast<float>(dash)*26; paint.push_back({{-0.3F,0.3F,z,z+12},8U});
		}
		// Each seven-segment numeral contributes rectangles; shared segment corners are also tessellated only once.
		const auto numeral=[&](float x,float z,unsigned digit,float facing) {
			constexpr std::array masks{63U,6U,91U,79U,102U,109U,125U,7U,127U,111U};
			constexpr std::array<vve::Vec2,7> centres{vve::Vec2{0,-3},{1.5F,-1.5F},{1.5F,1.5F},{0,3},{-1.5F,1.5F},{-1.5F,-1.5F},{0,0}};
			for (unsigned segment{}; segment<7U; ++segment) {
				if (!(masks[digit] & (1U<<segment))) { continue; }
				const bool horizontal=segment==0U || segment==3U || segment==6U;
				const float cx=x+facing*centres[segment].x, cz=z+facing*centres[segment].y;
				const float hx=horizontal?1.5F:0.24F, hz=horizontal?0.24F:1.5F;
				paint.push_back({{cx-hx,cx+hx,cz-hz,cz+hz},8U});
			}
		};
		numeral(-2.4F,306,3,1); numeral(2.4F,306,6,1); numeral(2.4F,-306,1,-1); numeral(-2.4F,-306,8,-1);
		// Yellow paint stays inside the exact unloading footprint, including its corner intersections.
		paint.push_back({{-25,-24.76F,-300,-130},9U}); paint.push_back({{24.76F,25,-300,-130},9U});
		paint.push_back({{-25,25,-300,-299.76F},9U}); paint.push_back({{-25,25,-130.24F,-130},9U});
		return paint;
	}
	/// @brief Builds a conventional paved airstrip, its apron, open hangar, terminal, windsock and cargo.
	void airfield(std::array<vve::MeshDescriptor,18> &m, float z, bool detailed) {
		const float y{runwayHeight}; // Asphalt and wheel collision use the same reference elevation.
		if (!detailed) { return; } // Ground batches already contain the distant strip and its flush paint.
		for (const float side : {-1.0F,1.0F}) {
			// Warm edge lights stand just outside the paved strip.
			for (const auto index : std::views::iota(0U,29U)) {
				const float pz = z-336.0F+static_cast<float>(index)*24.0F;
				box(m[6],{side*20.0F-0.10F,y,pz-0.1F},{side*20.0F+0.10F,y+0.45F,pz+0.1F});
				box(m[15],{side*20.0F-0.16F,y+0.45F,pz-0.16F},{side*20.0F+0.16F,y+0.57F,pz+0.16F});
			}
		}
		// The curved corrugated roof and a dark interior make the hangar readable from the final approach.
		const auto arch = [&](float x, float angle) { return vve::Vec3{x,y+std::sin(angle)*12.0F,z-202+std::cos(angle)*18.0F}; };
		// The hangar is open toward the apron; its back face gives the interior depth.
		for (const auto segment : std::views::iota(0U,24U)) {
			const float a = pi*static_cast<float>(segment)/24.0F, b = pi*static_cast<float>(segment+1U)/24.0F;
			face(m[6],std::array{arch(44,a),arch(75,a),arch(75,b),arch(44,b)},{0,std::sin(a),std::cos(a)},2.0F);
			face(m[12],std::array{vve::Vec3{74.9F,y,z-202},arch(74.9F,a),arch(74.9F,b)},{-1,0,0});
			tube(m[8],arch(43.9F,a),arch(43.9F,b),0.12F,0.12F,5U);
		}
		box(m[5],{49,y,z-130},{73,y+4.7F,z-117},2.0F); box(m[6],{48.5F,y+4.7F,z-131},{73.5F,y+5.0F,z-116},2.0F);
		// Dark inset panels read as terminal windows from the runway side.
		for (const auto window : std::views::iota(0U,5U)) {
			const float pz = z-128+static_cast<float>(window)*2.1F;
			box(m[12],{48.96F,y+1.5F,pz},{49,y+3.6F,pz+1.5F});
		}
		// Pallet stacks, crates and a small service tower establish the working cargo airfield.
		for (const auto crate : std::views::iota(0U,24U)) {
			const float x = 31+static_cast<float>(crate%4U)*2.4F, pz = z-163+static_cast<float>(crate/4U)*2.1F, height=1.3F+noise(crate)*0.6F;
			box(m[7],{x,y+0.15F,pz},{x+1.5F,y+height,pz+1.5F},1.3F);
			box(m[12],{x+0.35F,y+0.1F,pz-0.02F},{x+0.44F,y+height+0.01F,pz+1.52F});
		}
		box(m[5],{62,y,z-106},{68,y+8,z-100},2); box(m[12],{61.8F,y+8,z-106.2F},{68.2F,y+10,z-99.8F});
		box(m[6],{61.4F,y+10,z-106.6F},{68.6F,y+10.4F,z-99.4F},2);
		const vve::Vec3 pole{-32,y,z-195}; tube(m[8],pole,vm::add(pole,{0,8,0}),0.09F,0.07F);
		// Alternating sleeves and a downward taper distinguish the windsock from a flag.
		for (const auto stripe : std::views::iota(0U,8U)) {
			const float a = static_cast<float>(stripe)/8.0F, b = static_cast<float>(stripe+1U)/8.0F;
			tube(m[stripe%2U ? 8U : 13U],vm::add(pole,{a*3.7F,7.8F-a*0.7F,0}),vm::add(pole,{b*3.7F,7.8F-b*0.7F,0}),0.5F*(1-a)+0.1F,0.5F*(1-b)+0.1F,12U);
		}
	}
	/// @brief Submits one batch and applies its PBR surface, preserving fallible facade results.
	auto submit(vve::RenderSystem &render, vve::MeshDescriptor mesh, unsigned material,
		vve::Vector<vve::RenderObjectHandle> &objects) -> std::expected<void,vve::Error> {
		if (mesh.indices.empty()) { return {}; } // Unused looks do not allocate renderer resources.
		const auto object = render.addMesh(std::move(mesh)); if (!object) { return std::unexpected(object.error()); }
		objects.push_back(*object); // The caller owns every new handle, including cleanup after a later failure.
		if (const auto result = render.setObjectSurface(*object,{.roughness=material==6U?0.48F:0.9F,.metalness=material==6U?0.7F:0.0F}); !result) { return result; }
		if (material >= 15U) { if (const auto result = render.setObjectUnlit(*object,true); !result) { return result; } }
		// Foam is translucent and cannot cast opaque shadows onto the water.
		if (material == 14U) {
			if (const auto result = render.setObjectOpacity(*object,0.38F); !result) { return result; }
			if (const auto result = render.setObjectCastsShadow(*object,false); !result) { return result; }
		}
		return {};
	}
	/// @brief Builds one island in local coordinates and returns the exact render objects its resident entry owns.
	auto buildIsland(vve::RenderSystem &render,const std::filesystem::path &root,std::size_t id,bool detailed)
		-> std::expected<vve::Vector<vve::RenderObjectHandle>,vve::Error> {
		const auto centre=islandCenter(id); auto meshes=groundMeshBatches(centre,detailed);
		const std::array<vve::Vec3,18> colors{vve::Vec3{0.22F,0.38F,0.12F},{0.32F,0.44F,0.16F},{0.98F,0.94F,0.80F},
			{0.62F,0.65F,0.63F},{0.74F,0.76F,0.78F},{0.95F,0.94F,0.91F},{0.68F,0.74F,0.76F},{0.75F,0.64F,0.45F},
			{0.90F,0.91F,0.85F},{0.92F,0.60F,0.10F},{0.23F,0.34F,0.095F},{0.55F,0.40F,0.24F},{0.025F,0.045F,0.052F},
			{0.80F,0.19F,0.035F},{0.54F,0.67F,0.62F},{1.0F,0.68F,0.22F},{0.08F,0.3F,0.9F},{0.1F,0.7F,0.15F}};
		const std::array<std::string_view,18> textures{"grass.jpg","grass.jpg","sand.jpg","rock.jpg","asphalt.jpg","concrete.jpg","metal.jpg","planks.jpg","","","grass.jpg","planks.jpg","","","","","",""};
		// All batches share the island origin, so even the distant grid sites retain well-conditioned mesh coordinates.
		for (const auto index : std::views::iota(0U,18U)) {
			meshes[index].color.value=colors[index]; meshes[index].transform=at(centre);
			if (!textures[index].empty()) { meshes[index].base_color_texture=root/textures[index]; }
		}
		airfield(meshes,0,detailed);
		// Full shoreline vegetation is only resident within approach distance; stable island seeds retain its layout.
		if (detailed) {
			for (const auto rock : std::views::iota(0U,105U)) {
				const unsigned seed=rock+static_cast<unsigned>(id)*541U;
				const float angle=static_cast<float>(rock)*2.39996F,radius=0.82F+noise(seed*13U+1U)*0.19F;
				const float x=std::cos(angle)*650*radius,z=std::sin(angle)*750*radius,y=terrainHeight(x+centre.x,z+centre.z);
				if (std::abs(x)<85 || y<0.25F || y>14.0F) { continue; }
				boulder(meshes[3],{x,y,z},1.1F+noise(seed*47U)*4.0F,seed*17U);
			}
			for (const auto tree : std::views::iota(0U,340U)) {
				const unsigned seed=tree+static_cast<unsigned>(id)*127U;
				const float angle=static_cast<float>(tree)*2.39996F,radius=0.48F+noise(seed*71U+9U)*0.38F;
				const float x=std::cos(angle)*650*radius,z=std::sin(angle)*750*radius,y=terrainHeight(x+centre.x,z+centre.z);
				if (std::abs(x)<95 || y<3.0F || y>47.0F) { continue; }
				palm(meshes[11],meshes[10],{x,y-0.05F,z},7.0F+noise(seed*27U+1U)*7.0F,angle);
			}
			// Fill the interior with broadleaf groves and lower plants, leaving a generous runway/approach corridor.
			for (unsigned item{}; item<1900U; ++item) {
				const unsigned seed=item+static_cast<unsigned>(id)*1879U;
				const float angle=static_cast<float>(item)*2.39996F, radius=std::sqrt(noise(seed*37U+3U))*0.86F;
				const float x=std::cos(angle)*650*radius,z=std::sin(angle)*750*radius,y=terrainHeight(x+centre.x,z+centre.z);
				if (std::abs(x)<100 || y<3.0F || y>65.0F) { continue; }
				if (item<420U) { broadleaf(meshes[11],meshes[10],{x,y-0.1F,z},4+noise(seed*23U)*8,seed); }
				else { plant(meshes[item%3U==0U?1:10],{x,y-0.08F,z},0.7F+noise(seed*19U)*1.5F,angle); }
			}
			// A few planted palms frame the terminal while staying beyond the service lanes and wing clearance.
			for (const auto z : {-265.0F,-232.0F,-170.0F,-138.0F,-106.0F}) {
				palm(meshes[11],meshes[10],{87,terrainHeight(centre.x+87,centre.z+z)-0.1F,z},9.5F,z);
			}
		}
		vve::Vector<vve::RenderObjectHandle> objects{}; objects.reserve(18);
		// Failed loads release every object already created for this island, without touching aircraft or neighbours.
		for (const auto index : std::views::iota(0U,18U)) {
			if (const auto result=submit(render,std::move(meshes[index]),index,objects); !result) {
				std::cerr<<"[island_delivery] scenery_failed island="<<id<<" material="<<index<<" detailed="<<detailed<<" error="<<vve::errorName(result.error())<<'\n';
				for (const auto object : objects) { (void)render.removeObject(object); }
				return std::unexpected(result.error());
			}
		}
		return objects;
	}
}

namespace island {
	/// @brief Authors disjoint ground surfaces at the collision height, shared by scenery and CPU geometry verification.
	auto groundMeshBatches(vve::Vec3 centre,bool detailed) -> std::array<vve::MeshDescriptor,18> {
		std::array<vve::MeshDescriptor,18> meshes{};
		scenery::terrain(meshes,centre,detailed?200:50,detailed);
		auto xs=scenery::surfaceCoordinates(0,detailed,0), zs=scenery::surfaceCoordinates(0,detailed,2);
		const auto paint=scenery::runwayPaint(detailed);
		for (const auto &[area,material] : paint) { xs.insert(xs.end(),{area[0],area[1]}); zs.insert(zs.end(),{area[2],area[3]}); }
		for (auto *axis : {&xs,&zs}) { std::ranges::sort(*axis); axis->erase(std::unique(axis->begin(),axis->end()),axis->end()); }
		// Paving and markings share one plane and one tessellation, with no substrate beneath the paint.
		for (std::size_t row{}; row+1U<zs.size(); ++row) { for (std::size_t column{}; column+1U<xs.size(); ++column) {
			const float x=xs[column], nextX=xs[column+1U], z=zs[row], nextZ=zs[row+1U];
			const float cx=(x+nextX)*0.5F, cz=(z+nextZ)*0.5F;
			auto material=scenery::pavementMaterial(cx,cz,detailed);
			if (material==0U) { continue; }
			for (const auto &[area,colour] : paint) {
				if (cx>area[0] && cx<area[1] && cz>area[2] && cz<area[3]) { material=colour; }
			}
			scenery::face(meshes[material],std::array<vve::Vec3,4>{vve::Vec3{x,runwayHeight,z},{x,runwayHeight,nextZ},
				{nextX,runwayHeight,nextZ},{nextX,runwayHeight,z}},{0,1,0},4.0F);
		} }
		for (auto &mesh : meshes) { mesh.transform=scenery::at(centre); } return meshes;
	}
	/// @brief Applies a weather preset: physical sky, clouds and haze. Clouds shade the sun, so the sun light stays the same.
	auto setWeather(vve::RenderSystem &render, std::size_t preset) -> std::expected<void, vve::Error> {
		const std::array<std::pair<vve::SkySettings, vve::AtmosphereSettings>, weatherNames.size()> presets{{
			{{.enabled=true,.cloud_coverage=0.35F,.cloud_base=900,.cloud_thickness=1100}, {.density=0.00003F,.height_falloff=0.003F}},
			{{.enabled=true,.haze=5,.cloud_coverage=0.2F,.cloud_base=2200,.cloud_thickness=500,.cloud_density=0.5F}, {.density=0.00015F,.height_falloff=0.002F}},
			{{.enabled=true,.haze=3,.cloud_coverage=1,.cloud_base=450,.cloud_thickness=900,.cloud_density=1.3F}, {.density=0.00012F,.height_falloff=0.002F}},
			{{.enabled=true,.haze=2,.cloud_coverage=0.45F,.cloud_base=1400,.cloud_thickness=900}, {.density=0.0025F,.height_falloff=0.015F}}}}; // Sea fog
		const auto &[sky, haze] = presets[preset % presets.size()];
		if (auto result = render.setSky(sky); !result) { return result; }
		return render.setAtmosphere(haze);
	}
	/// @brief Sets sunlight, materials and the calm horizon; weather, nearby islands and waves are set separately.
	auto buildEnvironment(vve::RenderSystem &render, vve::AssetSystem &assets, const std::filesystem::path &root)
		-> std::expected<void,vve::Error> {
		using namespace scenery; (void)assets; (void)root; // Scenery meshes use the facade's direct authoring API.
		if (const auto result = render.setMaterialLighting(vve::MaterialLighting::physically_based); !result) { return result; }
		render.setDirectionalLight({.value={-0.45F,-0.74F,0.28F}},{.value={1.0F,0.95F,0.85F}},{.value=1.4F},{.value={0.07F,0.10F,0.13F}});
		const auto ocean = render.addOcean({.half_extent={60000,60000},.color={.value={0.018F,0.16F,0.19F}},
			.wave_height=0,.wavelength=48,.speed=0.8F,.position={.value={18000,-0.52F,-18000}}});
		if (!ocean) { return std::unexpected(ocean.error()); }
		return {};
	}
	/// @brief Keeps the nearest nine islands within four kilometres resident, with full airfield detail on approach.
	auto updateEnvironment(vve::RenderSystem &render,const std::filesystem::path &root,vve::Vec3 eye,
		std::map<std::size_t,vve::Vector<vve::RenderObjectHandle>> &resident,vve::Vector<vve::RenderObjectHandle> &water)
		-> std::expected<void,vve::Error> {
		using namespace scenery;
		std::array<std::pair<float,std::size_t>,islandCount> nearest{};
		// A fixed stack array avoids frame allocations while choosing a deterministic bounded working set.
		for (const auto id : std::views::iota(std::size_t{},islandCount)) {
			const auto centre=islandCenter(id); nearest[id]={std::hypot(eye.x-centre.x,eye.z-centre.z),id};
		}
		std::ranges::sort(nearest); const auto wanted=std::span{nearest}.first(9);
		// Remove departed or superseded detail batches explicitly; removing a node alone would retain their resources.
		for (auto entry=resident.begin(); entry!=resident.end();) {
			const auto found=std::ranges::find(wanted,entry->first,[](const auto &item) { return item.second; });
			const bool detailed=entry->second.size()>8U; // Distant islands have at most eight terrain/runway batches.
			const bool keep=found!=wanted.end() && found->first<=4000 && (detailed?found->first<=2200:found->first>=1800);
			if (keep) { ++entry; continue; }
			for (const auto object : entry->second) { if (const auto result=render.removeObject(object); !result) { return result; } }
			entry=resident.erase(entry);
		}
		// Unchanged entries retain their handles and decoded textures, including during ordinary frame updates.
		for (const auto [distance,id] : wanted) {
			if (distance>4000 || resident.contains(id)) { continue; }
			auto objects=buildIsland(render,root,id,distance<=2000); if (!objects) { return std::unexpected(objects.error()); }
			resident.emplace(id,std::move(*objects));
		}
		const vve::Vec3 origin{std::round(eye.x/512)*512,-0.16F,std::round(eye.z/512)*512};
		// Waves are fixed eight-metre grids; their handles and geometry survive travel across the entire archipelago.
		water.reserve(45);
		for (std::size_t index=water.size(); index<45U; ++index) {
			const auto tile=render.addOcean({.half_extent={256,256},.color={.value={0.018F,0.16F,0.19F}},
				.wave_height=0.18F,.wavelength=48,.speed=0.8F});
			if (!tile) { return std::unexpected(tile.error()); } water.push_back(*tile);
			if (const auto result=render.setObjectSurface(*tile,{.roughness=0.14F}); !result) { return result; }
		}
		const auto first=render.objectTransform(water.front()); if (!first) { return std::unexpected(first.error()); }
		if (first->translation.value.x==origin.x-1024 && first->translation.value.y==origin.y && first->translation.value.z==origin.z-2048) { return {}; }
		// World-space wave phases match on shared edges, even after repositioning the patch grid at a cell boundary.
		for (const auto index : std::views::iota(std::size_t{},water.size())) {
			const auto offset=vve::Vec3{static_cast<float>(index%5U)*512-1024,0,static_cast<float>(index/5U)*512-2048};
			if (const auto result=render.setObjectTransform(water[index],at(vm::add(origin,offset))); !result) { return result; }
		}
		return {};
	}
	/// @brief Instantiates the textured Cesium twin-prop aircraft, keeping its animated propeller hierarchy.
	auto buildAircraft(vve::RenderSystem &render, vve::AssetSystem &assets, vve::SceneNodeHandle parent, const std::filesystem::path &root)
		-> std::expected<void,vve::Error> {
		const auto model=assets.loadScene(root/"aircraft.glb"); if (!model) { return std::unexpected(model.error()); } // Downloaded, licensed glTF asset.
		const auto instance=render.instantiateScene(*model,{.keep_hierarchy=true}); if (!instance) { return std::unexpected(instance.error()); }
		const auto node=render.sceneInstanceRoot(*instance); if (!node) { return std::unexpected(node.error()); }
		// The source faces +Z; reverse it to match flight's -Z forward. Scaling also gives exact tyre contact.
		auto placement=scenery::at({0,-0.01016F,0},0.54F); placement.rotation.value={0,0,1,0};
		if (const auto result=render.setNodeTransform(*node,placement); !result) { return result; }
		if (const auto result=render.setNodeParent(*node,parent); !result) { return result; }
		return {};
	}
	/// @brief Moves the entire aircraft and animates its two propellers from the deterministic flight clock.
	auto updateAircraft(vve::RenderSystem &render, vve::SceneNodeHandle parent, const Flight &flight)
		-> std::expected<void,vve::Error> {
		using namespace scenery;
		const auto axis=[](float angle,unsigned coordinate) { const float s=std::sin(angle*0.5F);
			return vve::Quat{std::cos(angle*0.5F),coordinate==0U?s:0.0F,coordinate==1U?s:0.0F,coordinate==2U?s:0.0F}; };
		auto transform=at(flight.position); transform.rotation.value=vm::multiply(axis(-flight.heading,1),vm::multiply(axis(flight.pitch,0),axis(-flight.bank,2)));
		if (const auto result=render.setNodeTransform(parent,transform); !result) { return result; }
		if (const auto result=render.setAnimationTime({.seconds=flight.elapsed}); !result) { return result; }
		// The downloaded model has one chain ending in the two propeller pivots; no global rig state is required.
		auto nodes=render.nodeChildren(parent); if (!nodes) { return std::unexpected(nodes.error()); }
		for (unsigned depth{}; depth<8U && nodes->size()==1U; ++depth) {
			nodes=render.nodeChildren(nodes->front()); if (!nodes) { return std::unexpected(nodes.error()); }
		}
		if (nodes->size()!=2U) { return {}; } // An unexpected asset hierarchy still permits normal aircraft movement.
		// Preserve each propeller's translation and scale while rotating around its native local axle.
		for (const auto propeller : *nodes) {
			auto local=render.nodeTransform(propeller); if (!local) { return std::unexpected(local.error()); }
			local->rotation.value=axis(static_cast<float>(flight.elapsed)*(90.0F+flight.throttle*160.0F),0);
			if (const auto result=render.setNodeTransform(propeller,*local); !result) { return result; }
		}
		return {};
	}
}
