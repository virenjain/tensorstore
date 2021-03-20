// Copyright 2020 The TensorStore Authors
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

#include "python/tensorstore/numpy.h"

// numpy.h must be included first.

#include <array>
#include <new>
#include <string>
#include <string_view>
#include <utility>

#include "absl/hash/hash.h"
#include <nlohmann/json.hpp>
#include "python/tensorstore/data_type.h"
#include "python/tensorstore/json_type_caster.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "tensorstore/data_type.h"
#include "tensorstore/util/quote_string.h"
#include "tensorstore/util/str_cat.h"

namespace tensorstore {
namespace internal_python {

namespace py = ::pybind11;

pybind11::dtype GetNumpyDtype(int type_num) {
  if (auto* obj = PyArray_DescrFromType(type_num)) {
    return py::reinterpret_borrow<py::dtype>(reinterpret_cast<PyObject*>(obj));
  }
  throw py::error_already_set();
}

DataType GetDataTypeOrThrow(std::string_view name) {
  auto d = GetDataType(name);
  if (!d.valid()) {
    throw py::value_error(
        StrCat("No TensorStore data type with name: ", QuoteString(name)));
  }
  return d;
}

int GetNumpyTypeNum(DataType dtype) {
  const DataTypeId id = dtype.id();
  switch (id) {
    case DataTypeId::custom:
      return -1;
    case DataTypeId::bfloat16_t:
      return Bfloat16NumpyTypeNum();
    default:
      return kNumpyTypeNumForDataTypeId[static_cast<size_t>(id)];
  }
}

py::dtype GetNumpyDtypeOrThrow(DataType dtype) {
  int type_num = GetNumpyTypeNum(dtype);
  if (type_num != -1) {
    return GetNumpyDtype(type_num);
  }
  throw py::value_error(
      StrCat("No NumPy dtype corresponding to TensorStore data type: ",
             QuoteString(dtype.name())));
}

DataType GetDataType(pybind11::dtype dt) {
  const int type_num = py::detail::array_descriptor_proxy(dt.ptr())->type_num;
  if (type_num == Bfloat16NumpyTypeNum()) {
    return dtype_v<bfloat16_t>;
  }
  if (type_num < 0 || type_num > NPY_NTYPES) {
    return DataType();
  }
  const DataTypeId id = kDataTypeIdForNumpyTypeNum[type_num];
  if (id == DataTypeId::custom) return DataType();
  return kDataTypes[static_cast<size_t>(id)];
}

DataType GetDataTypeOrThrow(py::dtype dt) {
  auto dtype = GetDataType(dt);
  if (dtype.valid()) return dtype;
  throw py::value_error(
      StrCat("No TensorStore data type corresponding to NumPy dtype: ",
             py::cast<std::string>(py::repr(dt))));
}

py::object GetTypeObjectOrThrow(DataType dtype) {
  switch (dtype.id()) {
    case DataTypeId::ustring_t:
      return py::reinterpret_borrow<py::object>(
          reinterpret_cast<PyObject*>(&PyUnicode_Type));
    case DataTypeId::string_t:
      return py::reinterpret_borrow<py::object>(
          reinterpret_cast<PyObject*>(&PyBytes_Type));
    default:
      break;
  }
  auto numpy_dtype = GetNumpyDtypeOrThrow(dtype);
  return py::reinterpret_borrow<py::object>(
      py::detail::array_descriptor_proxy(numpy_dtype.ptr())->typeobj);
}

void RegisterDataTypeBindings(pybind11::module m) {
  if (!internal_python::RegisterNumpyBfloat16()) {
    throw py::error_already_set();
  }
  py::class_<DataType> cls_data_type(m, "dtype", R"(
Represents a TensorStore data type.
)");
  cls_data_type
      .def(py::init([](std::string name) { return GetDataTypeOrThrow(name); }),
           "Construct by name.", py::arg("name"))
      .def(py::init([](DataTypeLike dtype) { return dtype.value; }),
           "Construct from an existing TensorStore or NumPy data type.",
           py::arg("dtype"))
      .def_property_readonly(
          "name", [](DataType self) { return std::string(self.name()); })
      .def("__repr__",
           [](DataType self) {
             return StrCat("dtype(", QuoteString(self.name()), ")");
           })
      .def(
          py::pickle([](DataType self) { return std::string(self.name()); },
                     [](std::string name) { return GetDataTypeOrThrow(name); }))
      .def("to_json", [](DataType self) { return std::string(self.name()); })
      .def_property_readonly(
          "numpy_dtype",
          [](DataType self) { return GetNumpyDtypeOrThrow(self); })
      .def("__hash__",
           [](DataType self) {
             absl::Hash<DataType> h;
             return h(self);
           })
      .def_property_readonly("type",
                             [](DataType self) -> py::object {
                               return GetTypeObjectOrThrow(self);
                             })
      .def(
          "__call__",
          [](DataType self, py::object arg) -> py::object {
            if (self.id() == DataTypeId::json_t) {
              return py::cast(PyObjectToJson(arg));
            }
            return GetTypeObjectOrThrow(self)(std::move(arg));
          },
          "Construct a scalar instance of this data type")
      .def(
          "__eq__", [](DataType self, DataType other) { return self == other; },
          py::arg("other"));

  // Like NumPy and Tensorflow, define `tensorstore.<dtype>` constants for each
  // supported data type.
  for (const DataType dtype : kDataTypes) {
    m.attr(std::string(dtype.name()).c_str()) = dtype;
  }
}

}  // namespace internal_python
}  // namespace tensorstore

namespace pybind11 {
namespace detail {

bool type_caster<tensorstore::internal_python::DataTypeLike>::load(
    handle src, bool convert) {
  using tensorstore::DataType;
  using tensorstore::dtype_v;
  // Handle the case that `src` is already a Python-wrapped
  // `tensorstore::DataType`.
  if (pybind11::isinstance<tensorstore::DataType>(src)) {
    value.value = pybind11::cast<tensorstore::DataType>(src);
    return true;
  }
  if (src.is_none()) return false;
  if (!convert) return false;
  if (src.ptr() == reinterpret_cast<PyObject*>(&PyUnicode_Type)) {
    value.value = dtype_v<tensorstore::ustring_t>;
    return true;
  }
  if (src.ptr() == reinterpret_cast<PyObject*>(&PyBytes_Type)) {
    value.value = dtype_v<tensorstore::string_t>;
    return true;
  }
  PyArray_Descr* ptr = nullptr;
  if (!PyArray_DescrConverter(
          pybind11::reinterpret_borrow<pybind11::object>(src).release().ptr(),
          &ptr) ||
      !ptr) {
    PyErr_Clear();
    return false;
  }
  value.value = tensorstore::internal_python::GetDataTypeOrThrow(
      pybind11::reinterpret_steal<pybind11::dtype>(
          reinterpret_cast<PyObject*>(ptr)));
  return true;
}

handle type_caster<tensorstore::internal_python::DataTypeLike>::cast(
    tensorstore::internal_python::DataTypeLike value,
    return_value_policy policy, handle parent) {
  return pybind11::cast(std::move(value.value));
}

}  // namespace detail
}  // namespace pybind11
