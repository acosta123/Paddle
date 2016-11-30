/* Copyright (c) 2016 Baidu, Inc. All Rights Reserve.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once

/**
 * TestUtils.h is used to automatically compare CPU and GPU code is consistent.
 * This file provides a class(AutoCompare) and a template
 * function(BaseMatrixCompare) to simplify the comparison
 * of CPU and GPU member functions.
 * Refer test_Matrix.cpp and test_BaseMatrix.cpp for how to use autotest.
*/

#include <gtest/gtest.h>
#include "paddle/math/Matrix.h"
#include "paddle/math/SparseMatrix.h"
#include "TensorCheck.h"

namespace autotest {

using paddle::BaseMatrix;
using paddle::CpuIVector;
using paddle::GpuIVector;
using paddle::CpuSparseMatrix;
using paddle::GpuSparseMatrix;

template <typename T1, typename T2>
class ReplaceType {
public:
  typedef T1 type;
};

template <>
class ReplaceType<BaseMatrix, CpuMatrix> {
public:
  typedef CpuMatrix type;
};

template <>
class ReplaceType<BaseMatrix, GpuMatrix> {
public:
  typedef GpuMatrix type;
};

template <>
class ReplaceType<Matrix, CpuMatrix> {
public:
  typedef CpuMatrix type;
};

template <>
class ReplaceType<Matrix, GpuMatrix> {
public:
  typedef GpuMatrix type;
};

// construct a argument
template <typename T>
T construct(int height, int width);

template <>
float construct(int height, int width) {
  return 0.5;
}

template <>
size_t construct(int height, int width) {
  size_t offset = std::rand() % (height < width ? height : width);
  return offset;
}

template <>
CpuMatrix construct(int height, int width) {
  CpuMatrix a(height, width);
  return a;
}

template <>
GpuMatrix construct(int height, int width) {
  GpuMatrix a(height, width);
  return a;
}

// init a argument
template <typename T>
void init(T& v) {
  return;
}

template <>
void init(CpuMatrix& v) {
  v.randomizeUniform();
}

template <>
void init(GpuMatrix& v) {
  v.randomizeUniform();
}

// init a tuple which contains a set of arguments.
template <std::size_t I = 0, typename... Args>
inline typename std::enable_if<I == sizeof...(Args), void>::type initTuple(
    std::tuple<Args...>& t) {}

template <std::size_t I = 0, typename... Args>
    inline typename std::enable_if <
    I<sizeof...(Args), void>::type initTuple(std::tuple<Args...>& t) {
  init(std::get<I>(t));
  initTuple<I + 1>(t);
}

// copy a argument, copy src to dest
template <typename T1, typename T2>
void copy(T1& dest, T2& src) {
  dest = src;
}

template <>
void copy(GpuMatrix& dest, CpuMatrix& src) {
  dest.copyFrom(src);
}

// copy a tuple, copy src to dest
template <std::size_t I = 0, typename... Args1, typename... Args2>
inline typename std::enable_if<I == sizeof...(Args1), void>::type copyTuple(
    std::tuple<Args1...>& dest, std::tuple<Args2...>& src) {}

template <std::size_t I = 0, typename... Args1, typename... Args2>
    inline typename std::enable_if <
    I<sizeof...(Args1), void>::type copyTuple(std::tuple<Args1...>& dest,
                                              std::tuple<Args2...>& src) {
  copy(std::get<I>(dest), std::get<I>(src));
  copyTuple<I + 1>(dest, src);
}

// call member function
template <typename C,
          typename FC,
          typename R,
          typename... FArgs,
          typename... Args>
R call(C& obj, R (FC::*f)(FArgs...), Args&&... args) {
  return (obj.*f)(args...);
}

template <bool AsRowVector,
          bool AsColVector,
          std::size_t... I,
          typename C,
          typename R,
          typename... Args,
          typename AssertEq>
void BaseMatrixCompare(R (C::*f)(Args...), AssertEq compare) {
  for (auto height : {1, 11, 73, 128, 200, 330}) {
    for (auto width : {1, 3, 32, 100, 512, 1000}) {
      CpuMatrix obj1(AsRowVector ? 1 : height, AsColVector ? 1 : width);
      GpuMatrix obj2(AsRowVector ? 1 : height, AsColVector ? 1 : width);
      init(obj1);
      copy(obj2, obj1);

      auto tuple1 = std::make_tuple(
          construct<typename ReplaceType<
              typename std::decay<
                  typename std::tuple_element<I,
                                              std::tuple<Args...>>::type>::type,
              CpuMatrix>::type>(height, width)...);

      auto tuple2 = std::make_tuple(
          construct<typename ReplaceType<
              typename std::decay<
                  typename std::tuple_element<I,
                                              std::tuple<Args...>>::type>::type,
              GpuMatrix>::type>(height, width)...);

      initTuple(tuple1);
      copyTuple(tuple2, tuple1);

      call(obj1, f, std::get<I>(tuple1)...);
      call(obj2, f, std::get<I>(tuple2)...);

      TensorCheck(compare, obj1, obj2);
    }
  }
}

// AutoCompare
template <typename T>
class ReturnType {
public:
  typedef T type;
};

template <>
class ReturnType<CpuMatrix> {
public:
  typedef GpuMatrix type;
};

template <>
class ReturnType<CpuIVector> {
public:
  typedef GpuIVector type;
};

template <>
class ReturnType<CpuSparseMatrix> {
public:
  typedef GpuSparseMatrix type;
};

template <typename T>
typename ReturnType<T>::type autoArgs(T& v) {
  return v;
}

template <>
GpuMatrix autoArgs(CpuMatrix& v) {
  GpuMatrix a(v.getHeight(), v.getWidth());
  a.copyFrom(v);
  return a;
}

template <>
GpuIVector autoArgs(CpuIVector& v) {
  GpuIVector a(v.getSize());
  a.copyFrom(v);
  return a;
}

template <>
GpuSparseMatrix autoArgs(CpuSparseMatrix& v) {
  GpuSparseMatrix a(v.getHeight(),
                    v.getWidth(),
                    v.getElementCnt(),
                    v.getValueType(),
                    v.getFormat());
  a.copyFrom(v, HPPL_STREAM_DEFAULT);
  hl_stream_synchronize(HPPL_STREAM_DEFAULT);
  return a;
}

class AutoCompare {
public:
  AutoCompare(size_t height, size_t width)
      : cpu(height, width), gpu(height, width) {
    init(cpu);
    copy(gpu, cpu);
  }

  template <typename C, typename R, typename... FArgs, typename... Args>
  void operator()(R (C::*f)(FArgs...), Args&&... args) {
    call(cpu, f, args...);
    call(gpu, f, autoArgs(args)...);

    TensorCheckErr(cpu, gpu);
  }

protected:
  CpuMatrix cpu;
  GpuMatrix gpu;
};

}  // namespace autotest

template <std::size_t... I, typename C, typename R, typename... Args>
void BaseMatrixCompare(R (C::*f)(Args...)) {
  static_assert(sizeof...(I) == sizeof...(Args),
                "size of parameter packs are not equal");

#ifndef PADDLE_TYPE_DOUBLE
  autotest::AssertEqual compare(1e-5);
#else
  autotest::AssertEqual compare(1e-10);
#endif

  autotest::BaseMatrixCompare<false, false, I...>(f, compare);
}

template <std::size_t... I, typename C, typename R, typename... Args>
void BaseMatrixAsColVector(R (C::*f)(Args...)) {
  static_assert(sizeof...(I) == sizeof...(Args),
                "size of parameter packs are not equal");

#ifndef PADDLE_TYPE_DOUBLE
  autotest::AssertEqual compare(1e-3);
#else
  autotest::AssertEqual compare(1e-8);
#endif

  autotest::BaseMatrixCompare<false, true, I...>(f, compare);
}

template <std::size_t... I, typename C, typename R, typename... Args>
void BaseMatrixAsRowVector(R (C::*f)(Args...)) {
  static_assert(sizeof...(I) == sizeof...(Args),
                "size of parameter packs are not equal");

#ifndef PADDLE_TYPE_DOUBLE
  autotest::AssertEqual compare(1e-3);
#else
  autotest::AssertEqual compare(1e-8);
#endif
  autotest::BaseMatrixCompare<true, false, I...>(f, compare);
}
