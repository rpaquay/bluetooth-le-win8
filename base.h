#pragma once

#include <algorithm>
#include <string>
#include <codecvt>

template <class T>
class scoped_ptr {
public:
  scoped_ptr() : ptr_(NULL) {
  }

  explicit scoped_ptr(typename T* ptr) : ptr_(ptr) {
  }

  ~scoped_ptr() {
    Delete();
  }

  T* get() const {
    return ptr_;
  }

  void set(T* ptr) {
    Delete();
    ptr_ = ptr;
  }

  void Delete() {
    delete ptr_;
    ptr_ = NULL;
  }

  T* Pass() {
    T* temp = ptr_;
    ptr_ = NULL;
    return temp;
  }

private:
  T* ptr_;

  scoped_ptr(const scoped_ptr<T>& other);
  const scoped_ptr<T>& operator=(const scoped_ptr<T>& other);
};

template <class T>
class scoped_array {
public:
  explicit scoped_array(T* t) {
    ptr_ = t;
  }
  ~scoped_array() {
    delete[] ptr_;
  }

  T* get() const {
    return ptr_;
  }

  T* Pass() {
    T* temp = ptr_;
    ptr_ = NULL;
    return temp;
  }

private:
  T* ptr_;

  scoped_array(const scoped_array<T>& other);
  const scoped_array<T>& operator=(const scoped_array<T>& other);
};

template <class T>
class scoped_handle {
public:
  scoped_handle() {
    handle_ = INVALID_HANDLE_VALUE;
  }

  explicit scoped_handle(T handle) {
    handle_ = handle;
  }

  ~scoped_handle() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
  }

  T get() const {
    return handle_;
  }

  void set(HANDLE handle) {
    Close();
    handle_ = handle;
  }

private:
  T handle_;

  void Close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

  scoped_handle(const scoped_handle<T>& other);
  const scoped_handle<T>& operator=(const scoped_handle<T>& other);
};


template<class T>
class RefCounted {
public:
  RefCounted() : ref_count_(0) {
  }

  void AddRef() {
    ref_count_++;
  }

  void Release() {
    ref_count_--;
    if (ref_count_ == 0)
      delete this;
  }

protected:
  virtual ~RefCounted() {
  }

private:
  int ref_count_;
};


template<class T>
class scoped_refptr {
public:
  scoped_refptr() : ptr_(NULL) {
  }

  explicit scoped_refptr(T *ptr) : ptr_(ptr) {
    AddRef();
  }

  scoped_refptr(const scoped_refptr<T>& other) : ptr_(NULL) {
    CopyFrom(other);
  }

  ~scoped_refptr() {
    Release();
  }

  const scoped_refptr<T>&operator=(const scoped_refptr<T>& other) {
    CopyFrom(other);
    return *this;
  }

  T* operator->() const {
    return ptr_;
  }

  operator bool() const {
    return ptr_;
  }

private:
  void AddRef() {
    if (ptr_)
      ptr_->AddRef();
  }

  void Release() {
    if (ptr_)
      ptr_->Release();
  }

  void CopyFrom(const scoped_refptr<T>& other) {
    Release();
    ptr_ = other.ptr_;
    AddRef();
  }

  T* ptr_;
};

inline
std::string to_std_string(const std::wstring& value) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(value);
}

inline
std::string to_lower_string(const std::string& value) {
  std::string data = value;
  std::transform(data.begin(), data.end(), data.begin(), ::tolower);
  return data;
}
