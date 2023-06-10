/**
 * pugixml parser - version 1.13 (modified by serge-rs)
 * --------------------------------------------------------
 * Copyright (C) 2006-2022, by Arseny Kapoulkine (arseny.kapoulkine@gmail.com)
 * Report bugs and download new versions at https://pugixml.org/
 *
 * This library is distributed under the MIT License. See notice at the end
 * of this file.
 *
 * This work is based on the pugxml parser, which is:
 * Copyright (C) 2003, by Kristen Wegner (kristen@tima.net)
 */

#include "file_readers/pinc_pugixml.h"
#include "util/pinc_log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <cstdint>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <new>


#define PUGI__NO_INLINE __attribute__((noinline))

#define PUGI__UNLIKELY(cond) __builtin_expect(cond, 0)

#define PUGI__DMC_VOLATILE


// Integer sanitizer workaround; we only apply this for clang since gcc8 has no_sanitize but not unsigned-integer-overflow and produces "attribute directive ignored" warnings
#if defined(__clang__) && defined(__has_attribute)
#    if __has_attribute(no_sanitize)
#        define PUGI__UNSIGNED_OVERFLOW __attribute__((no_sanitize("unsigned-integer-overflow")))
#    else
#        define PUGI__UNSIGNED_OVERFLOW
#    endif
#else
#    define PUGI__UNSIGNED_OVERFLOW
#endif

#define PUGI__SNPRINTF(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

#define PUGI__NS_BEGIN namespace pugRd { namespace impl {
#define PUGI__NS_END } }

#define PUGI__FN
#define PUGI__FN_NO_INLINE PUGI__NO_INLINE

// String utilities
PUGI__NS_BEGIN

    inline static bool strequal(const char* src, const char* dst) noexcept
    {
        assert(src && dst);

        return strcmp(src, dst) == 0;
    }

    // Compare lhs with [rhs_begin, rhs_end)
    PUGI__FN bool strequalrange(const char* lhs, const char* rhs, size_t count) noexcept
    {
        for (size_t i = 0; i < count; ++i)
            if (lhs[i] != rhs[i])
                return false;

        return lhs[count] == 0;
    }


// auto_ptr-like object for exception recovery
  template <typename T>
  struct auto_deleter
  {
      typedef void (*D)(T*);

      T* data = nullptr;
      D deleter;

      auto_deleter(T* data_, D deleter_) noexcept
        : data(data_), deleter(deleter_)
      {
      }

      ~auto_deleter()
      {
          if (data) deleter(data);
      }

      T* release() noexcept
      {
          T* result = data;
          data = 0;
          return result;
      }
  };

    static const uintptr_t xml_memory_block_alignment = sizeof(void*);

    // extra metadata bits
    static const uintptr_t xml_memory_page_contents_shared_mask = 64;
    static const uintptr_t xml_memory_page_name_allocated_mask = 32;
    static const uintptr_t xml_memory_page_value_allocated_mask = 16;
    static const uintptr_t xml_memory_page_type_mask = 15;

    // combined masks for string uniqueness
    static const uintptr_t xml_memory_page_name_allocated_or_shared_mask = xml_memory_page_name_allocated_mask | xml_memory_page_contents_shared_mask;
    static const uintptr_t xml_memory_page_value_allocated_or_shared_mask = xml_memory_page_value_allocated_mask | xml_memory_page_contents_shared_mask;

    #define PUGI__GETHEADER_IMPL(object, page, flags) (((reinterpret_cast<char*>(object) - reinterpret_cast<char*>(page)) << 8) | (flags))
    // this macro casts pointers through void* to avoid 'cast increases required alignment of target type' warnings
    #define PUGI__GETPAGE_IMPL(header) static_cast<impl::xml_memory_page*>(const_cast<void*>(static_cast<const void*>(reinterpret_cast<const char*>(&header) - (header >> 8))))

    #define PUGI__GETPAGE(n) PUGI__GETPAGE_IMPL((n)->header)
    #define PUGI__NODETYPE(n) static_cast<xml_node_type>((n)->header & impl::xml_memory_page_type_mask)

    struct xml_allocator;

    struct xml_memory_page
    {
        static xml_memory_page* construct(void* memory)
        {
            xml_memory_page* result = static_cast<xml_memory_page*>(memory);

            result->allocator = 0;
            result->prev = 0;
            result->next = 0;
            result->busy_size = 0;
            result->freed_size = 0;

            return result;
        }

        xml_allocator* allocator;

        xml_memory_page* prev;
        xml_memory_page* next;

        size_t busy_size;
        size_t freed_size;
    };

    static const size_t xml_memory_page_size =
    #ifdef PUGIXML_MEMORY_PAGE_SIZE
        (PUGIXML_MEMORY_PAGE_SIZE)
    #else
        32768
    #endif
        - sizeof(xml_memory_page);

    struct xml_memory_string_header
    {
        uint16_t page_offset; // offset from page->data
        uint16_t full_size; // 0 if string occupies whole page
    };

    struct xml_allocator
    {
        xml_allocator(xml_memory_page* root) noexcept
            : _root(root), _busy_size(root->busy_size)
        { }

        xml_memory_page* allocate_page(size_t data_size) noexcept
        {
            size_t size = sizeof(xml_memory_page) + data_size;

            // allocate block with some alignment, leaving memory for worst-case padding
            void* memory = ::malloc(size);
            if (!memory) return 0;

            // prepare page structure
            xml_memory_page* page = xml_memory_page::construct(memory);
            assert(page);

            assert(this == _root->allocator);
            page->allocator = this;

            return page;
        }

        static void deallocate_page(xml_memory_page* page) noexcept
        {
            ::free(page);
        }

        void* allocate_memory_oob(size_t size, xml_memory_page*& out_page) noexcept;

        void* allocate_memory(size_t size, xml_memory_page*& out_page) noexcept
        {
            if (PUGI__UNLIKELY(_busy_size + size > xml_memory_page_size))
                return allocate_memory_oob(size, out_page);

            void* buf = reinterpret_cast<char*>(_root) + sizeof(xml_memory_page) + _busy_size;

            _busy_size += size;

            out_page = _root;

            return buf;
        }

        void* allocate_object(size_t size, xml_memory_page*& out_page) noexcept
        {
            return allocate_memory(size, out_page);
        }

        void deallocate_memory(void* ptr, size_t size, xml_memory_page* page) noexcept
        {
            if (page == _root) page->busy_size = _busy_size;

            assert(ptr >= reinterpret_cast<char*>(page) + sizeof(xml_memory_page) && ptr < reinterpret_cast<char*>(page) + sizeof(xml_memory_page) + page->busy_size);
            (void)!ptr;

            page->freed_size += size;
            assert(page->freed_size <= page->busy_size);

            if (page->freed_size == page->busy_size)
            {
                if (page->next == 0)
                {
                    assert(_root == page);

                    // top page freed, just reset sizes
                    page->busy_size = 0;
                    page->freed_size = 0;

                #ifdef PUGIXML_COMPACT
                    // reset compact state to maximize efficiency
                    page->compact_string_base = 0;
                    page->compact_shared_parent = 0;
                    page->compact_page_marker = 0;
                #endif

                    _busy_size = 0;
                }
                else
                {
                    assert(_root != page);
                    assert(page->prev);

                    // remove from the list
                    page->prev->next = page->next;
                    page->next->prev = page->prev;

                    // deallocate
                    deallocate_page(page);
                }
            }
        }

        char* allocate_string(size_t length)
        {
            static const size_t max_encoded_offset = (1 << 16) * xml_memory_block_alignment;

            static_assert(xml_memory_page_size <= max_encoded_offset);

            // allocate memory for string and header block
            size_t size = sizeof(xml_memory_string_header) + length * sizeof(char);

            // round size up to block alignment boundary
            size_t full_size = (size + (xml_memory_block_alignment - 1)) & ~(xml_memory_block_alignment - 1);

            xml_memory_page* page;
            xml_memory_string_header* header = static_cast<xml_memory_string_header*>(allocate_memory(full_size, page));

            if (!header) return 0;

            // setup header
            ptrdiff_t page_offset = reinterpret_cast<char*>(header) - reinterpret_cast<char*>(page) - sizeof(xml_memory_page);

            assert(page_offset % xml_memory_block_alignment == 0);
            assert(page_offset >= 0 && static_cast<size_t>(page_offset) < max_encoded_offset);
            header->page_offset = static_cast<uint16_t>(static_cast<size_t>(page_offset) / xml_memory_block_alignment);

            // full_size == 0 for large strings that occupy the whole page
            assert(full_size % xml_memory_block_alignment == 0);
            assert(full_size < max_encoded_offset || (page->busy_size == full_size && page_offset == 0));
            header->full_size = static_cast<uint16_t>(full_size < max_encoded_offset ? full_size / xml_memory_block_alignment : 0);

            // round-trip through void* to avoid 'cast increases required alignment of target type' warning
            // header is guaranteed a pointer-sized alignment, which should be enough for char
            return static_cast<char*>(static_cast<void*>(header + 1));
        }

        void deallocate_string(char* string)
        {
            // this function casts pointers through void* to avoid 'cast increases required alignment of target type' warnings
            // we're guaranteed the proper (pointer-sized) alignment on the input string if it was allocated via allocate_string

            // get header
            xml_memory_string_header* header = static_cast<xml_memory_string_header*>(static_cast<void*>(string)) - 1;
            assert(header);

            // deallocate
            size_t page_offset = sizeof(xml_memory_page) + header->page_offset * xml_memory_block_alignment;
            xml_memory_page* page = reinterpret_cast<xml_memory_page*>(static_cast<void*>(reinterpret_cast<char*>(header) - page_offset));

            // if full_size == 0 then this string occupies the whole page
            size_t full_size = header->full_size == 0 ? page->busy_size : header->full_size * xml_memory_block_alignment;

            deallocate_memory(header, full_size, page);
        }

        bool reserve()
        {
        #ifdef PUGIXML_COMPACT
            return _hash->reserve();
        #else
            return true;
        #endif
        }

        xml_memory_page* _root;
        size_t _busy_size;

    #ifdef PUGIXML_COMPACT
        compact_hash_table* _hash;
    #endif
    };

    PUGI__FN_NO_INLINE void* xml_allocator::allocate_memory_oob(size_t size, xml_memory_page*& out_page) noexcept
    {
        const size_t large_allocation_threshold = xml_memory_page_size / 4;

        xml_memory_page* page = allocate_page(size <= large_allocation_threshold ? xml_memory_page_size : size);
        out_page = page;

        if (!page) return 0;

        if (size <= large_allocation_threshold)
        {
            _root->busy_size = _busy_size;

            // insert page at the end of linked list
            page->prev = _root;
            _root->next = page;
            _root = page;

            _busy_size = size;
        }
        else
        {
            // insert page before the end of linked list, so that it is deleted as soon as possible
            // the last page is not deleted even if it's empty (see deallocate_memory)
            assert(_root->prev);

            page->prev = _root->prev;
            page->next = _root;

            _root->prev->next = page;
            _root->prev = page;

            page->busy_size = size;
        }

        return reinterpret_cast<char*>(page) + sizeof(xml_memory_page);
    }
PUGI__NS_END

#ifdef PUGIXML_COMPACT
PUGI__NS_BEGIN
    static const uintptr_t compact_alignment_log2 = 2;
    static const uintptr_t compact_alignment = 1 << compact_alignment_log2;

    class compact_header
    {
    public:
        compact_header(xml_memory_page* page, uint flags)
        {
            static_assert(xml_memory_block_alignment == compact_alignment);

            ptrdiff_t offset = (reinterpret_cast<char*>(this) - reinterpret_cast<char*>(page->compact_page_marker));
            assert(offset % compact_alignment == 0 && static_cast<uintptr_t>(offset) < 256 * compact_alignment);

            _page = static_cast<unsigned char>(offset >> compact_alignment_log2);
            _flags = static_cast<unsigned char>(flags);
        }

        void operator&=(uintptr_t mod)
        {
            _flags &= static_cast<unsigned char>(mod);
        }

        void operator|=(uintptr_t mod)
        {
            _flags |= static_cast<unsigned char>(mod);
        }

        uintptr_t operator&(uintptr_t mod) const
        {
            return _flags & mod;
        }

        xml_memory_page* get_page() const
        {
            // round-trip through void* to silence 'cast increases required alignment of target type' warnings
            const char* page_marker = reinterpret_cast<const char*>(this) - (_page << compact_alignment_log2);
            const char* page = page_marker - *reinterpret_cast<const uint32_t*>(static_cast<const void*>(page_marker));

            return const_cast<xml_memory_page*>(reinterpret_cast<const xml_memory_page*>(static_cast<const void*>(page)));
        }

    private:
        unsigned char _page;
        unsigned char _flags;
    };

    PUGI__FN xml_memory_page* compact_get_page(const void* object, int header_offset)
    {
        const compact_header* header = reinterpret_cast<const compact_header*>(static_cast<const char*>(object) - header_offset);

        return header->get_page();
    }

    template <int header_offset, typename T> PUGI__FN_NO_INLINE T* compact_get_value(const void* object)
    {
        return static_cast<T*>(compact_get_page(object, header_offset)->allocator->_hash->find(object));
    }

    template <int header_offset, typename T> PUGI__FN_NO_INLINE void compact_set_value(const void* object, T* value)
    {
        compact_get_page(object, header_offset)->allocator->_hash->insert(object, value);
    }

    template <typename T, int header_offset, int start = -126> class compact_pointer
    {
    public:
        compact_pointer(): _data(0)
        {
        }

        void operator=(const compact_pointer& rhs)
        {
            *this = rhs + 0;
        }

        void operator=(T* value)
        {
            if (value)
            {
                // value is guaranteed to be compact-aligned; 'this' is not
                // our decoding is based on 'this' aligned to compact alignment downwards (see operator T*)
                // so for negative offsets (e.g. -3) we need to adjust the diff by compact_alignment - 1 to
                // compensate for arithmetic shift rounding for negative values
                ptrdiff_t diff = reinterpret_cast<char*>(value) - reinterpret_cast<char*>(this);
                ptrdiff_t offset = ((diff + int(compact_alignment - 1)) >> compact_alignment_log2) - start;

                if (static_cast<uintptr_t>(offset) <= 253)
                    _data = static_cast<unsigned char>(offset + 1);
                else
                {
                    compact_set_value<header_offset>(this, value);

                    _data = 255;
                }
            }
            else
                _data = 0;
        }

        operator T*() const
        {
            if (_data)
            {
                if (_data < 255)
                {
                    uintptr_t base = reinterpret_cast<uintptr_t>(this) & ~(compact_alignment - 1);

                    return reinterpret_cast<T*>(base + (_data - 1 + start) * compact_alignment);
                }
                else
                    return compact_get_value<header_offset, T>(this);
            }
            else
                return 0;
        }

        T* operator->() const
        {
            return *this;
        }

    private:
        unsigned char _data;
    };

    template <typename T, int header_offset> class compact_pointer_parent
    {
    public:
        compact_pointer_parent(): _data(0)
        {
        }

        void operator=(const compact_pointer_parent& rhs)
        {
            *this = rhs + 0;
        }

        void operator=(T* value)
        {
            if (value)
            {
                // value is guaranteed to be compact-aligned; 'this' is not
                // our decoding is based on 'this' aligned to compact alignment downwards (see operator T*)
                // so for negative offsets (e.g. -3) we need to adjust the diff by compact_alignment - 1 to
                // compensate for arithmetic shift behavior for negative values
                ptrdiff_t diff = reinterpret_cast<char*>(value) - reinterpret_cast<char*>(this);
                ptrdiff_t offset = ((diff + int(compact_alignment - 1)) >> compact_alignment_log2) + 65533;

                if (static_cast<uintptr_t>(offset) <= 65533)
                {
                    _data = static_cast<unsigned short>(offset + 1);
                }
                else
                {
                    xml_memory_page* page = compact_get_page(this, header_offset);

                    if (PUGI__UNLIKELY(page->compact_shared_parent == 0))
                        page->compact_shared_parent = value;

                    if (page->compact_shared_parent == value)
                    {
                        _data = 65534;
                    }
                    else
                    {
                        compact_set_value<header_offset>(this, value);

                        _data = 65535;
                    }
                }
            }
            else
            {
                _data = 0;
            }
        }

        operator T*() const
        {
            if (_data)
            {
                if (_data < 65534)
                {
                    uintptr_t base = reinterpret_cast<uintptr_t>(this) & ~(compact_alignment - 1);

                    return reinterpret_cast<T*>(base + (_data - 1 - 65533) * compact_alignment);
                }
                else if (_data == 65534)
                    return static_cast<T*>(compact_get_page(this, header_offset)->compact_shared_parent);
                else
                    return compact_get_value<header_offset, T>(this);
            }
            else
                return 0;
        }

        T* operator->() const
        {
            return *this;
        }

    private:
        uint16_t _data;
    };

    template <int header_offset, int base_offset> class compact_string
    {
    public:
        compact_string(): _data(0)
        {
        }

        void operator=(const compact_string& rhs)
        {
            *this = rhs + 0;
        }

        void operator=(char* value)
        {
            if (value)
            {
                xml_memory_page* page = compact_get_page(this, header_offset);

                if (PUGI__UNLIKELY(page->compact_string_base == 0))
                    page->compact_string_base = value;

                ptrdiff_t offset = value - page->compact_string_base;

                if (static_cast<uintptr_t>(offset) < (65535 << 7))
                {
                    // round-trip through void* to silence 'cast increases required alignment of target type' warnings
                    uint16_t* base = reinterpret_cast<uint16_t*>(static_cast<void*>(reinterpret_cast<char*>(this) - base_offset));

                    if (*base == 0)
                    {
                        *base = static_cast<uint16_t>((offset >> 7) + 1);
                        _data = static_cast<unsigned char>((offset & 127) + 1);
                    }
                    else
                    {
                        ptrdiff_t remainder = offset - ((*base - 1) << 7);

                        if (static_cast<uintptr_t>(remainder) <= 253)
                        {
                            _data = static_cast<unsigned char>(remainder + 1);
                        }
                        else
                        {
                            compact_set_value<header_offset>(this, value);

                            _data = 255;
                        }
                    }
                }
                else
                {
                    compact_set_value<header_offset>(this, value);

                    _data = 255;
                }
            }
            else
            {
                _data = 0;
            }
        }

        operator char*() const
        {
            if (_data)
            {
                if (_data < 255)
                {
                    xml_memory_page* page = compact_get_page(this, header_offset);

                    // round-trip through void* to silence 'cast increases required alignment of target type' warnings
                    const uint16_t* base = reinterpret_cast<const uint16_t*>(static_cast<const void*>(reinterpret_cast<const char*>(this) - base_offset));
                    assert(*base);

                    ptrdiff_t offset = ((*base - 1) << 7) + (_data - 1);

                    return page->compact_string_base + offset;
                }
                else
                {
                    return compact_get_value<header_offset, char>(this);
                }
            }
            else
                return 0;
        }

    private:
        unsigned char _data;
    };
PUGI__NS_END
#endif

#ifdef PUGIXML_COMPACT
namespace pugRd
{
    struct xml_attribute_struct
    {
        xml_attribute_struct(impl::xml_memory_page* page): header(page, 0), namevalue_base(0)
        {
            static_assert(sizeof(xml_attribute_struct) == 8);
        }

        impl::compact_header header;

        uint16_t namevalue_base;

        impl::compact_string<4, 2> name;
        impl::compact_string<5, 3> value;

        impl::compact_pointer<xml_attribute_struct, 6> prev_attribute_c;
        impl::compact_pointer<xml_attribute_struct, 7, 0> next_attribute;
    };

    struct xml_node_struct
    {
        xml_node_struct(impl::xml_memory_page* page, xml_node_type type): header(page, type), namevalue_base(0)
        {
            static_assert(sizeof(xml_node_struct) == 12);
        }

        impl::compact_header header;

        uint16_t namevalue_base;

        impl::compact_string<4, 2> name;
        impl::compact_string<5, 3> value;

        impl::compact_pointer_parent<xml_node_struct, 6> parent;

        impl::compact_pointer<xml_node_struct, 8, 0> first_child;

        impl::compact_pointer<xml_node_struct,  9>    prev_sibling_c;
        impl::compact_pointer<xml_node_struct, 10, 0> next_sibling;

        impl::compact_pointer<xml_attribute_struct, 11, 0> first_attribute;
    };
}
#else
namespace pugRd
{
    struct xml_attribute_struct
    {
        xml_attribute_struct(impl::xml_memory_page* page): name(0), value(0), prev_attribute_c(0), next_attribute(0)
        {
            header = PUGI__GETHEADER_IMPL(this, page, 0);
        }

        uintptr_t header;

        char*    name;
        char*    value;

        xml_attribute_struct* prev_attribute_c;
        xml_attribute_struct* next_attribute;
    };

    struct xml_node_struct
    {
        xml_node_struct(impl::xml_memory_page* page, xml_node_type type): name(0), value(0), parent(0), first_child(0), prev_sibling_c(0), next_sibling(0), first_attribute(0)
        {
            header = PUGI__GETHEADER_IMPL(this, page, type);
        }

        uintptr_t header;

        char* name;
        char* value;

        xml_node_struct* parent;

        xml_node_struct* first_child;

        xml_node_struct* prev_sibling_c;
        xml_node_struct* next_sibling;

        xml_attribute_struct* first_attribute;
    };
}
#endif

PUGI__NS_BEGIN
    struct xml_extra_buffer
    {
        char* buffer;
        xml_extra_buffer* next;
    };

    struct xml_document_struct: public xml_node_struct, public xml_allocator
    {
        xml_document_struct(xml_memory_page* page): xml_node_struct(page, node_document), xml_allocator(page), buffer(0), extra_buffers(0)
        {
        }

        const char* buffer;

        xml_extra_buffer* extra_buffers;

    #ifdef PUGIXML_COMPACT
        compact_hash_table hash;
    #endif
    };

    template <typename Object> inline xml_allocator& get_allocator(const Object* object)
    {
        assert(object);

        return *PUGI__GETPAGE(object)->allocator;
    }

    template <typename Object> inline xml_document_struct& get_document(const Object* object)
    {
        assert(object);

        return *static_cast<xml_document_struct*>(PUGI__GETPAGE(object)->allocator);
    }
PUGI__NS_END

// Low-level DOM operations
PUGI__NS_BEGIN
    inline xml_attribute_struct* allocate_attribute(xml_allocator& alloc) noexcept
    {
        xml_memory_page* page;
        void* memory = alloc.allocate_object(sizeof(xml_attribute_struct), page);
        if (!memory) return 0;

        return new (memory) xml_attribute_struct(page);
    }

    inline xml_node_struct* allocate_node(xml_allocator& alloc, xml_node_type type) noexcept
    {
        xml_memory_page* page;
        void* memory = alloc.allocate_object(sizeof(xml_node_struct), page);
        if (!memory) return 0;

        return new (memory) xml_node_struct(page, type);
    }

    inline void destroy_attribute(xml_attribute_struct* a, xml_allocator& alloc) noexcept
    {
        if (a->header & impl::xml_memory_page_name_allocated_mask)
            alloc.deallocate_string(a->name);

        if (a->header & impl::xml_memory_page_value_allocated_mask)
            alloc.deallocate_string(a->value);

        alloc.deallocate_memory(a, sizeof(xml_attribute_struct), PUGI__GETPAGE(a));
    }

    inline void destroy_node(xml_node_struct* n, xml_allocator& alloc) noexcept
    {
        if (n->header & impl::xml_memory_page_name_allocated_mask)
            alloc.deallocate_string(n->name);

        if (n->header & impl::xml_memory_page_value_allocated_mask)
            alloc.deallocate_string(n->value);

        for (xml_attribute_struct* attr = n->first_attribute; attr; )
        {
            xml_attribute_struct* next = attr->next_attribute;

            destroy_attribute(attr, alloc);

            attr = next;
        }

        for (xml_node_struct* child = n->first_child; child; )
        {
            xml_node_struct* next = child->next_sibling;

            destroy_node(child, alloc);

            child = next;
        }

        alloc.deallocate_memory(n, sizeof(xml_node_struct), PUGI__GETPAGE(n));
    }

    inline void append_node(xml_node_struct* child, xml_node_struct* node) noexcept
    {
        child->parent = node;

        xml_node_struct* head = node->first_child;

        if (head)
        {
            xml_node_struct* tail = head->prev_sibling_c;

            tail->next_sibling = child;
            child->prev_sibling_c = tail;
            head->prev_sibling_c = child;
        }
        else
        {
            node->first_child = child;
            child->prev_sibling_c = child;
        }
    }

    inline void prepend_node(xml_node_struct* child, xml_node_struct* node) noexcept
    {
        child->parent = node;

        xml_node_struct* head = node->first_child;

        if (head)
        {
            child->prev_sibling_c = head->prev_sibling_c;
            head->prev_sibling_c = child;
        }
        else
            child->prev_sibling_c = child;

        child->next_sibling = head;
        node->first_child = child;
    }

    inline void insert_node_after(xml_node_struct* child, xml_node_struct* node) noexcept
    {
        xml_node_struct* parent = node->parent;

        child->parent = parent;

        xml_node_struct* next = node->next_sibling;

        if (next)
            next->prev_sibling_c = child;
        else
            parent->first_child->prev_sibling_c = child;

        child->next_sibling = next;
        child->prev_sibling_c = node;

        node->next_sibling = child;
    }

    inline void insert_node_before(xml_node_struct* child, xml_node_struct* node) noexcept
    {
        xml_node_struct* parent = node->parent;

        child->parent = parent;

        xml_node_struct* prev = node->prev_sibling_c;

        if (prev->next_sibling)
            prev->next_sibling = child;
        else
            parent->first_child = child;

        child->prev_sibling_c = prev;
        child->next_sibling = node;

        node->prev_sibling_c = child;
    }

    inline void remove_node(xml_node_struct* node) noexcept
    {
        xml_node_struct* parent = node->parent;

        xml_node_struct* next = node->next_sibling;
        xml_node_struct* prev = node->prev_sibling_c;

        if (next)
            next->prev_sibling_c = prev;
        else
            parent->first_child->prev_sibling_c = prev;

        if (prev->next_sibling)
            prev->next_sibling = next;
        else
            parent->first_child = next;

        node->parent = 0;
        node->prev_sibling_c = 0;
        node->next_sibling = 0;
    }

    inline void append_attribute(xml_attribute_struct* attr, xml_node_struct* node) noexcept
    {
        xml_attribute_struct* head = node->first_attribute;

        if (head)
        {
            xml_attribute_struct* tail = head->prev_attribute_c;

            tail->next_attribute = attr;
            attr->prev_attribute_c = tail;
            head->prev_attribute_c = attr;
        }
        else
        {
            node->first_attribute = attr;
            attr->prev_attribute_c = attr;
        }
    }

    inline void prepend_attribute(xml_attribute_struct* attr, xml_node_struct* node) noexcept
    {
        xml_attribute_struct* head = node->first_attribute;

        if (head)
        {
            attr->prev_attribute_c = head->prev_attribute_c;
            head->prev_attribute_c = attr;
        }
        else
            attr->prev_attribute_c = attr;

        attr->next_attribute = head;
        node->first_attribute = attr;
    }

    inline void insert_attribute_after(xml_attribute_struct* attr, xml_attribute_struct* place, xml_node_struct* node) noexcept
    {
        xml_attribute_struct* next = place->next_attribute;

        if (next)
            next->prev_attribute_c = attr;
        else
            node->first_attribute->prev_attribute_c = attr;

        attr->next_attribute = next;
        attr->prev_attribute_c = place;
        place->next_attribute = attr;
    }

    inline void insert_attribute_before(xml_attribute_struct* attr, xml_attribute_struct* place, xml_node_struct* node) noexcept
    {
        xml_attribute_struct* prev = place->prev_attribute_c;

        if (prev->next_attribute)
            prev->next_attribute = attr;
        else
            node->first_attribute = attr;

        attr->prev_attribute_c = prev;
        attr->next_attribute = place;
        place->prev_attribute_c = attr;
    }

    inline void remove_attribute(xml_attribute_struct* attr, xml_node_struct* node) noexcept
    {
        xml_attribute_struct* next = attr->next_attribute;
        xml_attribute_struct* prev = attr->prev_attribute_c;

        if (next)
            next->prev_attribute_c = prev;
        else
            node->first_attribute->prev_attribute_c = prev;

        if (prev->next_attribute)
            prev->next_attribute = next;
        else
            node->first_attribute = next;

        attr->prev_attribute_c = 0;
        attr->next_attribute = 0;
    }

    PUGI__FN_NO_INLINE xml_node_struct* append_new_node(xml_node_struct* node, xml_allocator& alloc, xml_node_type type = node_element)
    {
        if (!alloc.reserve()) return 0;

        xml_node_struct* child = allocate_node(alloc, type);
        if (!child) return 0;

        append_node(child, node);

        return child;
    }

    PUGI__FN_NO_INLINE xml_attribute_struct* append_new_attribute(xml_node_struct* node, xml_allocator& alloc) noexcept
    {
        if (!alloc.reserve()) return 0;

        xml_attribute_struct* attr = allocate_attribute(alloc);
        if (!attr) return 0;

        append_attribute(attr, node);

        return attr;
    }
PUGI__NS_END

// Helper classes for code generation
PUGI__NS_BEGIN
    struct opt_false
    {
        enum { value = 0 };
    };

    struct opt_true
    {
        enum { value = 1 };
    };
PUGI__NS_END

// Unicode utilities
PUGI__NS_BEGIN

    inline uint16_t endian_swap(uint16_t value) noexcept
    {
        return static_cast<uint16_t>(((value & 0xff) << 8) | (value >> 8));
    }

    inline uint32_t endian_swap(uint32_t value) noexcept
    {
        return ((value & 0xff) << 24) | ((value & 0xff00) << 8) | ((value & 0xff0000) >> 8) | (value >> 24);
    }

    struct utf8_counter
    {
        typedef size_t value_type;

        static value_type low(value_type result, uint32_t ch)
        {
            // U+0000..U+007F
            if (ch < 0x80) return result + 1;
            // U+0080..U+07FF
            else if (ch < 0x800) return result + 2;
            // U+0800..U+FFFF
            else return result + 3;
        }

        static value_type high(value_type result, uint32_t)
        {
            // U+10000..U+10FFFF
            return result + 4;
        }
    };


    struct utf8_writer
    {
        typedef uint8_t* value_type;

        static value_type low(value_type result, uint32_t ch)
        {
            // U+0000..U+007F
            if (ch < 0x80)
            {
                *result = static_cast<uint8_t>(ch);
                return result + 1;
            }
            // U+0080..U+07FF
            else if (ch < 0x800)
            {
                result[0] = static_cast<uint8_t>(0xC0 | (ch >> 6));
                result[1] = static_cast<uint8_t>(0x80 | (ch & 0x3F));
                return result + 2;
            }
            // U+0800..U+FFFF
            else
            {
                result[0] = static_cast<uint8_t>(0xE0 | (ch >> 12));
                result[1] = static_cast<uint8_t>(0x80 | ((ch >> 6) & 0x3F));
                result[2] = static_cast<uint8_t>(0x80 | (ch & 0x3F));
                return result + 3;
            }
        }

        static value_type high(value_type result, uint32_t ch)
        {
            // U+10000..U+10FFFF
            result[0] = static_cast<uint8_t>(0xF0 | (ch >> 18));
            result[1] = static_cast<uint8_t>(0x80 | ((ch >> 12) & 0x3F));
            result[2] = static_cast<uint8_t>(0x80 | ((ch >> 6) & 0x3F));
            result[3] = static_cast<uint8_t>(0x80 | (ch & 0x3F));
            return result + 4;
        }

        static value_type any(value_type result, uint32_t ch)
        {
            return (ch < 0x10000) ? low(result, ch) : high(result, ch);
        }
    };



PUGI__NS_END

PUGI__NS_BEGIN

    enum chartype_t
    {
        ct_parse_pcdata = 1,    // \0, &, \r, <
        ct_parse_attr = 2,      // \0, &, \r, ', "
        ct_parse_attr_ws = 4,   // \0, &, \r, ', ", \n, tab
        ct_space = 8,           // \r, \n, space, tab
        ct_parse_cdata = 16,    // \0, ], >, \r
        ct_parse_comment = 32,  // \0, -, >, \r
        ct_symbol = 64,         // Any symbol > 127, a-z, A-Z, 0-9, _, :, -, .
        ct_start_symbol = 128   // Any symbol > 127, a-z, A-Z, _, :
    };

    static const unsigned char chartype_table[256] =
    {
        55,  0,   0,   0,   0,   0,   0,   0,      0,   12,  12,  0,   0,   63,  0,   0,   // 0-15
        0,   0,   0,   0,   0,   0,   0,   0,      0,   0,   0,   0,   0,   0,   0,   0,   // 16-31
        8,   0,   6,   0,   0,   0,   7,   6,      0,   0,   0,   0,   0,   96,  64,  0,   // 32-47
        64,  64,  64,  64,  64,  64,  64,  64,     64,  64,  192, 0,   1,   0,   48,  0,   // 48-63
        0,   192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192, // 64-79
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 0,   0,   16,  0,   192, // 80-95
        0,   192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192, // 96-111
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 0, 0, 0, 0, 0,           // 112-127

        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192, // 128+
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
        192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192
    };

    enum chartypex_t
    {
        ctx_special_pcdata = 1,   // Any symbol >= 0 and < 32 (except \t, \r, \n), &, <, >
        ctx_special_attr = 2,     // Any symbol >= 0 and < 32, &, <, ", '
        ctx_start_symbol = 4,      // Any symbol > 127, a-z, A-Z, _
        ctx_digit = 8,              // 0-9
        ctx_symbol = 16              // Any symbol > 127, a-z, A-Z, 0-9, _, -, .
    };

    static const unsigned char chartypex_table[256] =
    {
        3,  3,  3,  3,  3,  3,  3,  3,     3,  2,  2,  3,  3,  2,  3,  3,     // 0-15
        3,  3,  3,  3,  3,  3,  3,  3,     3,  3,  3,  3,  3,  3,  3,  3,     // 16-31
        0,  0,  2,  0,  0,  0,  3,  2,     0,  0,  0,  0,  0, 16, 16,  0,     // 32-47
        24, 24, 24, 24, 24, 24, 24, 24,    24, 24, 0,  0,  3,  0,  1,  0,     // 48-63

        0,  20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,    // 64-79
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 0,  0,  0,  0,  20,    // 80-95
        0,  20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,    // 96-111
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 0,  0,  0,  0,  0,     // 112-127

        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,    // 128+
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20
    };

    #define PUGI__IS_CHARTYPE_IMPL(c, ct, table) (table[static_cast<unsigned char>(c)] & (ct))

    #define PUGI__IS_CHARTYPE(c, ct) PUGI__IS_CHARTYPE_IMPL(c, ct, chartype_table)
    #define PUGI__IS_CHARTYPEX(c, ct) PUGI__IS_CHARTYPE_IMPL(c, ct, chartypex_table)



PUGI__FN bool get_mutable_buffer(char*& out_buffer, size_t& out_length, const void* contents, size_t size, bool is_mutable) noexcept
{
    size_t length = size / sizeof(char);

    if (is_mutable)
    {
        out_buffer = static_cast<char*>(const_cast<void*>(contents));
        out_length = length;
    }
    else
    {
        char* buffer = static_cast<char*>(::malloc((length + 1) * sizeof(char)));
        if (!buffer) return false;

        if (contents) {
            memcpy(buffer, contents, length * sizeof(char));
        } else {
            assert(length == 0);
        }

        buffer[length] = 0;

        out_buffer = buffer;
        out_length = length + 1;
    }

    return true;
}



PUGI__FN bool convert_buffer(char*& out_buffer, size_t& out_length, xml_encoding encoding, const void* contents, size_t size, bool is_mutable)
{
    return get_mutable_buffer(out_buffer, out_length, contents, size, is_mutable);
}



template <typename Header>
inline bool strcpy_insitu_allow(size_t length, const Header& header, uintptr_t header_mask, char* target)
{
    // never reuse shared memory
    if (header & xml_memory_page_contents_shared_mask) return false;

    size_t target_length = ::strlen(target);

    // always reuse document buffer memory if possible
    if ((header & header_mask) == 0) return target_length >= length;

    // reuse heap memory if waste is not too great
    const size_t reuse_threshold = 32;

    return target_length >= length && (target_length < reuse_threshold || target_length - length < target_length / 2);
}


    template <typename String, typename Header>
    PUGI__FN bool strcpy_insitu(String& dest, Header& header, uintptr_t header_mask, const char* source, size_t source_length)
    {
        if (source_length == 0)
        {
            // empty string and null pointer are equivalent, so just deallocate old memory
            xml_allocator* alloc = PUGI__GETPAGE_IMPL(header)->allocator;

            if (header & header_mask) alloc->deallocate_string(dest);

            // mark the string as not allocated
            dest = 0;
            header &= ~header_mask;

            return true;
        }
        else if (dest && strcpy_insitu_allow(source_length, header, header_mask, dest))
        {
            // we can reuse old buffer, so just copy the new data (including zero terminator)
            memcpy(dest, source, source_length * sizeof(char));
            dest[source_length] = 0;

            return true;
        }
        else
        {
            xml_allocator* alloc = PUGI__GETPAGE_IMPL(header)->allocator;

            if (!alloc->reserve()) return false;

            // allocate new buffer
            char* buf = alloc->allocate_string(source_length + 1);
            if (!buf) return false;

            // copy the string (including zero terminator)
            memcpy(buf, source, source_length * sizeof(char));
            buf[source_length] = 0;

            // deallocate old buffer (*after* the above to protect against overlapping memory and/or allocation failures)
            if (header & header_mask) alloc->deallocate_string(dest);

            // the string is now allocated, so set the flag
            dest = buf;
            header |= header_mask;

            return true;
        }
    }

    struct gap
    {
        char* end;
        size_t size;

        gap(): end(0), size(0)
        {
        }

        // Push new gap, move s count bytes further (skipping the gap).
        // Collapse previous gap.
        void push(char*& s, size_t count)
        {
            if (end) // there was a gap already; collapse it
            {
                // Move [old_gap_end, new_gap_start) to [old_gap_start, ...)
                assert(s >= end);
                memmove(end - size, end, reinterpret_cast<char*>(s) - reinterpret_cast<char*>(end));
            }

            s += count; // end of current gap

            // "merge" two gaps
            end = s;
            size += count;
        }

        // Collapse all gaps, return past-the-end pointer
        char* flush(char* s)
        {
            if (end)
            {
                // Move [old_gap_end, current_pos) to [old_gap_start, ...)
                assert(s >= end);
                memmove(end - size, end, reinterpret_cast<char*>(s) - reinterpret_cast<char*>(end));

                return s - size;
            }
            else return s;
        }
    };

    PUGI__FN char* strconv_escape(char* s, gap& g)
    {
        char* stre = s + 1;

        switch (*stre)
        {
            case '#':    // &#...
            {
                uint ucsc = 0;

                if (stre[1] == 'x') // &#x... (hex code)
                {
                    stre += 2;

                    char ch = *stre;

                    if (ch == ';') return stre;

                    for (;;)
                    {
                        if (static_cast<uint>(ch - '0') <= 9)
                            ucsc = 16 * ucsc + (ch - '0');
                        else if (static_cast<uint>((ch | ' ') - 'a') <= 5)
                            ucsc = 16 * ucsc + ((ch | ' ') - 'a' + 10);
                        else if (ch == ';')
                            break;
                        else // cancel
                            return stre;

                        ch = *++stre;
                    }

                    ++stre;
                }
                else    // &#... (dec code)
                {
                    char ch = *++stre;

                    if (ch == ';') return stre;

                    for (;;)
                    {
                        if (static_cast<uint>(ch - '0') <= 9)
                            ucsc = 10 * ucsc + (ch - '0');
                        else if (ch == ';')
                            break;
                        else // cancel
                            return stre;

                        ch = *++stre;
                    }

                    ++stre;
                }

                s = reinterpret_cast<char*>(utf8_writer::any(reinterpret_cast<uint8_t*>(s), ucsc));

                g.push(s, stre - s);
                return stre;
            }

            case 'a':    // &a
            {
                ++stre;

                if (*stre == 'm') // &am
                {
                    if (*++stre == 'p' && *++stre == ';') // &amp;
                    {
                        *s++ = '&';
                        ++stre;

                        g.push(s, stre - s);
                        return stre;
                    }
                }
                else if (*stre == 'p') // &ap
                {
                    if (*++stre == 'o' && *++stre == 's' && *++stre == ';') // &apos;
                    {
                        *s++ = '\'';
                        ++stre;

                        g.push(s, stre - s);
                        return stre;
                    }
                }
                break;
            }

            case 'g': // &g
            {
                if (*++stre == 't' && *++stre == ';') // &gt;
                {
                    *s++ = '>';
                    ++stre;

                    g.push(s, stre - s);
                    return stre;
                }
                break;
            }

            case 'l': // &l
            {
                if (*++stre == 't' && *++stre == ';') // &lt;
                {
                    *s++ = '<';
                    ++stre;

                    g.push(s, stre - s);
                    return stre;
                }
                break;
            }

            case 'q': // &q
            {
                if (*++stre == 'u' && *++stre == 'o' && *++stre == 't' && *++stre == ';') // &quot;
                {
                    *s++ = '"';
                    ++stre;

                    g.push(s, stre - s);
                    return stre;
                }
                break;
            }

            default:
                break;
        }

        return stre;
    }

    // Parser utilities
    #define PUGI__ENDSWITH(c, e)        ((c) == (e) || ((c) == 0 && endch == (e)))
    #define PUGI__SKIPWS()              { while (PUGI__IS_CHARTYPE(*s, ct_space)) ++s; }
    #define PUGI__OPTSET(OPT)           ( optmsk & (OPT) )
    #define PUGI__PUSHNODE(TYPE)        { cursor = append_new_node(cursor, *alloc, TYPE); if (!cursor) PUGI__THROW_ERROR(status_out_of_memory, s); }
    #define PUGI__POPNODE()             { cursor = cursor->parent; }
    #define PUGI__SCANFOR(X)            { while (*s != 0 && !(X)) ++s; }
    #define PUGI__SCANWHILE(X)          { while (X) ++s; }
    #define PUGI__SCANWHILE_UNROLL(X)   { for (;;) { char ss = s[0]; if (PUGI__UNLIKELY(!(X))) { break; } ss = s[1]; if (PUGI__UNLIKELY(!(X))) { s += 1; break; } ss = s[2]; if (PUGI__UNLIKELY(!(X))) { s += 2; break; } ss = s[3]; if (PUGI__UNLIKELY(!(X))) { s += 3; break; } s += 4; } }
    #define PUGI__ENDSEG()              { ch = *s; *s = 0; ++s; }
    #define PUGI__THROW_ERROR(err, m)   return error_offset = m, error_status = err, static_cast<char*>(0)
    #define PUGI__CHECK_ERROR(err, m)   { if (*s == 0) PUGI__THROW_ERROR(err, m); }

    PUGI__FN char* strconv_comment(char* s, char endch)
    {
        gap g;

        while (true)
        {
            PUGI__SCANWHILE_UNROLL(!PUGI__IS_CHARTYPE(ss, ct_parse_comment));

            if (*s == '\r') // Either a single 0x0d or 0x0d 0x0a pair
            {
                *s++ = '\n'; // replace first one with 0x0a

                if (*s == '\n') g.push(s, 1);
            }
            else if (s[0] == '-' && s[1] == '-' && PUGI__ENDSWITH(s[2], '>')) // comment ends here
            {
                *g.flush(s) = 0;

                return s + (s[2] == '>' ? 3 : 2);
            }
            else if (*s == 0)
            {
                return 0;
            }
            else ++s;
        }
    }

    PUGI__FN char* strconv_cdata(char* s, char endch)
    {
        gap g;

        while (true)
        {
            PUGI__SCANWHILE_UNROLL(!PUGI__IS_CHARTYPE(ss, ct_parse_cdata));

            if (*s == '\r') // Either a single 0x0d or 0x0d 0x0a pair
            {
                *s++ = '\n'; // replace first one with 0x0a

                if (*s == '\n') g.push(s, 1);
            }
            else if (s[0] == ']' && s[1] == ']' && PUGI__ENDSWITH(s[2], '>')) // CDATA ends here
            {
                *g.flush(s) = 0;

                return s + 1;
            }
            else if (*s == 0)
            {
                return 0;
            }
            else ++s;
        }
    }

    typedef char* (*strconv_pcdata_t)(char*);

    template <typename opt_trim, typename opt_eol, typename opt_escape> struct strconv_pcdata_impl
    {
        static char* parse(char* s)
        {
            gap g;

            char* begin = s;

            while (true)
            {
                PUGI__SCANWHILE_UNROLL(!PUGI__IS_CHARTYPE(ss, ct_parse_pcdata));

                if (*s == '<') // PCDATA ends here
                {
                    char* end = g.flush(s);

                    if (opt_trim::value)
                        while (end > begin && PUGI__IS_CHARTYPE(end[-1], ct_space))
                            --end;

                    *end = 0;

                    return s + 1;
                }
                else if (opt_eol::value && *s == '\r') // Either a single 0x0d or 0x0d 0x0a pair
                {
                    *s++ = '\n'; // replace first one with 0x0a

                    if (*s == '\n') g.push(s, 1);
                }
                else if (opt_escape::value && *s == '&')
                {
                    s = strconv_escape(s, g);
                }
                else if (*s == 0)
                {
                    char* end = g.flush(s);

                    if (opt_trim::value)
                        while (end > begin && PUGI__IS_CHARTYPE(end[-1], ct_space))
                            --end;

                    *end = 0;

                    return s;
                }
                else ++s;
            }
        }
    };

    PUGI__FN strconv_pcdata_t get_strconv_pcdata(uint optmask)
    {
        static_assert(parse_escapes == 0x10 && parse_eol == 0x20 && parse_trim_pcdata == 0x0800);

        switch (((optmask >> 4) & 3) | ((optmask >> 9) & 4)) // get bitmask for flags (trim eol escapes); this simultaneously checks 3 options from assertion above
        {
        case 0: return strconv_pcdata_impl<opt_false, opt_false, opt_false>::parse;
        case 1: return strconv_pcdata_impl<opt_false, opt_false, opt_true>::parse;
        case 2: return strconv_pcdata_impl<opt_false, opt_true, opt_false>::parse;
        case 3: return strconv_pcdata_impl<opt_false, opt_true, opt_true>::parse;
        case 4: return strconv_pcdata_impl<opt_true, opt_false, opt_false>::parse;
        case 5: return strconv_pcdata_impl<opt_true, opt_false, opt_true>::parse;
        case 6: return strconv_pcdata_impl<opt_true, opt_true, opt_false>::parse;
        case 7: return strconv_pcdata_impl<opt_true, opt_true, opt_true>::parse;
        default: assert(false); return 0; // unreachable
        }
    }

    typedef char* (*strconv_attribute_t)(char*, char);

    template <typename opt_escape> struct strconv_attribute_impl
    {
        static char* parse_wnorm(char* s, char end_quote)
        {
            gap g;

            // trim leading whitespaces
            if (PUGI__IS_CHARTYPE(*s, ct_space))
            {
                char* str = s;

                do ++str;
                while (PUGI__IS_CHARTYPE(*str, ct_space));

                g.push(s, str - s);
            }

            while (true)
            {
                PUGI__SCANWHILE_UNROLL(!PUGI__IS_CHARTYPE(ss, ct_parse_attr_ws | ct_space));

                if (*s == end_quote)
                {
                    char* str = g.flush(s);

                    do *str-- = 0;
                    while (PUGI__IS_CHARTYPE(*str, ct_space));

                    return s + 1;
                }
                else if (PUGI__IS_CHARTYPE(*s, ct_space))
                {
                    *s++ = ' ';

                    if (PUGI__IS_CHARTYPE(*s, ct_space))
                    {
                        char* str = s + 1;
                        while (PUGI__IS_CHARTYPE(*str, ct_space)) ++str;

                        g.push(s, str - s);
                    }
                }
                else if (opt_escape::value && *s == '&')
                {
                    s = strconv_escape(s, g);
                }
                else if (!*s)
                {
                    return 0;
                }
                else ++s;
            }
        }

        static char* parse_wconv(char* s, char end_quote)
        {
            gap g;

            while (true)
            {
                PUGI__SCANWHILE_UNROLL(!PUGI__IS_CHARTYPE(ss, ct_parse_attr_ws));

                if (*s == end_quote)
                {
                    *g.flush(s) = 0;

                    return s + 1;
                }
                else if (PUGI__IS_CHARTYPE(*s, ct_space))
                {
                    if (*s == '\r')
                    {
                        *s++ = ' ';

                        if (*s == '\n') g.push(s, 1);
                    }
                    else *s++ = ' ';
                }
                else if (opt_escape::value && *s == '&')
                {
                    s = strconv_escape(s, g);
                }
                else if (!*s)
                {
                    return 0;
                }
                else ++s;
            }
        }

        static char* parse_eol(char* s, char end_quote)
        {
            gap g;

            while (true)
            {
                PUGI__SCANWHILE_UNROLL(!PUGI__IS_CHARTYPE(ss, ct_parse_attr));

                if (*s == end_quote)
                {
                    *g.flush(s) = 0;

                    return s + 1;
                }
                else if (*s == '\r')
                {
                    *s++ = '\n';

                    if (*s == '\n') g.push(s, 1);
                }
                else if (opt_escape::value && *s == '&')
                {
                    s = strconv_escape(s, g);
                }
                else if (!*s)
                {
                    return 0;
                }
                else ++s;
            }
        }

        static char* parse_simple(char* s, char end_quote)
        {
            gap g;

            while (true)
            {
                PUGI__SCANWHILE_UNROLL(!PUGI__IS_CHARTYPE(ss, ct_parse_attr));

                if (*s == end_quote)
                {
                    *g.flush(s) = 0;

                    return s + 1;
                }
                else if (opt_escape::value && *s == '&')
                {
                    s = strconv_escape(s, g);
                }
                else if (!*s)
                {
                    return 0;
                }
                else ++s;
            }
        }
    };

    PUGI__FN strconv_attribute_t get_strconv_attribute(uint optmask)
    {
        static_assert(parse_escapes == 0x10 && parse_eol == 0x20 && parse_wconv_attribute == 0x40 && parse_wnorm_attribute == 0x80);

        switch ((optmask >> 4) & 15) // get bitmask for flags (wnorm wconv eol escapes); this simultaneously checks 4 options from assertion above
        {
        case 0:  return strconv_attribute_impl<opt_false>::parse_simple;
        case 1:  return strconv_attribute_impl<opt_true>::parse_simple;
        case 2:  return strconv_attribute_impl<opt_false>::parse_eol;
        case 3:  return strconv_attribute_impl<opt_true>::parse_eol;
        case 4:  return strconv_attribute_impl<opt_false>::parse_wconv;
        case 5:  return strconv_attribute_impl<opt_true>::parse_wconv;
        case 6:  return strconv_attribute_impl<opt_false>::parse_wconv;
        case 7:  return strconv_attribute_impl<opt_true>::parse_wconv;
        case 8:  return strconv_attribute_impl<opt_false>::parse_wnorm;
        case 9:  return strconv_attribute_impl<opt_true>::parse_wnorm;
        case 10: return strconv_attribute_impl<opt_false>::parse_wnorm;
        case 11: return strconv_attribute_impl<opt_true>::parse_wnorm;
        case 12: return strconv_attribute_impl<opt_false>::parse_wnorm;
        case 13: return strconv_attribute_impl<opt_true>::parse_wnorm;
        case 14: return strconv_attribute_impl<opt_false>::parse_wnorm;
        case 15: return strconv_attribute_impl<opt_true>::parse_wnorm;
        default: assert(false); return 0; // unreachable
        }
    }

    inline xml_parse_result make_parse_result(xml_parse_status status, ptrdiff_t offset = 0) noexcept
    {
        xml_parse_result result;
        result.status = status;
        result.offset = offset;

        return result;
    }

    struct xml_parser
    {
        xml_allocator* alloc = nullptr;
        char* error_offset = nullptr;
        xml_parse_status error_status;

        xml_parser(xml_allocator* alloc_) noexcept
            : alloc(alloc_), error_offset(0), error_status(status_ok)
        {
        }

        // DOCTYPE consists of nested sections of the following possible types:
        // <!-- ... -->, <? ... ?>, "...", '...'
        // <![...]]>
        // <!...>
        // First group can not contain nested groups
        // Second group can contain nested groups of the same type
        // Third group can contain all other groups
        char* parse_doctype_primitive(char* s) noexcept
        {
            if (*s == '"' || *s == '\'')
            {
                // quoted string
                char ch = *s++;
                PUGI__SCANFOR(*s == ch);
                if (!*s) PUGI__THROW_ERROR(status_bad_doctype, s);

                s++;
            }
            else if (s[0] == '<' && s[1] == '?')
            {
                // <? ... ?>
                s += 2;
                PUGI__SCANFOR(s[0] == '?' && s[1] == '>'); // no need for ENDSWITH because ?> can't terminate proper doctype
                if (!*s) PUGI__THROW_ERROR(status_bad_doctype, s);

                s += 2;
            }
            else if (s[0] == '<' && s[1] == '!' && s[2] == '-' && s[3] == '-')
            {
                s += 4;
                PUGI__SCANFOR(s[0] == '-' && s[1] == '-' && s[2] == '>'); // no need for ENDSWITH because --> can't terminate proper doctype
                if (!*s) PUGI__THROW_ERROR(status_bad_doctype, s);

                s += 3;
            }
            else PUGI__THROW_ERROR(status_bad_doctype, s);

            return s;
        }

        char* parse_doctype_ignore(char* s) noexcept
        {
            size_t depth = 0;

            assert(s[0] == '<' && s[1] == '!' && s[2] == '[');
            s += 3;

            while (*s)
            {
                if (s[0] == '<' && s[1] == '!' && s[2] == '[')
                {
                    // nested ignore section
                    s += 3;
                    depth++;
                }
                else if (s[0] == ']' && s[1] == ']' && s[2] == '>')
                {
                    // ignore section end
                    s += 3;

                    if (depth == 0)
                        return s;

                    depth--;
                }
                else s++;
            }

            PUGI__THROW_ERROR(status_bad_doctype, s);
        }

        char* parse_doctype_group(char* s, char endch)
        {
            size_t depth = 0;

            assert((s[0] == '<' || s[0] == 0) && s[1] == '!');
            s += 2;

            while (*s)
            {
                if (s[0] == '<' && s[1] == '!' && s[2] != '-')
                {
                    if (s[2] == '[')
                    {
                        // ignore
                        s = parse_doctype_ignore(s);
                        if (!s) return s;
                    }
                    else
                    {
                        // some control group
                        s += 2;
                        depth++;
                    }
                }
                else if (s[0] == '<' || s[0] == '"' || s[0] == '\'')
                {
                    // unknown tag (forbidden), or some primitive group
                    s = parse_doctype_primitive(s);
                    if (!s) return s;
                }
                else if (*s == '>')
                {
                    if (depth == 0)
                        return s;

                    depth--;
                    s++;
                }
                else s++;
            }

            if (depth != 0 || endch != '>') PUGI__THROW_ERROR(status_bad_doctype, s);

            return s;
        }

        char* parse_exclamation(char* s, xml_node_struct* cursor, uint optmsk, char endch)
        {
            // parse node contents, starting with exclamation mark
            ++s;

            if (*s == '-') // '<!-...'
            {
                ++s;

                if (*s == '-') // '<!--...'
                {
                    ++s;

                    if (PUGI__OPTSET(parse_comments))
                    {
                        PUGI__PUSHNODE(node_comment); // Append a new node on the tree.
                        cursor->value = s; // Save the offset.
                    }

                    if (PUGI__OPTSET(parse_eol) && PUGI__OPTSET(parse_comments))
                    {
                        s = strconv_comment(s, endch);

                        if (!s) PUGI__THROW_ERROR(status_bad_comment, cursor->value);
                    }
                    else
                    {
                        // Scan for terminating '-->'.
                        PUGI__SCANFOR(s[0] == '-' && s[1] == '-' && PUGI__ENDSWITH(s[2], '>'));
                        PUGI__CHECK_ERROR(status_bad_comment, s);

                        if (PUGI__OPTSET(parse_comments))
                            *s = 0; // Zero-terminate this segment at the first terminating '-'.

                        s += (s[2] == '>' ? 3 : 2); // Step over the '\0->'.
                    }
                }
                else PUGI__THROW_ERROR(status_bad_comment, s);
            }
            else if (*s == '[')
            {
                // '<![CDATA[...'
                if (*++s=='C' && *++s=='D' && *++s=='A' && *++s=='T' && *++s=='A' && *++s == '[')
                {
                    ++s;

                    if (PUGI__OPTSET(parse_cdata))
                    {
                        PUGI__PUSHNODE(node_cdata); // Append a new node on the tree.
                        cursor->value = s; // Save the offset.

                        if (PUGI__OPTSET(parse_eol))
                        {
                            s = strconv_cdata(s, endch);

                            if (!s) PUGI__THROW_ERROR(status_bad_cdata, cursor->value);
                        }
                        else
                        {
                            // Scan for terminating ']]>'.
                            PUGI__SCANFOR(s[0] == ']' && s[1] == ']' && PUGI__ENDSWITH(s[2], '>'));
                            PUGI__CHECK_ERROR(status_bad_cdata, s);

                            *s++ = 0; // Zero-terminate this segment.
                        }
                    }
                    else // Flagged for discard, but we still have to scan for the terminator.
                    {
                        // Scan for terminating ']]>'.
                        PUGI__SCANFOR(s[0] == ']' && s[1] == ']' && PUGI__ENDSWITH(s[2], '>'));
                        PUGI__CHECK_ERROR(status_bad_cdata, s);

                        ++s;
                    }

                    s += (s[1] == '>' ? 2 : 1); // Step over the last ']>'.
                }
                else PUGI__THROW_ERROR(status_bad_cdata, s);
            }
            else if (s[0] == 'D' && s[1] == 'O' && s[2] == 'C' && s[3] == 'T' && s[4] == 'Y' && s[5] == 'P' && PUGI__ENDSWITH(s[6], 'E'))
            {
                s -= 2;

                if (cursor->parent) PUGI__THROW_ERROR(status_bad_doctype, s);

                char* mark = s + 9;

                s = parse_doctype_group(s, endch);
                if (!s) return s;

                assert((*s == 0 && endch == '>') || *s == '>');
                if (*s) *s++ = 0;

                if (PUGI__OPTSET(parse_doctype))
                {
                    while (PUGI__IS_CHARTYPE(*mark, ct_space)) ++mark;

                    PUGI__PUSHNODE(node_doctype);

                    cursor->value = mark;
                }
            }
            else if (*s == 0 && endch == '-') PUGI__THROW_ERROR(status_bad_comment, s);
            else if (*s == 0 && endch == '[') PUGI__THROW_ERROR(status_bad_cdata, s);
            else PUGI__THROW_ERROR(status_unrecognized_tag, s);

            return s;
        }

        char* parse_question(char* s, xml_node_struct*& ref_cursor, uint optmsk, char endch) noexcept
        {
            // load into registers
            xml_node_struct* cursor = ref_cursor;
            char ch = 0;

            // parse node contents, starting with question mark
            ++s;

            // read PI target
            char* target = s;

            if (!PUGI__IS_CHARTYPE(*s, ct_start_symbol)) PUGI__THROW_ERROR(status_bad_pi, s);

            PUGI__SCANWHILE(PUGI__IS_CHARTYPE(*s, ct_symbol));
            PUGI__CHECK_ERROR(status_bad_pi, s);

            // determine node type; stricmp / strcasecmp is not portable
            bool declaration = (target[0] | ' ') == 'x' && (target[1] | ' ') == 'm' && (target[2] | ' ') == 'l' && target + 3 == s;

            if (declaration ? PUGI__OPTSET(parse_declaration) : PUGI__OPTSET(parse_pi))
            {
                if (declaration)
                {
                    // disallow non top-level declarations
                    if (cursor->parent) PUGI__THROW_ERROR(status_bad_pi, s);

                    PUGI__PUSHNODE(node_declaration);
                }
                else
                {
                    PUGI__PUSHNODE(node_pi);
                }

                cursor->name = target;

                PUGI__ENDSEG();

                // parse value/attributes
                if (ch == '?')
                {
                    // empty node
                    if (!PUGI__ENDSWITH(*s, '>')) PUGI__THROW_ERROR(status_bad_pi, s);
                    s += (*s == '>');

                    PUGI__POPNODE();
                }
                else if (PUGI__IS_CHARTYPE(ch, ct_space))
                {
                    PUGI__SKIPWS();

                    // scan for tag end
                    char* value = s;

                    PUGI__SCANFOR(s[0] == '?' && PUGI__ENDSWITH(s[1], '>'));
                    PUGI__CHECK_ERROR(status_bad_pi, s);

                    if (declaration)
                    {
                        // replace ending ? with / so that 'element' terminates properly
                        *s = '/';

                        // we exit from this function with cursor at node_declaration, which is a signal to parse() to go to LOC_ATTRIBUTES
                        s = value;
                    }
                    else
                    {
                        // store value and step over >
                        cursor->value = value;

                        PUGI__POPNODE();

                        PUGI__ENDSEG();

                        s += (*s == '>');
                    }
                }
                else PUGI__THROW_ERROR(status_bad_pi, s);
            }
            else
            {
                // scan for tag end
                PUGI__SCANFOR(s[0] == '?' && PUGI__ENDSWITH(s[1], '>'));
                PUGI__CHECK_ERROR(status_bad_pi, s);

                s += (s[1] == '>' ? 2 : 1);
            }

            // store from registers
            ref_cursor = cursor;

            return s;
        }

        char* parse_tree(char* s, xml_node_struct* root, uint optmsk, char endch)
        {
            strconv_attribute_t strconv_attribute = get_strconv_attribute(optmsk);
            strconv_pcdata_t strconv_pcdata = get_strconv_pcdata(optmsk);

            char ch = 0;
            xml_node_struct* cursor = root;
            char* mark = s;

            while (*s != 0)
            {
                if (*s == '<')
                {
                    ++s;

                LOC_TAG:
                    if (PUGI__IS_CHARTYPE(*s, ct_start_symbol)) // '<#...'
                    {
                        PUGI__PUSHNODE(node_element); // Append a new node to the tree.

                        cursor->name = s;

                        PUGI__SCANWHILE_UNROLL(PUGI__IS_CHARTYPE(ss, ct_symbol)); // Scan for a terminator.
                        PUGI__ENDSEG(); // Save char in 'ch', terminate & step over.

                        if (ch == '>')
                        {
                            // end of tag
                        }
                        else if (PUGI__IS_CHARTYPE(ch, ct_space))
                        {
                        LOC_ATTRIBUTES:
                            while (true)
                            {
                                PUGI__SKIPWS(); // Eat any whitespace.

                                if (PUGI__IS_CHARTYPE(*s, ct_start_symbol)) // <... #...
                                {
                                    xml_attribute_struct* a = append_new_attribute(cursor, *alloc); // Make space for this attribute.
                                    if (!a) PUGI__THROW_ERROR(status_out_of_memory, s);

                                    a->name = s; // Save the offset.

                                    PUGI__SCANWHILE_UNROLL(PUGI__IS_CHARTYPE(ss, ct_symbol)); // Scan for a terminator.
                                    PUGI__ENDSEG(); // Save char in 'ch', terminate & step over.

                                    if (PUGI__IS_CHARTYPE(ch, ct_space))
                                    {
                                        PUGI__SKIPWS(); // Eat any whitespace.

                                        ch = *s;
                                        ++s;
                                    }

                                    if (ch == '=') // '<... #=...'
                                    {
                                        PUGI__SKIPWS(); // Eat any whitespace.

                                        if (*s == '"' || *s == '\'') // '<... #="...'
                                        {
                                            ch = *s; // Save quote char to avoid breaking on "''" -or- '""'.
                                            ++s; // Step over the quote.
                                            a->value = s; // Save the offset.

                                            s = strconv_attribute(s, ch);

                                            if (!s) PUGI__THROW_ERROR(status_bad_attribute, a->value);

                                            // After this line the loop continues from the start;
                                            // Whitespaces, / and > are ok, symbols and EOF are wrong,
                                            // everything else will be detected
                                            if (PUGI__IS_CHARTYPE(*s, ct_start_symbol)) PUGI__THROW_ERROR(status_bad_attribute, s);
                                        }
                                        else PUGI__THROW_ERROR(status_bad_attribute, s);
                                    }
                                    else PUGI__THROW_ERROR(status_bad_attribute, s);
                                }
                                else if (*s == '/')
                                {
                                    ++s;

                                    if (*s == '>')
                                    {
                                        PUGI__POPNODE();
                                        s++;
                                        break;
                                    }
                                    else if (*s == 0 && endch == '>')
                                    {
                                        PUGI__POPNODE();
                                        break;
                                    }
                                    else PUGI__THROW_ERROR(status_bad_start_element, s);
                                }
                                else if (*s == '>')
                                {
                                    ++s;

                                    break;
                                }
                                else if (*s == 0 && endch == '>')
                                {
                                    break;
                                }
                                else PUGI__THROW_ERROR(status_bad_start_element, s);
                            }

                            // !!!
                        }
                        else if (ch == '/') // '<#.../'
                        {
                            if (!PUGI__ENDSWITH(*s, '>')) PUGI__THROW_ERROR(status_bad_start_element, s);

                            PUGI__POPNODE(); // Pop.

                            s += (*s == '>');
                        }
                        else if (ch == 0)
                        {
                            // we stepped over null terminator, backtrack & handle closing tag
                            --s;

                            if (endch != '>') PUGI__THROW_ERROR(status_bad_start_element, s);
                        }
                        else PUGI__THROW_ERROR(status_bad_start_element, s);
                    }
                    else if (*s == '/')
                    {
                        ++s;

                        mark = s;

                        char* name = cursor->name;
                        if (!name) PUGI__THROW_ERROR(status_end_element_mismatch, mark);

                        while (PUGI__IS_CHARTYPE(*s, ct_symbol))
                        {
                            if (*s++ != *name++) PUGI__THROW_ERROR(status_end_element_mismatch, mark);
                        }

                        if (*name)
                        {
                            if (*s == 0 && name[0] == endch && name[1] == 0) PUGI__THROW_ERROR(status_bad_end_element, s);
                            else PUGI__THROW_ERROR(status_end_element_mismatch, mark);
                        }

                        PUGI__POPNODE(); // Pop.

                        PUGI__SKIPWS();

                        if (*s == 0)
                        {
                            if (endch != '>') PUGI__THROW_ERROR(status_bad_end_element, s);
                        }
                        else
                        {
                            if (*s != '>') PUGI__THROW_ERROR(status_bad_end_element, s);
                            ++s;
                        }
                    }
                    else if (*s == '?') // '<?...'
                    {
                        s = parse_question(s, cursor, optmsk, endch);
                        if (!s) return s;

                        assert(cursor);
                        if (PUGI__NODETYPE(cursor) == node_declaration) goto LOC_ATTRIBUTES;
                    }
                    else if (*s == '!') // '<!...'
                    {
                        s = parse_exclamation(s, cursor, optmsk, endch);
                        if (!s) return s;
                    }
                    else if (*s == 0 && endch == '?') PUGI__THROW_ERROR(status_bad_pi, s);
                    else PUGI__THROW_ERROR(status_unrecognized_tag, s);
                }
                else
                {
                    mark = s; // Save this offset while searching for a terminator.

                    PUGI__SKIPWS(); // Eat whitespace if no genuine PCDATA here.

                    if (*s == '<' || !*s)
                    {
                        // We skipped some whitespace characters because otherwise we would take the tag branch instead of PCDATA one
                        assert(mark != s);

                        if (!PUGI__OPTSET(parse_ws_pcdata | parse_ws_pcdata_single) || PUGI__OPTSET(parse_trim_pcdata))
                        {
                            continue;
                        }
                        else if (PUGI__OPTSET(parse_ws_pcdata_single))
                        {
                            if (s[0] != '<' || s[1] != '/' || cursor->first_child) continue;
                        }
                    }

                    if (!PUGI__OPTSET(parse_trim_pcdata))
                        s = mark;

                    if (cursor->parent || PUGI__OPTSET(parse_fragment))
                    {
                        if (PUGI__OPTSET(parse_embed_pcdata) && cursor->parent && !cursor->first_child && !cursor->value)
                        {
                            cursor->value = s; // Save the offset.
                        }
                        else
                        {
                            PUGI__PUSHNODE(node_pcdata); // Append a new node on the tree.

                            cursor->value = s; // Save the offset.

                            PUGI__POPNODE(); // Pop since this is a standalone.
                        }

                        s = strconv_pcdata(s);

                        if (!*s) break;
                    }
                    else
                    {
                        PUGI__SCANFOR(*s == '<'); // '...<'
                        if (!*s) break;

                        ++s;
                    }

                    // We're after '<'
                    goto LOC_TAG;
                }
            }

            // check that last tag is closed
            if (cursor != root) PUGI__THROW_ERROR(status_end_element_mismatch, s);

            return s;
        }

        static char* parse_skip_bom(char* s)
        {
            return (s[0] == '\xef' && s[1] == '\xbb' && s[2] == '\xbf') ? s + 3 : s;
        }

        static bool has_element_node_siblings(xml_node_struct* node)
        {
            while (node)
            {
                if (PUGI__NODETYPE(node) == node_element) return true;

                node = node->next_sibling;
            }

            return false;
        }

        static xml_parse_result parse(char* buffer, size_t length, xml_document_struct* xmldoc, xml_node_struct* root, uint optmsk)
        {
            // early-out for empty documents
            if (length == 0)
                return make_parse_result(PUGI__OPTSET(parse_fragment) ? status_ok : status_no_document_element);

            // get last child of the root before parsing
            xml_node_struct* last_root_child = root->first_child ? root->first_child->prev_sibling_c + 0 : 0;

            // create parser on stack
            xml_parser parser(static_cast<xml_allocator*>(xmldoc));

            // save last character and make buffer zero-terminated (speeds up parsing)
            char endch = buffer[length - 1];
            buffer[length - 1] = 0;

            // skip BOM to make sure it does not end up as part of parse output
            char* buffer_data = parse_skip_bom(buffer);

            // perform actual parsing
            parser.parse_tree(buffer_data, root, optmsk, endch);

            xml_parse_result result = make_parse_result(parser.error_status, parser.error_offset ? parser.error_offset - buffer : 0);
            assert(result.offset >= 0 && static_cast<size_t>(result.offset) <= length);

            if (result)
            {
                // since we removed last character, we have to handle the only possible false positive (stray <)
                if (endch == '<')
                    return make_parse_result(status_unrecognized_tag, length - 1);

                // check if there are any element nodes parsed
                xml_node_struct* first_root_child_parsed = last_root_child ? last_root_child->next_sibling + 0 : root->first_child+ 0;

                if (!PUGI__OPTSET(parse_fragment) && !has_element_node_siblings(first_root_child_parsed))
                    return make_parse_result(status_no_document_element, length - 1);
            }
            else
            {
                // roll back offset if it occurs on a null terminator in the source buffer
                if (result.offset > 0 && static_cast<size_t>(result.offset) == length - 1 && endch == 0)
                    result.offset--;
            }

            return result;
        }
    };

  template <typename D, typename T>
  size_t convert_buffer_output_generic(typename T::value_type dest, const char* data, size_t length, D, T) noexcept
  {
      static_assert(sizeof(char) == sizeof(typename D::type));

      typename T::value_type end = D::process(reinterpret_cast<const typename D::type*>(data), length, dest, T());

      return static_cast<size_t>(end - dest) * sizeof(*dest);
  }

  template <typename D, typename T>
  size_t convert_buffer_output_generic(typename T::value_type dest, const char* data, size_t length, D, T, bool opt_swap) noexcept
  {
      static_assert(sizeof(char) == sizeof(typename D::type));

      typename T::value_type end = D::process(reinterpret_cast<const typename D::type*>(data), length, dest, T());

      if (opt_swap)
      {
          for (typename T::value_type i = dest; i != end; ++i)
              *i = endian_swap(*i);
      }

      return static_cast<size_t>(end - dest) * sizeof(*dest);
  }


  enum indent_flags_t
  {
      indent_newline = 1,
      indent_indent = 2
  };


  PUGI__FN bool is_attribute_of(xml_attribute_struct* attr, xml_node_struct* node) noexcept
  {
      for (xml_attribute_struct* a = node->first_attribute; a; a = a->next_attribute)
          if (a == attr)
              return true;

      return false;
  }

  PUGI__FN bool allow_insert_attribute(xml_node_type parent) noexcept
  {
      return parent == node_element || parent == node_declaration;
  }

  PUGI__FN bool allow_insert_child(xml_node_type parent, xml_node_type child) noexcept
  {
      if (parent != node_document && parent != node_element) return false;
      if (child == node_document || child == node_null) return false;
      if (parent != node_document && (child == node_declaration || child == node_doctype)) return false;

      return true;
  }

  PUGI__FN bool allow_move(xml_node parent, xml_node child) noexcept
  {
      // check that child can be a child of parent
      if (!allow_insert_child(parent.type(), child.type()))
          return false;

      // check that node is not moved between documents
      if (parent.root() != child.root())
          return false;

      // check that new parent is not in the child subtree
      xml_node cur = parent;

      while (cur)
      {
          if (cur == child)
              return false;

          cur = cur.parent();
      }

      return true;
  }

  template <typename String, typename Header>
  PUGI__FN void node_copy_string(String& dest, Header& header, uintptr_t header_mask,
                                 char* source, Header& source_header, xml_allocator* alloc) noexcept
  {
      assert(!dest && (header & header_mask) == 0);

      if (source)
      {
          if (alloc && (source_header & header_mask) == 0)
          {
              dest = source;

              // since strcpy_insitu can reuse document buffer memory we need to mark both source and dest as shared
              header |= xml_memory_page_contents_shared_mask;
              source_header |= xml_memory_page_contents_shared_mask;
          }
          else {
              strcpy_insitu(dest, header, header_mask, source, ::strlen(source));
          }
      }
  }

  PUGI__FN void node_copy_contents(xml_node_struct* dn, xml_node_struct* sn, xml_allocator* shared_alloc) noexcept
  {
      node_copy_string(dn->name, dn->header, xml_memory_page_name_allocated_mask, sn->name, sn->header, shared_alloc);
      node_copy_string(dn->value, dn->header, xml_memory_page_value_allocated_mask, sn->value, sn->header, shared_alloc);

      for (xml_attribute_struct* sa = sn->first_attribute; sa; sa = sa->next_attribute)
      {
          xml_attribute_struct* da = append_new_attribute(dn, get_allocator(dn));

          if (da)
          {
              node_copy_string(da->name, da->header, xml_memory_page_name_allocated_mask, sa->name, sa->header, shared_alloc);
              node_copy_string(da->value, da->header, xml_memory_page_value_allocated_mask, sa->value, sa->header, shared_alloc);
          }
      }
  }

  PUGI__FN void node_copy_tree(xml_node_struct* dn, xml_node_struct* sn) noexcept
  {
      xml_allocator& alloc = get_allocator(dn);
      xml_allocator* shared_alloc = (&alloc == &get_allocator(sn)) ? &alloc : 0;

      node_copy_contents(dn, sn, shared_alloc);

      xml_node_struct* dit = dn;
      xml_node_struct* sit = sn->first_child;

      while (sit && sit != sn)
      {
          // loop invariant: dit is inside the subtree rooted at dn
          assert(dit);

          // when a tree is copied into one of the descendants, we need to skip that subtree to avoid an infinite loop
          if (sit != dn)
          {
              xml_node_struct* copy = append_new_node(dit, alloc, PUGI__NODETYPE(sit));

              if (copy)
              {
                  node_copy_contents(copy, sit, shared_alloc);

                  if (sit->first_child)
                  {
                      dit = copy;
                      sit = sit->first_child;
                      continue;
                  }
              }
          }

          // continue to the next node
          do
          {
              if (sit->next_sibling)
              {
                  sit = sit->next_sibling;
                  break;
              }

              sit = sit->parent;
              dit = dit->parent;

              // loop invariant: dit is inside the subtree rooted at dn while sit is inside sn
              assert(sit == sn || dit);
          }
          while (sit != sn);
      }

      assert(!sit || dit == dn->parent);
  }

  PUGI__FN void node_copy_attribute(xml_attribute_struct* da, xml_attribute_struct* sa) noexcept
  {
      xml_allocator& alloc = get_allocator(da);
      xml_allocator* shared_alloc = (&alloc == &get_allocator(sa)) ? &alloc : 0;

      node_copy_string(da->name, da->header, xml_memory_page_name_allocated_mask, sa->name, sa->header, shared_alloc);
      node_copy_string(da->value, da->header, xml_memory_page_value_allocated_mask, sa->value, sa->header, shared_alloc);
  }

  inline bool is_text_node(xml_node_struct* node) noexcept
  {
      xml_node_type type = PUGI__NODETYPE(node);

      return type == node_pcdata || type == node_cdata;
  }

  // get value with conversion functions
  template <typename U>
  PUGI__FN PUGI__UNSIGNED_OVERFLOW U string_to_integer(const char* value, U minv, U maxv) noexcept
  {
      U result = 0;
      const char* s = value;

      while (PUGI__IS_CHARTYPE(*s, ct_space))
          s++;

      bool negative = (*s == '-');

      s += (*s == '+' || *s == '-');

      bool overflow = false;

      if (s[0] == '0' && (s[1] | ' ') == 'x')
      {
          s += 2;

          // since overflow detection relies on length of the sequence skip leading zeros
          while (*s == '0')
              s++;

          const char* start = s;

          for (;;)
          {
              if (static_cast<unsigned>(*s - '0') < 10)
                  result = result * 16 + (*s - '0');
              else if (static_cast<unsigned>((*s | ' ') - 'a') < 6)
                  result = result * 16 + ((*s | ' ') - 'a' + 10);
              else
                  break;

              s++;
          }

          size_t digits = static_cast<size_t>(s - start);

          overflow = digits > sizeof(U) * 2;
      }
      else
      {
          // since overflow detection relies on length of the sequence skip leading zeros
          while (*s == '0')
              s++;

          const char* start = s;

          for (;;)
          {
              if (static_cast<unsigned>(*s - '0') < 10)
                  result = result * 10 + (*s - '0');
              else
                  break;

              s++;
          }

          size_t digits = static_cast<size_t>(s - start);

          static_assert(sizeof(U) == 8 || sizeof(U) == 4 || sizeof(U) == 2);

          const size_t max_digits10 = sizeof(U) == 8 ? 20 : sizeof(U) == 4 ? 10 : 5;
          const char max_lead = sizeof(U) == 8 ? '1' : sizeof(U) == 4 ? '4' : '6';
          const size_t high_bit = sizeof(U) * 8 - 1;

          overflow = digits >= max_digits10 && !(digits == max_digits10 && (*start < max_lead || (*start == max_lead && result >> high_bit)));
      }

      if (negative)
      {
          // Workaround for crayc++ CC-3059: Expected no overflow in routine.
      #ifdef _CRAYC
          return (overflow || result > ~minv + 1) ? minv : ~result + 1;
      #else
          return (overflow || result > 0 - minv) ? minv : 0 - result;
      #endif
      }
      else
          return (overflow || result > maxv) ? maxv : result;
  }

  PUGI__FN int get_value_int(const char* value) noexcept
  {
      return string_to_integer<uint>(value, static_cast<uint>(INT_MIN), INT_MAX);
  }

  PUGI__FN uint get_value_uint(const char* value) noexcept
  {
      return string_to_integer<uint>(value, 0, UINT_MAX);
  }

  PUGI__FN double get_value_double(const char* value) noexcept
  {
      return strtod(value, 0);
  }

  PUGI__FN float get_value_float(const char* value) noexcept
  {
      return static_cast<float>(strtod(value, 0));
  }

  PUGI__FN bool get_value_bool(const char* value) noexcept
  {
      // only look at first char
      char first = *value;

      // 1*, t* (true), T* (True), y* (yes), Y* (YES)
      return (first == '1' || first == 't' || first == 'T' || first == 'y' || first == 'Y');
  }

  PUGI__FN long long get_value_llong(const char* value) noexcept
  {
      return string_to_integer<unsigned long long>(value, static_cast<unsigned long long>(LLONG_MIN), LLONG_MAX);
  }

  PUGI__FN unsigned long long get_value_ullong(const char* value) noexcept
  {
      return string_to_integer<unsigned long long>(value, 0, ULLONG_MAX);
  }

  template <typename U>
  PUGI__UNSIGNED_OVERFLOW char* integer_to_string(char* begin, char* end, U value, bool negative) noexcept
  {
      char* result = end - 1;
      U rest = negative ? 0 - value : value;

      do
      {
          *result-- = static_cast<char>('0' + (rest % 10));
          rest /= 10;
      }
      while (rest);

      assert(result >= begin);
      (void)begin;

      *result = '-';

      return result + !negative;
  }

  // set value with conversion functions
  template <typename String, typename Header>
  PUGI__FN bool set_value_ascii(String& dest, Header& header, uintptr_t header_mask, char* buf) noexcept
  {
      return strcpy_insitu(dest, header, header_mask, buf, strlen(buf));
  }

  template <typename U, typename String, typename Header>
  PUGI__FN bool set_value_integer(String& dest, Header& header, uintptr_t header_mask, U value, bool negative) noexcept
  {
      char buf[64];
      char* end = buf + sizeof(buf) / sizeof(buf[0]);
      char* begin = integer_to_string(buf, end, value, negative);

      return strcpy_insitu(dest, header, header_mask, begin, end - begin);
  }

  template <typename String, typename Header>
  PUGI__FN bool set_value_convert(String& dest, Header& header, uintptr_t header_mask, float value, int precision) noexcept
  {
      char buf[128];
      PUGI__SNPRINTF(buf, "%.*g", precision, double(value));

      return set_value_ascii(dest, header, header_mask, buf);
  }

  template <typename String, typename Header>
  PUGI__FN bool set_value_convert(String& dest, Header& header, uintptr_t header_mask, double value, int precision) noexcept
  {
      char buf[128];
      PUGI__SNPRINTF(buf, "%.*g", precision, value);

      return set_value_ascii(dest, header, header_mask, buf);
  }

  template <typename String, typename Header>
  PUGI__FN bool set_value_bool(String& dest, Header& header, uintptr_t header_mask, bool value) noexcept
  {
      return strcpy_insitu(dest, header, header_mask, value ? "true" : "false", value ? 4 : 5);
  }

PUGI__FN xml_parse_result load_buffer_impl( xml_document_struct* doc, xml_node_struct* root, void* contents, size_t size,
                            uint options, xml_encoding e, bool is_mutable, bool own, char** out_buffer ) noexcept
{
    // check input buffer
    if (!contents && size) return make_parse_result(status_io_error);

    auto_deleter<void> contents_guard(own ? contents : 0, ::free);

    // get private buffer
    char* buffer = 0;
    size_t length = 0;

    // coverity[var_deref_model]
    if (!impl::convert_buffer(buffer, length, encoding_utf8, contents, size, is_mutable))
        return impl::make_parse_result(status_out_of_memory);

    // after this we either deallocate contents (below) or hold on to it via doc->buffer, so we don't need to guard it
    contents_guard.release();

    // delete original buffer if we performed a conversion
    if (own && buffer != contents && contents) ::free(contents);

    // grab onto buffer if it's our buffer, user is responsible for deallocating contents himself
    if (own || buffer != contents) *out_buffer = buffer;

    // store buffer for offset_debug
    doc->buffer = buffer;

    // parse
    xml_parse_result res = impl::xml_parser::parse(buffer, length, doc, root, options);

    return res;
}

    // we need to get length of entire file to load it in memory; the only (relatively) sane way to do it is via seek/tell trick
    PUGI__FN xml_parse_status get_file_size(FILE* file, size_t& out_result)
    {
        // if this is a 32-bit OS, long is enough; if this is a unix system, long is 64-bit, which is enough; otherwise we can't do anything anyway.
        typedef long length_type;

        fseek(file, 0, SEEK_END);
        length_type length = ftell(file);
        fseek(file, 0, SEEK_SET);

        // check for I/O errors
        if (length < 0) return status_io_error;

        // check for overflow
        size_t result = static_cast<size_t>(length);

        if (static_cast<length_type>(result) != length) return status_out_of_memory;

        // finalize
        out_result = result;

        return status_ok;
    }

// This function assumes that buffer has extra sizeof(char) writable bytes after size
inline static size_t zero_terminate_buffer(void* buffer, size_t size) noexcept
{
    static_cast<char*>(buffer)[size] = 0;
    return size + 1;
}

PUGI__FN xml_parse_result load_file_impl( xml_document_struct* doc, FILE* file, uint options,
                            xml_encoding encoding, char** out_buffer ) noexcept
{
    if (!file) return make_parse_result(status_file_not_found);

    // get file size (can result in I/O errors)
    size_t size = 0;
    xml_parse_status size_status = get_file_size(file, size);
    if (size_status != status_ok) return make_parse_result(size_status);

    size_t max_suffix_size = sizeof(char);

    // allocate buffer for the whole file
    char* contents = static_cast<char*>(::malloc(size + max_suffix_size));
    if (!contents) return make_parse_result(status_out_of_memory);

    // read file in memory
    size_t read_size = fread(contents, 1, size, file);

    if (read_size != size)
    {
        ::free(contents);
        return make_parse_result(status_io_error);
    }

    return load_buffer_impl( doc, doc, contents, zero_terminate_buffer(contents, size),
                options, encoding_utf8, true, true, out_buffer );
}

PUGI__FN void close_file(FILE* file) noexcept
{
    fclose(file);
}

    template <typename T> struct xml_stream_chunk
    {
        static xml_stream_chunk* create()
        {
            void* memory = ::malloc(sizeof(xml_stream_chunk));
            if (!memory) return 0;

            return new (memory) xml_stream_chunk();
        }

        static void destroy(xml_stream_chunk* chunk)
        {
            // free chunk chain
            while (chunk)
            {
                xml_stream_chunk* next_ = chunk->next;

                ::free(chunk);

                chunk = next_;
            }
        }

        xml_stream_chunk(): next(0), size(0)
        {
        }

        xml_stream_chunk* next;
        size_t size;

        T data[xml_memory_page_size / sizeof(T)];
    };

    template <typename T> PUGI__FN xml_parse_status load_stream_data_noseek(std::basic_istream<T>& stream, void** out_buffer, size_t* out_size)
    {
        auto_deleter<xml_stream_chunk<T> > chunks(0, xml_stream_chunk<T>::destroy);

        // read file to a chunk list
        size_t total = 0;
        xml_stream_chunk<T>* last = 0;

        while (!stream.eof())
        {
            // allocate new chunk
            xml_stream_chunk<T>* chunk = xml_stream_chunk<T>::create();
            if (!chunk) return status_out_of_memory;

            // append chunk to list
            if (last) last = last->next = chunk;
            else chunks.data = last = chunk;

            // read data to chunk
            stream.read(chunk->data, static_cast<std::streamsize>(sizeof(chunk->data) / sizeof(T)));
            chunk->size = static_cast<size_t>(stream.gcount()) * sizeof(T);

            // read may set failbit | eofbit in case gcount() is less than read length, so check for other I/O errors
            if (stream.bad() || (!stream.eof() && stream.fail())) return status_io_error;

            // guard against huge files (chunk size is small enough to make this overflow check work)
            if (total + chunk->size < total) return status_out_of_memory;
            total += chunk->size;
        }

        size_t max_suffix_size = sizeof(char);

        // copy chunk list to a contiguous buffer
        char* buffer = static_cast<char*>(::malloc(total + max_suffix_size));
        if (!buffer) return status_out_of_memory;

        char* write = buffer;

        for (xml_stream_chunk<T>* chunk = chunks.data; chunk; chunk = chunk->next)
        {
            assert(write + chunk->size <= buffer + total);
            memcpy(write, chunk->data, chunk->size);
            write += chunk->size;
        }

        assert(write == buffer + total);

        // return buffer
        *out_buffer = buffer;
        *out_size = total;

        return status_ok;
    }

    template <typename T> PUGI__FN xml_parse_status load_stream_data_seek(std::basic_istream<T>& stream, void** out_buffer, size_t* out_size)
    {
        // get length of remaining data in stream
        typename std::basic_istream<T>::pos_type pos = stream.tellg();
        stream.seekg(0, std::ios::end);
        std::streamoff length = stream.tellg() - pos;
        stream.seekg(pos);

        if (stream.fail() || pos < 0) return status_io_error;

        // guard against huge files
        size_t read_length = static_cast<size_t>(length);

        if (static_cast<std::streamsize>(read_length) != length || length < 0) return status_out_of_memory;

        size_t max_suffix_size = sizeof(char);

        // read stream data into memory (guard against stream exceptions with buffer holder)
        auto_deleter<void> buffer(::malloc(read_length * sizeof(T) + max_suffix_size), ::free);
        if (!buffer.data) return status_out_of_memory;

        stream.read(static_cast<T*>(buffer.data), static_cast<std::streamsize>(read_length));

        // read may set failbit | eofbit in case gcount() is less than read_length (i.e. line ending conversion), so check for other I/O errors
        if (stream.bad() || (!stream.eof() && stream.fail())) return status_io_error;

        // return buffer
        size_t actual_length = static_cast<size_t>(stream.gcount());
        assert(actual_length <= read_length);

        *out_buffer = buffer.release();
        *out_size = actual_length * sizeof(T);

        return status_ok;
    }


PUGI__FN xml_parse_result load_stream_impl( xml_document_struct* doc, std::istream& stream,
                            uint options, xml_encoding encoding, char** out_buffer ) noexcept
{
    void* buffer = 0;
    size_t size = 0;
    xml_parse_status status = status_ok;

    // if stream has an error bit set, bail out (otherwise tellg() can fail and we'll clear error bits)
    if (stream.fail()) return make_parse_result(status_io_error);

    // load stream to memory
    // (using seek-based implementation if possible, since it's faster and takes less memory)
    if (stream.tellg() < 0)
    {
        stream.clear(); // clear error flags that could be set by a failing tellg
        status = load_stream_data_noseek(stream, &buffer, &size);
    }
    else {
        status = load_stream_data_seek(stream, &buffer, &size);
    }

    if (status != status_ok)
        return make_parse_result(status);

    return load_buffer_impl( doc, doc, buffer, zero_terminate_buffer(buffer, size), options,
                encoding_utf8, true, true, out_buffer );
}


    struct name_null_sentry
    {
        xml_node_struct* node;
        char* name;

        name_null_sentry(xml_node_struct* node_): node(node_), name(node_->name)
        {
            node->name = 0;
        }

        ~name_null_sentry()
        {
            node->name = name;
        }
    };

PUGI__NS_END

namespace pugRd
{

PUGI__FN bool xml_tree_walker::begin(xml_node&) noexcept
{
    return true;
}

PUGI__FN bool xml_tree_walker::end(xml_node&) noexcept
{
    return true;
}

PUGI__FN static void unspecified_bool_xml_attribute(xml_attribute***) noexcept
{
}

PUGI__FN xml_attribute::operator xml_attribute::unspecified_bool_type() const noexcept
{
    return _attr ? unspecified_bool_xml_attribute : 0;
}

PUGI__FN bool xml_attribute::operator==(const xml_attribute& r) const noexcept
{
    return (_attr == r._attr);
}

PUGI__FN bool xml_attribute::operator!=(const xml_attribute& r) const noexcept
{
    return (_attr != r._attr);
}

PUGI__FN bool xml_attribute::operator<(const xml_attribute& r) const noexcept
{
    return (_attr < r._attr);
}

PUGI__FN bool xml_attribute::operator>(const xml_attribute& r) const noexcept
{
    return (_attr > r._attr);
}

PUGI__FN bool xml_attribute::operator<=(const xml_attribute& r) const noexcept
{
    return (_attr <= r._attr);
}

PUGI__FN bool xml_attribute::operator>=(const xml_attribute& r) const noexcept
{
    return (_attr >= r._attr);
}

PUGI__FN xml_attribute xml_attribute::next_attribute() const noexcept
{
    if (!_attr) return xml_attribute();
    return xml_attribute(_attr->next_attribute);
}

PUGI__FN xml_attribute xml_attribute::previous_attribute() const noexcept
{
    if (!_attr) return xml_attribute();
    xml_attribute_struct* prev = _attr->prev_attribute_c;
    return prev->next_attribute ? xml_attribute(prev) : xml_attribute();
}

PUGI__FN const char* xml_attribute::as_string(const char* def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? value : def;
}

PUGI__FN int xml_attribute::as_int(int def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? impl::get_value_int(value) : def;
}

PUGI__FN uint xml_attribute::as_uint(uint def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? impl::get_value_uint(value) : def;
}

PUGI__FN double xml_attribute::as_double(double def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? impl::get_value_double(value) : def;
}

PUGI__FN float xml_attribute::as_float(float def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? impl::get_value_float(value) : def;
}

PUGI__FN bool xml_attribute::as_bool(bool def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? impl::get_value_bool(value) : def;
}

PUGI__FN long long xml_attribute::as_llong(long long def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? impl::get_value_llong(value) : def;
}

PUGI__FN unsigned long long xml_attribute::as_ullong(unsigned long long def) const noexcept
{
    if (!_attr) return def;
    const char* value = _attr->value;
    return value ? impl::get_value_ullong(value) : def;
}

PUGI__FN const char* xml_attribute::name() const noexcept
{
    if (!_attr) return "";
    const char* name = _attr->name;
    return name ? name : "";
}

PUGI__FN const char* xml_attribute::value() const noexcept
{
    if (!_attr) return "";
    const char* value = _attr->value;
    return value ? value : "";
}

PUGI__FN size_t xml_attribute::hash_value() const noexcept
{
    return static_cast<size_t>(reinterpret_cast<uintptr_t>(_attr) / sizeof(xml_attribute_struct));
}

PUGI__FN xml_attribute_struct* xml_attribute::internal_object() const noexcept
{
    return _attr;
}

PUGI__FN xml_attribute& xml_attribute::operator=(const char* rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(int rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(uint rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(long rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(unsigned long rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(double rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(float rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(bool rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(long long rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN xml_attribute& xml_attribute::operator=(unsigned long long rhs) noexcept
{
    set_value(rhs);
    return *this;
}

PUGI__FN bool xml_attribute::set_name(const char* rhs) noexcept
{
    if (!_attr) return false;

    return impl::strcpy_insitu(_attr->name, _attr->header, impl::xml_memory_page_name_allocated_mask, rhs, ::strlen(rhs));
}

PUGI__FN bool xml_attribute::set_value(const char* rhs, size_t sz) noexcept
{
    if (!_attr) return false;

    return impl::strcpy_insitu(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, sz);
}

PUGI__FN bool xml_attribute::set_value(const char* rhs) noexcept
{
    if (!_attr) return false;

    return impl::strcpy_insitu(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, ::strlen(rhs));
}

PUGI__FN bool xml_attribute::set_value(int rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_integer<uint>(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, rhs < 0);
}

PUGI__FN bool xml_attribute::set_value(uint rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_integer<uint>(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, false);
}

PUGI__FN bool xml_attribute::set_value(long rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_integer<unsigned long>(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, rhs < 0);
}

PUGI__FN bool xml_attribute::set_value(unsigned long rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_integer<unsigned long>(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, false);
}

PUGI__FN bool xml_attribute::set_value(double rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_convert(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, default_double_precision);
}

PUGI__FN bool xml_attribute::set_value(double rhs, int precision) noexcept
{
    if (!_attr) return false;

    return impl::set_value_convert(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, precision);
}

PUGI__FN bool xml_attribute::set_value(float rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_convert(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, default_float_precision);
}

PUGI__FN bool xml_attribute::set_value(float rhs, int precision) noexcept
{
    if (!_attr) return false;

    return impl::set_value_convert(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, precision);
}

PUGI__FN bool xml_attribute::set_value(bool rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_bool(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs);
}

PUGI__FN bool xml_attribute::set_value(long long rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_integer<unsigned long long>(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, rhs < 0);
}

PUGI__FN bool xml_attribute::set_value(unsigned long long rhs) noexcept
{
    if (!_attr) return false;

    return impl::set_value_integer<unsigned long long>(_attr->value, _attr->header, impl::xml_memory_page_value_allocated_mask, rhs, false);
}

PUGI__FN static void unspecified_bool_xml_node(xml_node***) noexcept
{
}

PUGI__FN xml_node::operator xml_node::unspecified_bool_type() const noexcept
{
    return _root ? unspecified_bool_xml_node : 0;
}

PUGI__FN xml_node::iterator xml_node::begin() const noexcept
{
    return iterator(_root ? _root->first_child + 0 : 0, _root);
}

PUGI__FN xml_node::iterator xml_node::end() const noexcept
{
    return iterator(0, _root);
}

PUGI__FN xml_node::attribute_iterator xml_node::attributes_begin() const noexcept
{
    return attribute_iterator(_root ? _root->first_attribute + 0 : 0, _root);
}

PUGI__FN xml_node::attribute_iterator xml_node::attributes_end() const noexcept
{
    return attribute_iterator(0, _root);
}

PUGI__FN xml_object_range<xml_node_iterator> xml_node::children() const noexcept
{
    return xml_object_range<xml_node_iterator>(begin(), end());
}

PUGI__FN xml_object_range<xml_named_node_iterator> xml_node::children(const char* name_) const noexcept
{
    return xml_object_range<xml_named_node_iterator>(xml_named_node_iterator(child(name_)._root, _root, name_), xml_named_node_iterator(0, _root, name_));
}

PUGI__FN xml_object_range<xml_attribute_iterator> xml_node::attributes() const noexcept
{
    return xml_object_range<xml_attribute_iterator>(attributes_begin(), attributes_end());
}

PUGI__FN bool xml_node::operator==(const xml_node& r) const noexcept
{
    return (_root == r._root);
}

PUGI__FN bool xml_node::operator!=(const xml_node& r) const noexcept
{
    return (_root != r._root);
}

PUGI__FN bool xml_node::operator<(const xml_node& r) const noexcept
{
    return (_root < r._root);
}

PUGI__FN bool xml_node::operator>(const xml_node& r) const noexcept
{
    return (_root > r._root);
}

PUGI__FN bool xml_node::operator<=(const xml_node& r) const noexcept
{
    return (_root <= r._root);
}

PUGI__FN bool xml_node::operator>=(const xml_node& r) const noexcept
{
    return (_root >= r._root);
}

PUGI__FN bool xml_node::empty() const noexcept
{
    return !_root;
}

PUGI__FN const char* xml_node::name() const noexcept
{
    if (!_root) return "";
    const char* name = _root->name;
    return name ? name : "";
}

PUGI__FN xml_node_type xml_node::type() const noexcept
{
    return _root ? PUGI__NODETYPE(_root) : node_null;
}

PUGI__FN const char* xml_node::value() const noexcept
{
    if (!_root) return "";
    const char* value = _root->value;
    return value ? value : "";
}

PUGI__FN xml_node xml_node::child(const char* name_) const noexcept
{
    if (!_root) return xml_node();

    for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
    {
        const char* iname = i->name;
        if (iname && impl::strequal(name_, iname))
            return xml_node(i);
    }

    return xml_node();
}

PUGI__FN xml_attribute xml_node::attribute(const char* name_) const noexcept
{
    if (!_root) return xml_attribute();

    for (xml_attribute_struct* i = _root->first_attribute; i; i = i->next_attribute)
    {
        const char* iname = i->name;
        if (iname && impl::strequal(name_, iname))
            return xml_attribute(i);
    }

    return xml_attribute();
}

PUGI__FN xml_node xml_node::next_sibling(const char* name_) const noexcept
{
    if (!_root) return xml_node();

    for (xml_node_struct* i = _root->next_sibling; i; i = i->next_sibling)
    {
        const char* iname = i->name;
        if (iname && impl::strequal(name_, iname))
            return xml_node(i);
    }

    return xml_node();
}

PUGI__FN xml_node xml_node::next_sibling() const noexcept
{
    return _root ? xml_node(_root->next_sibling) : xml_node();
}

PUGI__FN xml_node xml_node::previous_sibling(const char* name_) const noexcept
{
    if (!_root) return xml_node();

    for (xml_node_struct* i = _root->prev_sibling_c; i->next_sibling; i = i->prev_sibling_c)
    {
        const char* iname = i->name;
        if (iname && impl::strequal(name_, iname))
            return xml_node(i);
    }

    return xml_node();
}

PUGI__FN xml_attribute xml_node::attribute(const char* name_, xml_attribute& hint_) const noexcept
{
    xml_attribute_struct* hint = hint_._attr;

    // if hint is not an attribute of node, behavior is not defined
    assert(!hint || (_root && impl::is_attribute_of(hint, _root)));

    if (!_root) return xml_attribute();

    // optimistically search from hint up until the end
    for (xml_attribute_struct* i = hint; i; i = i->next_attribute)
    {
        const char* iname = i->name;
        if (iname && impl::strequal(name_, iname))
        {
            // update hint to maximize efficiency of searching for consecutive attributes
            hint_._attr = i->next_attribute;

            return xml_attribute(i);
        }
    }

    // wrap around and search from the first attribute until the hint
    // 'j' null pointer check is technically redundant, but it prevents a crash in case the assertion above fails
    for (xml_attribute_struct* j = _root->first_attribute; j && j != hint; j = j->next_attribute)
    {
        const char* jname = j->name;
        if (jname && impl::strequal(name_, jname))
        {
            // update hint to maximize efficiency of searching for consecutive attributes
            hint_._attr = j->next_attribute;

            return xml_attribute(j);
        }
    }

    return xml_attribute();
}

PUGI__FN xml_node xml_node::previous_sibling() const noexcept
{
    if (!_root) return xml_node();
    xml_node_struct* prev = _root->prev_sibling_c;
    return prev->next_sibling ? xml_node(prev) : xml_node();
}

PUGI__FN xml_node xml_node::parent() const noexcept
{
    return _root ? xml_node(_root->parent) : xml_node();
}

PUGI__FN xml_node xml_node::root() const noexcept
{
    return _root ? xml_node(&impl::get_document(_root)) : xml_node();
}

PUGI__FN xml_text xml_node::text() const noexcept
{
    return xml_text(_root);
}

PUGI__FN const char* xml_node::child_value() const noexcept
{
    if (!_root) return "";

    // element nodes can have value if parse_embed_pcdata was used
    if (PUGI__NODETYPE(_root) == node_element && _root->value)
        return _root->value;

    for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
    {
        const char* ivalue = i->value;
        if (impl::is_text_node(i) && ivalue)
            return ivalue;
    }

    return "";
}

PUGI__FN const char* xml_node::child_value(const char* name_) const noexcept
{
    return child(name_).child_value();
}

PUGI__FN xml_attribute xml_node::first_attribute() const noexcept
{
    if (!_root) return xml_attribute();
    return xml_attribute(_root->first_attribute);
}

PUGI__FN xml_attribute xml_node::last_attribute() const noexcept
{
    if (!_root) return xml_attribute();
    xml_attribute_struct* first = _root->first_attribute;
    return first ? xml_attribute(first->prev_attribute_c) : xml_attribute();
}

PUGI__FN xml_node xml_node::first_child() const noexcept
{
    if (!_root) return xml_node();
    return xml_node(_root->first_child);
}

PUGI__FN xml_node xml_node::last_child() const noexcept
{
    if (!_root) return xml_node();
    xml_node_struct* first = _root->first_child;
    return first ? xml_node(first->prev_sibling_c) : xml_node();
}

PUGI__FN bool xml_node::set_name(const char* rhs) noexcept
{
    xml_node_type type_ = _root ? PUGI__NODETYPE(_root) : node_null;

    if (type_ != node_element && type_ != node_pi && type_ != node_declaration)
        return false;

    return impl::strcpy_insitu(_root->name, _root->header, impl::xml_memory_page_name_allocated_mask, rhs, ::strlen(rhs));
}

PUGI__FN bool xml_node::set_value(const char* rhs, size_t sz) noexcept
{
    xml_node_type type_ = _root ? PUGI__NODETYPE(_root) : node_null;

    if (type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
        return false;

    return impl::strcpy_insitu(_root->value, _root->header, impl::xml_memory_page_value_allocated_mask, rhs, sz);
}

PUGI__FN bool xml_node::set_value(const char* rhs) noexcept
{
    xml_node_type type_ = _root ? PUGI__NODETYPE(_root) : node_null;

    if (type_ != node_pcdata && type_ != node_cdata && type_ != node_comment && type_ != node_pi && type_ != node_doctype)
        return false;

    return impl::strcpy_insitu(_root->value, _root->header, impl::xml_memory_page_value_allocated_mask, rhs, ::strlen(rhs));
}

PUGI__FN xml_attribute xml_node::append_attribute(const char* name_) noexcept
{
    if (!impl::allow_insert_attribute(type())) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::append_attribute(a._attr, _root);

    a.set_name(name_);

    return a;
}

PUGI__FN xml_attribute xml_node::prepend_attribute(const char* name_) noexcept
{
    if (!impl::allow_insert_attribute(type())) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::prepend_attribute(a._attr, _root);

    a.set_name(name_);

    return a;
}

PUGI__FN xml_attribute xml_node::insert_attribute_after(const char* name_, const xml_attribute& attr) noexcept
{
    if (!impl::allow_insert_attribute(type())) return xml_attribute();
    if (!attr || !impl::is_attribute_of(attr._attr, _root)) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::insert_attribute_after(a._attr, attr._attr, _root);

    a.set_name(name_);

    return a;
}

PUGI__FN xml_attribute xml_node::insert_attribute_before(const char* name_, const xml_attribute& attr) noexcept
{
    if (!impl::allow_insert_attribute(type())) return xml_attribute();
    if (!attr || !impl::is_attribute_of(attr._attr, _root)) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::insert_attribute_before(a._attr, attr._attr, _root);

    a.set_name(name_);

    return a;
}

PUGI__FN xml_attribute xml_node::append_copy(const xml_attribute& proto) noexcept
{
    if (!proto) return xml_attribute();
    if (!impl::allow_insert_attribute(type())) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::append_attribute(a._attr, _root);
    impl::node_copy_attribute(a._attr, proto._attr);

    return a;
}

PUGI__FN xml_attribute xml_node::prepend_copy(const xml_attribute& proto) noexcept
{
    if (!proto) return xml_attribute();
    if (!impl::allow_insert_attribute(type())) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::prepend_attribute(a._attr, _root);
    impl::node_copy_attribute(a._attr, proto._attr);

    return a;
}

PUGI__FN xml_attribute xml_node::insert_copy_after(const xml_attribute& proto, const xml_attribute& attr) noexcept
{
    if (!proto) return xml_attribute();
    if (!impl::allow_insert_attribute(type())) return xml_attribute();
    if (!attr || !impl::is_attribute_of(attr._attr, _root)) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::insert_attribute_after(a._attr, attr._attr, _root);
    impl::node_copy_attribute(a._attr, proto._attr);

    return a;
}

PUGI__FN xml_attribute xml_node::insert_copy_before(const xml_attribute& proto, const xml_attribute& attr) noexcept
{
    if (!proto) return xml_attribute();
    if (!impl::allow_insert_attribute(type())) return xml_attribute();
    if (!attr || !impl::is_attribute_of(attr._attr, _root)) return xml_attribute();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_attribute();

    xml_attribute a(impl::allocate_attribute(alloc));
    if (!a) return xml_attribute();

    impl::insert_attribute_before(a._attr, attr._attr, _root);
    impl::node_copy_attribute(a._attr, proto._attr);

    return a;
}

PUGI__FN xml_node xml_node::append_child(xml_node_type type_) noexcept
{
    if (!impl::allow_insert_child(type(), type_)) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::append_node(n._root, _root);

    if (type_ == node_declaration) n.set_name("xml");

    return n;
}

PUGI__FN xml_node xml_node::prepend_child(xml_node_type type_) noexcept
{
    if (!impl::allow_insert_child(type(), type_)) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::prepend_node(n._root, _root);

    if (type_ == node_declaration) n.set_name("xml");

    return n;
}

PUGI__FN xml_node xml_node::insert_child_before(xml_node_type type_, const xml_node& node) noexcept
{
    if (!impl::allow_insert_child(type(), type_)) return xml_node();
    if (!node._root || node._root->parent != _root) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::insert_node_before(n._root, node._root);

    if (type_ == node_declaration) n.set_name("xml");

    return n;
}

PUGI__FN xml_node xml_node::insert_child_after(xml_node_type type_, const xml_node& node) noexcept
{
    if (!impl::allow_insert_child(type(), type_)) return xml_node();
    if (!node._root || node._root->parent != _root) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::insert_node_after(n._root, node._root);

    if (type_ == node_declaration) n.set_name("xml");

    return n;
}

PUGI__FN xml_node xml_node::append_child(const char* name_) noexcept
{
    xml_node result = append_child(node_element);

    result.set_name(name_);

    return result;
}

PUGI__FN xml_node xml_node::prepend_child(const char* name_) noexcept
{
    xml_node result = prepend_child(node_element);

    result.set_name(name_);

    return result;
}

PUGI__FN xml_node xml_node::insert_child_after(const char* name_, const xml_node& node) noexcept
{
    xml_node result = insert_child_after(node_element, node);

    result.set_name(name_);

    return result;
}

PUGI__FN xml_node xml_node::insert_child_before(const char* name_, const xml_node& node) noexcept
{
    xml_node result = insert_child_before(node_element, node);

    result.set_name(name_);

    return result;
}

PUGI__FN xml_node xml_node::append_copy(const xml_node& proto) noexcept
{
    xml_node_type type_ = proto.type();
    if (!impl::allow_insert_child(type(), type_)) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::append_node(n._root, _root);
    impl::node_copy_tree(n._root, proto._root);

    return n;
}

PUGI__FN xml_node xml_node::prepend_copy(const xml_node& proto) noexcept
{
    xml_node_type type_ = proto.type();
    if (!impl::allow_insert_child(type(), type_)) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::prepend_node(n._root, _root);
    impl::node_copy_tree(n._root, proto._root);

    return n;
}

PUGI__FN xml_node xml_node::insert_copy_after(const xml_node& proto, const xml_node& node) noexcept
{
    xml_node_type type_ = proto.type();
    if (!impl::allow_insert_child(type(), type_)) return xml_node();
    if (!node._root || node._root->parent != _root) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::insert_node_after(n._root, node._root);
    impl::node_copy_tree(n._root, proto._root);

    return n;
}

PUGI__FN xml_node xml_node::insert_copy_before(const xml_node& proto, const xml_node& node) noexcept
{
    xml_node_type type_ = proto.type();
    if (!impl::allow_insert_child(type(), type_)) return xml_node();
    if (!node._root || node._root->parent != _root) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    xml_node n(impl::allocate_node(alloc, type_));
    if (!n) return xml_node();

    impl::insert_node_before(n._root, node._root);
    impl::node_copy_tree(n._root, proto._root);

    return n;
}

PUGI__FN xml_node xml_node::append_move(const xml_node& moved) noexcept
{
    if (!impl::allow_move(*this, moved)) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    // disable document_buffer_order optimization since moving nodes around changes document order without changing buffer pointers
    impl::get_document(_root).header |= impl::xml_memory_page_contents_shared_mask;

    impl::remove_node(moved._root);
    impl::append_node(moved._root, _root);

    return moved;
}

PUGI__FN xml_node xml_node::prepend_move(const xml_node& moved) noexcept
{
    if (!impl::allow_move(*this, moved)) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    // disable document_buffer_order optimization since moving nodes around changes document order without changing buffer pointers
    impl::get_document(_root).header |= impl::xml_memory_page_contents_shared_mask;

    impl::remove_node(moved._root);
    impl::prepend_node(moved._root, _root);

    return moved;
}

PUGI__FN xml_node xml_node::insert_move_after(const xml_node& moved, const xml_node& node) noexcept
{
    if (!impl::allow_move(*this, moved)) return xml_node();
    if (!node._root || node._root->parent != _root) return xml_node();
    if (moved._root == node._root) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    // disable document_buffer_order optimization since moving nodes around changes document order without changing buffer pointers
    impl::get_document(_root).header |= impl::xml_memory_page_contents_shared_mask;

    impl::remove_node(moved._root);
    impl::insert_node_after(moved._root, node._root);

    return moved;
}

PUGI__FN xml_node xml_node::insert_move_before(const xml_node& moved, const xml_node& node) noexcept
{
    if (!impl::allow_move(*this, moved)) return xml_node();
    if (!node._root || node._root->parent != _root) return xml_node();
    if (moved._root == node._root) return xml_node();

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return xml_node();

    // disable document_buffer_order optimization since moving nodes around changes document order without changing buffer pointers
    impl::get_document(_root).header |= impl::xml_memory_page_contents_shared_mask;

    impl::remove_node(moved._root);
    impl::insert_node_before(moved._root, node._root);

    return moved;
}

PUGI__FN bool xml_node::remove_attribute(const char* name_) noexcept
{
    return remove_attribute(attribute(name_));
}

PUGI__FN bool xml_node::remove_attribute(const xml_attribute& a) noexcept
{
    if (!_root || !a._attr) return false;
    if (!impl::is_attribute_of(a._attr, _root)) return false;

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return false;

    impl::remove_attribute(a._attr, _root);
    impl::destroy_attribute(a._attr, alloc);

    return true;
}

PUGI__FN bool xml_node::remove_attributes() noexcept
{
    if (!_root) return false;

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return false;

    for (xml_attribute_struct* attr = _root->first_attribute; attr; )
    {
        xml_attribute_struct* next = attr->next_attribute;

        impl::destroy_attribute(attr, alloc);

        attr = next;
    }

    _root->first_attribute = 0;

    return true;
}

PUGI__FN bool xml_node::remove_child(const char* name_) noexcept
{
    return remove_child(child(name_));
}

PUGI__FN bool xml_node::remove_child(const xml_node& n) noexcept
{
    if (!_root || !n._root || n._root->parent != _root) return false;

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return false;

    impl::remove_node(n._root);
    impl::destroy_node(n._root, alloc);

    return true;
}

PUGI__FN bool xml_node::remove_children() noexcept
{
    if (!_root) return false;

    impl::xml_allocator& alloc = impl::get_allocator(_root);
    if (!alloc.reserve()) return false;

    for (xml_node_struct* cur = _root->first_child; cur; )
    {
        xml_node_struct* next = cur->next_sibling;

        impl::destroy_node(cur, alloc);

        cur = next;
    }

    _root->first_child = 0;

    return true;
}

PUGI__FN xml_parse_result xml_node::append_buffer(const void* contents, size_t size, uint options, xml_encoding encoding) noexcept
{
    // append_buffer is only valid for elements/documents
    if (!impl::allow_insert_child(type(), node_element)) return impl::make_parse_result(status_append_invalid_root);

    // get document node
    impl::xml_document_struct* doc = &impl::get_document(_root);

    // disable document_buffer_order optimization since in a document with multiple buffers comparing buffer pointers does not make sense
    doc->header |= impl::xml_memory_page_contents_shared_mask;

    // get extra buffer element (we'll store the document fragment buffer there so that we can deallocate it later)
    impl::xml_memory_page* page = 0;
    impl::xml_extra_buffer* extra = static_cast<impl::xml_extra_buffer*>(doc->allocate_memory(sizeof(impl::xml_extra_buffer) + sizeof(void*), page));
    (void)page;

    if (!extra) return impl::make_parse_result(status_out_of_memory);

    // add extra buffer to the list
    extra->buffer = 0;
    extra->next = doc->extra_buffers;
    doc->extra_buffers = extra;

    // name of the root has to be NULL before parsing - otherwise closing node mismatches will not be detected at the top level
    impl::name_null_sentry sentry(_root);

    return impl::load_buffer_impl(doc, _root, const_cast<void*>(contents), size, options, encoding, false, false, &extra->buffer);
}

PUGI__FN xml_node xml_node::find_child_by_attribute(const char* name_, const char* attr_name, const char* attr_value) const noexcept
{
    if (!_root) return xml_node();

    for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
    {
        const char* iname = i->name;
        if (iname && impl::strequal(name_, iname))
        {
            for (xml_attribute_struct* a = i->first_attribute; a; a = a->next_attribute)
            {
                const char* aname = a->name;
                if (aname && impl::strequal(attr_name, aname))
                {
                    const char* avalue = a->value;
                    if (impl::strequal(attr_value, avalue ? avalue : ""))
                        return xml_node(i);
                }
            }
        }
    }

    return xml_node();
}

PUGI__FN xml_node xml_node::find_child_by_attribute(const char* attr_name, const char* attr_value) const noexcept
{
    if (!_root) return xml_node();

    for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
        for (xml_attribute_struct* a = i->first_attribute; a; a = a->next_attribute)
        {
            const char* aname = a->name;
            if (aname && impl::strequal(attr_name, aname))
            {
                const char* avalue = a->value;
                if (impl::strequal(attr_value, avalue ? avalue : ""))
                    return xml_node(i);
            }
        }

    return xml_node();
}

PUGI__FN std::string xml_node::path(char delimiter) const noexcept
{
    if (!_root) return std::string();

    size_t offset = 0;

    for (xml_node_struct* i = _root; i; i = i->parent)
    {
        const char* iname = i->name;
        offset += (i != _root);
        offset += iname ? ::strlen(iname) : 0;
    }

    std::string result;
    result.resize(offset);

    for (xml_node_struct* j = _root; j; j = j->parent)
    {
        if (j != _root)
            result[--offset] = delimiter;

        const char* jname = j->name;
        if (jname)
        {
            size_t length = ::strlen(jname);

            offset -= length;
            memcpy(&result[offset], jname, length * sizeof(char));
        }
    }

    assert(offset == 0);

    return result;
}

PUGI__FN xml_node xml_node::first_element_by_path(const char* path_, char delimiter) const noexcept
{
    xml_node context = path_[0] == delimiter ? root() : *this;

    if (!context._root) return xml_node();

    const char* path_segment = path_;

    while (*path_segment == delimiter) ++path_segment;

    const char* path_segment_end = path_segment;

    while (*path_segment_end && *path_segment_end != delimiter) ++path_segment_end;

    if (path_segment == path_segment_end) return context;

    const char* next_segment = path_segment_end;

    while (*next_segment == delimiter) ++next_segment;

    if (*path_segment == '.' && path_segment + 1 == path_segment_end)
        return context.first_element_by_path(next_segment, delimiter);
    else if (*path_segment == '.' && *(path_segment+1) == '.' && path_segment + 2 == path_segment_end)
        return context.parent().first_element_by_path(next_segment, delimiter);
    else
    {
        for (xml_node_struct* j = context._root->first_child; j; j = j->next_sibling)
        {
            const char* jname = j->name;
            if (jname && impl::strequalrange(jname, path_segment, static_cast<size_t>(path_segment_end - path_segment)))
            {
                xml_node subsearch = xml_node(j).first_element_by_path(next_segment, delimiter);

                if (subsearch) return subsearch;
            }
        }

        return xml_node();
    }
}

PUGI__FN bool xml_node::traverse(xml_tree_walker& walker) noexcept
{
    walker._depth = -1;

    xml_node arg_begin(_root);
    if (!walker.begin(arg_begin)) return false;

    xml_node_struct* cur = _root ? _root->first_child + 0 : 0;

    if (cur)
    {
        ++walker._depth;

        do
        {
            xml_node arg_for_each(cur);
            if (!walker.for_each(arg_for_each))
                return false;

            if (cur->first_child)
            {
                ++walker._depth;
                cur = cur->first_child;
            }
            else if (cur->next_sibling)
                cur = cur->next_sibling;
            else
            {
                while (!cur->next_sibling && cur != _root && cur->parent)
                {
                    --walker._depth;
                    cur = cur->parent;
                }

                if (cur != _root)
                    cur = cur->next_sibling;
            }
        }
        while (cur && cur != _root);
    }

    assert(walker._depth == -1);

    xml_node arg_end(_root);
    return walker.end(arg_end);
}

PUGI__FN ptrdiff_t xml_node::offset_debug() const noexcept
{
    if (!_root) return -1;

    impl::xml_document_struct& doc = impl::get_document(_root);

    // we can determine the offset reliably only if there is exactly once parse buffer
    if (!doc.buffer || doc.extra_buffers) return -1;

    switch (type())
    {
    case node_document:
        return 0;

    case node_element:
    case node_declaration:
    case node_pi:
        return _root->name && (_root->header & impl::xml_memory_page_name_allocated_or_shared_mask) == 0 ? _root->name - doc.buffer : -1;

    case node_pcdata:
    case node_cdata:
    case node_comment:
    case node_doctype:
        return _root->value && (_root->header & impl::xml_memory_page_value_allocated_or_shared_mask) == 0 ? _root->value - doc.buffer : -1;

    default:
        assert(false && "Invalid node type"); // unreachable
        return -1;
    }
}


PUGI__FN xml_node_struct* xml_text::_data() const noexcept
{
    if (!_root || impl::is_text_node(_root)) return _root;

    // element nodes can have value if parse_embed_pcdata was used
    if (PUGI__NODETYPE(_root) == node_element && _root->value)
        return _root;

    for (xml_node_struct* node = _root->first_child; node; node = node->next_sibling)
        if (impl::is_text_node(node))
            return node;

    return 0;
}

PUGI__FN xml_node_struct* xml_text::_data_new() noexcept
{
    xml_node_struct* d = _data();
    if (d) return d;

    return xml_node(_root).append_child(node_pcdata).internal_object();
}

PUGI__FN static void unspecified_bool_xml_text(xml_text***) noexcept
{
}

PUGI__FN xml_text::operator xml_text::unspecified_bool_type() const noexcept
{
    return _data() ? unspecified_bool_xml_text : 0;
}

PUGI__FN bool xml_text::empty() const noexcept
{
    return _data() == 0;
}

PUGI__FN const char* xml_text::get() const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return "";
    const char* value = d->value;
    return value ? value : "";
}

PUGI__FN const char* xml_text::as_string(const char* def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? value : def;
}

PUGI__FN int xml_text::as_int(int def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? impl::get_value_int(value) : def;
}

PUGI__FN uint xml_text::as_uint(uint def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? impl::get_value_uint(value) : def;
}

PUGI__FN double xml_text::as_double(double def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? impl::get_value_double(value) : def;
}

PUGI__FN float xml_text::as_float(float def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? impl::get_value_float(value) : def;
}

PUGI__FN bool xml_text::as_bool(bool def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? impl::get_value_bool(value) : def;
}

PUGI__FN long long xml_text::as_llong(long long def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? impl::get_value_llong(value) : def;
}

PUGI__FN unsigned long long xml_text::as_ullong(unsigned long long def) const noexcept
{
    xml_node_struct* d = _data();
    if (!d) return def;
    const char* value = d->value;
    return value ? impl::get_value_ullong(value) : def;
}

PUGI__FN bool xml_text::set(const char* rhs, size_t sz) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::strcpy_insitu(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, sz) : false;
}

PUGI__FN bool xml_text::set(const char* rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::strcpy_insitu(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, ::strlen(rhs)) : false;
}

PUGI__FN bool xml_text::set(int rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_integer<uint>(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, rhs < 0) : false;
}

PUGI__FN bool xml_text::set(uint rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_integer<uint>(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, false) : false;
}

PUGI__FN bool xml_text::set(long rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_integer<unsigned long>(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, rhs < 0) : false;
}

PUGI__FN bool xml_text::set(unsigned long rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_integer<unsigned long>(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, false) : false;
}

PUGI__FN bool xml_text::set(float rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_convert(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, default_float_precision) : false;
}

PUGI__FN bool xml_text::set(float rhs, int precision) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_convert(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, precision) : false;
}

PUGI__FN bool xml_text::set(double rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_convert(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, default_double_precision) : false;
}

PUGI__FN bool xml_text::set(double rhs, int precision) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_convert(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, precision) : false;
}

PUGI__FN bool xml_text::set(bool rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_bool(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs) : false;
}

PUGI__FN bool xml_text::set(long long rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_integer<unsigned long long>(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, rhs < 0) : false;
}

PUGI__FN bool xml_text::set(unsigned long long rhs) noexcept
{
    xml_node_struct* dn = _data_new();

    return dn ? impl::set_value_integer<unsigned long long>(dn->value, dn->header, impl::xml_memory_page_value_allocated_mask, rhs, false) : false;
}

PUGI__FN xml_text& xml_text::operator=(const char* rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(int rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(uint rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(long rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(unsigned long rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(double rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(float rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(bool rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(long long rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_text& xml_text::operator=(unsigned long long rhs) noexcept
{
    set(rhs);
    return *this;
}

PUGI__FN xml_node xml_text::data() const noexcept
{
    return xml_node(_data());
}

PUGI__FN xml_node_iterator::xml_node_iterator(const xml_node& node) noexcept
    : _wrap(node), _parent(node.parent())
{
}

PUGI__FN xml_node_iterator::xml_node_iterator(xml_node_struct* ref, xml_node_struct* parent) noexcept
    : _wrap(ref), _parent(parent)
{
}

PUGI__FN bool xml_node_iterator::operator==(const xml_node_iterator& rhs) const noexcept
{
    return _wrap._root == rhs._wrap._root && _parent._root == rhs._parent._root;
}

PUGI__FN bool xml_node_iterator::operator!=(const xml_node_iterator& rhs) const noexcept
{
    return _wrap._root != rhs._wrap._root || _parent._root != rhs._parent._root;
}

PUGI__FN xml_node& xml_node_iterator::operator*() const noexcept
{
    assert(_wrap._root);
    return _wrap;
}

PUGI__FN xml_node* xml_node_iterator::operator->() const noexcept
{
    assert(_wrap._root);
    return const_cast<xml_node*>(&_wrap); // BCC5 workaround
}

PUGI__FN xml_node_iterator& xml_node_iterator::operator++() noexcept
{
    assert(_wrap._root);
    _wrap._root = _wrap._root->next_sibling;
    return *this;
}

PUGI__FN xml_node_iterator xml_node_iterator::operator++(int) noexcept
{
    xml_node_iterator temp = *this;
    ++*this;
    return temp;
}

PUGI__FN xml_node_iterator& xml_node_iterator::operator--() noexcept
{
    _wrap = _wrap._root ? _wrap.previous_sibling() : _parent.last_child();
    return *this;
}

PUGI__FN xml_node_iterator xml_node_iterator::operator--(int) noexcept
{
    xml_node_iterator temp = *this;
    --*this;
    return temp;
}


PUGI__FN xml_attribute_iterator::xml_attribute_iterator(const xml_attribute& attr, const xml_node& parent) noexcept
    : _wrap(attr), _parent(parent)
{
}

PUGI__FN xml_attribute_iterator::xml_attribute_iterator(xml_attribute_struct* ref, xml_node_struct* parent) noexcept
    : _wrap(ref), _parent(parent)
{
}

PUGI__FN bool xml_attribute_iterator::operator==(const xml_attribute_iterator& rhs) const noexcept
{
    return _wrap._attr == rhs._wrap._attr && _parent._root == rhs._parent._root;
}

PUGI__FN bool xml_attribute_iterator::operator!=(const xml_attribute_iterator& rhs) const noexcept
{
    return _wrap._attr != rhs._wrap._attr || _parent._root != rhs._parent._root;
}

PUGI__FN xml_attribute& xml_attribute_iterator::operator*() const noexcept
{
    assert(_wrap._attr);
    return _wrap;
}

PUGI__FN xml_attribute* xml_attribute_iterator::operator->() const noexcept
{
    assert(_wrap._attr);
    return const_cast<xml_attribute*>(&_wrap); // BCC5 workaround
}

PUGI__FN xml_attribute_iterator& xml_attribute_iterator::operator++() noexcept
{
    assert(_wrap._attr);
    _wrap._attr = _wrap._attr->next_attribute;
    return *this;
}

PUGI__FN xml_attribute_iterator xml_attribute_iterator::operator++(int) noexcept
{
    xml_attribute_iterator temp = *this;
    ++*this;
    return temp;
}

PUGI__FN xml_attribute_iterator& xml_attribute_iterator::operator--() noexcept
{
    _wrap = _wrap._attr ? _wrap.previous_attribute() : _parent.last_attribute();
    return *this;
}

PUGI__FN xml_attribute_iterator xml_attribute_iterator::operator--(int) noexcept
{
    xml_attribute_iterator temp = *this;
    --*this;
    return temp;
}


PUGI__FN xml_named_node_iterator::xml_named_node_iterator(const xml_node& node, const char* name) noexcept
    : _wrap(node), _parent(node.parent()), _name(name)
{
}

PUGI__FN xml_named_node_iterator::xml_named_node_iterator(xml_node_struct* ref, xml_node_struct* parent, const char* name) noexcept
    : _wrap(ref), _parent(parent), _name(name)
{
}

PUGI__FN bool xml_named_node_iterator::operator==(const xml_named_node_iterator& rhs) const noexcept
{
    return _wrap._root == rhs._wrap._root && _parent._root == rhs._parent._root;
}

PUGI__FN bool xml_named_node_iterator::operator!=(const xml_named_node_iterator& rhs) const noexcept
{
    return _wrap._root != rhs._wrap._root || _parent._root != rhs._parent._root;
}

PUGI__FN xml_node& xml_named_node_iterator::operator*() const noexcept
{
    assert(_wrap._root);
    return _wrap;
}

    PUGI__FN xml_node* xml_named_node_iterator::operator->() const noexcept
    {
        assert(_wrap._root);
        return const_cast<xml_node*>(&_wrap); // BCC5 workaround
    }

    PUGI__FN xml_named_node_iterator& xml_named_node_iterator::operator++() noexcept
    {
        assert(_wrap._root);
        _wrap = _wrap.next_sibling(_name);
        return *this;
    }

    PUGI__FN xml_named_node_iterator xml_named_node_iterator::operator++(int) noexcept
    {
        xml_named_node_iterator temp = *this;
        ++*this;
        return temp;
    }

    PUGI__FN xml_named_node_iterator& xml_named_node_iterator::operator--() noexcept
    {
        if (_wrap._root)
            _wrap = _wrap.previous_sibling(_name);
        else
        {
            _wrap = _parent.last_child();

            if (!impl::strequal(_wrap.name(), _name))
                _wrap = _wrap.previous_sibling(_name);
        }

        return *this;
    }

PUGI__FN xml_named_node_iterator xml_named_node_iterator::operator--(int) noexcept
{
    xml_named_node_iterator temp = *this;
    --*this;
    return temp;
}

PUGI__FN const char* xml_parse_result::description() const noexcept
{
    switch (status)
    {
    case status_ok: return "No error";

    case status_file_not_found: return "File was not found";
    case status_io_error: return "Error reading from file/stream";
    case status_out_of_memory: return "Could not allocate memory";
    case status_internal_error: return "Internal error occurred";

    case status_unrecognized_tag: return "Could not determine tag type";

    case status_bad_pi: return "Error parsing document declaration/processing instruction";
    case status_bad_comment: return "Error parsing comment";
    case status_bad_cdata: return "Error parsing CDATA section";
    case status_bad_doctype: return "Error parsing document type declaration";
    case status_bad_pcdata: return "Error parsing PCDATA section";
    case status_bad_start_element: return "Error parsing start element tag";
    case status_bad_attribute: return "Error parsing element attribute";
    case status_bad_end_element: return "Error parsing end element tag";
    case status_end_element_mismatch: return "Start-end tags mismatch";

    case status_append_invalid_root: return "Unable to append nodes: root is not an element or document";

    case status_no_document_element: return "No document element found";

    default: return "Unknown error";
    }
}


PUGI__FN xml_document::xml_document() noexcept
    : _buffer(0)
{
    _create();
}

PUGI__FN xml_document::~xml_document()
{
    _destroy();
}

PUGI__FN xml_document::xml_document(xml_document&& rhs) noexcept
    : _buffer(0)
{
    _create();
    _move(rhs);
}

PUGI__FN xml_document& xml_document::operator=(xml_document&& rhs) noexcept
{
    if (this == &rhs) return *this;

    _destroy();
    _create();
    _move(rhs);

    return *this;
}

PUGI__FN void xml_document::reset() noexcept
{
    _destroy();
    _create();
}

PUGI__FN void xml_document::reset(const xml_document& proto) noexcept
{
    reset();

    impl::node_copy_tree(_root, proto._root);
}

PUGI__FN void xml_document::_create() noexcept
{
    assert(!_root);

#ifdef PUGIXML_COMPACT
    // space for page marker for the first page (uint32_t), rounded up to pointer size; assumes pointers are at least 32-bit
    const size_t page_offset = sizeof(void*);
#else
    const size_t page_offset = 0;
#endif

    // initialize sentinel page
    static_assert(sizeof(impl::xml_memory_page) + sizeof(impl::xml_document_struct) + page_offset <= sizeof(_memory));

    // prepare page structure
    impl::xml_memory_page* page = impl::xml_memory_page::construct(_memory);
    assert(page);

    page->busy_size = impl::xml_memory_page_size;

    // setup first page marker
#ifdef PUGIXML_COMPACT
    // round-trip through void* to avoid 'cast increases required alignment of target type' warning
    page->compact_page_marker = reinterpret_cast<uint32_t*>(static_cast<void*>(reinterpret_cast<char*>(page) + sizeof(impl::xml_memory_page)));
    *page->compact_page_marker = sizeof(impl::xml_memory_page);
#endif

    // allocate new root
    _root = new (reinterpret_cast<char*>(page) + sizeof(impl::xml_memory_page) + page_offset) impl::xml_document_struct(page);
    _root->prev_sibling_c = _root;

    // setup sentinel page
    page->allocator = static_cast<impl::xml_document_struct*>(_root);

    // setup hash table pointer in allocator
#ifdef PUGIXML_COMPACT
    page->allocator->_hash = &static_cast<impl::xml_document_struct*>(_root)->hash;
#endif

    // verify the document allocation
    assert(reinterpret_cast<char*>(_root) + sizeof(impl::xml_document_struct) <= _memory + sizeof(_memory));
}

PUGI__FN void xml_document::_destroy() noexcept
{
    assert(_root);

    // destroy static storage
    if (_buffer)
    {
        ::free(_buffer);
        _buffer = 0;
    }

    // destroy extra buffers (note: no need to destroy linked list nodes, they're allocated using document allocator)
    for (impl::xml_extra_buffer* extra = static_cast<impl::xml_document_struct*>(_root)->extra_buffers; extra; extra = extra->next)
    {
        if (extra->buffer) ::free(extra->buffer);
    }

    // destroy dynamic storage, leave sentinel page (it's in static memory)
    impl::xml_memory_page* root_page = PUGI__GETPAGE(_root);
    assert(root_page && !root_page->prev);
    assert(reinterpret_cast<char*>(root_page) >= _memory && reinterpret_cast<char*>(root_page) < _memory + sizeof(_memory));

    for (impl::xml_memory_page* page = root_page->next; page; )
    {
        impl::xml_memory_page* next = page->next;

        impl::xml_allocator::deallocate_page(page);

        page = next;
    }

#ifdef PUGIXML_COMPACT
    // destroy hash table
    static_cast<impl::xml_document_struct*>(_root)->hash.clear();
#endif

    _root = 0;
}

PUGI__FN void xml_document::_move(xml_document& rhs) noexcept
{
    impl::xml_document_struct* doc = static_cast<impl::xml_document_struct*>(_root);
    impl::xml_document_struct* other = static_cast<impl::xml_document_struct*>(rhs._root);

    // save first child pointer for later; this needs hash access
    xml_node_struct* other_first_child = other->first_child;

    // move allocation state
    // note that other->_root may point to the embedded document page, in which case we should keep original (empty) state
    if (other->_root != PUGI__GETPAGE(other))
    {
        doc->_root = other->_root;
        doc->_busy_size = other->_busy_size;
    }

    // move buffer state
    doc->buffer = other->buffer;
    doc->extra_buffers = other->extra_buffers;
    _buffer = rhs._buffer;

    // move page structure
    impl::xml_memory_page* doc_page = PUGI__GETPAGE(doc);
    assert(doc_page && !doc_page->prev && !doc_page->next);

    impl::xml_memory_page* other_page = PUGI__GETPAGE(other);
    assert(other_page && !other_page->prev);

    // relink pages since root page is embedded into xml_document
    if (impl::xml_memory_page* page = other_page->next)
    {
        assert(page->prev == other_page);

        page->prev = doc_page;

        doc_page->next = page;
        other_page->next = 0;
    }

    // make sure pages point to the correct document state
    for (impl::xml_memory_page* page = doc_page->next; page; page = page->next)
    {
        assert(page->allocator == other);

        page->allocator = doc;

    #ifdef PUGIXML_COMPACT
        // this automatically migrates most children between documents and prevents ->parent assignment from allocating
        if (page->compact_shared_parent == other)
            page->compact_shared_parent = doc;
    #endif
    }

    // move tree structure
    assert(!doc->first_child);

    doc->first_child = other_first_child;

    for (xml_node_struct* node = other_first_child; node; node = node->next_sibling)
    {
    #ifdef PUGIXML_COMPACT
        // most children will have migrated when we reassigned compact_shared_parent
        assert(node->parent == other || node->parent == doc);

        node->parent = doc;
    #else
        assert(node->parent == other);
        node->parent = doc;
    #endif
    }

    // reset other document
    new (other) impl::xml_document_struct(PUGI__GETPAGE(other));
    rhs._buffer = 0;
}

PUGI__FN xml_parse_result xml_document::load(std::istream& stream, uint options, xml_encoding) noexcept
{
    reset();

    return impl::load_stream_impl(static_cast<impl::xml_document_struct*>(_root),
                    stream, options, encoding_utf8, &_buffer);
}

PUGI__FN xml_parse_result xml_document::load_string(const char* contents, uint options) noexcept
{
    return load_buffer(contents, ::strlen(contents) * sizeof(char), options, encoding_utf8);
}

PUGI__FN xml_parse_result xml_document::load(const char* contents, uint options) noexcept
{
    return load_string(contents, options);
}

PUGI__FN xml_parse_result xml_document::load_file(const char* path_, uint options, xml_encoding) noexcept
{
    reset();

    FILE* f = ::fopen(path_, "rb");
    if (!f)
        return impl::make_parse_result(status_io_error);

    xml_parse_result result;
    result = impl::load_file_impl(static_cast<impl::xml_document_struct*>(_root), f, options, encoding_utf8, &_buffer);
    if (f) ::fclose(f);
    return result;
}

PUGI__FN xml_parse_result xml_document::load_buffer(const void* contents, size_t size, uint options, xml_encoding) noexcept
{
    reset();

    return impl::load_buffer_impl( static_cast<impl::xml_document_struct*>(_root), _root,
                const_cast<void*>(contents), size, options, encoding_utf8, false, false, &_buffer );
}

PUGI__FN xml_parse_result xml_document::load_buffer_inplace(void* contents, size_t size, uint options, xml_encoding) noexcept
{
    reset();

    return impl::load_buffer_impl( static_cast<impl::xml_document_struct*>(_root), _root,
                contents, size, options, encoding_utf8, true, false, &_buffer );
}


//PUGI__FN xml_parse_result xml_document::load_buffer_inplace_own(void* contents, size_t size, uint options, xml_encoding) noexcept
//{
//    reset();
//
//    return impl::load_buffer_impl( static_cast<impl::xml_document_struct*>(_root), _root,
//                contents, size, options, encoding_utf8, true, true, &_buffer );
//}



PUGI__FN xml_node xml_document::document_element() const noexcept
{
    assert(_root);

    for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
        if (PUGI__NODETYPE(i) == node_element)
            return xml_node(i);

    return xml_node();
}

} // NS

#if !defined(PUGIXML_NO_STL) && (defined(_MSC_VER) || defined(__ICC))
namespace std
{
    // Workarounds for (non-standard) iterator category detection for older versions (MSVC7/IC8 and earlier)
    PUGI__FN std::bidirectional_iterator_tag _Iter_cat(const pugRd::xml_node_iterator&)
    {
        return std::bidirectional_iterator_tag();
    }

    PUGI__FN std::bidirectional_iterator_tag _Iter_cat(const pugRd::xml_attribute_iterator&)
    {
        return std::bidirectional_iterator_tag();
    }

    PUGI__FN std::bidirectional_iterator_tag _Iter_cat(const pugRd::xml_named_node_iterator&)
    {
        return std::bidirectional_iterator_tag();
    }
}
#endif


#ifndef PUGIXML_NO_XPATH
// STL replacements
PUGI__NS_BEGIN
    struct equal_to
    {
        template <typename T> bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs == rhs;
        }
    };

    struct not_equal_to
    {
        template <typename T> bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs != rhs;
        }
    };

    struct less
    {
        template <typename T> bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs < rhs;
        }
    };

    struct less_equal
    {
        template <typename T> bool operator()(const T& lhs, const T& rhs) const
        {
            return lhs <= rhs;
        }
    };

    template <typename T> inline void swap(T& lhs, T& rhs)
    {
        T temp = lhs;
        lhs = rhs;
        rhs = temp;
    }

    template <typename I, typename Pred> PUGI__FN I min_element(I begin, I end, const Pred& pred)
    {
        I result = begin;

        for (I it = begin + 1; it != end; ++it)
            if (pred(*it, *result))
                result = it;

        return result;
    }

    template <typename I> PUGI__FN void reverse(I begin, I end)
    {
        while (end - begin > 1)
            swap(*begin++, *--end);
    }

    template <typename I> PUGI__FN I unique(I begin, I end)
    {
        // fast skip head
        while (end - begin > 1 && *begin != *(begin + 1))
            begin++;

        if (begin == end)
            return begin;

        // last written element
        I write = begin++;

        // merge unique elements
        while (begin != end)
        {
            if (*begin != *write)
                *++write = *begin++;
            else
                begin++;
        }

        // past-the-end (write points to live element)
        return write + 1;
    }

    template <typename T, typename Pred> PUGI__FN void insertion_sort(T* begin, T* end, const Pred& pred)
    {
        if (begin == end)
            return;

        for (T* it = begin + 1; it != end; ++it)
        {
            T val = *it;
            T* hole = it;

            // move hole backwards
            while (hole > begin && pred(val, *(hole - 1)))
            {
                *hole = *(hole - 1);
                hole--;
            }

            // fill hole with element
            *hole = val;
        }
    }

    template <typename I, typename Pred> inline I median3(I first, I middle, I last, const Pred& pred)
    {
        if (pred(*middle, *first))
            swap(middle, first);
        if (pred(*last, *middle))
            swap(last, middle);
        if (pred(*middle, *first))
            swap(middle, first);

        return middle;
    }

    template <typename T, typename Pred> PUGI__FN void partition3(T* begin, T* end, T pivot, const Pred& pred, T** out_eqbeg, T** out_eqend)
    {
        // invariant: array is split into 4 groups: = < ? > (each variable denotes the boundary between the groups)
        T* eq = begin;
        T* lt = begin;
        T* gt = end;

        while (lt < gt)
        {
            if (pred(*lt, pivot))
                lt++;
            else if (*lt == pivot)
                swap(*eq++, *lt++);
            else
                swap(*lt, *--gt);
        }

        // we now have just 4 groups: = < >; move equal elements to the middle
        T* eqbeg = gt;

        for (T* it = begin; it != eq; ++it)
            swap(*it, *--eqbeg);

        *out_eqbeg = eqbeg;
        *out_eqend = gt;
    }

    template <typename I, typename Pred> PUGI__FN void sort(I begin, I end, const Pred& pred)
    {
        // sort large chunks
        while (end - begin > 16)
        {
            // find median element
            I middle = begin + (end - begin) / 2;
            I median = median3(begin, middle, end - 1, pred);

            // partition in three chunks (< = >)
            I eqbeg, eqend;
            partition3(begin, end, *median, pred, &eqbeg, &eqend);

            // loop on larger half
            if (eqbeg - begin > end - eqend)
            {
                sort(eqend, end, pred);
                end = eqbeg;
            }
            else
            {
                sort(begin, eqbeg, pred);
                begin = eqend;
            }
        }

        // insertion sort small chunk
        insertion_sort(begin, end, pred);
    }

    PUGI__FN bool hash_insert(const void** table, size_t size, const void* key)
    {
        assert(key);

        uint h = static_cast<uint>(reinterpret_cast<uintptr_t>(key));

        // MurmurHash3 32-bit finalizer
        h ^= h >> 16;
        h *= 0x85ebca6bu;
        h ^= h >> 13;
        h *= 0xc2b2ae35u;
        h ^= h >> 16;

        size_t hashmod = size - 1;
        size_t bucket = h & hashmod;

        for (size_t probe = 0; probe <= hashmod; ++probe)
        {
            if (table[bucket] == 0)
            {
                table[bucket] = key;
                return true;
            }

            if (table[bucket] == key)
                return false;

            // hash collision, quadratic probing
            bucket = (bucket + probe + 1) & hashmod;
        }

        assert(false && "Hash table is full"); // unreachable
        return false;
    }
PUGI__NS_END

// Allocator used for AST and evaluation stacks
PUGI__NS_BEGIN
    static const size_t xpath_memory_page_size =
    #ifdef PUGIXML_MEMORY_XPATH_PAGE_SIZE
        PUGIXML_MEMORY_XPATH_PAGE_SIZE
    #else
        4096
    #endif
        ;

    static const uintptr_t xpath_memory_block_alignment = sizeof(double) > sizeof(void*) ? sizeof(double) : sizeof(void*);

    struct xpath_memory_block
    {
        xpath_memory_block* next;
        size_t capacity;

        union
        {
            char data[xpath_memory_page_size];
            double alignment;
        };
    };

    struct xpath_allocator
    {
        xpath_memory_block* _root;
        size_t _root_size;
        bool* _error;

        xpath_allocator(xpath_memory_block* root, bool* error = 0): _root(root), _root_size(0), _error(error)
        {
        }

        void* allocate(size_t size)
        {
            // round size up to block alignment boundary
            size = (size + xpath_memory_block_alignment - 1) & ~(xpath_memory_block_alignment - 1);

            if (_root_size + size <= _root->capacity)
            {
                void* buf = &_root->data[0] + _root_size;
                _root_size += size;
                return buf;
            }
            else
            {
                // make sure we have at least 1/4th of the page free after allocation to satisfy subsequent allocation requests
                size_t block_capacity_base = sizeof(_root->data);
                size_t block_capacity_req = size + block_capacity_base / 4;
                size_t block_capacity = (block_capacity_base > block_capacity_req) ? block_capacity_base : block_capacity_req;

                size_t block_size = block_capacity + offsetof(xpath_memory_block, data);

                xpath_memory_block* block = static_cast<xpath_memory_block*>(::malloc(block_size));
                if (!block)
                {
                    if (_error) *_error = true;
                    return 0;
                }

                block->next = _root;
                block->capacity = block_capacity;

                _root = block;
                _root_size = size;

                return block->data;
            }
        }

        void* reallocate(void* ptr, size_t old_size, size_t new_size)
        {
            // round size up to block alignment boundary
            old_size = (old_size + xpath_memory_block_alignment - 1) & ~(xpath_memory_block_alignment - 1);
            new_size = (new_size + xpath_memory_block_alignment - 1) & ~(xpath_memory_block_alignment - 1);

            // we can only reallocate the last object
            assert(ptr == 0 || static_cast<char*>(ptr) + old_size == &_root->data[0] + _root_size);

            // try to reallocate the object inplace
            if (ptr && _root_size - old_size + new_size <= _root->capacity)
            {
                _root_size = _root_size - old_size + new_size;
                return ptr;
            }

            // allocate a new block
            void* result = allocate(new_size);
            if (!result) return 0;

            // we have a new block
            if (ptr)
            {
                // copy old data (we only support growing)
                assert(new_size >= old_size);
                memcpy(result, ptr, old_size);

                // free the previous page if it had no other objects
                assert(_root->data == result);
                assert(_root->next);

                if (_root->next->data == ptr)
                {
                    // deallocate the whole page, unless it was the first one
                    xpath_memory_block* next = _root->next->next;

                    if (next)
                    {
                        ::free(_root->next);
                        _root->next = next;
                    }
                }
            }

            return result;
        }

        void revert(const xpath_allocator& state)
        {
            // free all new pages
            xpath_memory_block* cur = _root;

            while (cur != state._root)
            {
                xpath_memory_block* next = cur->next;

                ::free(cur);

                cur = next;
            }

            // restore state
            _root = state._root;
            _root_size = state._root_size;
        }

        void release()
        {
            xpath_memory_block* cur = _root;
            assert(cur);

            while (cur->next)
            {
                xpath_memory_block* next = cur->next;

                ::free(cur);

                cur = next;
            }
        }
    };

    struct xpath_allocator_capture
    {
        xpath_allocator_capture(xpath_allocator* alloc): _target(alloc), _state(*alloc)
        {
        }

        ~xpath_allocator_capture()
        {
            _target->revert(_state);
        }

        xpath_allocator* _target;
        xpath_allocator _state;
    };

    struct xpath_stack
    {
        xpath_allocator* result;
        xpath_allocator* temp;
    };

    struct xpath_stack_data
    {
        xpath_memory_block blocks[2];
        xpath_allocator result;
        xpath_allocator temp;
        xpath_stack stack;
        bool oom;

        xpath_stack_data(): result(blocks + 0, &oom), temp(blocks + 1, &oom), oom(false)
        {
            blocks[0].next = blocks[1].next = 0;
            blocks[0].capacity = blocks[1].capacity = sizeof(blocks[0].data);

            stack.result = &result;
            stack.temp = &temp;
        }

        ~xpath_stack_data()
        {
            result.release();
            temp.release();
        }
    };
PUGI__NS_END

// String class
PUGI__NS_BEGIN
    class xpath_string
    {
        const char* _buffer;
        bool _uses_heap;
        size_t _length_heap;

        static char* duplicate_string(const char* string, size_t length, xpath_allocator* alloc)
        {
            char* result = static_cast<char*>(alloc->allocate((length + 1) * sizeof(char)));
            if (!result) return 0;

            memcpy(result, string, length * sizeof(char));
            result[length] = 0;

            return result;
        }

        xpath_string(const char* buffer, bool uses_heap_, size_t length_heap): _buffer(buffer), _uses_heap(uses_heap_), _length_heap(length_heap)
        {
        }

    public:
        static xpath_string from_const(const char* str)
        {
            return xpath_string(str, false, 0);
        }

        static xpath_string from_heap_preallocated(const char* begin, const char* end)
        {
            assert(begin <= end && *end == 0);

            return xpath_string(begin, true, static_cast<size_t>(end - begin));
        }

        static xpath_string from_heap(const char* begin, const char* end, xpath_allocator* alloc)
        {
            assert(begin <= end);

            if (begin == end)
                return xpath_string();

            size_t length = static_cast<size_t>(end - begin);
            const char* data = duplicate_string(begin, length, alloc);

            return data ? xpath_string(data, true, length) : xpath_string();
        }

        xpath_string(): _buffer(""), _uses_heap(false), _length_heap(0)
        {
        }

        void append(const xpath_string& o, xpath_allocator* alloc)
        {
            // skip empty sources
            if (!*o._buffer) return;

            // fast append for constant empty target and constant source
            if (!*_buffer && !_uses_heap && !o._uses_heap)
            {
                _buffer = o._buffer;
            }
            else
            {
                // need to make heap copy
                size_t target_length = length();
                size_t source_length = o.length();
                size_t result_length = target_length + source_length;

                // allocate new buffer
                char* result = static_cast<char*>(alloc->reallocate(_uses_heap ? const_cast<char*>(_buffer) : 0, (target_length + 1) * sizeof(char), (result_length + 1) * sizeof(char)));
                if (!result) return;

                // append first string to the new buffer in case there was no reallocation
                if (!_uses_heap) memcpy(result, _buffer, target_length * sizeof(char));

                // append second string to the new buffer
                memcpy(result + target_length, o._buffer, source_length * sizeof(char));
                result[result_length] = 0;

                // finalize
                _buffer = result;
                _uses_heap = true;
                _length_heap = result_length;
            }
        }

        const char* c_str() const
        {
            return _buffer;
        }

        size_t length() const
        {
            return _uses_heap ? _length_heap : ::strlen(_buffer);
        }

        char* data(xpath_allocator* alloc)
        {
            // make private heap copy
            if (!_uses_heap)
            {
                size_t length_ = ::strlen(_buffer);
                const char* data_ = duplicate_string(_buffer, length_, alloc);

                if (!data_) return 0;

                _buffer = data_;
                _uses_heap = true;
                _length_heap = length_;
            }

            return const_cast<char*>(_buffer);
        }

        bool empty() const
        {
            return *_buffer == 0;
        }

        bool operator==(const xpath_string& o) const
        {
            return strequal(_buffer, o._buffer);
        }

        bool operator!=(const xpath_string& o) const
        {
            return !strequal(_buffer, o._buffer);
        }

        bool uses_heap() const
        {
            return _uses_heap;
        }
    };
PUGI__NS_END

PUGI__NS_BEGIN
    PUGI__FN bool starts_with(const char* string, const char* pattern)
    {
        while (*pattern && *string == *pattern)
        {
            string++;
            pattern++;
        }

        return *pattern == 0;
    }

    PUGI__FN const char* find_char(const char* s, char c)
    {
        return strchr(s, c);
    }

    PUGI__FN const char* find_substring(const char* s, const char* p)
    {
        return strstr(s, p);
    }

    // Converts symbol to lower case, if it is an ASCII one
    PUGI__FN char tolower_ascii(char ch)
    {
        return static_cast<uint>(ch - 'A') < 26 ? static_cast<char>(ch | ' ') : ch;
    }

    PUGI__FN xpath_string string_value(const xpath_node& na, xpath_allocator* alloc)
    {
        if (na.attribute())
            return xpath_string::from_const(na.attribute().value());
        else
        {
            xml_node n = na.node();

            switch (n.type())
            {
            case node_pcdata:
            case node_cdata:
            case node_comment:
            case node_pi:
                return xpath_string::from_const(n.value());

            case node_document:
            case node_element:
            {
                xpath_string result;

                // element nodes can have value if parse_embed_pcdata was used
                if (n.value()[0])
                    result.append(xpath_string::from_const(n.value()), alloc);

                xml_node cur = n.first_child();

                while (cur && cur != n)
                {
                    if (cur.type() == node_pcdata || cur.type() == node_cdata)
                        result.append(xpath_string::from_const(cur.value()), alloc);

                    if (cur.first_child())
                        cur = cur.first_child();
                    else if (cur.next_sibling())
                        cur = cur.next_sibling();
                    else
                    {
                        while (!cur.next_sibling() && cur != n)
                            cur = cur.parent();

                        if (cur != n) cur = cur.next_sibling();
                    }
                }

                return result;
            }

            default:
                return xpath_string();
            }
        }
    }

    PUGI__FN bool node_is_before_sibling(xml_node_struct* ln, xml_node_struct* rn)
    {
        assert(ln->parent == rn->parent);

        // there is no common ancestor (the shared parent is null), nodes are from different documents
        if (!ln->parent) return ln < rn;

        // determine sibling order
        xml_node_struct* ls = ln;
        xml_node_struct* rs = rn;

        while (ls && rs)
        {
            if (ls == rn) return true;
            if (rs == ln) return false;

            ls = ls->next_sibling;
            rs = rs->next_sibling;
        }

        // if rn sibling chain ended ln must be before rn
        return !rs;
    }

    PUGI__FN bool node_is_before(xml_node_struct* ln, xml_node_struct* rn)
    {
        // find common ancestor at the same depth, if any
        xml_node_struct* lp = ln;
        xml_node_struct* rp = rn;

        while (lp && rp && lp->parent != rp->parent)
        {
            lp = lp->parent;
            rp = rp->parent;
        }

        // parents are the same!
        if (lp && rp) return node_is_before_sibling(lp, rp);

        // nodes are at different depths, need to normalize heights
        bool left_higher = !lp;

        while (lp)
        {
            lp = lp->parent;
            ln = ln->parent;
        }

        while (rp)
        {
            rp = rp->parent;
            rn = rn->parent;
        }

        // one node is the ancestor of the other
        if (ln == rn) return left_higher;

        // find common ancestor... again
        while (ln->parent != rn->parent)
        {
            ln = ln->parent;
            rn = rn->parent;
        }

        return node_is_before_sibling(ln, rn);
    }

    PUGI__FN bool node_is_ancestor(xml_node_struct* parent, xml_node_struct* node)
    {
        while (node && node != parent) node = node->parent;

        return parent && node == parent;
    }

    PUGI__FN const void* document_buffer_order(const xpath_node& xnode)
    {
        xml_node_struct* node = xnode.node().internal_object();

        if (node)
        {
            if ((get_document(node).header & xml_memory_page_contents_shared_mask) == 0)
            {
                if (node->name && (node->header & impl::xml_memory_page_name_allocated_or_shared_mask) == 0) return node->name;
                if (node->value && (node->header & impl::xml_memory_page_value_allocated_or_shared_mask) == 0) return node->value;
            }

            return 0;
        }

        xml_attribute_struct* attr = xnode.attribute().internal_object();

        if (attr)
        {
            if ((get_document(attr).header & xml_memory_page_contents_shared_mask) == 0)
            {
                if ((attr->header & impl::xml_memory_page_name_allocated_or_shared_mask) == 0) return attr->name;
                if ((attr->header & impl::xml_memory_page_value_allocated_or_shared_mask) == 0) return attr->value;
            }

            return 0;
        }

        return 0;
    }

    struct document_order_comparator
    {
        bool operator()(const xpath_node& lhs, const xpath_node& rhs) const noexcept
        {
            // optimized document order based check
            const void* lo = document_buffer_order(lhs);
            const void* ro = document_buffer_order(rhs);

            if (lo && ro) return lo < ro;

            // slow comparison
            xml_node ln = lhs.node(), rn = rhs.node();

            // compare attributes
            if (lhs.attribute() && rhs.attribute())
            {
                // shared parent
                if (lhs.parent() == rhs.parent())
                {
                    // determine sibling order
                    for (xml_attribute a = lhs.attribute(); a; a = a.next_attribute())
                        if (a == rhs.attribute())
                            return true;

                    return false;
                }

                // compare attribute parents
                ln = lhs.parent();
                rn = rhs.parent();
            }
            else if (lhs.attribute())
            {
                // attributes go after the parent element
                if (lhs.parent() == rhs.node()) return false;

                ln = lhs.parent();
            }
            else if (rhs.attribute())
            {
                // attributes go after the parent element
                if (rhs.parent() == lhs.node()) return true;

                rn = rhs.parent();
            }

            if (ln == rn) return false;

            if (!ln || !rn) return ln < rn;

            return node_is_before(ln.internal_object(), rn.internal_object());
        }
    };

    PUGI__FN double gen_nan() noexcept
    {
    #if defined(__STDC_IEC_559__) || ((FLT_RADIX - 0 == 2) && (FLT_MAX_EXP - 0 == 128) && (FLT_MANT_DIG - 0 == 24))
        static_assert(sizeof(float) == sizeof(uint32_t));
        typedef uint32_t UI; // BCC5 workaround
        union { float f; UI i; } u;
        u.i = 0x7fc00000;
        return double(u.f);
    #else
        // fallback
        const volatile double zero = 0.0;
        return zero / zero;
    #endif
    }

    PUGI__FN bool is_nan(double value) noexcept
    {
    #if defined(PUGI__MSVC_CRT_VERSION) || defined(__BORLANDC__)
        return !!_isnan(value);
    #elif defined(fpclassify) && defined(FP_NAN)
        return fpclassify(value) == FP_NAN;
    #else
        // fallback
        const volatile double v = value;
        return v != v;
    #endif
    }

    PUGI__FN const char* convert_number_to_string_special(double value) noexcept
    {
    #if defined(PUGI__MSVC_CRT_VERSION) || defined(__BORLANDC__)
        if (_finite(value)) return (value == 0) ? "0" : 0;
        if (_isnan(value)) return "NaN";
        return value > 0 ? "Infinity" : "-Infinity";
    #elif defined(fpclassify) && defined(FP_NAN) && defined(FP_INFINITE) && defined(FP_ZERO)
        switch (fpclassify(value))
        {
        case FP_NAN:
            return "NaN";

        case FP_INFINITE:
            return value > 0 ? "Infinity" : "-Infinity";

        case FP_ZERO:
            return "0";

        default:
            return 0;
        }
    #else
        // fallback
        const volatile double v = value;

        if (v == 0) return "0";
        if (v != v) return "NaN";
        if (v * 2 == v) return value > 0 ? "Infinity" : "-Infinity";
        return 0;
    #endif
    }

    PUGI__FN bool convert_number_to_boolean(double value)
    {
        return (value != 0 && !is_nan(value));
    }

    PUGI__FN void truncate_zeros(char* begin, char* end)
    {
        while (begin != end && end[-1] == '0') end--;

        *end = 0;
    }

    // gets mantissa digits in the form of 0.xxxxx with 0. implied and the exponent
#if defined(PUGI__MSVC_CRT_VERSION) && PUGI__MSVC_CRT_VERSION >= 1400
    PUGI__FN void convert_number_to_mantissa_exponent(double value, char (&buffer)[32], char** out_mantissa, int* out_exponent)
    {
        // get base values
        int sign, exponent;
        _ecvt_s(buffer, sizeof(buffer), value, DBL_DIG + 1, &exponent, &sign);

        // truncate redundant zeros
        truncate_zeros(buffer, buffer + strlen(buffer));

        // fill results
        *out_mantissa = buffer;
        *out_exponent = exponent;
    }
#else
    PUGI__FN void convert_number_to_mantissa_exponent(double value, char (&buffer)[32], char** out_mantissa, int* out_exponent)
    {
        // get a scientific notation value with IEEE DBL_DIG decimals
        PUGI__SNPRINTF(buffer, "%.*e", DBL_DIG, value);

        // get the exponent (possibly negative)
        char* exponent_string = strchr(buffer, 'e');
        assert(exponent_string);

        int exponent = atoi(exponent_string + 1);

        // extract mantissa string: skip sign
        char* mantissa = buffer[0] == '-' ? buffer + 1 : buffer;
        assert(mantissa[0] != '0' && mantissa[1] == '.');

        // divide mantissa by 10 to eliminate integer part
        mantissa[1] = mantissa[0];
        mantissa++;
        exponent++;

        // remove extra mantissa digits and zero-terminate mantissa
        truncate_zeros(mantissa, exponent_string);

        // fill results
        *out_mantissa = mantissa;
        *out_exponent = exponent;
    }
#endif

    PUGI__FN xpath_string convert_number_to_string(double value, xpath_allocator* alloc)
    {
        // try special number conversion
        const char* special = convert_number_to_string_special(value);
        if (special) return xpath_string::from_const(special);

        // get mantissa + exponent form
        char mantissa_buffer[32];

        char* mantissa;
        int exponent;
        convert_number_to_mantissa_exponent(value, mantissa_buffer, &mantissa, &exponent);

        // allocate a buffer of suitable length for the number
        size_t result_size = strlen(mantissa_buffer) + (exponent > 0 ? exponent : -exponent) + 4;
        char* result = static_cast<char*>(alloc->allocate(sizeof(char) * result_size));
        if (!result) return xpath_string();

        // make the number!
        char* s = result;

        // sign
        if (value < 0) *s++ = '-';

        // integer part
        if (exponent <= 0)
        {
            *s++ = '0';
        }
        else
        {
            while (exponent > 0)
            {
                assert(*mantissa == 0 || static_cast<uint>(*mantissa - '0') <= 9);
                *s++ = *mantissa ? *mantissa++ : '0';
                exponent--;
            }
        }

        // fractional part
        if (*mantissa)
        {
            // decimal point
            *s++ = '.';

            // extra zeroes from negative exponent
            while (exponent < 0)
            {
                *s++ = '0';
                exponent++;
            }

            // extra mantissa digits
            while (*mantissa)
            {
                assert(static_cast<uint>(*mantissa - '0') <= 9);
                *s++ = *mantissa++;
            }
        }

        // zero-terminate
        assert(s < result + result_size);
        *s = 0;

        return xpath_string::from_heap_preallocated(result, s);
    }

    PUGI__FN bool check_string_to_number_format(const char* string)
    {
        // parse leading whitespace
        while (PUGI__IS_CHARTYPE(*string, ct_space)) ++string;

        // parse sign
        if (*string == '-') ++string;

        if (!*string) return false;

        // if there is no integer part, there should be a decimal part with at least one digit
        if (!PUGI__IS_CHARTYPEX(string[0], ctx_digit) && (string[0] != '.' || !PUGI__IS_CHARTYPEX(string[1], ctx_digit))) return false;

        // parse integer part
        while (PUGI__IS_CHARTYPEX(*string, ctx_digit)) ++string;

        // parse decimal part
        if (*string == '.')
        {
            ++string;

            while (PUGI__IS_CHARTYPEX(*string, ctx_digit)) ++string;
        }

        // parse trailing whitespace
        while (PUGI__IS_CHARTYPE(*string, ct_space)) ++string;

        return *string == 0;
    }

    PUGI__FN double convert_string_to_number(const char* string)
    {
        // check string format
        if (!check_string_to_number_format(string)) return gen_nan();

        return strtod(string, 0);
    }

    PUGI__FN bool convert_string_to_number_scratch(char (&buffer)[32], const char* begin, const char* end, double* out_result)
    {
        size_t length = static_cast<size_t>(end - begin);
        char* scratch = buffer;

        if (length >= sizeof(buffer) / sizeof(buffer[0]))
        {
            // need to make dummy on-heap copy
            scratch = static_cast<char*>(::malloc((length + 1) * sizeof(char)));
            if (!scratch) return false;
        }

        // copy string to zero-terminated buffer and perform conversion
        memcpy(scratch, begin, length * sizeof(char));
        scratch[length] = 0;

        *out_result = convert_string_to_number(scratch);

        // free dummy buffer
        if (scratch != buffer) ::free(scratch);

        return true;
    }

    PUGI__FN double round_nearest(double value)
    {
        return floor(value + 0.5);
    }

    PUGI__FN double round_nearest_nzero(double value)
    {
        // same as round_nearest, but returns -0 for [-0.5, -0]
        // ceil is used to differentiate between +0 and -0 (we return -0 for [-0.5, -0] and +0 for +0)
        return (value >= -0.5 && value <= 0) ? ceil(value) : floor(value + 0.5);
    }

    PUGI__FN const char* qualified_name(const xpath_node& node)
    {
        return node.attribute() ? node.attribute().name() : node.node().name();
    }

    PUGI__FN const char* local_name(const xpath_node& node)
    {
        const char* name = qualified_name(node);
        const char* p = find_char(name, ':');

        return p ? p + 1 : name;
    }

    struct namespace_uri_predicate
    {
        const char* prefix;
        size_t prefix_length;

        namespace_uri_predicate(const char* name)
        {
            const char* pos = find_char(name, ':');

            prefix = pos ? name : 0;
            prefix_length = pos ? static_cast<size_t>(pos - name) : 0;
        }

        bool operator()(xml_attribute a) const
        {
            const char* name = a.name();

            if (!starts_with(name, "xmlns")) return false;

            return prefix ? name[5] == ':' && strequalrange(name + 6, prefix, prefix_length) : name[5] == 0;
        }
    };

    PUGI__FN const char* namespace_uri(xml_node node)
    {
        namespace_uri_predicate pred = node.name();

        xml_node p = node;

        while (p)
        {
            xml_attribute a = p.find_attribute(pred);

            if (a) return a.value();

            p = p.parent();
        }

        return "";
    }

    PUGI__FN const char* namespace_uri(xml_attribute attr, xml_node parent)
    {
        namespace_uri_predicate pred = attr.name();

        // Default namespace does not apply to attributes
        if (!pred.prefix) return "";

        xml_node p = parent;

        while (p)
        {
            xml_attribute a = p.find_attribute(pred);

            if (a) return a.value();

            p = p.parent();
        }

        return "";
    }

    PUGI__FN const char* namespace_uri(const xpath_node& node)
    {
        return node.attribute() ? namespace_uri(node.attribute(), node.parent()) : namespace_uri(node.node());
    }

    PUGI__FN char* normalize_space(char* buffer)
    {
        char* write = buffer;

        for (char* it = buffer; *it; )
        {
            char ch = *it++;

            if (PUGI__IS_CHARTYPE(ch, ct_space))
            {
                // replace whitespace sequence with single space
                while (PUGI__IS_CHARTYPE(*it, ct_space)) it++;

                // avoid leading spaces
                if (write != buffer) *write++ = ' ';
            }
            else *write++ = ch;
        }

        // remove trailing space
        if (write != buffer && PUGI__IS_CHARTYPE(write[-1], ct_space)) write--;

        // zero-terminate
        *write = 0;

        return write;
    }

    PUGI__FN char* translate(char* buffer, const char* from, const char* to, size_t to_length)
    {
        char* write = buffer;

        while (*buffer)
        {
            PUGI__DMC_VOLATILE char ch = *buffer++;

            const char* pos = find_char(from, ch);

            if (!pos)
                *write++ = ch; // do not process
            else if (static_cast<size_t>(pos - from) < to_length)
                *write++ = to[pos - from]; // replace
        }

        // zero-terminate
        *write = 0;

        return write;
    }

    PUGI__FN unsigned char* translate_table_generate(xpath_allocator* alloc, const char* from, const char* to)
    {
        unsigned char table[128] = {0};

        while (*from)
        {
            uint fc = static_cast<uint>(*from);
            uint tc = static_cast<uint>(*to);

            if (fc >= 128 || tc >= 128)
                return 0;

            // code=128 means "skip character"
            if (!table[fc])
                table[fc] = static_cast<unsigned char>(tc ? tc : 128);

            from++;
            if (tc) to++;
        }

        for (int i = 0; i < 128; ++i)
            if (!table[i])
                table[i] = static_cast<unsigned char>(i);

        void* result = alloc->allocate(sizeof(table));
        if (!result) return 0;

        memcpy(result, table, sizeof(table));

        return static_cast<unsigned char*>(result);
    }

    PUGI__FN char* translate_table(char* buffer, const unsigned char* table)
    {
        char* write = buffer;

        while (*buffer)
        {
            char ch = *buffer++;
            uint index = static_cast<uint>(ch);

            if (index < 128)
            {
                unsigned char code = table[index];

                // code=128 means "skip character" (table size is 128 so 128 can be a special value)
                // this code skips these characters without extra branches
                *write = static_cast<char>(code);
                write += 1 - (code >> 7);
            }
            else
            {
                *write++ = ch;
            }
        }

        // zero-terminate
        *write = 0;

        return write;
    }

    inline bool is_xpath_attribute(const char* name)
    {
        return !(starts_with(name, "xmlns") && (name[5] == 0 || name[5] == ':'));
    }

    struct xpath_variable_boolean: xpath_variable
    {
        xpath_variable_boolean(): xpath_variable(xpath_type_boolean), value(false)
        {
        }

        bool value;
        char name[1];
    };

    struct xpath_variable_number: xpath_variable
    {
        xpath_variable_number(): xpath_variable(xpath_type_number), value(0)
        {
        }

        double value;
        char name[1];
    };

    struct xpath_variable_string: xpath_variable
    {
        xpath_variable_string(): xpath_variable(xpath_type_string), value(0)
        {
        }

        ~xpath_variable_string()
        {
            if (value) ::free(value);
        }

        char* value;
        char name[1];
    };

    struct xpath_variable_node_set: xpath_variable
    {
        xpath_variable_node_set(): xpath_variable(xpath_type_node_set)
        {
        }

        xpath_node_set value;
        char name[1];
    };

    static const xpath_node_set dummy_node_set;

    PUGI__FN PUGI__UNSIGNED_OVERFLOW uint hash_string(const char* str)
    {
        // Jenkins one-at-a-time hash (http://en.wikipedia.org/wiki/Jenkins_hash_function#one-at-a-time)
        uint result = 0;

        while (*str)
        {
            result += static_cast<uint>(*str++);
            result += result << 10;
            result ^= result >> 6;
        }

        result += result << 3;
        result ^= result >> 11;
        result += result << 15;

        return result;
    }

    template <typename T> PUGI__FN T* new_xpath_variable(const char* name)
    {
        size_t length = ::strlen(name);
        if (length == 0) return 0; // empty variable names are invalid

        // $$ we can't use offsetof(T, name) because T is non-POD, so we just allocate additional length characters
        void* memory = ::malloc(sizeof(T) + length * sizeof(char));
        if (!memory) return 0;

        T* result = new (memory) T();

        memcpy(result->name, name, (length + 1) * sizeof(char));

        return result;
    }

    PUGI__FN xpath_variable* new_xpath_variable(xpath_value_type type, const char* name)
    {
        switch (type)
        {
        case xpath_type_node_set:
            return new_xpath_variable<xpath_variable_node_set>(name);

        case xpath_type_number:
            return new_xpath_variable<xpath_variable_number>(name);

        case xpath_type_string:
            return new_xpath_variable<xpath_variable_string>(name);

        case xpath_type_boolean:
            return new_xpath_variable<xpath_variable_boolean>(name);

        default:
            return 0;
        }
    }

    template <typename T> PUGI__FN void delete_xpath_variable(T* var)
    {
        var->~T();
        ::free(var);
    }

    PUGI__FN void delete_xpath_variable(xpath_value_type type, xpath_variable* var)
    {
        switch (type)
        {
        case xpath_type_node_set:
            delete_xpath_variable(static_cast<xpath_variable_node_set*>(var));
            break;

        case xpath_type_number:
            delete_xpath_variable(static_cast<xpath_variable_number*>(var));
            break;

        case xpath_type_string:
            delete_xpath_variable(static_cast<xpath_variable_string*>(var));
            break;

        case xpath_type_boolean:
            delete_xpath_variable(static_cast<xpath_variable_boolean*>(var));
            break;

        default:
            assert(false && "Invalid variable type"); // unreachable
        }
    }

    PUGI__FN bool copy_xpath_variable(xpath_variable* lhs, const xpath_variable* rhs)
    {
        switch (rhs->type())
        {
        case xpath_type_node_set:
            return lhs->set(static_cast<const xpath_variable_node_set*>(rhs)->value);

        case xpath_type_number:
            return lhs->set(static_cast<const xpath_variable_number*>(rhs)->value);

        case xpath_type_string:
            return lhs->set(static_cast<const xpath_variable_string*>(rhs)->value);

        case xpath_type_boolean:
            return lhs->set(static_cast<const xpath_variable_boolean*>(rhs)->value);

        default:
            assert(false && "Invalid variable type"); // unreachable
            return false;
        }
    }

    PUGI__FN bool get_variable_scratch(char (&buffer)[32], xpath_variable_set* set, const char* begin, const char* end, xpath_variable** out_result)
    {
        size_t length = static_cast<size_t>(end - begin);
        char* scratch = buffer;

        if (length >= sizeof(buffer) / sizeof(buffer[0]))
        {
            // need to make dummy on-heap copy
            scratch = static_cast<char*>(::malloc((length + 1) * sizeof(char)));
            if (!scratch) return false;
        }

        // copy string to zero-terminated buffer and perform lookup
        memcpy(scratch, begin, length * sizeof(char));
        scratch[length] = 0;

        *out_result = set->get(scratch);

        // free dummy buffer
        if (scratch != buffer) ::free(scratch);

        return true;
    }
PUGI__NS_END

// Internal node set class
PUGI__NS_BEGIN
    PUGI__FN xpath_node_set::type_t xpath_get_order(const xpath_node* begin, const xpath_node* end)
    {
        if (end - begin < 2)
            return xpath_node_set::type_sorted;

        document_order_comparator cmp;

        bool first = cmp(begin[0], begin[1]);

        for (const xpath_node* it = begin + 1; it + 1 < end; ++it)
            if (cmp(it[0], it[1]) != first)
                return xpath_node_set::type_unsorted;

        return first ? xpath_node_set::type_sorted : xpath_node_set::type_sorted_reverse;
    }

    PUGI__FN xpath_node_set::type_t xpath_sort(xpath_node* begin, xpath_node* end, xpath_node_set::type_t type, bool rev)
    {
        xpath_node_set::type_t order = rev ? xpath_node_set::type_sorted_reverse : xpath_node_set::type_sorted;

        if (type == xpath_node_set::type_unsorted)
        {
            xpath_node_set::type_t sorted = xpath_get_order(begin, end);

            if (sorted == xpath_node_set::type_unsorted)
            {
                sort(begin, end, document_order_comparator());

                type = xpath_node_set::type_sorted;
            }
            else
                type = sorted;
        }

        if (type != order) reverse(begin, end);

        return order;
    }

    PUGI__FN xpath_node xpath_first(const xpath_node* begin, const xpath_node* end, xpath_node_set::type_t type)
    {
        if (begin == end) return xpath_node();

        switch (type)
        {
        case xpath_node_set::type_sorted:
            return *begin;

        case xpath_node_set::type_sorted_reverse:
            return *(end - 1);

        case xpath_node_set::type_unsorted:
            return *min_element(begin, end, document_order_comparator());

        default:
            assert(false && "Invalid node set type"); // unreachable
            return xpath_node();
        }
    }

    class xpath_node_set_raw
    {
        xpath_node_set::type_t _type;

        xpath_node* _begin;
        xpath_node* _end;
        xpath_node* _eos;

    public:
        xpath_node_set_raw(): _type(xpath_node_set::type_unsorted), _begin(0), _end(0), _eos(0)
        {
        }

        xpath_node* begin() const
        {
            return _begin;
        }

        xpath_node* end() const
        {
            return _end;
        }

        bool empty() const
        {
            return _begin == _end;
        }

        size_t size() const
        {
            return static_cast<size_t>(_end - _begin);
        }

        xpath_node first() const
        {
            return xpath_first(_begin, _end, _type);
        }

        void push_back_grow(const xpath_node& node, xpath_allocator* alloc);

        void push_back(const xpath_node& node, xpath_allocator* alloc)
        {
            if (_end != _eos)
                *_end++ = node;
            else
                push_back_grow(node, alloc);
        }

        void append(const xpath_node* begin_, const xpath_node* end_, xpath_allocator* alloc)
        {
            if (begin_ == end_) return;

            size_t size_ = static_cast<size_t>(_end - _begin);
            size_t capacity = static_cast<size_t>(_eos - _begin);
            size_t count = static_cast<size_t>(end_ - begin_);

            if (size_ + count > capacity)
            {
                // reallocate the old array or allocate a new one
                xpath_node* data = static_cast<xpath_node*>(alloc->reallocate(_begin, capacity * sizeof(xpath_node), (size_ + count) * sizeof(xpath_node)));
                if (!data) return;

                // finalize
                _begin = data;
                _end = data + size_;
                _eos = data + size_ + count;
            }

            memcpy(_end, begin_, count * sizeof(xpath_node));
            _end += count;
        }

        void sort_do()
        {
            _type = xpath_sort(_begin, _end, _type, false);
        }

        void truncate(xpath_node* pos)
        {
            assert(_begin <= pos && pos <= _end);

            _end = pos;
        }

        void remove_duplicates(xpath_allocator* alloc)
        {
            if (_type == xpath_node_set::type_unsorted && _end - _begin > 2)
            {
                xpath_allocator_capture cr(alloc);

                size_t size_ = static_cast<size_t>(_end - _begin);

                size_t hash_size = 1;
                while (hash_size < size_ + size_ / 2) hash_size *= 2;

                const void** hash_data = static_cast<const void**>(alloc->allocate(hash_size * sizeof(void**)));
                if (!hash_data) return;

                memset(hash_data, 0, hash_size * sizeof(const void**));

                xpath_node* write = _begin;

                for (xpath_node* it = _begin; it != _end; ++it)
                {
                    const void* attr = it->attribute().internal_object();
                    const void* node = it->node().internal_object();
                    const void* key = attr ? attr : node;

                    if (key && hash_insert(hash_data, hash_size, key))
                    {
                        *write++ = *it;
                    }
                }

                _end = write;
            }
            else
            {
                _end = unique(_begin, _end);
            }
        }

        xpath_node_set::type_t type() const
        {
            return _type;
        }

        void set_type(xpath_node_set::type_t value)
        {
            _type = value;
        }
    };

    PUGI__FN_NO_INLINE void xpath_node_set_raw::push_back_grow(const xpath_node& node, xpath_allocator* alloc)
    {
        size_t capacity = static_cast<size_t>(_eos - _begin);

        // get new capacity (1.5x rule)
        size_t new_capacity = capacity + capacity / 2 + 1;

        // reallocate the old array or allocate a new one
        xpath_node* data = static_cast<xpath_node*>(alloc->reallocate(_begin, capacity * sizeof(xpath_node), new_capacity * sizeof(xpath_node)));
        if (!data) return;

        // finalize
        _begin = data;
        _end = data + capacity;
        _eos = data + new_capacity;

        // push
        *_end++ = node;
    }
PUGI__NS_END

PUGI__NS_BEGIN
    struct xpath_context
    {
        xpath_node n;
        size_t position, size;

        xpath_context(const xpath_node& n_, size_t position_, size_t size_): n(n_), position(position_), size(size_)
        {
        }
    };

    enum lexeme_t
    {
        lex_none = 0,
        lex_equal,
        lex_not_equal,
        lex_less,
        lex_greater,
        lex_less_or_equal,
        lex_greater_or_equal,
        lex_plus,
        lex_minus,
        lex_multiply,
        lex_union,
        lex_var_ref,
        lex_open_brace,
        lex_close_brace,
        lex_quoted_string,
        lex_number,
        lex_slash,
        lex_double_slash,
        lex_open_square_brace,
        lex_close_square_brace,
        lex_string,
        lex_comma,
        lex_axis_attribute,
        lex_dot,
        lex_double_dot,
        lex_double_colon,
        lex_eof
    };

    struct xpath_lexer_string
    {
        const char* begin;
        const char* end;

        xpath_lexer_string(): begin(0), end(0)
        {
        }

        bool operator==(const char* other) const
        {
            size_t length = static_cast<size_t>(end - begin);

            return strequalrange(other, begin, length);
        }
    };

    class xpath_lexer
    {
        const char* _cur;
        const char* _cur_lexeme_pos;
        xpath_lexer_string _cur_lexeme_contents;

        lexeme_t _cur_lexeme;

    public:
        explicit xpath_lexer(const char* query): _cur(query)
        {
            next();
        }

        const char* state() const
        {
            return _cur;
        }

        void next()
        {
            const char* cur = _cur;

            while (PUGI__IS_CHARTYPE(*cur, ct_space)) ++cur;

            // save lexeme position for error reporting
            _cur_lexeme_pos = cur;

            switch (*cur)
            {
            case 0:
                _cur_lexeme = lex_eof;
                break;

            case '>':
                if (*(cur+1) == '=')
                {
                    cur += 2;
                    _cur_lexeme = lex_greater_or_equal;
                }
                else
                {
                    cur += 1;
                    _cur_lexeme = lex_greater;
                }
                break;

            case '<':
                if (*(cur+1) == '=')
                {
                    cur += 2;
                    _cur_lexeme = lex_less_or_equal;
                }
                else
                {
                    cur += 1;
                    _cur_lexeme = lex_less;
                }
                break;

            case '!':
                if (*(cur+1) == '=')
                {
                    cur += 2;
                    _cur_lexeme = lex_not_equal;
                }
                else
                {
                    _cur_lexeme = lex_none;
                }
                break;

            case '=':
                cur += 1;
                _cur_lexeme = lex_equal;

                break;

            case '+':
                cur += 1;
                _cur_lexeme = lex_plus;

                break;

            case '-':
                cur += 1;
                _cur_lexeme = lex_minus;

                break;

            case '*':
                cur += 1;
                _cur_lexeme = lex_multiply;

                break;

            case '|':
                cur += 1;
                _cur_lexeme = lex_union;

                break;

            case '$':
                cur += 1;

                if (PUGI__IS_CHARTYPEX(*cur, ctx_start_symbol))
                {
                    _cur_lexeme_contents.begin = cur;

                    while (PUGI__IS_CHARTYPEX(*cur, ctx_symbol)) cur++;

                    if (cur[0] == ':' && PUGI__IS_CHARTYPEX(cur[1], ctx_symbol)) // qname
                    {
                        cur++; // :

                        while (PUGI__IS_CHARTYPEX(*cur, ctx_symbol)) cur++;
                    }

                    _cur_lexeme_contents.end = cur;

                    _cur_lexeme = lex_var_ref;
                }
                else
                {
                    _cur_lexeme = lex_none;
                }

                break;

            case '(':
                cur += 1;
                _cur_lexeme = lex_open_brace;

                break;

            case ')':
                cur += 1;
                _cur_lexeme = lex_close_brace;

                break;

            case '[':
                cur += 1;
                _cur_lexeme = lex_open_square_brace;

                break;

            case ']':
                cur += 1;
                _cur_lexeme = lex_close_square_brace;

                break;

            case ',':
                cur += 1;
                _cur_lexeme = lex_comma;

                break;

            case '/':
                if (*(cur+1) == '/')
                {
                    cur += 2;
                    _cur_lexeme = lex_double_slash;
                }
                else
                {
                    cur += 1;
                    _cur_lexeme = lex_slash;
                }
                break;

            case '.':
                if (*(cur+1) == '.')
                {
                    cur += 2;
                    _cur_lexeme = lex_double_dot;
                }
                else if (PUGI__IS_CHARTYPEX(*(cur+1), ctx_digit))
                {
                    _cur_lexeme_contents.begin = cur; // .

                    ++cur;

                    while (PUGI__IS_CHARTYPEX(*cur, ctx_digit)) cur++;

                    _cur_lexeme_contents.end = cur;

                    _cur_lexeme = lex_number;
                }
                else
                {
                    cur += 1;
                    _cur_lexeme = lex_dot;
                }
                break;

            case '@':
                cur += 1;
                _cur_lexeme = lex_axis_attribute;

                break;

            case '"':
            case '\'':
            {
                char terminator = *cur;

                ++cur;

                _cur_lexeme_contents.begin = cur;
                while (*cur && *cur != terminator) cur++;
                _cur_lexeme_contents.end = cur;

                if (!*cur)
                    _cur_lexeme = lex_none;
                else
                {
                    cur += 1;
                    _cur_lexeme = lex_quoted_string;
                }

                break;
            }

            case ':':
                if (*(cur+1) == ':')
                {
                    cur += 2;
                    _cur_lexeme = lex_double_colon;
                }
                else
                {
                    _cur_lexeme = lex_none;
                }
                break;

            default:
                if (PUGI__IS_CHARTYPEX(*cur, ctx_digit))
                {
                    _cur_lexeme_contents.begin = cur;

                    while (PUGI__IS_CHARTYPEX(*cur, ctx_digit)) cur++;

                    if (*cur == '.')
                    {
                        cur++;

                        while (PUGI__IS_CHARTYPEX(*cur, ctx_digit)) cur++;
                    }

                    _cur_lexeme_contents.end = cur;

                    _cur_lexeme = lex_number;
                }
                else if (PUGI__IS_CHARTYPEX(*cur, ctx_start_symbol))
                {
                    _cur_lexeme_contents.begin = cur;

                    while (PUGI__IS_CHARTYPEX(*cur, ctx_symbol)) cur++;

                    if (cur[0] == ':')
                    {
                        if (cur[1] == '*') // namespace test ncname:*
                        {
                            cur += 2; // :*
                        }
                        else if (PUGI__IS_CHARTYPEX(cur[1], ctx_symbol)) // namespace test qname
                        {
                            cur++; // :

                            while (PUGI__IS_CHARTYPEX(*cur, ctx_symbol)) cur++;
                        }
                    }

                    _cur_lexeme_contents.end = cur;

                    _cur_lexeme = lex_string;
                }
                else
                {
                    _cur_lexeme = lex_none;
                }
            }

            _cur = cur;
        }

        lexeme_t current() const
        {
            return _cur_lexeme;
        }

        const char* current_pos() const
        {
            return _cur_lexeme_pos;
        }

        const xpath_lexer_string& contents() const
        {
            assert(_cur_lexeme == lex_var_ref || _cur_lexeme == lex_number || _cur_lexeme == lex_string || _cur_lexeme == lex_quoted_string);

            return _cur_lexeme_contents;
        }
    };

    enum ast_type_t
    {
        ast_unknown,
        ast_op_or,                        // left or right
        ast_op_and,                        // left and right
        ast_op_equal,                    // left = right
        ast_op_not_equal,                // left != right
        ast_op_less,                    // left < right
        ast_op_greater,                    // left > right
        ast_op_less_or_equal,            // left <= right
        ast_op_greater_or_equal,        // left >= right
        ast_op_add,                        // left + right
        ast_op_subtract,                // left - right
        ast_op_multiply,                // left * right
        ast_op_divide,                    // left / right
        ast_op_mod,                        // left % right
        ast_op_negate,                    // left - right
        ast_op_union,                    // left | right
        ast_predicate,                    // apply predicate to set; next points to next predicate
        ast_filter,                        // select * from left where right
        ast_string_constant,            // string constant
        ast_number_constant,            // number constant
        ast_variable,                    // variable
        ast_func_last,                    // last()
        ast_func_position,                // position()
        ast_func_count,                    // count(left)
        ast_func_id,                    // id(left)
        ast_func_local_name_0,            // local-name()
        ast_func_local_name_1,            // local-name(left)
        ast_func_namespace_uri_0,        // namespace-uri()
        ast_func_namespace_uri_1,        // namespace-uri(left)
        ast_func_name_0,                // name()
        ast_func_name_1,                // name(left)
        ast_func_string_0,                // string()
        ast_func_string_1,                // string(left)
        ast_func_concat,                // concat(left, right, siblings)
        ast_func_starts_with,            // starts_with(left, right)
        ast_func_contains,                // contains(left, right)
        ast_func_substring_before,        // substring-before(left, right)
        ast_func_substring_after,        // substring-after(left, right)
        ast_func_substring_2,            // substring(left, right)
        ast_func_substring_3,            // substring(left, right, third)
        ast_func_string_length_0,        // string-length()
        ast_func_string_length_1,        // string-length(left)
        ast_func_normalize_space_0,        // normalize-space()
        ast_func_normalize_space_1,        // normalize-space(left)
        ast_func_translate,                // translate(left, right, third)
        ast_func_boolean,                // boolean(left)
        ast_func_not,                    // not(left)
        ast_func_true,                    // true()
        ast_func_false,                    // false()
        ast_func_lang,                    // lang(left)
        ast_func_number_0,                // number()
        ast_func_number_1,                // number(left)
        ast_func_sum,                    // sum(left)
        ast_func_floor,                    // floor(left)
        ast_func_ceiling,                // ceiling(left)
        ast_func_round,                    // round(left)
        ast_step,                        // process set left with step
        ast_step_root,                    // select root node

        ast_opt_translate_table,        // translate(left, right, third) where right/third are constants
        ast_opt_compare_attribute        // @name = 'string'
    };

    enum axis_t
    {
        axis_ancestor,
        axis_ancestor_or_self,
        axis_attribute,
        axis_child,
        axis_descendant,
        axis_descendant_or_self,
        axis_following,
        axis_following_sibling,
        axis_namespace,
        axis_parent,
        axis_preceding,
        axis_preceding_sibling,
        axis_self
    };

    enum nodetest_t
    {
        nodetest_none,
        nodetest_name,
        nodetest_type_node,
        nodetest_type_comment,
        nodetest_type_pi,
        nodetest_type_text,
        nodetest_pi,
        nodetest_all,
        nodetest_all_in_namespace
    };

    enum predicate_t
    {
        predicate_default,
        predicate_posinv,
        predicate_constant,
        predicate_constant_one
    };

    enum nodeset_eval_t
    {
        nodeset_eval_all,
        nodeset_eval_any,
        nodeset_eval_first
    };

    template <axis_t N> struct axis_to_type
    {
        static const axis_t axis;
    };

    template <axis_t N> const axis_t axis_to_type<N>::axis = N;

    class xpath_ast_node
    {
    private:
        // node type
        char _type;
        char _rettype;

        // for ast_step
        char _axis;

        // for ast_step/ast_predicate/ast_filter
        char _test;

        // tree node structure
        xpath_ast_node* _left;
        xpath_ast_node* _right;
        xpath_ast_node* _next;

        union
        {
            // value for ast_string_constant
            const char* string;
            // value for ast_number_constant
            double number;
            // variable for ast_variable
            xpath_variable* variable;
            // node test for ast_step (node name/namespace/node type/pi target)
            const char* nodetest;
            // table for ast_opt_translate_table
            const unsigned char* table;
        } _data;

        xpath_ast_node(const xpath_ast_node&);
        xpath_ast_node& operator=(const xpath_ast_node&);

        template <class Comp> static bool compare_eq(xpath_ast_node* lhs, xpath_ast_node* rhs, const xpath_context& c, const xpath_stack& stack, const Comp& comp)
        {
            xpath_value_type lt = lhs->rettype(), rt = rhs->rettype();

            if (lt != xpath_type_node_set && rt != xpath_type_node_set)
            {
                if (lt == xpath_type_boolean || rt == xpath_type_boolean)
                    return comp(lhs->eval_boolean(c, stack), rhs->eval_boolean(c, stack));
                else if (lt == xpath_type_number || rt == xpath_type_number)
                    return comp(lhs->eval_number(c, stack), rhs->eval_number(c, stack));
                else if (lt == xpath_type_string || rt == xpath_type_string)
                {
                    xpath_allocator_capture cr(stack.result);

                    xpath_string ls = lhs->eval_string(c, stack);
                    xpath_string rs = rhs->eval_string(c, stack);

                    return comp(ls, rs);
                }
            }
            else if (lt == xpath_type_node_set && rt == xpath_type_node_set)
            {
                xpath_allocator_capture cr(stack.result);

                xpath_node_set_raw ls = lhs->eval_node_set(c, stack, nodeset_eval_all);
                xpath_node_set_raw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

                for (const xpath_node* li = ls.begin(); li != ls.end(); ++li)
                    for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
                    {
                        xpath_allocator_capture cri(stack.result);

                        if (comp(string_value(*li, stack.result), string_value(*ri, stack.result)))
                            return true;
                    }

                return false;
            }
            else
            {
                if (lt == xpath_type_node_set)
                {
                    swap(lhs, rhs);
                    swap(lt, rt);
                }

                if (lt == xpath_type_boolean)
                    return comp(lhs->eval_boolean(c, stack), rhs->eval_boolean(c, stack));
                else if (lt == xpath_type_number)
                {
                    xpath_allocator_capture cr(stack.result);

                    double l = lhs->eval_number(c, stack);
                    xpath_node_set_raw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

                    for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
                    {
                        xpath_allocator_capture cri(stack.result);

                        if (comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str())))
                            return true;
                    }

                    return false;
                }
                else if (lt == xpath_type_string)
                {
                    xpath_allocator_capture cr(stack.result);

                    xpath_string l = lhs->eval_string(c, stack);
                    xpath_node_set_raw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

                    for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
                    {
                        xpath_allocator_capture cri(stack.result);

                        if (comp(l, string_value(*ri, stack.result)))
                            return true;
                    }

                    return false;
                }
            }

            assert(false && "Wrong types"); // unreachable
            return false;
        }

        static bool eval_once(xpath_node_set::type_t type, nodeset_eval_t eval)
        {
            return type == xpath_node_set::type_sorted ? eval != nodeset_eval_all : eval == nodeset_eval_any;
        }

        template <class Comp> static bool compare_rel(xpath_ast_node* lhs, xpath_ast_node* rhs, const xpath_context& c, const xpath_stack& stack, const Comp& comp)
        {
            xpath_value_type lt = lhs->rettype(), rt = rhs->rettype();

            if (lt != xpath_type_node_set && rt != xpath_type_node_set)
                return comp(lhs->eval_number(c, stack), rhs->eval_number(c, stack));
            else if (lt == xpath_type_node_set && rt == xpath_type_node_set)
            {
                xpath_allocator_capture cr(stack.result);

                xpath_node_set_raw ls = lhs->eval_node_set(c, stack, nodeset_eval_all);
                xpath_node_set_raw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

                for (const xpath_node* li = ls.begin(); li != ls.end(); ++li)
                {
                    xpath_allocator_capture cri(stack.result);

                    double l = convert_string_to_number(string_value(*li, stack.result).c_str());

                    for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
                    {
                        xpath_allocator_capture crii(stack.result);

                        if (comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str())))
                            return true;
                    }
                }

                return false;
            }
            else if (lt != xpath_type_node_set && rt == xpath_type_node_set)
            {
                xpath_allocator_capture cr(stack.result);

                double l = lhs->eval_number(c, stack);
                xpath_node_set_raw rs = rhs->eval_node_set(c, stack, nodeset_eval_all);

                for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
                {
                    xpath_allocator_capture cri(stack.result);

                    if (comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str())))
                        return true;
                }

                return false;
            }
            else if (lt == xpath_type_node_set && rt != xpath_type_node_set)
            {
                xpath_allocator_capture cr(stack.result);

                xpath_node_set_raw ls = lhs->eval_node_set(c, stack, nodeset_eval_all);
                double r = rhs->eval_number(c, stack);

                for (const xpath_node* li = ls.begin(); li != ls.end(); ++li)
                {
                    xpath_allocator_capture cri(stack.result);

                    if (comp(convert_string_to_number(string_value(*li, stack.result).c_str()), r))
                        return true;
                }

                return false;
            }
            else
            {
                assert(false && "Wrong types"); // unreachable
                return false;
            }
        }

        static void apply_predicate_boolean(xpath_node_set_raw& ns, size_t first, xpath_ast_node* expr, const xpath_stack& stack, bool once)
        {
            assert(ns.size() >= first);
            assert(expr->rettype() != xpath_type_number);

            size_t i = 1;
            size_t size = ns.size() - first;

            xpath_node* last = ns.begin() + first;

            // remove_if... or well, sort of
            for (xpath_node* it = last; it != ns.end(); ++it, ++i)
            {
                xpath_context c(*it, i, size);

                if (expr->eval_boolean(c, stack))
                {
                    *last++ = *it;

                    if (once) break;
                }
            }

            ns.truncate(last);
        }

        static void apply_predicate_number(xpath_node_set_raw& ns, size_t first, xpath_ast_node* expr, const xpath_stack& stack, bool once)
        {
            assert(ns.size() >= first);
            assert(expr->rettype() == xpath_type_number);

            size_t i = 1;
            size_t size = ns.size() - first;

            xpath_node* last = ns.begin() + first;

            // remove_if... or well, sort of
            for (xpath_node* it = last; it != ns.end(); ++it, ++i)
            {
                xpath_context c(*it, i, size);

                if (expr->eval_number(c, stack) == static_cast<double>(i))
                {
                    *last++ = *it;

                    if (once) break;
                }
            }

            ns.truncate(last);
        }

        static void apply_predicate_number_const(xpath_node_set_raw& ns, size_t first, xpath_ast_node* expr, const xpath_stack& stack)
        {
            assert(ns.size() >= first);
            assert(expr->rettype() == xpath_type_number);

            size_t size = ns.size() - first;

            xpath_node* last = ns.begin() + first;

            xpath_context c(xpath_node(), 1, size);

            double er = expr->eval_number(c, stack);

            if (er >= 1.0 && er <= static_cast<double>(size))
            {
                size_t eri = static_cast<size_t>(er);

                if (er == static_cast<double>(eri))
                {
                    xpath_node r = last[eri - 1];

                    *last++ = r;
                }
            }

            ns.truncate(last);
        }

        void apply_predicate(xpath_node_set_raw& ns, size_t first, const xpath_stack& stack, bool once)
        {
            if (ns.size() == first) return;

            assert(_type == ast_filter || _type == ast_predicate);

            if (_test == predicate_constant || _test == predicate_constant_one)
                apply_predicate_number_const(ns, first, _right, stack);
            else if (_right->rettype() == xpath_type_number)
                apply_predicate_number(ns, first, _right, stack, once);
            else
                apply_predicate_boolean(ns, first, _right, stack, once);
        }

        void apply_predicates(xpath_node_set_raw& ns, size_t first, const xpath_stack& stack, nodeset_eval_t eval)
        {
            if (ns.size() == first) return;

            bool last_once = eval_once(ns.type(), eval);

            for (xpath_ast_node* pred = _right; pred; pred = pred->_next)
                pred->apply_predicate(ns, first, stack, !pred->_next && last_once);
        }

        bool step_push(xpath_node_set_raw& ns, xml_attribute_struct* a, xml_node_struct* parent, xpath_allocator* alloc)
        {
            assert(a);

            const char* name = a->name ? a->name + 0 : "";

            switch (_test)
            {
            case nodetest_name:
                if (strequal(name, _data.nodetest) && is_xpath_attribute(name))
                {
                    ns.push_back(xpath_node(xml_attribute(a), xml_node(parent)), alloc);
                    return true;
                }
                break;

            case nodetest_type_node:
            case nodetest_all:
                if (is_xpath_attribute(name))
                {
                    ns.push_back(xpath_node(xml_attribute(a), xml_node(parent)), alloc);
                    return true;
                }
                break;

            case nodetest_all_in_namespace:
                if (starts_with(name, _data.nodetest) && is_xpath_attribute(name))
                {
                    ns.push_back(xpath_node(xml_attribute(a), xml_node(parent)), alloc);
                    return true;
                }
                break;

            default:
                ;
            }

            return false;
        }

        bool step_push(xpath_node_set_raw& ns, xml_node_struct* n, xpath_allocator* alloc)
        {
            assert(n);

            xml_node_type type = PUGI__NODETYPE(n);

            switch (_test)
            {
            case nodetest_name:
                if (type == node_element && n->name && strequal(n->name, _data.nodetest))
                {
                    ns.push_back(xml_node(n), alloc);
                    return true;
                }
                break;

            case nodetest_type_node:
                ns.push_back(xml_node(n), alloc);
                return true;

            case nodetest_type_comment:
                if (type == node_comment)
                {
                    ns.push_back(xml_node(n), alloc);
                    return true;
                }
                break;

            case nodetest_type_text:
                if (type == node_pcdata || type == node_cdata)
                {
                    ns.push_back(xml_node(n), alloc);
                    return true;
                }
                break;

            case nodetest_type_pi:
                if (type == node_pi)
                {
                    ns.push_back(xml_node(n), alloc);
                    return true;
                }
                break;

            case nodetest_pi:
                if (type == node_pi && n->name && strequal(n->name, _data.nodetest))
                {
                    ns.push_back(xml_node(n), alloc);
                    return true;
                }
                break;

            case nodetest_all:
                if (type == node_element)
                {
                    ns.push_back(xml_node(n), alloc);
                    return true;
                }
                break;

            case nodetest_all_in_namespace:
                if (type == node_element && n->name && starts_with(n->name, _data.nodetest))
                {
                    ns.push_back(xml_node(n), alloc);
                    return true;
                }
                break;

            default:
                assert(false && "Unknown axis"); // unreachable
            }

            return false;
        }

        template <class T> void step_fill(xpath_node_set_raw& ns, xml_node_struct* n, xpath_allocator* alloc, bool once, T)
        {
            const axis_t axis = T::axis;

            switch (axis)
            {
            case axis_attribute:
            {
                for (xml_attribute_struct* a = n->first_attribute; a; a = a->next_attribute)
                    if (step_push(ns, a, n, alloc) & once)
                        return;

                break;
            }

            case axis_child:
            {
                for (xml_node_struct* c = n->first_child; c; c = c->next_sibling)
                    if (step_push(ns, c, alloc) & once)
                        return;

                break;
            }

            case axis_descendant:
            case axis_descendant_or_self:
            {
                if (axis == axis_descendant_or_self)
                    if (step_push(ns, n, alloc) & once)
                        return;

                xml_node_struct* cur = n->first_child;

                while (cur)
                {
                    if (step_push(ns, cur, alloc) & once)
                        return;

                    if (cur->first_child)
                        cur = cur->first_child;
                    else
                    {
                        while (!cur->next_sibling)
                        {
                            cur = cur->parent;

                            if (cur == n) return;
                        }

                        cur = cur->next_sibling;
                    }
                }

                break;
            }

            case axis_following_sibling:
            {
                for (xml_node_struct* c = n->next_sibling; c; c = c->next_sibling)
                    if (step_push(ns, c, alloc) & once)
                        return;

                break;
            }

            case axis_preceding_sibling:
            {
                for (xml_node_struct* c = n->prev_sibling_c; c->next_sibling; c = c->prev_sibling_c)
                    if (step_push(ns, c, alloc) & once)
                        return;

                break;
            }

            case axis_following:
            {
                xml_node_struct* cur = n;

                // exit from this node so that we don't include descendants
                while (!cur->next_sibling)
                {
                    cur = cur->parent;

                    if (!cur) return;
                }

                cur = cur->next_sibling;

                while (cur)
                {
                    if (step_push(ns, cur, alloc) & once)
                        return;

                    if (cur->first_child)
                        cur = cur->first_child;
                    else
                    {
                        while (!cur->next_sibling)
                        {
                            cur = cur->parent;

                            if (!cur) return;
                        }

                        cur = cur->next_sibling;
                    }
                }

                break;
            }

            case axis_preceding:
            {
                xml_node_struct* cur = n;

                // exit from this node so that we don't include descendants
                while (!cur->prev_sibling_c->next_sibling)
                {
                    cur = cur->parent;

                    if (!cur) return;
                }

                cur = cur->prev_sibling_c;

                while (cur)
                {
                    if (cur->first_child)
                        cur = cur->first_child->prev_sibling_c;
                    else
                    {
                        // leaf node, can't be ancestor
                        if (step_push(ns, cur, alloc) & once)
                            return;

                        while (!cur->prev_sibling_c->next_sibling)
                        {
                            cur = cur->parent;

                            if (!cur) return;

                            if (!node_is_ancestor(cur, n))
                                if (step_push(ns, cur, alloc) & once)
                                    return;
                        }

                        cur = cur->prev_sibling_c;
                    }
                }

                break;
            }

            case axis_ancestor:
            case axis_ancestor_or_self:
            {
                if (axis == axis_ancestor_or_self)
                    if (step_push(ns, n, alloc) & once)
                        return;

                xml_node_struct* cur = n->parent;

                while (cur)
                {
                    if (step_push(ns, cur, alloc) & once)
                        return;

                    cur = cur->parent;
                }

                break;
            }

            case axis_self:
            {
                step_push(ns, n, alloc);

                break;
            }

            case axis_parent:
            {
                if (n->parent)
                    step_push(ns, n->parent, alloc);

                break;
            }

            default:
                assert(false && "Unimplemented axis"); // unreachable
            }
        }

        template <class T> void step_fill(xpath_node_set_raw& ns, xml_attribute_struct* a, xml_node_struct* p, xpath_allocator* alloc, bool once, T v)
        {
            const axis_t axis = T::axis;

            switch (axis)
            {
            case axis_ancestor:
            case axis_ancestor_or_self:
            {
                if (axis == axis_ancestor_or_self && _test == nodetest_type_node) // reject attributes based on principal node type test
                    if (step_push(ns, a, p, alloc) & once)
                        return;

                xml_node_struct* cur = p;

                while (cur)
                {
                    if (step_push(ns, cur, alloc) & once)
                        return;

                    cur = cur->parent;
                }

                break;
            }

            case axis_descendant_or_self:
            case axis_self:
            {
                if (_test == nodetest_type_node) // reject attributes based on principal node type test
                    step_push(ns, a, p, alloc);

                break;
            }

            case axis_following:
            {
                xml_node_struct* cur = p;

                while (cur)
                {
                    if (cur->first_child)
                        cur = cur->first_child;
                    else
                    {
                        while (!cur->next_sibling)
                        {
                            cur = cur->parent;

                            if (!cur) return;
                        }

                        cur = cur->next_sibling;
                    }

                    if (step_push(ns, cur, alloc) & once)
                        return;
                }

                break;
            }

            case axis_parent:
            {
                step_push(ns, p, alloc);

                break;
            }

            case axis_preceding:
            {
                // preceding:: axis does not include attribute nodes and attribute ancestors (they are the same as parent's ancestors), so we can reuse node preceding
                step_fill(ns, p, alloc, once, v);
                break;
            }

            default:
                assert(false && "Unimplemented axis"); // unreachable
            }
        }

        template <class T> void step_fill(xpath_node_set_raw& ns, const xpath_node& xn, xpath_allocator* alloc, bool once, T v)
        {
            const axis_t axis = T::axis;
            const bool axis_has_attributes = (axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_descendant_or_self || axis == axis_following || axis == axis_parent || axis == axis_preceding || axis == axis_self);

            if (xn.node())
                step_fill(ns, xn.node().internal_object(), alloc, once, v);
            else if (axis_has_attributes && xn.attribute() && xn.parent())
                step_fill(ns, xn.attribute().internal_object(), xn.parent().internal_object(), alloc, once, v);
        }

        template <class T> xpath_node_set_raw step_do(const xpath_context& c, const xpath_stack& stack, nodeset_eval_t eval, T v)
        {
            const axis_t axis = T::axis;
            const bool axis_reverse = (axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_preceding || axis == axis_preceding_sibling);
            const xpath_node_set::type_t axis_type = axis_reverse ? xpath_node_set::type_sorted_reverse : xpath_node_set::type_sorted;

            bool once =
                (axis == axis_attribute && _test == nodetest_name) ||
                (!_right && eval_once(axis_type, eval)) ||
                // coverity[mixed_enums]
                (_right && !_right->_next && _right->_test == predicate_constant_one);

            xpath_node_set_raw ns;
            ns.set_type(axis_type);

            if (_left)
            {
                xpath_node_set_raw s = _left->eval_node_set(c, stack, nodeset_eval_all);

                // self axis preserves the original order
                if (axis == axis_self) ns.set_type(s.type());

                for (const xpath_node* it = s.begin(); it != s.end(); ++it)
                {
                    size_t size = ns.size();

                    // in general, all axes generate elements in a particular order, but there is no order guarantee if axis is applied to two nodes
                    if (axis != axis_self && size != 0) ns.set_type(xpath_node_set::type_unsorted);

                    step_fill(ns, *it, stack.result, once, v);
                    if (_right) apply_predicates(ns, size, stack, eval);
                }
            }
            else
            {
                step_fill(ns, c.n, stack.result, once, v);
                if (_right) apply_predicates(ns, 0, stack, eval);
            }

            // child, attribute and self axes always generate unique set of nodes
            // for other axis, if the set stayed sorted, it stayed unique because the traversal algorithms do not visit the same node twice
            if (axis != axis_child && axis != axis_attribute && axis != axis_self && ns.type() == xpath_node_set::type_unsorted)
                ns.remove_duplicates(stack.temp);

            return ns;
        }

    public:
        xpath_ast_node(ast_type_t type, xpath_value_type rettype_, const char* value):
            _type(static_cast<char>(type)), _rettype(static_cast<char>(rettype_)), _axis(0), _test(0), _left(0), _right(0), _next(0)
        {
            assert(type == ast_string_constant);
            _data.string = value;
        }

        xpath_ast_node(ast_type_t type, xpath_value_type rettype_, double value):
            _type(static_cast<char>(type)), _rettype(static_cast<char>(rettype_)), _axis(0), _test(0), _left(0), _right(0), _next(0)
        {
            assert(type == ast_number_constant);
            _data.number = value;
        }

        xpath_ast_node(ast_type_t type, xpath_value_type rettype_, xpath_variable* value):
            _type(static_cast<char>(type)), _rettype(static_cast<char>(rettype_)), _axis(0), _test(0), _left(0), _right(0), _next(0)
        {
            assert(type == ast_variable);
            _data.variable = value;
        }

        xpath_ast_node(ast_type_t type, xpath_value_type rettype_, xpath_ast_node* left = 0, xpath_ast_node* right = 0):
            _type(static_cast<char>(type)), _rettype(static_cast<char>(rettype_)), _axis(0), _test(0), _left(left), _right(right), _next(0)
        {
        }

        xpath_ast_node(ast_type_t type, xpath_ast_node* left, axis_t axis, nodetest_t test, const char* contents):
            _type(static_cast<char>(type)), _rettype(xpath_type_node_set), _axis(static_cast<char>(axis)), _test(static_cast<char>(test)), _left(left), _right(0), _next(0)
        {
            assert(type == ast_step);
            _data.nodetest = contents;
        }

        xpath_ast_node(ast_type_t type, xpath_ast_node* left, xpath_ast_node* right, predicate_t test):
            _type(static_cast<char>(type)), _rettype(xpath_type_node_set), _axis(0), _test(static_cast<char>(test)), _left(left), _right(right), _next(0)
        {
            assert(type == ast_filter || type == ast_predicate);
        }

        void set_next(xpath_ast_node* value)
        {
            _next = value;
        }

        void set_right(xpath_ast_node* value)
        {
            _right = value;
        }

        bool eval_boolean(const xpath_context& c, const xpath_stack& stack)
        {
            switch (_type)
            {
            case ast_op_or:
                return _left->eval_boolean(c, stack) || _right->eval_boolean(c, stack);

            case ast_op_and:
                return _left->eval_boolean(c, stack) && _right->eval_boolean(c, stack);

            case ast_op_equal:
                return compare_eq(_left, _right, c, stack, equal_to());

            case ast_op_not_equal:
                return compare_eq(_left, _right, c, stack, not_equal_to());

            case ast_op_less:
                return compare_rel(_left, _right, c, stack, less());

            case ast_op_greater:
                return compare_rel(_right, _left, c, stack, less());

            case ast_op_less_or_equal:
                return compare_rel(_left, _right, c, stack, less_equal());

            case ast_op_greater_or_equal:
                return compare_rel(_right, _left, c, stack, less_equal());

            case ast_func_starts_with:
            {
                xpath_allocator_capture cr(stack.result);

                xpath_string lr = _left->eval_string(c, stack);
                xpath_string rr = _right->eval_string(c, stack);

                return starts_with(lr.c_str(), rr.c_str());
            }

            case ast_func_contains:
            {
                xpath_allocator_capture cr(stack.result);

                xpath_string lr = _left->eval_string(c, stack);
                xpath_string rr = _right->eval_string(c, stack);

                return find_substring(lr.c_str(), rr.c_str()) != 0;
            }

            case ast_func_boolean:
                return _left->eval_boolean(c, stack);

            case ast_func_not:
                return !_left->eval_boolean(c, stack);

            case ast_func_true:
                return true;

            case ast_func_false:
                return false;

            case ast_func_lang:
            {
                if (c.n.attribute()) return false;

                xpath_allocator_capture cr(stack.result);

                xpath_string lang = _left->eval_string(c, stack);

                for (xml_node n = c.n.node(); n; n = n.parent())
                {
                    xml_attribute a = n.attribute("xml:lang");

                    if (a)
                    {
                        const char* value = a.value();

                        // strnicmp / strncasecmp is not portable
                        for (const char* lit = lang.c_str(); *lit; ++lit)
                        {
                            if (tolower_ascii(*lit) != tolower_ascii(*value)) return false;
                            ++value;
                        }

                        return *value == 0 || *value == '-';
                    }
                }

                return false;
            }

            case ast_opt_compare_attribute:
            {
                const char* value = (_right->_type == ast_string_constant) ? _right->_data.string : _right->_data.variable->get_string();

                xml_attribute attr = c.n.node().attribute(_left->_data.nodetest);

                return attr && strequal(attr.value(), value) && is_xpath_attribute(attr.name());
            }

            case ast_variable:
            {
                assert(_rettype == _data.variable->type());

                if (_rettype == xpath_type_boolean)
                    return _data.variable->get_boolean();

                // variable needs to be converted to the correct type, this is handled by the fallthrough block below
                break;
            }

            default:
                ;
            }

            // none of the ast types that return the value directly matched, we need to perform type conversion
            switch (_rettype)
            {
            case xpath_type_number:
                return convert_number_to_boolean(eval_number(c, stack));

            case xpath_type_string:
            {
                xpath_allocator_capture cr(stack.result);

                return !eval_string(c, stack).empty();
            }

            case xpath_type_node_set:
            {
                xpath_allocator_capture cr(stack.result);

                return !eval_node_set(c, stack, nodeset_eval_any).empty();
            }

            default:
                assert(false && "Wrong expression for return type boolean"); // unreachable
                return false;
            }
        }

        double eval_number(const xpath_context& c, const xpath_stack& stack)
        {
            switch (_type)
            {
            case ast_op_add:
                return _left->eval_number(c, stack) + _right->eval_number(c, stack);

            case ast_op_subtract:
                return _left->eval_number(c, stack) - _right->eval_number(c, stack);

            case ast_op_multiply:
                return _left->eval_number(c, stack) * _right->eval_number(c, stack);

            case ast_op_divide:
                return _left->eval_number(c, stack) / _right->eval_number(c, stack);

            case ast_op_mod:
                return fmod(_left->eval_number(c, stack), _right->eval_number(c, stack));

            case ast_op_negate:
                return -_left->eval_number(c, stack);

            case ast_number_constant:
                return _data.number;

            case ast_func_last:
                return static_cast<double>(c.size);

            case ast_func_position:
                return static_cast<double>(c.position);

            case ast_func_count:
            {
                xpath_allocator_capture cr(stack.result);

                return static_cast<double>(_left->eval_node_set(c, stack, nodeset_eval_all).size());
            }

            case ast_func_string_length_0:
            {
                xpath_allocator_capture cr(stack.result);

                return static_cast<double>(string_value(c.n, stack.result).length());
            }

            case ast_func_string_length_1:
            {
                xpath_allocator_capture cr(stack.result);

                return static_cast<double>(_left->eval_string(c, stack).length());
            }

            case ast_func_number_0:
            {
                xpath_allocator_capture cr(stack.result);

                return convert_string_to_number(string_value(c.n, stack.result).c_str());
            }

            case ast_func_number_1:
                return _left->eval_number(c, stack);

            case ast_func_sum:
            {
                xpath_allocator_capture cr(stack.result);

                double r = 0;

                xpath_node_set_raw ns = _left->eval_node_set(c, stack, nodeset_eval_all);

                for (const xpath_node* it = ns.begin(); it != ns.end(); ++it)
                {
                    xpath_allocator_capture cri(stack.result);

                    r += convert_string_to_number(string_value(*it, stack.result).c_str());
                }

                return r;
            }

            case ast_func_floor:
            {
                double r = _left->eval_number(c, stack);

                return r == r ? floor(r) : r;
            }

            case ast_func_ceiling:
            {
                double r = _left->eval_number(c, stack);

                return r == r ? ceil(r) : r;
            }

            case ast_func_round:
                return round_nearest_nzero(_left->eval_number(c, stack));

            case ast_variable:
            {
                assert(_rettype == _data.variable->type());

                if (_rettype == xpath_type_number)
                    return _data.variable->get_number();

                // variable needs to be converted to the correct type, this is handled by the fallthrough block below
                break;
            }

            default:
                ;
            }

            // none of the ast types that return the value directly matched, we need to perform type conversion
            switch (_rettype)
            {
            case xpath_type_boolean:
                return eval_boolean(c, stack) ? 1 : 0;

            case xpath_type_string:
            {
                xpath_allocator_capture cr(stack.result);

                return convert_string_to_number(eval_string(c, stack).c_str());
            }

            case xpath_type_node_set:
            {
                xpath_allocator_capture cr(stack.result);

                return convert_string_to_number(eval_string(c, stack).c_str());
            }

            default:
                assert(false && "Wrong expression for return type number"); // unreachable
                return 0;
            }
        }

        xpath_string eval_string_concat(const xpath_context& c, const xpath_stack& stack)
        {
            assert(_type == ast_func_concat);

            xpath_allocator_capture ct(stack.temp);

            // count the string number
            size_t count = 1;
            for (xpath_ast_node* nc = _right; nc; nc = nc->_next) count++;

            // allocate a buffer for temporary string objects
            xpath_string* buffer = static_cast<xpath_string*>(stack.temp->allocate(count * sizeof(xpath_string)));
            if (!buffer) return xpath_string();

            // evaluate all strings to temporary stack
            xpath_stack swapped_stack = {stack.temp, stack.result};

            buffer[0] = _left->eval_string(c, swapped_stack);

            size_t pos = 1;
            for (xpath_ast_node* n = _right; n; n = n->_next, ++pos) buffer[pos] = n->eval_string(c, swapped_stack);
            assert(pos == count);

            // get total length
            size_t length = 0;
            for (size_t i = 0; i < count; ++i) length += buffer[i].length();

            // create final string
            char* result = static_cast<char*>(stack.result->allocate((length + 1) * sizeof(char)));
            if (!result) return xpath_string();

            char* ri = result;

            for (size_t j = 0; j < count; ++j)
                for (const char* bi = buffer[j].c_str(); *bi; ++bi)
                    *ri++ = *bi;

            *ri = 0;

            return xpath_string::from_heap_preallocated(result, ri);
        }

        xpath_string eval_string(const xpath_context& c, const xpath_stack& stack)
        {
            switch (_type)
            {
            case ast_string_constant:
                return xpath_string::from_const(_data.string);

            case ast_func_local_name_0:
            {
                xpath_node na = c.n;

                return xpath_string::from_const(local_name(na));
            }

            case ast_func_local_name_1:
            {
                xpath_allocator_capture cr(stack.result);

                xpath_node_set_raw ns = _left->eval_node_set(c, stack, nodeset_eval_first);
                xpath_node na = ns.first();

                return xpath_string::from_const(local_name(na));
            }

            case ast_func_name_0:
            {
                xpath_node na = c.n;

                return xpath_string::from_const(qualified_name(na));
            }

            case ast_func_name_1:
            {
                xpath_allocator_capture cr(stack.result);

                xpath_node_set_raw ns = _left->eval_node_set(c, stack, nodeset_eval_first);
                xpath_node na = ns.first();

                return xpath_string::from_const(qualified_name(na));
            }

            case ast_func_namespace_uri_0:
            {
                xpath_node na = c.n;

                return xpath_string::from_const(namespace_uri(na));
            }

            case ast_func_namespace_uri_1:
            {
                xpath_allocator_capture cr(stack.result);

                xpath_node_set_raw ns = _left->eval_node_set(c, stack, nodeset_eval_first);
                xpath_node na = ns.first();

                return xpath_string::from_const(namespace_uri(na));
            }

            case ast_func_string_0:
                return string_value(c.n, stack.result);

            case ast_func_string_1:
                return _left->eval_string(c, stack);

            case ast_func_concat:
                return eval_string_concat(c, stack);

            case ast_func_substring_before:
            {
                xpath_allocator_capture cr(stack.temp);

                xpath_stack swapped_stack = {stack.temp, stack.result};

                xpath_string s = _left->eval_string(c, swapped_stack);
                xpath_string p = _right->eval_string(c, swapped_stack);

                const char* pos = find_substring(s.c_str(), p.c_str());

                return pos ? xpath_string::from_heap(s.c_str(), pos, stack.result) : xpath_string();
            }

            case ast_func_substring_after:
            {
                xpath_allocator_capture cr(stack.temp);

                xpath_stack swapped_stack = {stack.temp, stack.result};

                xpath_string s = _left->eval_string(c, swapped_stack);
                xpath_string p = _right->eval_string(c, swapped_stack);

                const char* pos = find_substring(s.c_str(), p.c_str());
                if (!pos) return xpath_string();

                const char* rbegin = pos + p.length();
                const char* rend = s.c_str() + s.length();

                return s.uses_heap() ? xpath_string::from_heap(rbegin, rend, stack.result) : xpath_string::from_const(rbegin);
            }

            case ast_func_substring_2:
            {
                xpath_allocator_capture cr(stack.temp);

                xpath_stack swapped_stack = {stack.temp, stack.result};

                xpath_string s = _left->eval_string(c, swapped_stack);
                size_t s_length = s.length();

                double first = round_nearest(_right->eval_number(c, stack));

                if (is_nan(first)) return xpath_string(); // NaN
                else if (first >= static_cast<double>(s_length + 1)) return xpath_string();

                size_t pos = first < 1 ? 1 : static_cast<size_t>(first);
                assert(1 <= pos && pos <= s_length + 1);

                const char* rbegin = s.c_str() + (pos - 1);
                const char* rend = s.c_str() + s.length();

                return s.uses_heap() ? xpath_string::from_heap(rbegin, rend, stack.result) : xpath_string::from_const(rbegin);
            }

            case ast_func_substring_3:
            {
                xpath_allocator_capture cr(stack.temp);

                xpath_stack swapped_stack = {stack.temp, stack.result};

                xpath_string s = _left->eval_string(c, swapped_stack);
                size_t s_length = s.length();

                double first = round_nearest(_right->eval_number(c, stack));
                double last = first + round_nearest(_right->_next->eval_number(c, stack));

                if (is_nan(first) || is_nan(last)) return xpath_string();
                else if (first >= static_cast<double>(s_length + 1)) return xpath_string();
                else if (first >= last) return xpath_string();
                else if (last < 1) return xpath_string();

                size_t pos = first < 1 ? 1 : static_cast<size_t>(first);
                size_t end = last >= static_cast<double>(s_length + 1) ? s_length + 1 : static_cast<size_t>(last);

                assert(1 <= pos && pos <= end && end <= s_length + 1);
                const char* rbegin = s.c_str() + (pos - 1);
                const char* rend = s.c_str() + (end - 1);

                return (end == s_length + 1 && !s.uses_heap()) ? xpath_string::from_const(rbegin) : xpath_string::from_heap(rbegin, rend, stack.result);
            }

            case ast_func_normalize_space_0:
            {
                xpath_string s = string_value(c.n, stack.result);

                char* begin = s.data(stack.result);
                if (!begin) return xpath_string();

                char* end = normalize_space(begin);

                return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_func_normalize_space_1:
            {
                xpath_string s = _left->eval_string(c, stack);

                char* begin = s.data(stack.result);
                if (!begin) return xpath_string();

                char* end = normalize_space(begin);

                return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_func_translate:
            {
                xpath_allocator_capture cr(stack.temp);

                xpath_stack swapped_stack = {stack.temp, stack.result};

                xpath_string s = _left->eval_string(c, stack);
                xpath_string from = _right->eval_string(c, swapped_stack);
                xpath_string to = _right->_next->eval_string(c, swapped_stack);

                char* begin = s.data(stack.result);
                if (!begin) return xpath_string();

                char* end = translate(begin, from.c_str(), to.c_str(), to.length());

                return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_opt_translate_table:
            {
                xpath_string s = _left->eval_string(c, stack);

                char* begin = s.data(stack.result);
                if (!begin) return xpath_string();

                char* end = translate_table(begin, _data.table);

                return xpath_string::from_heap_preallocated(begin, end);
            }

            case ast_variable:
            {
                assert(_rettype == _data.variable->type());

                if (_rettype == xpath_type_string)
                    return xpath_string::from_const(_data.variable->get_string());

                // variable needs to be converted to the correct type, this is handled by the fallthrough block below
                break;
            }

            default:
                ;
            }

            // none of the ast types that return the value directly matched, we need to perform type conversion
            switch (_rettype)
            {
            case xpath_type_boolean:
                return xpath_string::from_const(eval_boolean(c, stack) ? "true" : "false");

            case xpath_type_number:
                return convert_number_to_string(eval_number(c, stack), stack.result);

            case xpath_type_node_set:
            {
                xpath_allocator_capture cr(stack.temp);

                xpath_stack swapped_stack = {stack.temp, stack.result};

                xpath_node_set_raw ns = eval_node_set(c, swapped_stack, nodeset_eval_first);
                return ns.empty() ? xpath_string() : string_value(ns.first(), stack.result);
            }

            default:
                assert(false && "Wrong expression for return type string"); // unreachable
                return xpath_string();
            }
        }

        xpath_node_set_raw eval_node_set(const xpath_context& c, const xpath_stack& stack, nodeset_eval_t eval)
        {
            switch (_type)
            {
            case ast_op_union:
            {
                xpath_allocator_capture cr(stack.temp);

                xpath_stack swapped_stack = {stack.temp, stack.result};

                xpath_node_set_raw ls = _left->eval_node_set(c, stack, eval);
                xpath_node_set_raw rs = _right->eval_node_set(c, swapped_stack, eval);

                // we can optimize merging two sorted sets, but this is a very rare operation, so don't bother
                ls.set_type(xpath_node_set::type_unsorted);

                ls.append(rs.begin(), rs.end(), stack.result);
                ls.remove_duplicates(stack.temp);

                return ls;
            }

            case ast_filter:
            {
                xpath_node_set_raw set = _left->eval_node_set(c, stack, _test == predicate_constant_one ? nodeset_eval_first : nodeset_eval_all);

                // either expression is a number or it contains position() call; sort by document order
                if (_test != predicate_posinv) set.sort_do();

                bool once = eval_once(set.type(), eval);

                apply_predicate(set, 0, stack, once);

                return set;
            }

            case ast_func_id:
                return xpath_node_set_raw();

            case ast_step:
            {
                switch (_axis)
                {
                case axis_ancestor:
                    return step_do(c, stack, eval, axis_to_type<axis_ancestor>());

                case axis_ancestor_or_self:
                    return step_do(c, stack, eval, axis_to_type<axis_ancestor_or_self>());

                case axis_attribute:
                    return step_do(c, stack, eval, axis_to_type<axis_attribute>());

                case axis_child:
                    return step_do(c, stack, eval, axis_to_type<axis_child>());

                case axis_descendant:
                    return step_do(c, stack, eval, axis_to_type<axis_descendant>());

                case axis_descendant_or_self:
                    return step_do(c, stack, eval, axis_to_type<axis_descendant_or_self>());

                case axis_following:
                    return step_do(c, stack, eval, axis_to_type<axis_following>());

                case axis_following_sibling:
                    return step_do(c, stack, eval, axis_to_type<axis_following_sibling>());

                case axis_namespace:
                    // namespaced axis is not supported
                    return xpath_node_set_raw();

                case axis_parent:
                    return step_do(c, stack, eval, axis_to_type<axis_parent>());

                case axis_preceding:
                    return step_do(c, stack, eval, axis_to_type<axis_preceding>());

                case axis_preceding_sibling:
                    return step_do(c, stack, eval, axis_to_type<axis_preceding_sibling>());

                case axis_self:
                    return step_do(c, stack, eval, axis_to_type<axis_self>());

                default:
                    assert(false && "Unknown axis"); // unreachable
                    return xpath_node_set_raw();
                }
            }

            case ast_step_root:
            {
                assert(!_right); // root step can't have any predicates

                xpath_node_set_raw ns;

                ns.set_type(xpath_node_set::type_sorted);

                if (c.n.node()) ns.push_back(c.n.node().root(), stack.result);
                else if (c.n.attribute()) ns.push_back(c.n.parent().root(), stack.result);

                return ns;
            }

            case ast_variable:
            {
                assert(_rettype == _data.variable->type());

                if (_rettype == xpath_type_node_set)
                {
                    const xpath_node_set& s = _data.variable->get_node_set();

                    xpath_node_set_raw ns;

                    ns.set_type(s.type());
                    ns.append(s.begin(), s.end(), stack.result);

                    return ns;
                }

                // variable needs to be converted to the correct type, this is handled by the fallthrough block below
                break;
            }

            default:
                ;
            }

            // none of the ast types that return the value directly matched, but conversions to node set are invalid
            assert(false && "Wrong expression for return type node set"); // unreachable
            return xpath_node_set_raw();
        }

        void optimize(xpath_allocator* alloc)
        {
            if (_left)
                _left->optimize(alloc);

            if (_right)
                _right->optimize(alloc);

            if (_next)
                _next->optimize(alloc);

            // coverity[var_deref_model]
            optimize_self(alloc);
        }

        void optimize_self(xpath_allocator* alloc)
        {
            // Rewrite [position()=expr] with [expr]
            // Note that this step has to go before classification to recognize [position()=1]
            if ((_type == ast_filter || _type == ast_predicate) &&
                _right && // workaround for clang static analyzer (_right is never null for ast_filter/ast_predicate)
                _right->_type == ast_op_equal && _right->_left->_type == ast_func_position && _right->_right->_rettype == xpath_type_number)
            {
                _right = _right->_right;
            }

            // Classify filter/predicate ops to perform various optimizations during evaluation
            if ((_type == ast_filter || _type == ast_predicate) && _right) // workaround for clang static analyzer (_right is never null for ast_filter/ast_predicate)
            {
                assert(_test == predicate_default);

                if (_right->_type == ast_number_constant && _right->_data.number == 1.0)
                    _test = predicate_constant_one;
                else if (_right->_rettype == xpath_type_number && (_right->_type == ast_number_constant || _right->_type == ast_variable || _right->_type == ast_func_last))
                    _test = predicate_constant;
                else if (_right->_rettype != xpath_type_number && _right->is_posinv_expr())
                    _test = predicate_posinv;
            }

            // Rewrite descendant-or-self::node()/child::foo with descendant::foo
            // The former is a full form of //foo, the latter is much faster since it executes the node test immediately
            // Do a similar kind of rewrite for self/descendant/descendant-or-self axes
            // Note that we only rewrite positionally invariant steps (//foo[1] != /descendant::foo[1])
            if (_type == ast_step && (_axis == axis_child || _axis == axis_self || _axis == axis_descendant || _axis == axis_descendant_or_self) &&
                _left && _left->_type == ast_step && _left->_axis == axis_descendant_or_self && _left->_test == nodetest_type_node && !_left->_right &&
                is_posinv_step())
            {
                if (_axis == axis_child || _axis == axis_descendant)
                    _axis = axis_descendant;
                else
                    _axis = axis_descendant_or_self;

                _left = _left->_left;
            }

            // Use optimized lookup table implementation for translate() with constant arguments
            if (_type == ast_func_translate &&
                _right && // workaround for clang static analyzer (_right is never null for ast_func_translate)
                _right->_type == ast_string_constant && _right->_next->_type == ast_string_constant)
            {
                unsigned char* table = translate_table_generate(alloc, _right->_data.string, _right->_next->_data.string);

                if (table)
                {
                    _type = ast_opt_translate_table;
                    _data.table = table;
                }
            }

            // Use optimized path for @attr = 'value' or @attr = $value
            if (_type == ast_op_equal &&
                _left && _right && // workaround for clang static analyzer and Coverity (_left and _right are never null for ast_op_equal)
                // coverity[mixed_enums]
                _left->_type == ast_step && _left->_axis == axis_attribute && _left->_test == nodetest_name && !_left->_left && !_left->_right &&
                (_right->_type == ast_string_constant || (_right->_type == ast_variable && _right->_rettype == xpath_type_string)))
            {
                _type = ast_opt_compare_attribute;
            }
        }

        bool is_posinv_expr() const
        {
            switch (_type)
            {
            case ast_func_position:
            case ast_func_last:
                return false;

            case ast_string_constant:
            case ast_number_constant:
            case ast_variable:
                return true;

            case ast_step:
            case ast_step_root:
                return true;

            case ast_predicate:
            case ast_filter:
                return true;

            default:
                if (_left && !_left->is_posinv_expr()) return false;

                for (xpath_ast_node* n = _right; n; n = n->_next)
                    if (!n->is_posinv_expr()) return false;

                return true;
            }
        }

        bool is_posinv_step() const
        {
            assert(_type == ast_step);

            for (xpath_ast_node* n = _right; n; n = n->_next)
            {
                assert(n->_type == ast_predicate);

                if (n->_test != predicate_posinv)
                    return false;
            }

            return true;
        }

        xpath_value_type rettype() const
        {
            return static_cast<xpath_value_type>(_rettype);
        }
    };

    static const size_t xpath_ast_depth_limit =
    #ifdef PUGIXML_XPATH_DEPTH_LIMIT
        PUGIXML_XPATH_DEPTH_LIMIT
    #else
        1024
    #endif
        ;

    struct xpath_parser
    {
        xpath_allocator* _alloc;
        xpath_lexer _lexer;

        const char* _query;
        xpath_variable_set* _variables;

        xpath_parse_result* _result;

        char _scratch[32];

        size_t _depth;

        xpath_ast_node* error(const char* message)
        {
            _result->error = message;
            _result->offset = _lexer.current_pos() - _query;

            return 0;
        }

        xpath_ast_node* error_oom()
        {
            assert(_alloc->_error);
            *_alloc->_error = true;

            return 0;
        }

        xpath_ast_node* error_rec()
        {
            return error("Exceeded maximum allowed query depth");
        }

        void* alloc_node()
        {
            return _alloc->allocate(sizeof(xpath_ast_node));
        }

        xpath_ast_node* alloc_node(ast_type_t type, xpath_value_type rettype, const char* value)
        {
            void* memory = alloc_node();
            return memory ? new (memory) xpath_ast_node(type, rettype, value) : 0;
        }

        xpath_ast_node* alloc_node(ast_type_t type, xpath_value_type rettype, double value)
        {
            void* memory = alloc_node();
            return memory ? new (memory) xpath_ast_node(type, rettype, value) : 0;
        }

        xpath_ast_node* alloc_node(ast_type_t type, xpath_value_type rettype, xpath_variable* value)
        {
            void* memory = alloc_node();
            return memory ? new (memory) xpath_ast_node(type, rettype, value) : 0;
        }

        xpath_ast_node* alloc_node(ast_type_t type, xpath_value_type rettype, xpath_ast_node* left = 0, xpath_ast_node* right = 0)
        {
            void* memory = alloc_node();
            return memory ? new (memory) xpath_ast_node(type, rettype, left, right) : 0;
        }

        xpath_ast_node* alloc_node(ast_type_t type, xpath_ast_node* left, axis_t axis, nodetest_t test, const char* contents)
        {
            void* memory = alloc_node();
            return memory ? new (memory) xpath_ast_node(type, left, axis, test, contents) : 0;
        }

        xpath_ast_node* alloc_node(ast_type_t type, xpath_ast_node* left, xpath_ast_node* right, predicate_t test)
        {
            void* memory = alloc_node();
            return memory ? new (memory) xpath_ast_node(type, left, right, test) : 0;
        }

        const char* alloc_string(const xpath_lexer_string& value) noexcept
        {
            if (!value.begin)
                return "";

            size_t length = static_cast<size_t>(value.end - value.begin);

            char* c = static_cast<char*>(_alloc->allocate((length + 1) * sizeof(char)));
            if (!c) return 0;

            memcpy(c, value.begin, length * sizeof(char));
            c[length] = 0;

            return c;
        }

        xpath_ast_node* parse_function(const xpath_lexer_string& name, size_t argc, xpath_ast_node* args[2]) noexcept
        {
            switch (name.begin[0])
            {
            case 'b':
                if (name == "boolean" && argc == 1)
                    return alloc_node(ast_func_boolean, xpath_type_boolean, args[0]);

                break;

            case 'c':
                if (name == "count" && argc == 1)
                {
                    if (args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
                    return alloc_node(ast_func_count, xpath_type_number, args[0]);
                }
                else if (name == "contains" && argc == 2)
                    return alloc_node(ast_func_contains, xpath_type_boolean, args[0], args[1]);
                else if (name == "concat" && argc >= 2)
                    return alloc_node(ast_func_concat, xpath_type_string, args[0], args[1]);
                else if (name == "ceiling" && argc == 1)
                    return alloc_node(ast_func_ceiling, xpath_type_number, args[0]);

                break;

            case 'f':
                if (name == "false" && argc == 0)
                    return alloc_node(ast_func_false, xpath_type_boolean);
                else if (name == "floor" && argc == 1)
                    return alloc_node(ast_func_floor, xpath_type_number, args[0]);

                break;

            case 'i':
                if (name == "id" && argc == 1)
                    return alloc_node(ast_func_id, xpath_type_node_set, args[0]);

                break;

            case 'l':
                if (name == "last" && argc == 0)
                    return alloc_node(ast_func_last, xpath_type_number);
                else if (name == "lang" && argc == 1)
                    return alloc_node(ast_func_lang, xpath_type_boolean, args[0]);
                else if (name == "local-name" && argc <= 1)
                {
                    if (argc == 1 && args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
                    return alloc_node(argc == 0 ? ast_func_local_name_0 : ast_func_local_name_1, xpath_type_string, args[0]);
                }

                break;

            case 'n':
                if (name == "name" && argc <= 1)
                {
                    if (argc == 1 && args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
                    return alloc_node(argc == 0 ? ast_func_name_0 : ast_func_name_1, xpath_type_string, args[0]);
                }
                else if (name == "namespace-uri" && argc <= 1)
                {
                    if (argc == 1 && args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
                    return alloc_node(argc == 0 ? ast_func_namespace_uri_0 : ast_func_namespace_uri_1, xpath_type_string, args[0]);
                }
                else if (name == "normalize-space" && argc <= 1)
                    return alloc_node(argc == 0 ? ast_func_normalize_space_0 : ast_func_normalize_space_1, xpath_type_string, args[0], args[1]);
                else if (name == "not" && argc == 1)
                    return alloc_node(ast_func_not, xpath_type_boolean, args[0]);
                else if (name == "number" && argc <= 1)
                    return alloc_node(argc == 0 ? ast_func_number_0 : ast_func_number_1, xpath_type_number, args[0]);

                break;

            case 'p':
                if (name == "position" && argc == 0)
                    return alloc_node(ast_func_position, xpath_type_number);

                break;

            case 'r':
                if (name == "round" && argc == 1)
                    return alloc_node(ast_func_round, xpath_type_number, args[0]);

                break;

            case 's':
                if (name == "string" && argc <= 1)
                    return alloc_node(argc == 0 ? ast_func_string_0 : ast_func_string_1, xpath_type_string, args[0]);
                else if (name == "string-length" && argc <= 1)
                    return alloc_node(argc == 0 ? ast_func_string_length_0 : ast_func_string_length_1, xpath_type_number, args[0]);
                else if (name == "starts-with" && argc == 2)
                    return alloc_node(ast_func_starts_with, xpath_type_boolean, args[0], args[1]);
                else if (name == "substring-before" && argc == 2)
                    return alloc_node(ast_func_substring_before, xpath_type_string, args[0], args[1]);
                else if (name == "substring-after" && argc == 2)
                    return alloc_node(ast_func_substring_after, xpath_type_string, args[0], args[1]);
                else if (name == "substring" && (argc == 2 || argc == 3))
                    return alloc_node(argc == 2 ? ast_func_substring_2 : ast_func_substring_3, xpath_type_string, args[0], args[1]);
                else if (name == "sum" && argc == 1)
                {
                    if (args[0]->rettype() != xpath_type_node_set) return error("Function has to be applied to node set");
                    return alloc_node(ast_func_sum, xpath_type_number, args[0]);
                }

                break;

            case 't':
                if (name == "translate" && argc == 3)
                    return alloc_node(ast_func_translate, xpath_type_string, args[0], args[1]);
                else if (name == "true" && argc == 0)
                    return alloc_node(ast_func_true, xpath_type_boolean);

                break;

            default:
                break;
            }

            return error("Unrecognized function or wrong parameter count");
        }

        axis_t parse_axis_name(const xpath_lexer_string& name, bool& specified)
        {
            specified = true;

            switch (name.begin[0])
            {
            case 'a':
                if (name == "ancestor")
                    return axis_ancestor;
                else if (name == "ancestor-or-self")
                    return axis_ancestor_or_self;
                else if (name == "attribute")
                    return axis_attribute;

                break;

            case 'c':
                if (name == "child")
                    return axis_child;

                break;

            case 'd':
                if (name == "descendant")
                    return axis_descendant;
                else if (name == "descendant-or-self")
                    return axis_descendant_or_self;

                break;

            case 'f':
                if (name == "following")
                    return axis_following;
                else if (name == "following-sibling")
                    return axis_following_sibling;

                break;

            case 'n':
                if (name == "namespace")
                    return axis_namespace;

                break;

            case 'p':
                if (name == "parent")
                    return axis_parent;
                else if (name == "preceding")
                    return axis_preceding;
                else if (name == "preceding-sibling")
                    return axis_preceding_sibling;

                break;

            case 's':
                if (name == "self")
                    return axis_self;

                break;

            default:
                break;
            }

            specified = false;
            return axis_child;
        }

        nodetest_t parse_node_test_type(const xpath_lexer_string& name)
        {
            switch (name.begin[0])
            {
            case 'c':
                if (name == "comment")
                    return nodetest_type_comment;

                break;

            case 'n':
                if (name == "node")
                    return nodetest_type_node;

                break;

            case 'p':
                if (name == "processing-instruction")
                    return nodetest_type_pi;

                break;

            case 't':
                if (name == "text")
                    return nodetest_type_text;

                break;

            default:
                break;
            }

            return nodetest_none;
        }

        // PrimaryExpr ::= VariableReference | '(' Expr ')' | Literal | Number | FunctionCall
        xpath_ast_node* parse_primary_expression()
        {
            switch (_lexer.current())
            {
            case lex_var_ref:
            {
                xpath_lexer_string name = _lexer.contents();

                if (!_variables)
                    return error("Unknown variable: variable set is not provided");

                xpath_variable* var = 0;
                if (!get_variable_scratch(_scratch, _variables, name.begin, name.end, &var))
                    return error_oom();

                if (!var)
                    return error("Unknown variable: variable set does not contain the given name");

                _lexer.next();

                return alloc_node(ast_variable, var->type(), var);
            }

            case lex_open_brace:
            {
                _lexer.next();

                xpath_ast_node* n = parse_expression();
                if (!n) return 0;

                if (_lexer.current() != lex_close_brace)
                    return error("Expected ')' to match an opening '('");

                _lexer.next();

                return n;
            }

            case lex_quoted_string:
            {
                const char* value = alloc_string(_lexer.contents());
                if (!value) return 0;

                _lexer.next();

                return alloc_node(ast_string_constant, xpath_type_string, value);
            }

            case lex_number:
            {
                double value = 0;

                if (!convert_string_to_number_scratch(_scratch, _lexer.contents().begin, _lexer.contents().end, &value))
                    return error_oom();

                _lexer.next();

                return alloc_node(ast_number_constant, xpath_type_number, value);
            }

            case lex_string:
            {
                xpath_ast_node* args[2] = {0};
                size_t argc = 0;

                xpath_lexer_string function = _lexer.contents();
                _lexer.next();

                xpath_ast_node* last_arg = 0;

                if (_lexer.current() != lex_open_brace)
                    return error("Unrecognized function call");
                _lexer.next();

                size_t old_depth = _depth;

                while (_lexer.current() != lex_close_brace)
                {
                    if (argc > 0)
                    {
                        if (_lexer.current() != lex_comma)
                            return error("No comma between function arguments");
                        _lexer.next();
                    }

                    if (++_depth > xpath_ast_depth_limit)
                        return error_rec();

                    xpath_ast_node* n = parse_expression();
                    if (!n) return 0;

                    if (argc < 2) args[argc] = n;
                    else last_arg->set_next(n);

                    argc++;
                    last_arg = n;
                }

                _lexer.next();

                _depth = old_depth;

                return parse_function(function, argc, args);
            }

            default:
                return error("Unrecognizable primary expression");
            }
        }

        // FilterExpr ::= PrimaryExpr | FilterExpr Predicate
        // Predicate ::= '[' PredicateExpr ']'
        // PredicateExpr ::= Expr
        xpath_ast_node* parse_filter_expression()
        {
            xpath_ast_node* n = parse_primary_expression();
            if (!n) return 0;

            size_t old_depth = _depth;

            while (_lexer.current() == lex_open_square_brace)
            {
                _lexer.next();

                if (++_depth > xpath_ast_depth_limit)
                    return error_rec();

                if (n->rettype() != xpath_type_node_set)
                    return error("Predicate has to be applied to node set");

                xpath_ast_node* expr = parse_expression();
                if (!expr) return 0;

                n = alloc_node(ast_filter, n, expr, predicate_default);
                if (!n) return 0;

                if (_lexer.current() != lex_close_square_brace)
                    return error("Expected ']' to match an opening '['");

                _lexer.next();
            }

            _depth = old_depth;

            return n;
        }

        // Step ::= AxisSpecifier NodeTest Predicate* | AbbreviatedStep
        // AxisSpecifier ::= AxisName '::' | '@'?
        // NodeTest ::= NameTest | NodeType '(' ')' | 'processing-instruction' '(' Literal ')'
        // NameTest ::= '*' | NCName ':' '*' | QName
        // AbbreviatedStep ::= '.' | '..'
        xpath_ast_node* parse_step(xpath_ast_node* set)
        {
            if (set && set->rettype() != xpath_type_node_set)
                return error("Step has to be applied to node set");

            bool axis_specified = false;
            axis_t axis = axis_child; // implied child axis

            if (_lexer.current() == lex_axis_attribute)
            {
                axis = axis_attribute;
                axis_specified = true;

                _lexer.next();
            }
            else if (_lexer.current() == lex_dot)
            {
                _lexer.next();

                if (_lexer.current() == lex_open_square_brace)
                    return error("Predicates are not allowed after an abbreviated step");

                return alloc_node(ast_step, set, axis_self, nodetest_type_node, 0);
            }
            else if (_lexer.current() == lex_double_dot)
            {
                _lexer.next();

                if (_lexer.current() == lex_open_square_brace)
                    return error("Predicates are not allowed after an abbreviated step");

                return alloc_node(ast_step, set, axis_parent, nodetest_type_node, 0);
            }

            nodetest_t nt_type = nodetest_none;
            xpath_lexer_string nt_name;

            if (_lexer.current() == lex_string)
            {
                // node name test
                nt_name = _lexer.contents();
                _lexer.next();

                // was it an axis name?
                if (_lexer.current() == lex_double_colon)
                {
                    // parse axis name
                    if (axis_specified)
                        return error("Two axis specifiers in one step");

                    axis = parse_axis_name(nt_name, axis_specified);

                    if (!axis_specified)
                        return error("Unknown axis");

                    // read actual node test
                    _lexer.next();

                    if (_lexer.current() == lex_multiply)
                    {
                        nt_type = nodetest_all;
                        nt_name = xpath_lexer_string();
                        _lexer.next();
                    }
                    else if (_lexer.current() == lex_string)
                    {
                        nt_name = _lexer.contents();
                        _lexer.next();
                    }
                    else
                    {
                        return error("Unrecognized node test");
                    }
                }

                if (nt_type == nodetest_none)
                {
                    // node type test or processing-instruction
                    if (_lexer.current() == lex_open_brace)
                    {
                        _lexer.next();

                        if (_lexer.current() == lex_close_brace)
                        {
                            _lexer.next();

                            nt_type = parse_node_test_type(nt_name);

                            if (nt_type == nodetest_none)
                                return error("Unrecognized node type");

                            nt_name = xpath_lexer_string();
                        }
                        else if (nt_name == "processing-instruction")
                        {
                            if (_lexer.current() != lex_quoted_string)
                                return error("Only literals are allowed as arguments to processing-instruction()");

                            nt_type = nodetest_pi;
                            nt_name = _lexer.contents();
                            _lexer.next();

                            if (_lexer.current() != lex_close_brace)
                                return error("Unmatched brace near processing-instruction()");
                            _lexer.next();
                        }
                        else
                        {
                            return error("Unmatched brace near node type test");
                        }
                    }
                    // QName or NCName:*
                    else
                    {
                        if (nt_name.end - nt_name.begin > 2 && nt_name.end[-2] == ':' && nt_name.end[-1] == '*') // NCName:*
                        {
                            nt_name.end--; // erase *

                            nt_type = nodetest_all_in_namespace;
                        }
                        else
                        {
                            nt_type = nodetest_name;
                        }
                    }
                }
            }
            else if (_lexer.current() == lex_multiply)
            {
                nt_type = nodetest_all;
                _lexer.next();
            }
            else
            {
                return error("Unrecognized node test");
            }

            const char* nt_name_copy = alloc_string(nt_name);
            if (!nt_name_copy) return 0;

            xpath_ast_node* n = alloc_node(ast_step, set, axis, nt_type, nt_name_copy);
            if (!n) return 0;

            size_t old_depth = _depth;

            xpath_ast_node* last = 0;

            while (_lexer.current() == lex_open_square_brace)
            {
                _lexer.next();

                if (++_depth > xpath_ast_depth_limit)
                    return error_rec();

                xpath_ast_node* expr = parse_expression();
                if (!expr) return 0;

                xpath_ast_node* pred = alloc_node(ast_predicate, 0, expr, predicate_default);
                if (!pred) return 0;

                if (_lexer.current() != lex_close_square_brace)
                    return error("Expected ']' to match an opening '['");
                _lexer.next();

                if (last) last->set_next(pred);
                else n->set_right(pred);

                last = pred;
            }

            _depth = old_depth;

            return n;
        }

        // RelativeLocationPath ::= Step | RelativeLocationPath '/' Step | RelativeLocationPath '//' Step
        xpath_ast_node* parse_relative_location_path(xpath_ast_node* set)
        {
            xpath_ast_node* n = parse_step(set);
            if (!n) return 0;

            size_t old_depth = _depth;

            while (_lexer.current() == lex_slash || _lexer.current() == lex_double_slash)
            {
                lexeme_t l = _lexer.current();
                _lexer.next();

                if (l == lex_double_slash)
                {
                    n = alloc_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, 0);
                    if (!n) return 0;

                    ++_depth;
                }

                if (++_depth > xpath_ast_depth_limit)
                    return error_rec();

                n = parse_step(n);
                if (!n) return 0;
            }

            _depth = old_depth;

            return n;
        }

        // LocationPath ::= RelativeLocationPath | AbsoluteLocationPath
        // AbsoluteLocationPath ::= '/' RelativeLocationPath? | '//' RelativeLocationPath
        xpath_ast_node* parse_location_path()
        {
            if (_lexer.current() == lex_slash)
            {
                _lexer.next();

                xpath_ast_node* n = alloc_node(ast_step_root, xpath_type_node_set);
                if (!n) return 0;

                // relative location path can start from axis_attribute, dot, double_dot, multiply and string lexemes; any other lexeme means standalone root path
                lexeme_t l = _lexer.current();

                if (l == lex_string || l == lex_axis_attribute || l == lex_dot || l == lex_double_dot || l == lex_multiply)
                    return parse_relative_location_path(n);
                else
                    return n;
            }
            else if (_lexer.current() == lex_double_slash)
            {
                _lexer.next();

                xpath_ast_node* n = alloc_node(ast_step_root, xpath_type_node_set);
                if (!n) return 0;

                n = alloc_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, 0);
                if (!n) return 0;

                return parse_relative_location_path(n);
            }

            // else clause moved outside of if because of bogus warning 'control may reach end of non-void function being inlined' in gcc 4.0.1
            return parse_relative_location_path(0);
        }

        // PathExpr ::= LocationPath
        //                | FilterExpr
        //                | FilterExpr '/' RelativeLocationPath
        //                | FilterExpr '//' RelativeLocationPath
        // UnionExpr ::= PathExpr | UnionExpr '|' PathExpr
        // UnaryExpr ::= UnionExpr | '-' UnaryExpr
        xpath_ast_node* parse_path_or_unary_expression()
        {
            // Clarification.
            // PathExpr begins with either LocationPath or FilterExpr.
            // FilterExpr begins with PrimaryExpr
            // PrimaryExpr begins with '$' in case of it being a variable reference,
            // '(' in case of it being an expression, string literal, number constant or
            // function call.
            if (_lexer.current() == lex_var_ref || _lexer.current() == lex_open_brace ||
                _lexer.current() == lex_quoted_string || _lexer.current() == lex_number ||
                _lexer.current() == lex_string)
            {
                if (_lexer.current() == lex_string)
                {
                    // This is either a function call, or not - if not, we shall proceed with location path
                    const char* state = _lexer.state();

                    while (PUGI__IS_CHARTYPE(*state, ct_space)) ++state;

                    if (*state != '(')
                        return parse_location_path();

                    // This looks like a function call; however this still can be a node-test. Check it.
                    if (parse_node_test_type(_lexer.contents()) != nodetest_none)
                        return parse_location_path();
                }

                xpath_ast_node* n = parse_filter_expression();
                if (!n) return 0;

                if (_lexer.current() == lex_slash || _lexer.current() == lex_double_slash)
                {
                    lexeme_t l = _lexer.current();
                    _lexer.next();

                    if (l == lex_double_slash)
                    {
                        if (n->rettype() != xpath_type_node_set)
                            return error("Step has to be applied to node set");

                        n = alloc_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, 0);
                        if (!n) return 0;
                    }

                    // select from location path
                    return parse_relative_location_path(n);
                }

                return n;
            }
            else if (_lexer.current() == lex_minus)
            {
                _lexer.next();

                // precedence 7+ - only parses union expressions
                xpath_ast_node* n = parse_expression(7);
                if (!n) return 0;

                return alloc_node(ast_op_negate, xpath_type_number, n);
            }
            else
            {
                return parse_location_path();
            }
        }

        struct binary_op_t
        {
            ast_type_t asttype;
            xpath_value_type rettype;
            int precedence;

            binary_op_t(): asttype(ast_unknown), rettype(xpath_type_none), precedence(0)
            {
            }

            binary_op_t(ast_type_t asttype_, xpath_value_type rettype_, int precedence_): asttype(asttype_), rettype(rettype_), precedence(precedence_)
            {
            }

            static binary_op_t parse(xpath_lexer& lexer)
            {
                switch (lexer.current())
                {
                case lex_string:
                    if (lexer.contents() == "or")
                        return binary_op_t(ast_op_or, xpath_type_boolean, 1);
                    else if (lexer.contents() == "and")
                        return binary_op_t(ast_op_and, xpath_type_boolean, 2);
                    else if (lexer.contents() == "div")
                        return binary_op_t(ast_op_divide, xpath_type_number, 6);
                    else if (lexer.contents() == "mod")
                        return binary_op_t(ast_op_mod, xpath_type_number, 6);
                    else
                        return binary_op_t();

                case lex_equal:
                    return binary_op_t(ast_op_equal, xpath_type_boolean, 3);

                case lex_not_equal:
                    return binary_op_t(ast_op_not_equal, xpath_type_boolean, 3);

                case lex_less:
                    return binary_op_t(ast_op_less, xpath_type_boolean, 4);

                case lex_greater:
                    return binary_op_t(ast_op_greater, xpath_type_boolean, 4);

                case lex_less_or_equal:
                    return binary_op_t(ast_op_less_or_equal, xpath_type_boolean, 4);

                case lex_greater_or_equal:
                    return binary_op_t(ast_op_greater_or_equal, xpath_type_boolean, 4);

                case lex_plus:
                    return binary_op_t(ast_op_add, xpath_type_number, 5);

                case lex_minus:
                    return binary_op_t(ast_op_subtract, xpath_type_number, 5);

                case lex_multiply:
                    return binary_op_t(ast_op_multiply, xpath_type_number, 6);

                case lex_union:
                    return binary_op_t(ast_op_union, xpath_type_node_set, 7);

                default:
                    return binary_op_t();
                }
            }
        };

        xpath_ast_node* parse_expression_rec(xpath_ast_node* lhs, int limit)
        {
            binary_op_t op = binary_op_t::parse(_lexer);

            while (op.asttype != ast_unknown && op.precedence >= limit)
            {
                _lexer.next();

                if (++_depth > xpath_ast_depth_limit)
                    return error_rec();

                xpath_ast_node* rhs = parse_path_or_unary_expression();
                if (!rhs) return 0;

                binary_op_t nextop = binary_op_t::parse(_lexer);

                while (nextop.asttype != ast_unknown && nextop.precedence > op.precedence)
                {
                    rhs = parse_expression_rec(rhs, nextop.precedence);
                    if (!rhs) return 0;

                    nextop = binary_op_t::parse(_lexer);
                }

                if (op.asttype == ast_op_union && (lhs->rettype() != xpath_type_node_set || rhs->rettype() != xpath_type_node_set))
                    return error("Union operator has to be applied to node sets");

                lhs = alloc_node(op.asttype, op.rettype, lhs, rhs);
                if (!lhs) return 0;

                op = binary_op_t::parse(_lexer);
            }

            return lhs;
        }

        // Expr ::= OrExpr
        // OrExpr ::= AndExpr | OrExpr 'or' AndExpr
        // AndExpr ::= EqualityExpr | AndExpr 'and' EqualityExpr
        // EqualityExpr ::= RelationalExpr
        //                    | EqualityExpr '=' RelationalExpr
        //                    | EqualityExpr '!=' RelationalExpr
        // RelationalExpr ::= AdditiveExpr
        //                      | RelationalExpr '<' AdditiveExpr
        //                      | RelationalExpr '>' AdditiveExpr
        //                      | RelationalExpr '<=' AdditiveExpr
        //                      | RelationalExpr '>=' AdditiveExpr
        // AdditiveExpr ::= MultiplicativeExpr
        //                    | AdditiveExpr '+' MultiplicativeExpr
        //                    | AdditiveExpr '-' MultiplicativeExpr
        // MultiplicativeExpr ::= UnaryExpr
        //                          | MultiplicativeExpr '*' UnaryExpr
        //                          | MultiplicativeExpr 'div' UnaryExpr
        //                          | MultiplicativeExpr 'mod' UnaryExpr
        xpath_ast_node* parse_expression(int limit = 0)
        {
            size_t old_depth = _depth;

            if (++_depth > xpath_ast_depth_limit)
                return error_rec();

            xpath_ast_node* n = parse_path_or_unary_expression();
            if (!n) return 0;

            n = parse_expression_rec(n, limit);

            _depth = old_depth;

            return n;
        }

        xpath_parser(const char* query, xpath_variable_set* variables, xpath_allocator* alloc, xpath_parse_result* result): _alloc(alloc), _lexer(query), _query(query), _variables(variables), _result(result), _depth(0)
        {
        }

        xpath_ast_node* parse()
        {
            xpath_ast_node* n = parse_expression();
            if (!n) return 0;

            assert(_depth == 0);

            // check if there are unparsed tokens left
            if (_lexer.current() != lex_eof)
                return error("Incorrect query");

            return n;
        }

        static xpath_ast_node* parse(const char* query, xpath_variable_set* variables, xpath_allocator* alloc, xpath_parse_result* result)
        {
            xpath_parser parser(query, variables, alloc, result);

            return parser.parse();
        }
    };

    struct xpath_query_impl
    {
        static xpath_query_impl* create()
        {
            void* memory = ::malloc(sizeof(xpath_query_impl));
            if (!memory) return 0;

            return new (memory) xpath_query_impl();
        }

        static void destroy(xpath_query_impl* impl)
        {
            // free all allocated pages
            impl->alloc.release();

            // free allocator memory (with the first page)
            ::free(impl);
        }

        xpath_query_impl(): root(0), alloc(&block, &oom), oom(false)
        {
            block.next = 0;
            block.capacity = sizeof(block.data);
        }

        xpath_ast_node* root;
        xpath_allocator alloc;
        xpath_memory_block block;
        bool oom;
    };

    PUGI__FN impl::xpath_ast_node* evaluate_node_set_prepare(xpath_query_impl* impl)
    {
        if (!impl) return 0;

        if (impl->root->rettype() != xpath_type_node_set)
        {
        #ifdef PUGIXML_NO_EXCEPTIONS
            return 0;
        #else
            xpath_parse_result res;
            res.error = "Expression does not evaluate to node set";

            throw xpath_exception(res);
        #endif
        }

        return impl->root;
    }
PUGI__NS_END

namespace pugRd
{
#ifndef PUGIXML_NO_EXCEPTIONS
    PUGI__FN xpath_exception::xpath_exception(const xpath_parse_result& result_) noexcept
        : _result(result_)
    {
        assert(_result.error);
    }

    PUGI__FN const char* xpath_exception::what() const noexcept
    {
        return _result.error;
    }

    PUGI__FN const xpath_parse_result& xpath_exception::result() const noexcept
    {
        return _result;
    }
#endif

    PUGI__FN xpath_node::xpath_node(const xml_node& node_) noexcept
        : _node(node_)
    {
    }

    PUGI__FN xpath_node::xpath_node(const xml_attribute& attribute_, const xml_node& parent_) noexcept
        : _node(attribute_ ? parent_ : xml_node()), _attribute(attribute_)
    {
    }

    PUGI__FN xml_node xpath_node::node() const noexcept
    {
        return _attribute ? xml_node() : _node;
    }

    PUGI__FN xml_attribute xpath_node::attribute() const noexcept
    {
        return _attribute;
    }

    PUGI__FN xml_node xpath_node::parent() const noexcept
    {
        return _attribute ? _node : _node.parent();
    }

    PUGI__FN static void unspecified_bool_xpath_node(xpath_node***) noexcept
    {
    }

    PUGI__FN xpath_node::operator xpath_node::unspecified_bool_type() const noexcept
    {
        return (_node || _attribute) ? unspecified_bool_xpath_node : 0;
    }

    PUGI__FN bool xpath_node::operator!() const noexcept
    {
        return !(_node || _attribute);
    }

    PUGI__FN bool xpath_node::operator==(const xpath_node& n) const noexcept
    {
        return _node == n._node && _attribute == n._attribute;
    }

    PUGI__FN bool xpath_node::operator!=(const xpath_node& n) const noexcept
    {
        return _node != n._node || _attribute != n._attribute;
    }


    PUGI__FN void xpath_node_set::_assign(const_iterator begin_, const_iterator end_, type_t type_) noexcept
    {
        assert(begin_ <= end_);

        size_t size_ = static_cast<size_t>(end_ - begin_);

        // use internal buffer for 0 or 1 elements, heap buffer otherwise
        xpath_node* storage = (size_ <= 1) ? _storage : static_cast<xpath_node*>(::malloc(size_ * sizeof(xpath_node)));

        if (!storage)
        {
            assert(0);
            return;
        }

        // deallocate old buffer
        if (_begin != _storage)
            ::free(_begin);

        // size check is necessary because for begin_ = end_ = nullptr, memcpy is UB
        if (size_)
            memcpy(storage, begin_, size_ * sizeof(xpath_node));

        _begin = storage;
        _end = storage + size_;
        _type = type_;
    }

PUGI__FN void xpath_node_set::_move(xpath_node_set& rhs) noexcept
{
    _type = rhs._type;
    _storage[0] = rhs._storage[0];
    _begin = (rhs._begin == rhs._storage) ? _storage : rhs._begin;
    _end = _begin + (rhs._end - rhs._begin);

    rhs._type = type_unsorted;
    rhs._begin = rhs._storage;
    rhs._end = rhs._storage;
}

    PUGI__FN xpath_node_set::xpath_node_set(): _type(type_unsorted), _begin(_storage), _end(_storage)
    {
    }

    PUGI__FN xpath_node_set::xpath_node_set(const_iterator begin_, const_iterator end_, type_t type_): _type(type_unsorted), _begin(_storage), _end(_storage)
    {
        _assign(begin_, end_, type_);
    }

    PUGI__FN xpath_node_set::~xpath_node_set()
    {
        if (_begin != _storage)
            ::free(_begin);
    }

    PUGI__FN xpath_node_set::xpath_node_set(const xpath_node_set& ns): _type(type_unsorted), _begin(_storage), _end(_storage)
    {
        _assign(ns._begin, ns._end, ns._type);
    }

    PUGI__FN xpath_node_set& xpath_node_set::operator=(const xpath_node_set& ns)
    {
        if (this == &ns) return *this;

        _assign(ns._begin, ns._end, ns._type);

        return *this;
    }

PUGI__FN xpath_node_set::xpath_node_set(xpath_node_set&& rhs) noexcept
  : _type(type_unsorted), _begin(_storage), _end(_storage)
{
    _move(rhs);
}

PUGI__FN xpath_node_set& xpath_node_set::operator=(xpath_node_set&& rhs) noexcept
{
    if (this == &rhs) return *this;

    if (_begin != _storage)
        ::free(_begin);

    _move(rhs);

    return *this;
}

PUGI__FN xpath_node_set::type_t xpath_node_set::type() const noexcept
{
    return _type;
}

PUGI__FN size_t xpath_node_set::size() const noexcept
{
    return _end - _begin;
}

PUGI__FN bool xpath_node_set::empty() const noexcept
{
    return _begin == _end;
}

PUGI__FN const xpath_node& xpath_node_set::operator[](size_t index) const noexcept
{
    assert(index < size());
    return _begin[index];
}

PUGI__FN xpath_node_set::const_iterator xpath_node_set::begin() const noexcept
{
    return _begin;
}

PUGI__FN xpath_node_set::const_iterator xpath_node_set::end() const noexcept
{
    return _end;
}

PUGI__FN void xpath_node_set::sort(bool reverse) noexcept
{
    _type = impl::xpath_sort(_begin, _end, _type, reverse);
}

PUGI__FN xpath_node xpath_node_set::first() const noexcept
{
    return impl::xpath_first(_begin, _end, _type);
}

PUGI__FN xpath_variable::xpath_variable(xpath_value_type type_) noexcept
    : _type(type_), _next(0)
{
}

PUGI__FN const char* xpath_variable::name() const noexcept
{
    switch (_type)
    {
    case xpath_type_node_set:
        return static_cast<const impl::xpath_variable_node_set*>(this)->name;

    case xpath_type_number:
        return static_cast<const impl::xpath_variable_number*>(this)->name;

    case xpath_type_string:
        return static_cast<const impl::xpath_variable_string*>(this)->name;

    case xpath_type_boolean:
        return static_cast<const impl::xpath_variable_boolean*>(this)->name;

    default:
        assert(false && "Invalid variable type"); // unreachable
        return 0;
    }
}

PUGI__FN xpath_value_type xpath_variable::type() const noexcept
{
    return _type;
}

PUGI__FN bool xpath_variable::get_boolean() const noexcept
{
    return (_type == xpath_type_boolean) ? static_cast<const impl::xpath_variable_boolean*>(this)->value : false;
}

PUGI__FN double xpath_variable::get_number() const noexcept
{
    return (_type == xpath_type_number) ? static_cast<const impl::xpath_variable_number*>(this)->value : impl::gen_nan();
}

PUGI__FN const char* xpath_variable::get_string() const noexcept
{
    const char* value = (_type == xpath_type_string) ? static_cast<const impl::xpath_variable_string*>(this)->value : 0;
    return value ? value : "";
}

PUGI__FN const xpath_node_set& xpath_variable::get_node_set() const noexcept
{
    return (_type == xpath_type_node_set) ? static_cast<const impl::xpath_variable_node_set*>(this)->value : impl::dummy_node_set;
}

PUGI__FN bool xpath_variable::set(bool value) noexcept
{
    if (_type != xpath_type_boolean) return false;

    static_cast<impl::xpath_variable_boolean*>(this)->value = value;
    return true;
}

PUGI__FN bool xpath_variable::set(double value) noexcept
{
    if (_type != xpath_type_number) return false;

    static_cast<impl::xpath_variable_number*>(this)->value = value;
    return true;
}

PUGI__FN bool xpath_variable::set(const char* value) noexcept
{
    if (_type != xpath_type_string) return false;

    impl::xpath_variable_string* var = static_cast<impl::xpath_variable_string*>(this);

    // duplicate string
    size_t size = (::strlen(value) + 1) * sizeof(char);

    char* copy = static_cast<char*>(::malloc(size));
    if (!copy) return false;

    memcpy(copy, value, size);

    // replace old string
    if (var->value) ::free(var->value);
    var->value = copy;

    return true;
}

    PUGI__FN bool xpath_variable::set(const xpath_node_set& value) noexcept
    {
        if (_type != xpath_type_node_set) return false;

        static_cast<impl::xpath_variable_node_set*>(this)->value = value;
        return true;
    }

    PUGI__FN xpath_variable_set::xpath_variable_set() noexcept
    {
        for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
            _data[i] = 0;
    }

    PUGI__FN xpath_variable_set::~xpath_variable_set()
    {
        for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
            _destroy(_data[i]);
    }

    PUGI__FN xpath_variable_set::xpath_variable_set(const xpath_variable_set& rhs) noexcept
    {
        for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
            _data[i] = 0;

        _assign(rhs);
    }

    PUGI__FN xpath_variable_set& xpath_variable_set::operator=(const xpath_variable_set& rhs) noexcept
    {
        if (this == &rhs) return *this;

        _assign(rhs);

        return *this;
    }

#ifdef PUGIXML_HAS_MOVE
    PUGI__FN xpath_variable_set::xpath_variable_set(xpath_variable_set&& rhs) noexcept
    {
        for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
        {
            _data[i] = rhs._data[i];
            rhs._data[i] = 0;
        }
    }

    PUGI__FN xpath_variable_set& xpath_variable_set::operator=(xpath_variable_set&& rhs) noexcept
    {
        for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
        {
            _destroy(_data[i]);

            _data[i] = rhs._data[i];
            rhs._data[i] = 0;
        }

        return *this;
    }
#endif

PUGI__FN void xpath_variable_set::_assign(const xpath_variable_set& rhs) noexcept
{
    xpath_variable_set temp;

    for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
        if (rhs._data[i] && !_clone(rhs._data[i], &temp._data[i]))
            return;

    _swap(temp);
}

PUGI__FN void xpath_variable_set::_swap(xpath_variable_set& rhs) noexcept
{
    for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
    {
        xpath_variable* chain = _data[i];

        _data[i] = rhs._data[i];
        rhs._data[i] = chain;
    }
}

PUGI__FN xpath_variable* xpath_variable_set::_find(const char* name) const noexcept
{
    const size_t hash_size = sizeof(_data) / sizeof(_data[0]);
    size_t hash = impl::hash_string(name) % hash_size;

    // look for existing variable
    for (xpath_variable* var = _data[hash]; var; var = var->_next)
        if (impl::strequal(var->name(), name))
            return var;

    return 0;
}

PUGI__FN bool xpath_variable_set::_clone(xpath_variable* var, xpath_variable** out_result) noexcept
{
    xpath_variable* last = 0;

    while (var)
    {
        // allocate storage for new variable
        xpath_variable* nvar = impl::new_xpath_variable(var->_type, var->name());
        if (!nvar) return false;

        // link the variable to the result immediately to handle failures gracefully
        if (last)
            last->_next = nvar;
        else
            *out_result = nvar;

        last = nvar;

        // copy the value; this can fail due to out-of-memory conditions
        if (!impl::copy_xpath_variable(nvar, var)) return false;

        var = var->_next;
    }

    return true;
}

PUGI__FN void xpath_variable_set::_destroy(xpath_variable* var) noexcept
{
    while (var)
    {
        xpath_variable* next = var->_next;

        impl::delete_xpath_variable(var->_type, var);

        var = next;
    }
}

PUGI__FN xpath_variable* xpath_variable_set::add(const char* name, xpath_value_type type) noexcept
{
    const size_t hash_size = sizeof(_data) / sizeof(_data[0]);
    size_t hash = impl::hash_string(name) % hash_size;

    // look for existing variable
    for (xpath_variable* var = _data[hash]; var; var = var->_next)
        if (impl::strequal(var->name(), name))
            return var->type() == type ? var : 0;

    // add new variable
    xpath_variable* result = impl::new_xpath_variable(type, name);

    if (result)
    {
        result->_next = _data[hash];

        _data[hash] = result;
    }

    return result;
}

    PUGI__FN bool xpath_variable_set::set(const char* name, bool value) noexcept
    {
        xpath_variable* var = add(name, xpath_type_boolean);
        return var ? var->set(value) : false;
    }

    PUGI__FN bool xpath_variable_set::set(const char* name, double value) noexcept
    {
        xpath_variable* var = add(name, xpath_type_number);
        return var ? var->set(value) : false;
    }

    PUGI__FN bool xpath_variable_set::set(const char* name, const char* value) noexcept
    {
        xpath_variable* var = add(name, xpath_type_string);
        return var ? var->set(value) : false;
    }

    PUGI__FN bool xpath_variable_set::set(const char* name, const xpath_node_set& value) noexcept
    {
        xpath_variable* var = add(name, xpath_type_node_set);
        return var ? var->set(value) : false;
    }

    PUGI__FN xpath_variable* xpath_variable_set::get(const char* name) noexcept
    {
        return _find(name);
    }

    PUGI__FN const xpath_variable* xpath_variable_set::get(const char* name) const noexcept
    {
        return _find(name);
    }

    PUGI__FN xpath_query::xpath_query(const char* query, xpath_variable_set* variables) noexcept
        : _impl(0)
    {
        impl::xpath_query_impl* qimpl = impl::xpath_query_impl::create();

        if (!qimpl)
        {
            _result.error = "Out of memory";
            assert(0);
        }
        else
        {
            using impl::auto_deleter; // MSVC7 workaround
            auto_deleter<impl::xpath_query_impl> impl(qimpl, impl::xpath_query_impl::destroy);

            qimpl->root = impl::xpath_parser::parse(query, variables, &qimpl->alloc, &_result);

            if (qimpl->root)
            {
                qimpl->root->optimize(&qimpl->alloc);

                _impl = impl.release();
                _result.error = 0;
            }
            else
            {
                if (qimpl->oom) _result.error = "Out of memory";
                assert(0);
            }
        }
    }

    PUGI__FN xpath_query::~xpath_query()
    {
        if (_impl)
            impl::xpath_query_impl::destroy(static_cast<impl::xpath_query_impl*>(_impl));
    }

PUGI__FN xpath_query::xpath_query(xpath_query&& rhs) noexcept
{
    _impl = rhs._impl;
    _result = rhs._result;
    rhs._impl = 0;
    rhs._result = xpath_parse_result();
}

PUGI__FN xpath_query& xpath_query::operator=(xpath_query&& rhs) noexcept
{
    if (this == &rhs) return *this;

    if (_impl)
        impl::xpath_query_impl::destroy(static_cast<impl::xpath_query_impl*>(_impl));

    _impl = rhs._impl;
    _result = rhs._result;
    rhs._impl = 0;
    rhs._result = xpath_parse_result();

    return *this;
}

PUGI__FN xpath_value_type xpath_query::return_type() const noexcept
{
    if (!_impl) return xpath_type_none;

    return static_cast<impl::xpath_query_impl*>(_impl)->root->rettype();
}

PUGI__FN bool xpath_query::evaluate_boolean(const xpath_node& n) const noexcept
{
    if (!_impl) return false;

    impl::xpath_context c(n, 1, 1);
    impl::xpath_stack_data sd;

    bool r = static_cast<impl::xpath_query_impl*>(_impl)->root->eval_boolean(c, sd.stack);

    if (sd.oom)
    {
        assert(0);
        return false;
    }

    return r;
}

PUGI__FN double xpath_query::evaluate_number(const xpath_node& n) const noexcept
{
    if (!_impl) return impl::gen_nan();

    impl::xpath_context c(n, 1, 1);
    impl::xpath_stack_data sd;

    double r = static_cast<impl::xpath_query_impl*>(_impl)->root->eval_number(c, sd.stack);

    if (sd.oom)
    {
        assert(0);
        return impl::gen_nan();
    }

    return r;
}

std::string xpath_query::evaluate_string(const xpath_node& n) const noexcept
{
    if (!_impl) return {};

    impl::xpath_context c(n, 1, 1);
    impl::xpath_stack_data sd;

    impl::xpath_string r = static_cast<impl::xpath_query_impl*>(_impl)->root->eval_string(c, sd.stack);

    if (sd.oom) {
        assert(0);
        return {};
    }

    return std::string(r.c_str(), r.length());
}

PUGI__FN size_t xpath_query::evaluate_string(char* buffer, size_t capacity, const xpath_node& n) const noexcept
{
    impl::xpath_context c(n, 1, 1);
    impl::xpath_stack_data sd;

    impl::xpath_string r = _impl ? static_cast<impl::xpath_query_impl*>(_impl)->root->eval_string(c, sd.stack) : impl::xpath_string();

    if (sd.oom)
    {
        assert(0);
        r = impl::xpath_string();
    }

    size_t full_size = r.length() + 1;

    if (capacity > 0)
    {
        size_t size = (full_size < capacity) ? full_size : capacity;
        assert(size > 0);

        memcpy(buffer, r.c_str(), (size - 1) * sizeof(char));
        buffer[size - 1] = 0;
    }

    return full_size;
}

PUGI__FN xpath_node_set xpath_query::evaluate_node_set(const xpath_node& n) const noexcept
{
    impl::xpath_ast_node* root = impl::evaluate_node_set_prepare(static_cast<impl::xpath_query_impl*>(_impl));
    if (!root) return xpath_node_set();

    impl::xpath_context c(n, 1, 1);
    impl::xpath_stack_data sd;

    impl::xpath_node_set_raw r = root->eval_node_set(c, sd.stack, impl::nodeset_eval_all);

    if (sd.oom)
    {
        assert(0);
        return xpath_node_set();
    }

    return xpath_node_set(r.begin(), r.end(), r.type());
}

PUGI__FN xpath_node xpath_query::evaluate_node(const xpath_node& n) const noexcept
{
    impl::xpath_ast_node* root = impl::evaluate_node_set_prepare(static_cast<impl::xpath_query_impl*>(_impl));
    if (!root) return xpath_node();

    impl::xpath_context c(n, 1, 1);
    impl::xpath_stack_data sd;

    impl::xpath_node_set_raw r = root->eval_node_set(c, sd.stack, impl::nodeset_eval_first);

    if (sd.oom)
    {
        assert(0);
        return xpath_node();
    }

    return r.first();
}

PUGI__FN const xpath_parse_result& xpath_query::result() const noexcept
{
    return _result;
}

PUGI__FN static void unspecified_bool_xpath_query(xpath_query***) noexcept
{
}

PUGI__FN xpath_query::operator xpath_query::unspecified_bool_type() const noexcept
{
    return _impl ? unspecified_bool_xpath_query : 0;
}

PUGI__FN bool xpath_query::operator!() const noexcept
{
    return !_impl;
}

PUGI__FN xpath_node xml_node::select_node(const char* query, xpath_variable_set* variables) const noexcept
{
    xpath_query q(query, variables);
    return q.evaluate_node(*this);
}

PUGI__FN xpath_node xml_node::select_node(const xpath_query& query) const noexcept
{
    return query.evaluate_node(*this);
}

PUGI__FN xpath_node_set xml_node::select_nodes(const char* query, xpath_variable_set* variables) const noexcept
{
    xpath_query q(query, variables);
    return q.evaluate_node_set(*this);
}

PUGI__FN xpath_node_set xml_node::select_nodes(const xpath_query& query) const noexcept
{
    return query.evaluate_node_set(*this);
}

PUGI__FN xpath_node xml_node::select_single_node(const char* query, xpath_variable_set* variables) const noexcept
{
    xpath_query q(query, variables);
    return q.evaluate_node(*this);
}

PUGI__FN xpath_node xml_node::select_single_node(const xpath_query& query) const noexcept
{
    return query.evaluate_node(*this);
}

} // NS

#endif


/**
 * Copyright (c) 2006-2022 Arseny Kapoulkine
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

