export module VEEngine.Simple.Scene;
import std;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;

/**
	* @file
	* @brief CPU scene instance data for the simple forward renderer.
	*
	* Functional objects:
	* - Object pairs one CPU mesh with its model transform before any renderer upload exists.
	* - Scene stores the CPU drawable list for simple multi-object renderer tests.
	* - makeSampleScene creates a few translated cubes without touching Vulkan state.
	*/
export namespace vve::simple {

	/// @brief Host-side drawable object with geometry and a local-to-world transform.
	struct Object {
		Mesh mesh{};  ///< CPU geometry used by one drawable object.
		Mat4 model{}; ///< Model matrix placing the mesh in world space.
	};

	/// @brief Host-side scene container with drawable objects in submission order.
	struct Scene {
		std::vector<Object> objects{}; ///< Drawable objects owned by this CPU scene.
	};

	/**
		* @brief Creates a small CPU-only cube scene for early renderer integration.
		*
		* @return Scene containing several cube objects with distinct world transforms.
		*/
	Scene makeSampleScene() {
		return Scene{.objects{
			Object{.mesh = makeCube(), .model = identityMat4()},                                      ///< Center cube.
			Object{.mesh = makeCube(), .model = translate(identityMat4(), Vec3{-1.5F, 0.0F, 0.0F})}, ///< Left cube.
			Object{.mesh = makeCube(), .model = translate(identityMat4(), Vec3{1.5F, 0.0F, 0.0F})},  ///< Right cube.
		}};
	}

} // namespace vve::simple
