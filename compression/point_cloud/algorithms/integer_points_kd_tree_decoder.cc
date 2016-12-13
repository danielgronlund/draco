// Copyright 2016 The Draco Authors.
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
#include "compression/point_cloud/algorithms/integer_points_kd_tree_decoder.h"

#include "compression/point_cloud/algorithms/point_cloud_types.h"

// TODO(hemmer): make independent from dimension 3

namespace draco {

template class IntegerPointsKdTreeDecoder<Point3ui, 0>;
template class IntegerPointsKdTreeDecoder<Point3ui, 1>;
template class IntegerPointsKdTreeDecoder<Point3ui, 2>;
template class IntegerPointsKdTreeDecoder<Point3ui, 3>;
template class IntegerPointsKdTreeDecoder<Point3ui, 4>;
template class IntegerPointsKdTreeDecoder<Point3ui, 5>;
template class IntegerPointsKdTreeDecoder<Point3ui, 6>;
template class IntegerPointsKdTreeDecoder<Point3ui, 7>;
template class IntegerPointsKdTreeDecoder<Point3ui, 8>;
template class IntegerPointsKdTreeDecoder<Point3ui, 9>;
template class IntegerPointsKdTreeDecoder<Point3ui, 10>;

}  // namespace draco
