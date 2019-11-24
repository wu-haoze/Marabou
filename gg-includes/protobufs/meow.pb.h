// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: meow.proto

#ifndef PROTOBUF_meow_2eproto__INCLUDED
#define PROTOBUF_meow_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 3000000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 3000000 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)

namespace gg {
namespace protobuf {
namespace meow {

// Internal implementation detail -- do not call these.
void protobuf_AddDesc_meow_2eproto();
void protobuf_AssignDesc_meow_2eproto();
void protobuf_ShutdownFile_meow_2eproto();

class InvocationRequest;

// ===================================================================

class InvocationRequest : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:gg.protobuf.meow.InvocationRequest) */ {
 public:
  InvocationRequest();
  virtual ~InvocationRequest();

  InvocationRequest(const InvocationRequest& from);

  inline InvocationRequest& operator=(const InvocationRequest& from) {
    CopyFrom(from);
    return *this;
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const InvocationRequest& default_instance();

  void Swap(InvocationRequest* other);

  // implements Message ----------------------------------------------

  inline InvocationRequest* New() const { return New(NULL); }

  InvocationRequest* New(::google::protobuf::Arena* arena) const;
  void CopyFrom(const ::google::protobuf::Message& from);
  void MergeFrom(const ::google::protobuf::Message& from);
  void CopyFrom(const InvocationRequest& from);
  void MergeFrom(const InvocationRequest& from);
  void Clear();
  bool IsInitialized() const;

  int ByteSize() const;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input);
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const;
  ::google::protobuf::uint8* InternalSerializeWithCachedSizesToArray(
      bool deterministic, ::google::protobuf::uint8* output) const;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output) const {
    return InternalSerializeWithCachedSizesToArray(false, output);
  }
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  void InternalSwap(InvocationRequest* other);
  private:
  inline ::google::protobuf::Arena* GetArenaNoVirtual() const {
    return _internal_metadata_.arena();
  }
  inline void* MaybeArenaPtr() const {
    return _internal_metadata_.raw_arena_ptr();
  }
  public:

  ::google::protobuf::Metadata GetMetadata() const;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // optional string coordinator = 1;
  void clear_coordinator();
  static const int kCoordinatorFieldNumber = 1;
  const ::std::string& coordinator() const;
  void set_coordinator(const ::std::string& value);
  void set_coordinator(const char* value);
  void set_coordinator(const char* value, size_t size);
  ::std::string* mutable_coordinator();
  ::std::string* release_coordinator();
  void set_allocated_coordinator(::std::string* coordinator);

  // optional string storage_backend = 2;
  void clear_storage_backend();
  static const int kStorageBackendFieldNumber = 2;
  const ::std::string& storage_backend() const;
  void set_storage_backend(const ::std::string& value);
  void set_storage_backend(const char* value);
  void set_storage_backend(const char* value, size_t size);
  ::std::string* mutable_storage_backend();
  ::std::string* release_storage_backend();
  void set_allocated_storage_backend(::std::string* storage_backend);

  // optional bool timelog = 3;
  void clear_timelog();
  static const int kTimelogFieldNumber = 3;
  bool timelog() const;
  void set_timelog(bool value);

  // @@protoc_insertion_point(class_scope:gg.protobuf.meow.InvocationRequest)
 private:

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  bool _is_default_instance_;
  ::google::protobuf::internal::ArenaStringPtr coordinator_;
  ::google::protobuf::internal::ArenaStringPtr storage_backend_;
  bool timelog_;
  mutable int _cached_size_;
  friend void  protobuf_AddDesc_meow_2eproto();
  friend void protobuf_AssignDesc_meow_2eproto();
  friend void protobuf_ShutdownFile_meow_2eproto();

  void InitAsDefaultInstance();
  static InvocationRequest* default_instance_;
};
// ===================================================================


// ===================================================================

#if !PROTOBUF_INLINE_NOT_IN_HEADERS
// InvocationRequest

// optional string coordinator = 1;
inline void InvocationRequest::clear_coordinator() {
  coordinator_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline const ::std::string& InvocationRequest::coordinator() const {
  // @@protoc_insertion_point(field_get:gg.protobuf.meow.InvocationRequest.coordinator)
  return coordinator_.GetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void InvocationRequest::set_coordinator(const ::std::string& value) {
  
  coordinator_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:gg.protobuf.meow.InvocationRequest.coordinator)
}
inline void InvocationRequest::set_coordinator(const char* value) {
  
  coordinator_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:gg.protobuf.meow.InvocationRequest.coordinator)
}
inline void InvocationRequest::set_coordinator(const char* value, size_t size) {
  
  coordinator_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:gg.protobuf.meow.InvocationRequest.coordinator)
}
inline ::std::string* InvocationRequest::mutable_coordinator() {
  
  // @@protoc_insertion_point(field_mutable:gg.protobuf.meow.InvocationRequest.coordinator)
  return coordinator_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* InvocationRequest::release_coordinator() {
  // @@protoc_insertion_point(field_release:gg.protobuf.meow.InvocationRequest.coordinator)
  
  return coordinator_.ReleaseNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void InvocationRequest::set_allocated_coordinator(::std::string* coordinator) {
  if (coordinator != NULL) {
    
  } else {
    
  }
  coordinator_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), coordinator);
  // @@protoc_insertion_point(field_set_allocated:gg.protobuf.meow.InvocationRequest.coordinator)
}

// optional string storage_backend = 2;
inline void InvocationRequest::clear_storage_backend() {
  storage_backend_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline const ::std::string& InvocationRequest::storage_backend() const {
  // @@protoc_insertion_point(field_get:gg.protobuf.meow.InvocationRequest.storage_backend)
  return storage_backend_.GetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void InvocationRequest::set_storage_backend(const ::std::string& value) {
  
  storage_backend_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:gg.protobuf.meow.InvocationRequest.storage_backend)
}
inline void InvocationRequest::set_storage_backend(const char* value) {
  
  storage_backend_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:gg.protobuf.meow.InvocationRequest.storage_backend)
}
inline void InvocationRequest::set_storage_backend(const char* value, size_t size) {
  
  storage_backend_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:gg.protobuf.meow.InvocationRequest.storage_backend)
}
inline ::std::string* InvocationRequest::mutable_storage_backend() {
  
  // @@protoc_insertion_point(field_mutable:gg.protobuf.meow.InvocationRequest.storage_backend)
  return storage_backend_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* InvocationRequest::release_storage_backend() {
  // @@protoc_insertion_point(field_release:gg.protobuf.meow.InvocationRequest.storage_backend)
  
  return storage_backend_.ReleaseNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void InvocationRequest::set_allocated_storage_backend(::std::string* storage_backend) {
  if (storage_backend != NULL) {
    
  } else {
    
  }
  storage_backend_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), storage_backend);
  // @@protoc_insertion_point(field_set_allocated:gg.protobuf.meow.InvocationRequest.storage_backend)
}

// optional bool timelog = 3;
inline void InvocationRequest::clear_timelog() {
  timelog_ = false;
}
inline bool InvocationRequest::timelog() const {
  // @@protoc_insertion_point(field_get:gg.protobuf.meow.InvocationRequest.timelog)
  return timelog_;
}
inline void InvocationRequest::set_timelog(bool value) {
  
  timelog_ = value;
  // @@protoc_insertion_point(field_set:gg.protobuf.meow.InvocationRequest.timelog)
}

#endif  // !PROTOBUF_INLINE_NOT_IN_HEADERS

// @@protoc_insertion_point(namespace_scope)

}  // namespace meow
}  // namespace protobuf
}  // namespace gg

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_meow_2eproto__INCLUDED
