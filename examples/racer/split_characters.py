#!/usr/bin/env python3
"""Turns Kenney's Mini Characters (CC0) into the Ring Racer crowd models in assets/racer/crowd.

Usage: python split_characters.py <Mini Characters folder> [<output folder>]   (needs NumPy)

The download (https://kenney.nl/assets/mini-characters) holds skinned GLB characters whose seven joints move rigid body
parts. The engine draws no skins, so each character is written again as a plain node tree with the same joint names
(root, leg-left, leg-right, torso, arm-left, arm-right, head): every node carries the triangles of its body part in its
own space, and the game poses the figure by turning the nodes. Wheelchairs are flattened into one mesh. All models share
colormap.png, which is copied next to them.
"""
import json
import shutil
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
CHARACTERS = [f"character-{kind}-{letter}" for kind in ("female", "male") for letter in "abcdef"]
WHEELCHAIRS = ["wheelchair", "wheelchair-deluxe", "wheelchair-power", "wheelchair-power-deluxe"]
COMPONENTS = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
WIDTHS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def read_glb(path: Path) -> tuple[dict, bytes]:
    """Splits a binary glTF file into its JSON and its binary chunk."""
    data = path.read_bytes()
    magic, _, _ = struct.unpack_from("<III", data, 0)
    assert magic == 0x46546C67, f"{path} is not a GLB file"
    json_length, _ = struct.unpack_from("<II", data, 12)
    gltf = json.loads(data[20:20 + json_length])
    offset = 20 + json_length
    binary = b""
    if offset < len(data):
        bin_length, _ = struct.unpack_from("<II", data, offset)
        binary = data[offset + 8:offset + 8 + bin_length]
    return gltf, binary


def accessor(gltf: dict, binary: bytes, index: int) -> np.ndarray:
    """Reads an accessor as an array of shape (count, width), honouring the byte stride and normalisation."""
    item = gltf["accessors"][index]
    view = gltf["bufferViews"][item["bufferView"]]
    dtype = np.dtype(COMPONENTS[item["componentType"]])
    width = WIDTHS[item["type"]]
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    stride = view.get("byteStride", dtype.itemsize * width)
    rows = np.ndarray((item["count"], width), dtype=dtype, buffer=binary, offset=start, strides=(stride, dtype.itemsize))
    values = rows.astype(np.float64 if dtype == np.float32 else np.int64)
    if item.get("normalized"):
        values = values / np.iinfo(dtype).max
    return values


def quaternion_matrix(q) -> np.ndarray:
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def local_matrix(node: dict) -> np.ndarray:
    """The node's own transform as a 4x4 matrix."""
    if "matrix" in node:
        return np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T
    result = np.eye(4)
    result[:3, :3] = quaternion_matrix(node.get("rotation", [0, 0, 0, 1])) * np.array(node.get("scale", [1, 1, 1]))
    result[:3, 3] = node.get("translation", [0, 0, 0])
    return result


def world_matrices(gltf: dict) -> dict[int, np.ndarray]:
    """Rest-pose transform of every node relative to the scene."""
    result: dict[int, np.ndarray] = {}

    def visit(index: int, parent: np.ndarray) -> None:
        result[index] = parent @ local_matrix(gltf["nodes"][index])
        for child in gltf["nodes"][index].get("children", []):
            visit(child, result[index])

    for top in gltf["scenes"][gltf.get("scene", 0)]["nodes"]:
        visit(top, np.eye(4))
    return result


def primitives(gltf: dict, binary: bytes, mesh: int):
    """Positions, normals, texture coordinates, triangle indices and (if skinned) joints and weights of a mesh."""
    for primitive in gltf["meshes"][mesh]["primitives"]:
        attributes = primitive["attributes"]
        material = gltf["materials"][primitive["material"]] if "material" in primitive else {}
        transform = material.get("pbrMetallicRoughness", {}).get("baseColorTexture", {}).get("extensions", {}).get("KHR_texture_transform", {})
        assert not {"offset", "scale", "rotation"} & transform.keys(), "texture transforms are not supported"
        positions = accessor(gltf, binary, attributes["POSITION"])
        indices = accessor(gltf, binary, primitive["indices"])[:, 0] if "indices" in primitive else np.arange(len(positions))
        yield {"positions": positions, "normals": accessor(gltf, binary, attributes["NORMAL"]),
               "uvs": accessor(gltf, binary, attributes["TEXCOORD_0"]), "indices": indices.reshape(-1, 3),
               "joints": accessor(gltf, binary, attributes["JOINTS_0"]) if "JOINTS_0" in attributes else None,
               "weights": accessor(gltf, binary, attributes["WEIGHTS_0"]) if "WEIGHTS_0" in attributes else None}


class Part:
    """Triangles collected for one output mesh."""

    def __init__(self) -> None:
        self.positions, self.normals, self.uvs, self.triangles = [], [], [], []
        self.count = 0

    def add(self, positions: np.ndarray, normals: np.ndarray, uvs: np.ndarray, triangles: np.ndarray) -> None:
        """Adds the triangles with only the vertices they use."""
        used, remapped = np.unique(triangles, return_inverse=True)
        self.positions.append(positions[used])
        self.normals.append(normals[used])
        self.uvs.append(uvs[used])
        self.triangles.append(remapped.reshape(-1, 3) + self.count)
        self.count += len(used)

    def arrays(self):
        return (np.concatenate(self.positions), np.concatenate(self.normals), np.concatenate(self.uvs),
                np.concatenate(self.triangles))


class Writer:
    """Collects meshes and nodes and writes a GLB file whose material uses colormap.png."""

    def __init__(self) -> None:
        self.gltf = {"asset": {"version": "2.0", "generator": "examples/racer/split_characters.py"},
                     "scenes": [{"nodes": [0]}], "scene": 0, "nodes": [], "meshes": [], "accessors": [], "bufferViews": [],
                     "materials": [{"name": "colormap", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0}}],
                     "textures": [{"source": 0}], "images": [{"uri": "colormap.png"}], "buffers": [{"byteLength": 0}]}
        self.binary = bytearray()

    def view(self, values: np.ndarray, target: int) -> int:
        while len(self.binary) % 4:
            self.binary.append(0)
        self.gltf["bufferViews"].append({"buffer": 0, "byteOffset": len(self.binary), "byteLength": values.nbytes, "target": target})
        self.binary += values.tobytes()
        return len(self.gltf["bufferViews"]) - 1

    def attribute(self, values: np.ndarray, kind: str) -> int:
        values = np.ascontiguousarray(values, dtype=np.float32)
        item = {"bufferView": self.view(values, 34962), "componentType": 5126, "count": len(values), "type": kind}
        if kind == "VEC3":
            item["min"], item["max"] = values.min(axis=0).tolist(), values.max(axis=0).tolist()
        self.gltf["accessors"].append(item)
        return len(self.gltf["accessors"]) - 1

    def mesh(self, name: str, part: Part) -> int:
        positions, normals, uvs, triangles = part.arrays()
        normals = normals / np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-9)
        indices = np.ascontiguousarray(triangles.reshape(-1), dtype=np.uint16 if part.count < 65536 else np.uint32)
        self.gltf["accessors"].append({"bufferView": self.view(indices, 34963), "componentType": 5123 if indices.dtype == np.uint16 else 5125,
                                       "count": len(indices), "type": "SCALAR"})
        index_accessor = len(self.gltf["accessors"]) - 1
        attributes = {"POSITION": self.attribute(positions, "VEC3"), "NORMAL": self.attribute(normals, "VEC3"),
                      "TEXCOORD_0": self.attribute(uvs, "VEC2")}
        self.gltf["meshes"].append({"name": name, "primitives": [{"attributes": attributes, "indices": index_accessor, "material": 0}]})
        return len(self.gltf["meshes"]) - 1

    def node(self, name: str, translation=None, rotation=None, mesh=None, children=None) -> int:
        item = {"name": name}
        if translation is not None and np.any(np.abs(translation) > 1e-9):
            item["translation"] = [float(v) for v in translation]
        if rotation is not None and np.any(np.abs(np.asarray(rotation) - [0, 0, 0, 1]) > 1e-9):
            item["rotation"] = [float(v) for v in rotation]
        if mesh is not None:
            item["mesh"] = mesh
        if children:
            item["children"] = children
        self.gltf["nodes"].append(item)
        return len(self.gltf["nodes"]) - 1

    def save(self, path: Path) -> None:
        while len(self.binary) % 4:
            self.binary.append(0)
        self.gltf["buffers"][0]["byteLength"] = len(self.binary)
        text = json.dumps(self.gltf, separators=(",", ":")).encode()
        text += b" " * (-len(text) % 4)
        total = 12 + 8 + len(text) + 8 + len(self.binary)
        path.write_bytes(struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(text), 0x4E4F534A) + text
                         + struct.pack("<II", len(self.binary), 0x004E4942) + bytes(self.binary))


def split_character(source: Path, target: Path) -> str:
    """Writes the character as a node tree of rigid parts; returns a short report."""
    gltf, binary = read_glb(source)
    nodes = gltf["nodes"]
    skinned = [index for index, node in enumerate(nodes) if "skin" in node and "mesh" in node]
    joints = gltf["skins"][nodes[skinned[0]]["skin"]]["joints"]
    parts = {joint: Part() for joint in joints}
    blended = 0
    for node_index in skinned:
        skin = gltf["skins"][nodes[node_index]["skin"]]
        inverse_binds = accessor(gltf, binary, skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
        for data in primitives(gltf, binary, nodes[node_index]["mesh"]):
            dominant = data["joints"][np.arange(len(data["joints"])), np.argmax(data["weights"], axis=1)]
            blended += int(np.sum(np.max(data["weights"], axis=1) < 0.999))
            # A triangle belongs to the joint of its first corner; rigid parts never mix joints within a triangle.
            owner = dominant[data["indices"][:, 0]]
            for slot in np.unique(owner):
                joint = skin["joints"][slot]
                # Vertices go from the bind pose into the joint's own space: the inverse bind matrix does exactly that.
                inverse = inverse_binds[slot]
                positions = data["positions"] @ inverse[:3, :3].T + inverse[:3, 3]
                normals = data["normals"] @ inverse[:3, :3].T
                parts[joint].add(positions, normals, data["uvs"], data["indices"][owner == slot])
    writer = Writer()

    def copy(index: int) -> int:
        node = nodes[index]
        children = [copy(child) for child in node.get("children", []) if child in joints]
        mesh = writer.mesh(node["name"], parts[index]) if parts.get(index) and parts[index].count else None
        return writer.node(node["name"], node.get("translation"), node.get("rotation"), mesh, children)

    top = gltf["scenes"][gltf.get("scene", 0)]["nodes"][0]
    roots = [child for child in nodes[top].get("children", []) if child in joints]
    writer.gltf["nodes"].append(None)  # The figure's own node comes first; it is filled in below.
    children = [copy(root) for root in roots]
    writer.gltf["nodes"][0] = {"name": source.stem, "children": children}
    writer.save(target)
    return f"{sum(part.count for part in parts.values())} vertices, {blended} with blended weights"


def flatten(source: Path, target: Path) -> str:
    """Writes a static model as one mesh in model space."""
    gltf, binary = read_glb(source)
    worlds = world_matrices(gltf)
    part = Part()
    for index, node in enumerate(gltf["nodes"]):
        if "mesh" not in node or index not in worlds:
            continue
        matrix = worlds[index]
        normal_matrix = np.linalg.inv(matrix[:3, :3]).T
        for data in primitives(gltf, binary, node["mesh"]):
            part.add(data["positions"] @ matrix[:3, :3].T + matrix[:3, 3], data["normals"] @ normal_matrix.T, data["uvs"], data["indices"])
    writer = Writer()
    writer.gltf["nodes"].append(None)
    writer.gltf["nodes"][0] = {"name": source.stem, "mesh": writer.mesh(source.stem, part)}
    writer.save(target)
    return f"{part.count} vertices"


def main() -> None:
    pack = Path(sys.argv[1])
    models = pack / "Models" / "GLB format" if (pack / "Models").is_dir() else pack
    target = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "assets/racer/crowd"
    target.mkdir(parents=True, exist_ok=True)
    for name in CHARACTERS:
        print(f"{name}.glb: {split_character(models / f'{name}.glb', target / f'{name}.glb')}")
    for name in WHEELCHAIRS:
        print(f"{name}.glb: {flatten(models / f'{name}.glb', target / f'{name}.glb')}")
    shutil.copyfile(models / "Textures" / "colormap.png", target / "colormap.png")
    print("colormap.png")


if __name__ == "__main__":
    main()
