// Copyright (c) 2019 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_COMMON_GIN_HELPER_DICTIONARY_H_
#define SHELL_COMMON_GIN_HELPER_DICTIONARY_H_

#include <string>
#include <type_traits>

#include "gin/dictionary.h"
#include "shell/common/gin_helper/function_template.h"

namespace gin_helper {

// Adds a few more extends methods to gin::Dictionary.
//
// Note that as the destructor of gin::Dictionary is not virtual, and we want to
// convert between 2 types, we must not add any member.
class Dictionary : public gin::Dictionary {
 public:
  Dictionary() : gin::Dictionary(nullptr) {}
  Dictionary(v8::Isolate* isolate, v8::Local<v8::Object> object)
      : gin::Dictionary(isolate, object) {}

  // Allow implicitly converting from gin::Dictionary, as it is absolutely
  // safe in this case.
  Dictionary(const gin::Dictionary& dict)  // NOLINT(runtime/explicit)
      : gin::Dictionary(dict) {}

  // The Get method in gin::Dictionary is not a const method, work around it
  // by adding our own version.
  //
  // TODO(zcbenz): Add the const method in upstream.
  template <typename T>
  bool Get(const std::string& key, T* out) const {
    return const_cast<gin::Dictionary*>(
               static_cast<const gin::Dictionary*>(this))
        ->Get(key, out);
  }

  template <typename T>
  bool GetHidden(base::StringPiece key, T* out) const {
    v8::Local<v8::Context> context = isolate()->GetCurrentContext();
    v8::Local<v8::Private> privateKey =
        v8::Private::ForApi(isolate(), gin::StringToV8(isolate(), key));
    v8::Local<v8::Value> value;
    v8::Maybe<bool> result = GetHandle()->HasPrivate(context, privateKey);
    if (result.IsJust() && result.FromJust() &&
        GetHandle()->GetPrivate(context, privateKey).ToLocal(&value))
      return gin::ConvertFromV8(isolate(), value, out);
    return false;
  }

  template <typename T>
  bool SetHidden(base::StringPiece key, T val) {
    v8::Local<v8::Value> v8_value;
    if (!gin::TryConvertToV8(isolate(), val, &v8_value))
      return false;
    v8::Local<v8::Context> context = isolate()->GetCurrentContext();
    v8::Local<v8::Private> privateKey =
        v8::Private::ForApi(isolate(), gin::StringToV8(isolate(), key));
    v8::Maybe<bool> result =
        GetHandle()->SetPrivate(context, privateKey, v8_value);
    return !result.IsNothing() && result.FromJust();
  }

  template <typename T>
  bool SetMethod(base::StringPiece key, const T& callback) {
    auto context = isolate()->GetCurrentContext();
    auto templ = CallbackTraits<T>::CreateTemplate(isolate(), callback);
    return GetHandle()
        ->Set(context, gin::StringToV8(isolate(), key),
              templ->GetFunction(context).ToLocalChecked())
        .ToChecked();
  }

  template <typename T>
  bool SetReadOnly(base::StringPiece key, const T& val) {
    v8::Local<v8::Value> v8_value;
    if (!gin::TryConvertToV8(isolate(), val, &v8_value))
      return false;
    v8::Maybe<bool> result = GetHandle()->DefineOwnProperty(
        isolate()->GetCurrentContext(), gin::StringToV8(isolate(), key),
        v8_value, v8::ReadOnly);
    return !result.IsNothing() && result.FromJust();
  }

  bool Delete(base::StringPiece key) {
    v8::Maybe<bool> result = GetHandle()->Delete(
        isolate()->GetCurrentContext(), gin::StringToV8(isolate(), key));
    return !result.IsNothing() && result.FromJust();
  }

  bool IsEmpty() const { return isolate() == nullptr || GetHandle().IsEmpty(); }

  v8::Local<v8::Object> GetHandle() const {
    return gin::ConvertToV8(isolate(),
                            *static_cast<const gin::Dictionary*>(this))
        .As<v8::Object>();
  }

 private:
  // DO NOT ADD ANY DATA MEMBER.
};

}  // namespace gin_helper

namespace gin {

template <>
struct Converter<gin_helper::Dictionary> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   gin_helper::Dictionary val) {
    return val.GetHandle();
  }
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     gin_helper::Dictionary* out) {
    gin::Dictionary gdict(isolate);
    if (!ConvertFromV8(isolate, val, &gdict))
      return false;
    *out = gin_helper::Dictionary(gdict);
    return true;
  }
};

}  // namespace gin

#endif  // SHELL_COMMON_GIN_HELPER_DICTIONARY_H_
