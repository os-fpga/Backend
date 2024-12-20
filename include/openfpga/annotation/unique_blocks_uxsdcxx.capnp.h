// Generated by Cap'n Proto compiler, DO NOT EDIT
// source: unique_blocks_uxsdcxx.capnp

#pragma once

#include <capnp/generated-header-support.h>
#include <kj/windows-sanity.h>

#ifndef CAPNP_VERSION
#error "CAPNP_VERSION is not defined, is capnp/generated-header-support.h missing?"
#elif CAPNP_VERSION != 1000002
#error "Version mismatch between generated code and library headers.  You must use the same version of the Cap'n Proto compiler and library."
#endif


CAPNP_BEGIN_HEADER

namespace capnp {
namespace schemas {

CAPNP_DECLARE_SCHEMA(c16da73cfbff13c7);
enum class Type_c16da73cfbff13c7: uint16_t {
  UXSD_INVALID,
  CBX,
  CBY,
  SB,
};
CAPNP_DECLARE_ENUM(Type, c16da73cfbff13c7);
CAPNP_DECLARE_SCHEMA(cc86674979cf0fd6);
CAPNP_DECLARE_SCHEMA(e65125b552e5bccc);
CAPNP_DECLARE_SCHEMA(f6ad8a85615a2b06);

}  // namespace schemas
}  // namespace capnp

namespace ucap {

typedef ::capnp::schemas::Type_c16da73cfbff13c7 Type;

struct Instance {
  Instance() = delete;

  class Reader;
  class Builder;
  class Pipeline;

  struct _capnpPrivate {
    CAPNP_DECLARE_STRUCT_HEADER(cc86674979cf0fd6, 1, 0)
    #if !CAPNP_LITE
    static constexpr ::capnp::_::RawBrandedSchema const* brand() { return &schema->defaultBrand; }
    #endif  // !CAPNP_LITE
  };
};

struct Block {
  Block() = delete;

  class Reader;
  class Builder;
  class Pipeline;

  struct _capnpPrivate {
    CAPNP_DECLARE_STRUCT_HEADER(e65125b552e5bccc, 2, 1)
    #if !CAPNP_LITE
    static constexpr ::capnp::_::RawBrandedSchema const* brand() { return &schema->defaultBrand; }
    #endif  // !CAPNP_LITE
  };
};

struct UniqueBlocks {
  UniqueBlocks() = delete;

  class Reader;
  class Builder;
  class Pipeline;

  struct _capnpPrivate {
    CAPNP_DECLARE_STRUCT_HEADER(f6ad8a85615a2b06, 0, 1)
    #if !CAPNP_LITE
    static constexpr ::capnp::_::RawBrandedSchema const* brand() { return &schema->defaultBrand; }
    #endif  // !CAPNP_LITE
  };
};

// =======================================================================================

class Instance::Reader {
public:
  typedef Instance Reads;

  Reader() = default;
  inline explicit Reader(::capnp::_::StructReader base): _reader(base) {}

  inline ::capnp::MessageSize totalSize() const {
    return _reader.totalSize().asPublic();
  }

#if !CAPNP_LITE
  inline ::kj::StringTree toString() const {
    return ::capnp::_::structString(_reader, *_capnpPrivate::brand());
  }
#endif  // !CAPNP_LITE

  inline  ::uint32_t getX() const;

  inline  ::uint32_t getY() const;

private:
  ::capnp::_::StructReader _reader;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::_::PointerHelpers;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::List;
  friend class ::capnp::MessageBuilder;
  friend class ::capnp::Orphanage;
};

class Instance::Builder {
public:
  typedef Instance Builds;

  Builder() = delete;  // Deleted to discourage incorrect usage.
                       // You can explicitly initialize to nullptr instead.
  inline Builder(decltype(nullptr)) {}
  inline explicit Builder(::capnp::_::StructBuilder base): _builder(base) {}
  inline operator Reader() const { return Reader(_builder.asReader()); }
  inline Reader asReader() const { return *this; }

  inline ::capnp::MessageSize totalSize() const { return asReader().totalSize(); }
#if !CAPNP_LITE
  inline ::kj::StringTree toString() const { return asReader().toString(); }
#endif  // !CAPNP_LITE

  inline  ::uint32_t getX();
  inline void setX( ::uint32_t value);

  inline  ::uint32_t getY();
  inline void setY( ::uint32_t value);

private:
  ::capnp::_::StructBuilder _builder;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
  friend class ::capnp::Orphanage;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::_::PointerHelpers;
};

#if !CAPNP_LITE
class Instance::Pipeline {
public:
  typedef Instance Pipelines;

  inline Pipeline(decltype(nullptr)): _typeless(nullptr) {}
  inline explicit Pipeline(::capnp::AnyPointer::Pipeline&& typeless)
      : _typeless(kj::mv(typeless)) {}

private:
  ::capnp::AnyPointer::Pipeline _typeless;
  friend class ::capnp::PipelineHook;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
};
#endif  // !CAPNP_LITE

class Block::Reader {
public:
  typedef Block Reads;

  Reader() = default;
  inline explicit Reader(::capnp::_::StructReader base): _reader(base) {}

  inline ::capnp::MessageSize totalSize() const {
    return _reader.totalSize().asPublic();
  }

#if !CAPNP_LITE
  inline ::kj::StringTree toString() const {
    return ::capnp::_::structString(_reader, *_capnpPrivate::brand());
  }
#endif  // !CAPNP_LITE

  inline  ::ucap::Type getType() const;

  inline  ::uint32_t getX() const;

  inline  ::uint32_t getY() const;

  inline bool hasInstances() const;
  inline  ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Reader getInstances() const;

private:
  ::capnp::_::StructReader _reader;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::_::PointerHelpers;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::List;
  friend class ::capnp::MessageBuilder;
  friend class ::capnp::Orphanage;
};

class Block::Builder {
public:
  typedef Block Builds;

  Builder() = delete;  // Deleted to discourage incorrect usage.
                       // You can explicitly initialize to nullptr instead.
  inline Builder(decltype(nullptr)) {}
  inline explicit Builder(::capnp::_::StructBuilder base): _builder(base) {}
  inline operator Reader() const { return Reader(_builder.asReader()); }
  inline Reader asReader() const { return *this; }

  inline ::capnp::MessageSize totalSize() const { return asReader().totalSize(); }
#if !CAPNP_LITE
  inline ::kj::StringTree toString() const { return asReader().toString(); }
#endif  // !CAPNP_LITE

  inline  ::ucap::Type getType();
  inline void setType( ::ucap::Type value);

  inline  ::uint32_t getX();
  inline void setX( ::uint32_t value);

  inline  ::uint32_t getY();
  inline void setY( ::uint32_t value);

  inline bool hasInstances();
  inline  ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Builder getInstances();
  inline void setInstances( ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Reader value);
  inline  ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Builder initInstances(unsigned int size);
  inline void adoptInstances(::capnp::Orphan< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>&& value);
  inline ::capnp::Orphan< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>> disownInstances();

private:
  ::capnp::_::StructBuilder _builder;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
  friend class ::capnp::Orphanage;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::_::PointerHelpers;
};

#if !CAPNP_LITE
class Block::Pipeline {
public:
  typedef Block Pipelines;

  inline Pipeline(decltype(nullptr)): _typeless(nullptr) {}
  inline explicit Pipeline(::capnp::AnyPointer::Pipeline&& typeless)
      : _typeless(kj::mv(typeless)) {}

private:
  ::capnp::AnyPointer::Pipeline _typeless;
  friend class ::capnp::PipelineHook;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
};
#endif  // !CAPNP_LITE

class UniqueBlocks::Reader {
public:
  typedef UniqueBlocks Reads;

  Reader() = default;
  inline explicit Reader(::capnp::_::StructReader base): _reader(base) {}

  inline ::capnp::MessageSize totalSize() const {
    return _reader.totalSize().asPublic();
  }

#if !CAPNP_LITE
  inline ::kj::StringTree toString() const {
    return ::capnp::_::structString(_reader, *_capnpPrivate::brand());
  }
#endif  // !CAPNP_LITE

  inline bool hasBlocks() const;
  inline  ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Reader getBlocks() const;

private:
  ::capnp::_::StructReader _reader;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::_::PointerHelpers;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::List;
  friend class ::capnp::MessageBuilder;
  friend class ::capnp::Orphanage;
};

class UniqueBlocks::Builder {
public:
  typedef UniqueBlocks Builds;

  Builder() = delete;  // Deleted to discourage incorrect usage.
                       // You can explicitly initialize to nullptr instead.
  inline Builder(decltype(nullptr)) {}
  inline explicit Builder(::capnp::_::StructBuilder base): _builder(base) {}
  inline operator Reader() const { return Reader(_builder.asReader()); }
  inline Reader asReader() const { return *this; }

  inline ::capnp::MessageSize totalSize() const { return asReader().totalSize(); }
#if !CAPNP_LITE
  inline ::kj::StringTree toString() const { return asReader().toString(); }
#endif  // !CAPNP_LITE

  inline bool hasBlocks();
  inline  ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Builder getBlocks();
  inline void setBlocks( ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Reader value);
  inline  ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Builder initBlocks(unsigned int size);
  inline void adoptBlocks(::capnp::Orphan< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>&& value);
  inline ::capnp::Orphan< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>> disownBlocks();

private:
  ::capnp::_::StructBuilder _builder;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
  friend class ::capnp::Orphanage;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::_::PointerHelpers;
};

#if !CAPNP_LITE
class UniqueBlocks::Pipeline {
public:
  typedef UniqueBlocks Pipelines;

  inline Pipeline(decltype(nullptr)): _typeless(nullptr) {}
  inline explicit Pipeline(::capnp::AnyPointer::Pipeline&& typeless)
      : _typeless(kj::mv(typeless)) {}

private:
  ::capnp::AnyPointer::Pipeline _typeless;
  friend class ::capnp::PipelineHook;
  template <typename, ::capnp::Kind>
  friend struct ::capnp::ToDynamic_;
};
#endif  // !CAPNP_LITE

// =======================================================================================

inline  ::uint32_t Instance::Reader::getX() const {
  return _reader.getDataField< ::uint32_t>(
      ::capnp::bounded<0>() * ::capnp::ELEMENTS);
}

inline  ::uint32_t Instance::Builder::getX() {
  return _builder.getDataField< ::uint32_t>(
      ::capnp::bounded<0>() * ::capnp::ELEMENTS);
}
inline void Instance::Builder::setX( ::uint32_t value) {
  _builder.setDataField< ::uint32_t>(
      ::capnp::bounded<0>() * ::capnp::ELEMENTS, value);
}

inline  ::uint32_t Instance::Reader::getY() const {
  return _reader.getDataField< ::uint32_t>(
      ::capnp::bounded<1>() * ::capnp::ELEMENTS);
}

inline  ::uint32_t Instance::Builder::getY() {
  return _builder.getDataField< ::uint32_t>(
      ::capnp::bounded<1>() * ::capnp::ELEMENTS);
}
inline void Instance::Builder::setY( ::uint32_t value) {
  _builder.setDataField< ::uint32_t>(
      ::capnp::bounded<1>() * ::capnp::ELEMENTS, value);
}

inline  ::ucap::Type Block::Reader::getType() const {
  return _reader.getDataField< ::ucap::Type>(
      ::capnp::bounded<0>() * ::capnp::ELEMENTS);
}

inline  ::ucap::Type Block::Builder::getType() {
  return _builder.getDataField< ::ucap::Type>(
      ::capnp::bounded<0>() * ::capnp::ELEMENTS);
}
inline void Block::Builder::setType( ::ucap::Type value) {
  _builder.setDataField< ::ucap::Type>(
      ::capnp::bounded<0>() * ::capnp::ELEMENTS, value);
}

inline  ::uint32_t Block::Reader::getX() const {
  return _reader.getDataField< ::uint32_t>(
      ::capnp::bounded<1>() * ::capnp::ELEMENTS);
}

inline  ::uint32_t Block::Builder::getX() {
  return _builder.getDataField< ::uint32_t>(
      ::capnp::bounded<1>() * ::capnp::ELEMENTS);
}
inline void Block::Builder::setX( ::uint32_t value) {
  _builder.setDataField< ::uint32_t>(
      ::capnp::bounded<1>() * ::capnp::ELEMENTS, value);
}

inline  ::uint32_t Block::Reader::getY() const {
  return _reader.getDataField< ::uint32_t>(
      ::capnp::bounded<2>() * ::capnp::ELEMENTS);
}

inline  ::uint32_t Block::Builder::getY() {
  return _builder.getDataField< ::uint32_t>(
      ::capnp::bounded<2>() * ::capnp::ELEMENTS);
}
inline void Block::Builder::setY( ::uint32_t value) {
  _builder.setDataField< ::uint32_t>(
      ::capnp::bounded<2>() * ::capnp::ELEMENTS, value);
}

inline bool Block::Reader::hasInstances() const {
  return !_reader.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS).isNull();
}
inline bool Block::Builder::hasInstances() {
  return !_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS).isNull();
}
inline  ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Reader Block::Reader::getInstances() const {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>::get(_reader.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS));
}
inline  ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Builder Block::Builder::getInstances() {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>::get(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS));
}
inline void Block::Builder::setInstances( ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Reader value) {
  ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>::set(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS), value);
}
inline  ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>::Builder Block::Builder::initInstances(unsigned int size) {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>::init(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS), size);
}
inline void Block::Builder::adoptInstances(
    ::capnp::Orphan< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>&& value) {
  ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>::adopt(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS), kj::mv(value));
}
inline ::capnp::Orphan< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>> Block::Builder::disownInstances() {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Instance,  ::capnp::Kind::STRUCT>>::disown(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS));
}

inline bool UniqueBlocks::Reader::hasBlocks() const {
  return !_reader.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS).isNull();
}
inline bool UniqueBlocks::Builder::hasBlocks() {
  return !_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS).isNull();
}
inline  ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Reader UniqueBlocks::Reader::getBlocks() const {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>::get(_reader.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS));
}
inline  ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Builder UniqueBlocks::Builder::getBlocks() {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>::get(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS));
}
inline void UniqueBlocks::Builder::setBlocks( ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Reader value) {
  ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>::set(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS), value);
}
inline  ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>::Builder UniqueBlocks::Builder::initBlocks(unsigned int size) {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>::init(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS), size);
}
inline void UniqueBlocks::Builder::adoptBlocks(
    ::capnp::Orphan< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>&& value) {
  ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>::adopt(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS), kj::mv(value));
}
inline ::capnp::Orphan< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>> UniqueBlocks::Builder::disownBlocks() {
  return ::capnp::_::PointerHelpers< ::capnp::List< ::ucap::Block,  ::capnp::Kind::STRUCT>>::disown(_builder.getPointerField(
      ::capnp::bounded<0>() * ::capnp::POINTERS));
}

}  // namespace

CAPNP_END_HEADER

