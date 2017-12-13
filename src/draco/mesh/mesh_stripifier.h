// Copyright 2017 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef DRACO_SRC_DRACO_MESH_MESH_STRIPIFIER_H_
#define DRACO_SRC_DRACO_MESH_MESH_STRIPIFIER_H_

#include "draco/mesh/mesh_misc_functions.h"

namespace draco {

// Class that generates triangle strips from a provided draco::Mesh data
// structure. The strips represent a more memory efficient storage of triangle
// connectivity that can be used directly on the GPU (see
// https://en.wikipedia.org/wiki/Triangle_strip ). In general, a mesh needs to
// be represented by several triangle strips and it has been proven that finding
// the optimal set of triangle strips is an NP-complete problem. The algorithm
// implemented by this class finds this set of triangle strips based on a greedy
// heuristic that always selects the longest available strip that covers the
// next unprocessed face. The longest strip is found by analyzing all strips
// that can cover the given face (three strips corresponding to three
// directions).
class MeshStripifier {
 public:
  MeshStripifier()
      : mesh_(nullptr),
        num_strips_(0),
        num_encoded_faces_(0),
        last_encoded_point_(kInvalidPointIndex) {}

  // Generate triangle strips for a given mesh and output them to the output
  // iterator |out_it|. In most cases |out_it| stores the values in a buffer
  // that can be used directly on the GPU. Note that the algorithm can generate
  // multiple strips to represent the whole mesh. In such cases multiple strips
  // are separated using a so called primitive restart index that is specified
  // by the |primitive_restart_index| (usually defined as the maximum allowed
  // value for the given type).
  // https://www.khronos.org/opengl/wiki/Vertex_Rendering#Primitive_Restart
  template <typename OutputIteratorT, typename IndexTypeT>
  bool GenerateTriangleStripsWithPrimitiveRestart(
      const Mesh &mesh, IndexTypeT primitive_restart_index,
      OutputIteratorT out_it);

  // The same as above but disjoint triangle strips are separated by degenerate
  // triangles instead of the primitive restart index. Degenerate triangles are
  // zero area triangles that are automatically discarded by the GPU. Using
  // degenerate triangles usually results in a slightly longer output indices
  // array compared to the similar triangle strips that use primitive restart
  // index. The advantage of this method is that it is supported by all hardware
  // and all relevant APIs (including WebGL 1.0).
  template <typename OutputIteratorT>
  bool GenerateTriangleStripsWithDegenerateTriangles(const Mesh &mesh,
                                                     OutputIteratorT out_it);

  // Returns the number of strips generated by the last call of the
  // GenerateTriangleStrips() method.
  int num_strips() const { return num_strips_; }

 private:
  bool Prepare(const Mesh &mesh) {
    mesh_ = &mesh;
    num_strips_ = 0;
    num_encoded_faces_ = 0;
    // TODO(ostava): We may be able to avoid computing the corner table if we
    // already have it stored somewhere.
    corner_table_ = CreateCornerTableFromPositionAttribute(mesh_);
    if (corner_table_ == nullptr)
      return false;

    // Mark all faces as unvisited.
    is_face_visited_.assign(mesh.num_faces(), false);
    return true;
  }

  // Returns local id of the longest strip that can be created from the given
  // face |fi|.
  int FindLongestStripFromFace(FaceIndex fi) {
    // There are three possible strip directions that can contain the provided
    // input face. We try all of them and select the direction that result in
    // the longest strip.
    const CornerIndex first_ci = corner_table_->FirstCorner(fi);
    int longest_strip_id = -1;
    int longest_strip_length = 0;
    for (int i = 0; i < 3; ++i) {
      GenerateStripsFromCroner(i, first_ci + i);
      if (strip_faces_[i].size() > longest_strip_length) {
        longest_strip_length = strip_faces_[i].size();
        longest_strip_id = i;
      }
    }
    return longest_strip_id;
  }

  // Generates strip from the data stored in |strip_faces_| and
  // |strip_start_start_corners_| and stores it to |out_it|.
  template <typename OutputIteratorT>
  void StoreStrip(int local_strip_id, OutputIteratorT out_it) {
    ++num_strips_;

    const int num_strip_faces = strip_faces_[local_strip_id].size();
    CornerIndex ci = strip_start_corners_[local_strip_id];
    for (int i = 0; i < num_strip_faces; ++i) {
      const FaceIndex fi = corner_table_->Face(ci);
      is_face_visited_[fi] = true;
      ++num_encoded_faces_;

      if (i == 0) {
        // Add the start face (three indices).
        *out_it++ = CornerToPointIndex(ci).value();
        *out_it++ = CornerToPointIndex(corner_table_->Next(ci)).value();
        last_encoded_point_ = CornerToPointIndex(corner_table_->Previous(ci));
        *out_it++ = last_encoded_point_.value();
      } else {
        // Store the point on the newly reached corner.
        last_encoded_point_ = CornerToPointIndex(ci);
        *out_it++ = last_encoded_point_.value();

        // Go to the correct source corner to proceed to the next face.
        if (i & 1) {
          ci = corner_table_->Previous(ci);
        } else {
          ci = corner_table_->Next(ci);
        }
      }
      ci = corner_table_->Opposite(ci);
    }
  }

  PointIndex CornerToPointIndex(CornerIndex ci) const {
    return mesh_->CornerToPointId(ci);
  }

  // Returns the opposite corner in case the opposite triangle does not lie
  // across an attribute seam. Otherwise return kInvalidCornerIndex.
  CornerIndex GetOppositeCorner(CornerIndex ci) const {
    const CornerIndex oci = corner_table_->Opposite(ci);
    if (oci < 0)
      return kInvalidCornerIndex;
    // Ensure the point ids are same on both sides of the shared edge between
    // the triangles.
    if (CornerToPointIndex(corner_table_->Next(ci)) !=
        CornerToPointIndex(corner_table_->Previous(oci)))
      return kInvalidCornerIndex;
    if (CornerToPointIndex(corner_table_->Previous(ci)) !=
        CornerToPointIndex(corner_table_->Next(oci)))
      return kInvalidCornerIndex;
    return oci;
  }

  void GenerateStripsFromCroner(int local_strip_id, CornerIndex ci);

  const Mesh *mesh_;
  std::unique_ptr<CornerTable> corner_table_;

  // Store strip faces for each of three possible directions from a given face.
  std::vector<FaceIndex> strip_faces_[3];
  // Start corner for each direction of the strip containing the processed face.
  CornerIndex strip_start_corners_[3];
  IndexTypeVector<FaceIndex, bool> is_face_visited_;
  // The number of strips generated by this method.
  int num_strips_;
  // The number of encoded triangles.
  int num_encoded_faces_;
  // Last encoded point.
  PointIndex last_encoded_point_;
};

template <typename OutputIteratorT, typename IndexTypeT>
bool MeshStripifier::GenerateTriangleStripsWithPrimitiveRestart(
    const Mesh &mesh, IndexTypeT primitive_restart_index,
    OutputIteratorT out_it) {
  if (!Prepare(mesh))
    return false;

  // Go over all faces and generate strips from the first unvisited one.
  for (FaceIndex fi(0); fi < mesh.num_faces(); ++fi) {
    if (is_face_visited_[fi])
      continue;

    const int longest_strip_id = FindLongestStripFromFace(fi);

    // Separate triangle strips with the primitive restart index.
    if (num_strips_ > 0) {
      *out_it++ = primitive_restart_index;
    }

    StoreStrip(longest_strip_id, out_it);
  }

  return true;
}

template <typename OutputIteratorT>
bool MeshStripifier::GenerateTriangleStripsWithDegenerateTriangles(
    const Mesh &mesh, OutputIteratorT out_it) {
  if (!Prepare(mesh))
    return false;

  // Go over all faces and generate strips from the first unvisited one.
  for (FaceIndex fi(0); fi < mesh.num_faces(); ++fi) {
    if (is_face_visited_[fi])
      continue;

    const int longest_strip_id = FindLongestStripFromFace(fi);

    // Separate triangle strips by degenerate triangles. There will be either
    // three or four degenerate triangles inserted based on the number of
    // triangles that are already encoded in the output strip (three degenerate
    // triangles for even number of existing triangles, four degenerate
    // triangles for odd number of triangles).
    if (num_strips_ > 0) {
      // Duplicate last encoded index (first degenerate face).
      *out_it++ = last_encoded_point_.value();

      // Connect it to the start point of the new triangle strip (second
      // degenerate face).
      const CornerIndex new_start_corner =
          strip_start_corners_[longest_strip_id];
      const PointIndex new_start_point = CornerToPointIndex(new_start_corner);
      *out_it++ = new_start_point.value();
      num_encoded_faces_ += 2;
      // If we have previously encoded number of faces we need to duplicate the
      // point one more time to preserve the correct orientation of the next
      // strip.
      if (num_encoded_faces_ & 1) {
        *out_it++ = new_start_point.value();
        num_encoded_faces_ += 1;
      }
      // The last degenerate face will be added implicitly in the StoreStrip()
      // function below as the first point index is going to be encoded there
      // again.
    }

    StoreStrip(longest_strip_id, out_it);
  }

  return true;
}

}  // namespace draco

#endif  // DRACO_SRC_DRACO_MESH_MESH_STRIPIFIER_H_
