/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// This is the utils file for the HLO dialect.

#ifndef HLO_UTILS
#define HLO_UTILS

include "mlir/IR/OpBase.td"

def NullArrayAttr : NativeCodeCall<"ArrayAttr()">;

def CastIntElementsAttr : NativeCodeCall<"$0.cast<DenseIntElementsAttr>()">;

class ConstantSplat<string value> : NativeCodeCall<
    "hlo::getSplat(&$_builder, $0, " # value # ")">;

class HLO_ConstantLike<string value> : NativeCodeCall<
    "chlo::getConstantLike($_builder, $_loc, " # value # ", $0)">;

def NullDenseIntElementsAttr : NativeCodeCall<"DenseIntElementsAttr()">;

def BinBroadcastDimensions : NativeCodeCall<
    "hlo::getBroadcastDimensionsAttr(&$_builder, $0, $1)">;

def BinBroadcastDimensionsNonEmpty : NativeCodeCall<
    "hlo::getBroadcastDimensionsAttr(&$_builder, $0, $1, /*allow_empty=*/false)">;

// Here, the element type can be any integer or float type. But, note that only
// 32 bit integers are supported for the value.
class GetScalarOfType<int value> : NativeCodeCall<
  "hlo::GetScalarOfType(getElementTypeOrSelf($0)," # value # ")">;

#endif // HLO_UTILS
