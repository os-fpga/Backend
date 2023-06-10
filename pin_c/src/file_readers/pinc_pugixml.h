#pragma once
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
#ifndef __pinc__HEADER_PUGIXML_HPP_9b8980f365718e_h_
#define __pinc__HEADER_PUGIXML_HPP_9b8980f365718e_h_

// Define version macro; evaluates to major * 1000 + minor * 10 + patch so that it's safe to use in less-than comparisons
// Note: pugixml used major * 100 + minor * 10 + patch format up until 1.9 (which had version identifier 190);
// starting from pugixml 1.10, the minor version number is two digits
#ifndef PUGIXML_VERSION
#    define PUGIXML_VERSION 1130 // 1.13
#endif


// Uncomment this to disable XPath
// #define PUGIXML_NO_XPATH

// Uncomment this to disable exceptions
// #define PUGIXML_NO_EXCEPTIONS


// Tune this constant to adjust max nesting for XPath queries
// #define PUGIXML_XPATH_DEPTH_LIMIT 1024


// Uncomment this to enable long long support
#define PUGIXML_HAS_LONG_LONG

////////


#include <iterator>
#include <iostream>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <exception>
#include <utility>
#include <vector>
#include <array>
#include <cfloat>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>


// Macro for deprecated features
#ifndef PUGIXML_DEPRECATED
#    if defined(__GNUC__)
#        define PUGIXML_DEPRECATED __attribute__((deprecated))
#    elif defined(_MSC_VER) && _MSC_VER >= 1300
#        define PUGIXML_DEPRECATED __declspec(deprecated)
#    else
#        define PUGIXML_DEPRECATED
#    endif
#endif

// If no API is defined, assume default
#ifndef PUGIXML_API
#define PUGIXML_API
#endif

// If no API for functions is defined, assume default
#ifndef PUGIXML_FUNCTION
#    define PUGIXML_FUNCTION PUGIXML_API
#endif

#ifndef PUGIXML_HAS_LONG_LONG
#define PUGIXML_HAS_LONG_LONG
#endif

#ifndef PUGIXML_HAS_MOVE
#define PUGIXML_HAS_MOVE
#endif

#ifndef PUGIXML_NULL
#define PUGIXML_NULL nullptr
#endif

#define PUGIXML_CHAR char

// RapidSilicon pin_c: changed namespace pugi -> pugRd to avoid potential link conflicts.

namespace pugRd {

    // Tree node types
    enum xml_node_type
    {
        node_null,         // Empty (null) node handle
        node_document,     // A document tree's absolute root
        node_element,      // Element tag, i.e. '<node/>'
        node_pcdata,       // Plain character data, i.e. 'text'
        node_cdata,        // Character data, i.e. '<![CDATA[text]]>'
        node_comment,      // Comment tag, i.e. '<!-- text -->'
        node_pi,           // Processing instruction, i.e. '<?name?>'
        node_declaration,  // Document declaration, i.e. '<?xml version="1.0"?>'
        node_doctype       // Document type declaration, i.e. '<!DOCTYPE doc>'
    };


    // Parsing options

    // Minimal parsing mode (equivalent to turning all other flags off).
    // Only elements and PCDATA sections are added to the DOM tree, no text conversions are performed.
    constexpr uint parse_minimal = 0x0000;

    // This flag determines if processing instructions (node_pi) are added to the DOM tree. This flag is off by default.
    constexpr uint parse_pi = 0x0001;

    // This flag determines if comments (node_comment) are added to the DOM tree. This flag is off by default.
    constexpr uint parse_comments = 0x0002;

    // This flag determines if CDATA sections (node_cdata) are added to the DOM tree. This flag is on by default.
    constexpr uint parse_cdata = 0x0004;

    // This flag determines if plain character data (node_pcdata) that consist only of whitespace are added to the DOM tree.
    // This flag is off by default; turning it on usually results in slower parsing and more memory consumption.
    constexpr uint parse_ws_pcdata = 0x0008;

    // This flag determines if character and entity references are expanded during parsing. This flag is on by default.
    constexpr uint parse_escapes = 0x0010;

    // This flag determines if EOL characters are normalized (converted to #xA) during parsing. This flag is on by default.
    constexpr uint parse_eol = 0x0020;

    // This flag determines if attribute values are normalized using CDATA normalization rules during parsing. This flag is on by default.
    constexpr uint parse_wconv_attribute = 0x0040;

    // This flag determines if attribute values are normalized using NMTOKENS normalization rules during parsing. This flag is off by default.
    constexpr uint parse_wnorm_attribute = 0x0080;

    // This flag determines if document declaration (node_declaration) is added to the DOM tree. This flag is off by default.
    constexpr uint parse_declaration = 0x0100;

    // This flag determines if document type declaration (node_doctype) is added to the DOM tree. This flag is off by default.
    constexpr uint parse_doctype = 0x0200;

    // This flag determines if plain character data (node_pcdata) that is the only child of the parent node and that consists only
    // of whitespace is added to the DOM tree.
    // This flag is off by default; turning it on may result in slower parsing and more memory consumption.
    constexpr uint parse_ws_pcdata_single = 0x0400;

    // This flag determines if leading and trailing whitespace is to be removed from plain character data. This flag is off by default.
    constexpr uint parse_trim_pcdata = 0x0800;

    // This flag determines if plain character data that does not have a parent node is added to the DOM tree, and if an empty document
    // is a valid document. This flag is off by default.
    constexpr uint parse_fragment = 0x1000;

    // This flag determines if plain character data is be stored in the parent element's value. This significantly changes the structure of
    // the document; this flag is only recommended for parsing documents with many PCDATA nodes in memory-constrained environments.
    // This flag is off by default.
    constexpr uint parse_embed_pcdata = 0x2000;

    // The default parsing mode.
    // Elements, PCDATA and CDATA sections are added to the DOM tree, character/reference entities are expanded,
    // End-of-Line characters are normalized, attribute values are normalized using CDATA normalization rules.
    constexpr uint parse_default = parse_cdata | parse_escapes | parse_wconv_attribute | parse_eol;

    // The full parsing mode.
    // Nodes of all types are added to the DOM tree, character/reference entities are expanded,
    // End-of-Line characters are normalized, attribute values are normalized using CDATA normalization rules.
    constexpr uint parse_full = parse_default | parse_pi | parse_comments | parse_declaration | parse_doctype;

    enum xml_encoding { encoding_utf8 };


    // Formatting flags

    // Indent the nodes that are written to output stream with as many indentation strings as deep the node is in DOM tree. This flag is on by default.
    constexpr uint format_indent = 0x01;

    // Write encoding-specific BOM to the output stream. This flag is off by default.
    constexpr uint format_write_bom = 0x02;

    // Use raw output mode (no indentation and no line breaks are written). This flag is off by default.
    constexpr uint format_raw = 0x04;

    // Omit default XML declaration even if there is no declaration in the document. This flag is off by default.
    constexpr uint format_no_declaration = 0x08;

    // Don't escape attribute values and PCDATA contents. This flag is off by default.
    constexpr uint format_no_escapes = 0x10;

    // Open file using text mode in xml_document::save_file. This enables special character (i.e. new-line) conversions on some systems. This flag is off by default.
    constexpr uint format_save_file_text = 0x20;

    // Write every attribute on a new line with appropriate indentation. This flag is off by default.
    constexpr uint format_indent_attributes = 0x40;

    // Don't output empty element tags, instead writing an explicit start and end tag even if there are no children. This flag is off by default.
    constexpr uint format_no_empty_element_tags = 0x80;

    // Skip characters belonging to range [0; 32) instead of "&#xNN;" encoding. This flag is off by default.
    constexpr uint format_skip_control_chars = 0x100;

    // Use single quotes ' instead of double quotes " for enclosing attribute values. This flag is off by default.
    constexpr uint format_attribute_single_quote = 0x200;

    // The default set of formatting flags.
    // Nodes are indented depending on their depth in DOM tree, a default declaration is output if document has none.
    constexpr uint format_default = format_indent;

    constexpr int default_double_precision = 17;
    constexpr int default_float_precision = 9;


    // Forward declarations
    struct xml_attribute_struct;
    struct xml_node_struct;

    class xml_node_iterator;
    class xml_attribute_iterator;
    class xml_named_node_iterator;

    class xml_tree_walker;

    struct xml_parse_result;

    class xml_node;

    class xml_text;

  #ifndef PUGIXML_NO_XPATH
    class xpath_node;
    class xpath_node_set;
    class xpath_query;
    class xpath_variable_set;
  #endif

    // Range-based for loop support
    template <typename It>
    class xml_object_range
    {
    public:
        typedef It const_iterator;
        typedef It iterator;

        xml_object_range(It b, It e) noexcept
            : _begin(b), _end(e)
        { }

        It begin() const noexcept { return _begin; }
        It end() const noexcept { return _end; }

        bool empty() const noexcept { return _begin == _end; }

    private:
        It _begin, _end;
    };


    // A light-weight handle for manipulating attributes in DOM tree
    class xml_attribute
    {
        friend class xml_attribute_iterator;
        friend class xml_node;

        xml_attribute_struct* _attr = nullptr;

        typedef void (*unspecified_bool_type)(xml_attribute***);

    public:

        xml_attribute() noexcept = default;

        // Constructs attribute from internal pointer
        explicit xml_attribute(xml_attribute_struct* attr) noexcept
            : _attr(attr)
        { }

        // Safe bool conversion operator
        operator unspecified_bool_type() const noexcept;

        // Borland C++ workaround
        bool operator!() const noexcept { return !_attr; }

        // Comparison operators (compares wrapped attribute pointers)
        bool operator==(const xml_attribute& r) const noexcept;
        bool operator!=(const xml_attribute& r) const noexcept;
        bool operator<(const xml_attribute& r) const noexcept;
        bool operator>(const xml_attribute& r) const noexcept;
        bool operator<=(const xml_attribute& r) const noexcept;
        bool operator>=(const xml_attribute& r) const noexcept;

        bool empty() const noexcept { return !_attr; }

        // Get attribute name/value, or "" if attribute is empty
        const char* name() const noexcept;
        const char* value() const noexcept;

        // Get attribute value, or the default value if attribute is empty
        const char* as_string(const char* def = "") const noexcept;

        // Get attribute value as a number, or the default value if conversion did not succeed or attribute is empty
        int as_int(int def = 0) const noexcept;
        uint as_uint(uint def = 0) const noexcept;
        double as_double(double def = 0) const noexcept;
        float as_float(float def = 0) const noexcept;

        long long as_llong(long long def = 0) const noexcept;
        unsigned long long as_ullong(unsigned long long def = 0) const noexcept;

        // Get attribute value as bool (returns true if first character is in '1tTyY' set), or the default value if attribute is empty
        bool as_bool(bool def = false) const noexcept;

        // Set attribute name/value (returns false if attribute is empty or there is not enough memory)
        bool set_name(const char* rhs) noexcept;
        bool set_value(const char* rhs, size_t sz) noexcept;
        bool set_value(const char* rhs) noexcept;

        // Set attribute value with type conversion (numbers are converted to strings, boolean is converted to "true"/"false")
        bool set_value(int rhs) noexcept;
        bool set_value(uint rhs) noexcept;
        bool set_value(long rhs) noexcept;
        bool set_value(unsigned long rhs) noexcept;
        bool set_value(double rhs) noexcept;
        bool set_value(double rhs, int precision) noexcept;
        bool set_value(float rhs) noexcept;
        bool set_value(float rhs, int precision) noexcept;
        bool set_value(bool rhs) noexcept;

        bool set_value(long long rhs) noexcept;
        bool set_value(unsigned long long rhs) noexcept;

        // Set attribute value (equivalent to set_value without error checking)
        xml_attribute& operator=(const char* rhs) noexcept;
        xml_attribute& operator=(int rhs) noexcept;
        xml_attribute& operator=(uint rhs) noexcept;
        xml_attribute& operator=(long rhs) noexcept;
        xml_attribute& operator=(unsigned long rhs) noexcept;
        xml_attribute& operator=(double rhs) noexcept;
        xml_attribute& operator=(float rhs) noexcept;
        xml_attribute& operator=(bool rhs) noexcept;

        xml_attribute& operator=(long long rhs) noexcept;
        xml_attribute& operator=(unsigned long long rhs) noexcept;

        // Get next/previous attribute in the attribute list of the parent node
        xml_attribute next_attribute() const noexcept;
        xml_attribute previous_attribute() const noexcept;

        // Get hash value (unique for handles to the same object)
        size_t hash_value() const noexcept;

        // Get internal pointer
        xml_attribute_struct* internal_object() const noexcept;
    };


// A light-weight handle for manipulating nodes in DOM tree
class xml_node
{
    friend class xml_attribute_iterator;
    friend class xml_node_iterator;
    friend class xml_named_node_iterator;

protected:
    xml_node_struct* _root = nullptr;

    typedef void (*unspecified_bool_type)(xml_node***);

public:

    static constexpr size_t sizeof_xml_node_struct = 12;

    xml_node() noexcept = default;

    explicit xml_node(xml_node_struct* p) noexcept
        : _root(p)
    { }

    // Safe bool conversion operator
    operator unspecified_bool_type() const noexcept;

    // Borland C++ workaround
    bool operator!() const noexcept { return !_root; }

    // Comparison operators (compares wrapped node pointers)
    bool operator==(const xml_node& r) const noexcept;
    bool operator!=(const xml_node& r) const noexcept;
    bool operator<(const xml_node& r) const noexcept;
    bool operator>(const xml_node& r) const noexcept;
    bool operator<=(const xml_node& r) const noexcept;
    bool operator>=(const xml_node& r) const noexcept;

    // Check if node is empty.
    bool empty() const noexcept;

    // Get node type
    xml_node_type type() const noexcept;

    // Get node name, or "" if node is empty or it has no name
    const char* name() const noexcept;

    // Get node value, or "" if node is empty or it has no value
    // Note: For <node>text</node> node.value() does not return "text"! Use child_value() or text() methods to access text inside nodes.
    const char* value() const noexcept;

    // Get attribute list
    xml_attribute first_attribute() const noexcept;
    xml_attribute last_attribute() const noexcept;

    // Get children list
    xml_node first_child() const noexcept;
    xml_node last_child() const noexcept;

    // Get next/previous sibling in the children list of the parent node
    xml_node next_sibling() const noexcept;
    xml_node previous_sibling() const noexcept;

    // Get parent node
    xml_node parent() const noexcept;

    // Get root of DOM tree this node belongs to
    xml_node root() const noexcept;

    // Get text object for the current node
    xml_text text() const noexcept;

    // Get child, attribute or next/previous sibling with the specified name
    xml_node child(const char* name) const noexcept;
    xml_attribute attribute(const char* name) const noexcept;
    xml_node next_sibling(const char* name) const noexcept;
    xml_node previous_sibling(const char* name) const noexcept;

    // Get attribute, starting the search from a hint (and updating hint so that searching for a sequence of attributes is fast)
    xml_attribute attribute(const char* name, xml_attribute& hint) const noexcept;

    // Get child value of current node; that is, value of the first child node of type PCDATA/CDATA
    const char* child_value() const noexcept;

    // Get child value of child with specified name. Equivalent to child(name).child_value().
    const char* child_value(const char* name) const noexcept;

    // Set node name/value (returns false if node is empty, there is not enough memory, or node can not have name/value)
    bool set_name(const char* rhs) noexcept;
    bool set_value(const char* rhs, size_t sz) noexcept;
    bool set_value(const char* rhs) noexcept;

    // Add attribute with specified name. Returns added attribute, or empty attribute on errors.
    xml_attribute append_attribute(const char* name) noexcept;
    xml_attribute prepend_attribute(const char* name) noexcept;
    xml_attribute insert_attribute_after(const char* name, const xml_attribute& attr) noexcept;
    xml_attribute insert_attribute_before(const char* name, const xml_attribute& attr) noexcept;

    // Add a copy of the specified attribute. Returns added attribute, or empty attribute on errors.
    xml_attribute append_copy(const xml_attribute& proto) noexcept;
    xml_attribute prepend_copy(const xml_attribute& proto) noexcept;
    xml_attribute insert_copy_after(const xml_attribute& proto, const xml_attribute& attr) noexcept;
    xml_attribute insert_copy_before(const xml_attribute& proto, const xml_attribute& attr) noexcept;

    // Add child node with specified type. Returns added node, or empty node on errors.
    xml_node append_child(xml_node_type type = node_element) noexcept;
    xml_node prepend_child(xml_node_type type = node_element) noexcept;
    xml_node insert_child_after(xml_node_type type, const xml_node& node) noexcept;
    xml_node insert_child_before(xml_node_type type, const xml_node& node) noexcept;

    // Add child element with specified name. Returns added node, or empty node on errors.
    xml_node append_child(const char* name) noexcept;
    xml_node prepend_child(const char* name) noexcept;
    xml_node insert_child_after(const char* name, const xml_node& node) noexcept;
    xml_node insert_child_before(const char* name, const xml_node& node) noexcept;

    // Add a copy of the specified node as a child. Returns added node, or empty node on errors.
    xml_node append_copy(const xml_node& proto) noexcept;
    xml_node prepend_copy(const xml_node& proto) noexcept;
    xml_node insert_copy_after(const xml_node& proto, const xml_node& node) noexcept;
    xml_node insert_copy_before(const xml_node& proto, const xml_node& node) noexcept;

    // Move the specified node to become a child of this node. Returns moved node, or empty node on errors.
    xml_node append_move(const xml_node& moved) noexcept;
    xml_node prepend_move(const xml_node& moved) noexcept;
    xml_node insert_move_after(const xml_node& moved, const xml_node& node) noexcept;
    xml_node insert_move_before(const xml_node& moved, const xml_node& node) noexcept;

    // Remove specified attribute
    bool remove_attribute(const xml_attribute& a) noexcept;
    bool remove_attribute(const char* name) noexcept;

    // Remove all attributes
    bool remove_attributes() noexcept;

    // Remove specified child
    bool remove_child(const xml_node& n) noexcept;
    bool remove_child(const char* name) noexcept;

    // Remove all children
    bool remove_children() noexcept;

    // Parses buffer as an XML document fragment and appends all nodes as children of the current node.
    // Copies/converts the buffer, so it may be deleted or changed after the function returns.
    // Note: append_buffer allocates memory that has the lifetime of the owning document; removing the appended nodes does not immediately reclaim that memory.
    xml_parse_result append_buffer(const void* contents, size_t size, uint options = parse_default, xml_encoding e = encoding_utf8) noexcept;

    // Find attribute using predicate. Returns first attribute for which predicate returned true.
    template <typename Predicate>
    xml_attribute find_attribute(Predicate pred) const noexcept
    {
        if (!_root) return xml_attribute();

        for (xml_attribute attrib = first_attribute(); attrib; attrib = attrib.next_attribute())
            if (pred(attrib))
                return attrib;

        return xml_attribute();
    }

    // Find child node using predicate. Returns first child for which predicate returned true.
    template <typename Predicate>
    xml_node find_child(Predicate pred) const noexcept
    {
        if (!_root) return xml_node();

        for (xml_node node = first_child(); node; node = node.next_sibling())
            if (pred(node))
                return node;

        return xml_node();
    }

    // Find node from subtree using predicate. Returns first node from subtree (depth-first), for which predicate returned true.
    template <typename Predicate>
    xml_node find_node(Predicate pred) const noexcept
    {
        if (!_root) return xml_node();

        xml_node cur = first_child();

        while (cur._root && cur._root != _root)
        {
            if (pred(cur)) return cur;

            if (cur.first_child()) cur = cur.first_child();
            else if (cur.next_sibling()) cur = cur.next_sibling();
            else
            {
                while (!cur.next_sibling() && cur._root != _root) cur = cur.parent();

                if (cur._root != _root) cur = cur.next_sibling();
            }
        }

        return xml_node();
    }

    // Find child node by attribute name/value
    xml_node find_child_by_attribute(const char* name, const char* attr_name, const char* attr_value) const noexcept;
    xml_node find_child_by_attribute(const char* attr_name, const char* attr_value) const noexcept;

    // Get the absolute node path from root as a text string.
    std::string path(char delimiter = '/') const noexcept;

    // Search for a node by path consisting of node names and . or .. elements.
    xml_node first_element_by_path(const char* path, char delimiter = '/') const noexcept;

    // Recursively traverse subtree with xml_tree_walker
    bool traverse(xml_tree_walker& walker) noexcept;

#ifndef PUGIXML_NO_XPATH

    // Select single node by evaluating XPath query. Returns first node from the resulting node set.
    xpath_node select_node(const char* query, xpath_variable_set* variables = PUGIXML_NULL) const noexcept;
    xpath_node select_node(const xpath_query& query) const noexcept;

    // Select node set by evaluating XPath query
    xpath_node_set select_nodes(const char* query, xpath_variable_set* variables = PUGIXML_NULL) const noexcept;
    xpath_node_set select_nodes(const xpath_query& query) const noexcept;

    // (deprecated: use select_node instead) Select single node by evaluating XPath query.
    PUGIXML_DEPRECATED xpath_node select_single_node(const char* query, xpath_variable_set* variables = PUGIXML_NULL) const noexcept;
    PUGIXML_DEPRECATED xpath_node select_single_node(const xpath_query& query) const noexcept;

#endif


    // Print subtree to stream
    void print( std::ostream& os, const char* indent = "\t", uint flags = format_default,
                xml_encoding e = encoding_utf8, uint depth = 0 ) const noexcept;

    // Child nodes iterators
    typedef xml_node_iterator iterator;

    iterator begin() const noexcept;
    iterator end() const noexcept;

    // Attribute iterators
    typedef xml_attribute_iterator attribute_iterator;

    attribute_iterator attributes_begin() const noexcept;
    attribute_iterator attributes_end() const noexcept;

    // Range-based for support
    xml_object_range<xml_node_iterator> children() const noexcept;
    xml_object_range<xml_named_node_iterator> children(const char* name) const noexcept;
    xml_object_range<xml_attribute_iterator> attributes() const noexcept;

    // Get node offset in parsed file/string (in char units) for debugging purposes
    ptrdiff_t offset_debug() const noexcept;

    // Get hash value (unique for handles to the same object)
    size_t hash_value() const noexcept {
        return static_cast<size_t>(reinterpret_cast<uintptr_t>(_root) / sizeof_xml_node_struct);
    }

    // Get internal pointer
    xml_node_struct* internal_object() const noexcept {
        return _root;
    }
}; // xml_node


// A helper for working with text inside PCDATA nodes
class xml_text
{
    friend class xml_node;

    xml_node_struct* _root = nullptr;

    typedef void (*unspecified_bool_type)(xml_text***);

    explicit xml_text(xml_node_struct* r) noexcept
      : _root(r)
    { }

    xml_node_struct* _data_new() noexcept;
    xml_node_struct* _data() const noexcept;

public:

    xml_text() noexcept = default;

    // Safe bool conversion operator
    operator unspecified_bool_type() const noexcept;

    // Borland C++ workaround
    bool operator!() const noexcept { return !_data(); }

    // Check if text object is empty
    bool empty() const noexcept;

    // Get text, or "" if object is empty
    const char* get() const noexcept;

    // Get text, or the default value if object is empty
    const char* as_string(const char* def = "") const noexcept;

    // Get text as a number, or the default value if conversion did not succeed or object is empty
    int as_int(int def = 0) const noexcept;
    uint as_uint(uint def = 0) const noexcept;
    double as_double(double def = 0) const noexcept;
    float as_float(float def = 0) const noexcept;

    long long as_llong(long long def = 0) const noexcept;
    unsigned long long as_ullong(unsigned long long def = 0) const noexcept;

    // Get text as bool (returns true if first character is in '1tTyY' set), or the default value if object is empty
    bool as_bool(bool def = false) const noexcept;

    // Set text (returns false if object is empty or there is not enough memory)
    bool set(const char* rhs, size_t sz) noexcept;
    bool set(const char* rhs) noexcept;

    // Set text with type conversion (numbers are converted to strings, boolean is converted to "true"/"false")
    bool set(int rhs) noexcept;
    bool set(uint rhs) noexcept;
    bool set(long rhs) noexcept;
    bool set(unsigned long rhs) noexcept;
    bool set(double rhs) noexcept;
    bool set(double rhs, int precision) noexcept;
    bool set(float rhs) noexcept;
    bool set(float rhs, int precision) noexcept;
    bool set(bool rhs) noexcept;

    bool set(long long rhs) noexcept;
    bool set(unsigned long long rhs) noexcept;

    // Set text (equivalent to set without error checking)
    xml_text& operator=(const char* rhs) noexcept;
    xml_text& operator=(int rhs) noexcept;
    xml_text& operator=(uint rhs) noexcept;
    xml_text& operator=(long rhs) noexcept;
    xml_text& operator=(unsigned long rhs) noexcept;
    xml_text& operator=(double rhs) noexcept;
    xml_text& operator=(float rhs) noexcept;
    xml_text& operator=(bool rhs) noexcept;

    xml_text& operator=(long long rhs) noexcept;
    xml_text& operator=(unsigned long long rhs) noexcept;

    // Get the data node (node_pcdata or node_cdata) for this object
    xml_node data() const noexcept;

}; // xml_text


    // Child node iterator (a bidirectional iterator over a collection of xml_node)
    class  xml_node_iterator
    {
        friend class xml_node;

        mutable xml_node _wrap;
        xml_node _parent;

        xml_node_iterator(xml_node_struct* ref, xml_node_struct* parent) noexcept;

    public:
        // Iterator traits
        typedef ptrdiff_t difference_type;
        typedef xml_node value_type;
        typedef xml_node* pointer;
        typedef xml_node& reference;

        typedef std::bidirectional_iterator_tag iterator_category;

        xml_node_iterator() noexcept = default;

        // Construct an iterator which points to the specified node
        xml_node_iterator(const xml_node& node) noexcept;

        // Iterator operators
        bool operator==(const xml_node_iterator& rhs) const noexcept;
        bool operator!=(const xml_node_iterator& rhs) const noexcept;

        xml_node& operator*() const noexcept;
        xml_node* operator->() const noexcept;

        xml_node_iterator& operator++() noexcept;
        xml_node_iterator operator++(int) noexcept;

        xml_node_iterator& operator--() noexcept;
        xml_node_iterator operator--(int) noexcept;
    };

// Attribute iterator (a bidirectional iterator over a collection of xml_attribute)
class xml_attribute_iterator
{
    friend class xml_node;

    mutable xml_attribute _wrap;
    xml_node _parent;

    xml_attribute_iterator(xml_attribute_struct* ref, xml_node_struct* parent) noexcept;

public:
    // Iterator traits
    typedef ptrdiff_t difference_type;
    typedef xml_attribute value_type;
    typedef xml_attribute* pointer;
    typedef xml_attribute& reference;

    typedef std::bidirectional_iterator_tag iterator_category;

    xml_attribute_iterator() noexcept = default;

    // Construct an iterator which points to the specified attribute
    xml_attribute_iterator(const xml_attribute& attr, const xml_node& parent) noexcept;

    // Iterator operators
    bool operator==(const xml_attribute_iterator& rhs) const noexcept;
    bool operator!=(const xml_attribute_iterator& rhs) const noexcept;

    xml_attribute& operator*() const noexcept;
    xml_attribute* operator->() const noexcept;

    xml_attribute_iterator& operator++() noexcept;
    xml_attribute_iterator operator++(int) noexcept;

    xml_attribute_iterator& operator--() noexcept;
    xml_attribute_iterator operator--(int) noexcept;
}; // xml_attribute_iterator

    // Named node range helper
    struct xml_named_node_iterator
    {
        friend class xml_node;

        // Iterator traits
        typedef ptrdiff_t difference_type;
        typedef xml_node value_type;
        typedef xml_node* pointer;
        typedef xml_node& reference;

        typedef std::bidirectional_iterator_tag iterator_category;

        xml_named_node_iterator() noexcept = default;

        // Construct an iterator which points to the specified node
        xml_named_node_iterator(const xml_node& node, const char* name) noexcept;

        // Iterator operators
        bool operator==(const xml_named_node_iterator& rhs) const noexcept;
        bool operator!=(const xml_named_node_iterator& rhs) const noexcept;

        xml_node& operator*() const noexcept;
        xml_node* operator->() const noexcept;

        xml_named_node_iterator& operator++() noexcept;
        xml_named_node_iterator operator++(int) noexcept;

        xml_named_node_iterator& operator--() noexcept;
        xml_named_node_iterator operator--(int) noexcept;

    private:
        mutable xml_node _wrap;
        xml_node _parent;
        const char* _name = nullptr;

        xml_named_node_iterator(xml_node_struct* ref, xml_node_struct* parent, const char* name) noexcept;
    };

    // Abstract tree walker class (see xml_node::traverse)
    class xml_tree_walker
    {
        friend class xml_node;

        int _depth = 0;

    protected:
        // Get current traversal depth
        int depth() const noexcept { return _depth; }

    public:

        xml_tree_walker() noexcept = default;

        virtual ~xml_tree_walker() { }

        // Callback that is called when traversal begins
        virtual bool begin(xml_node& node) noexcept;

        // Callback that is called for each node traversed
        virtual bool for_each(xml_node& node) = 0;

        // Callback that is called when traversal ends
        virtual bool end(xml_node& node) noexcept;
    };

    // Parsing status, returned as part of xml_parse_result object
    enum xml_parse_status
    {
        status_ok = 0,                // No error

        status_file_not_found,        // File was not found during load_file()
        status_io_error,            // Error reading from file/stream
        status_out_of_memory,        // Could not allocate memory
        status_internal_error,        // Internal error occurred

        status_unrecognized_tag,    // Parser could not determine tag type

        status_bad_pi,                // Parsing error occurred while parsing document declaration/processing instruction
        status_bad_comment,            // Parsing error occurred while parsing comment
        status_bad_cdata,            // Parsing error occurred while parsing CDATA section
        status_bad_doctype,            // Parsing error occurred while parsing document type declaration
        status_bad_pcdata,            // Parsing error occurred while parsing PCDATA section
        status_bad_start_element,    // Parsing error occurred while parsing start element tag
        status_bad_attribute,        // Parsing error occurred while parsing element attribute
        status_bad_end_element,        // Parsing error occurred while parsing end element tag
        status_end_element_mismatch,// There was a mismatch of start-end tags (closing tag had incorrect name, some tag was not closed or there was an excessive closing tag)

        status_append_invalid_root,    // Unable to append nodes since root type is not node_element or node_document (exclusive to xml_node::append_buffer)

        status_no_document_element    // Parsing resulted in a document without element nodes
    };


// Parsing result
struct  xml_parse_result
{
    // Parsing status (see xml_parse_status)
    xml_parse_status status;

    // Last parsed offset (in char units from start of input data)
    ptrdiff_t offset = 0;

    // Source document encoding
    static constexpr xml_encoding  encoding = encoding_utf8;

    // Default constructor, initializes object to failed state
    xml_parse_result() noexcept
      : status(status_internal_error), offset(0)
    { }

    // Cast to bool operator
    operator bool() const noexcept { return status == status_ok; }

    // Get error description
    const char* description() const noexcept;
};


// Document class (DOM tree root)
class  xml_document : public xml_node
{
    char* _buffer = nullptr;

    char _memory[192];

    xml_document(const xml_document&) = delete;
    xml_document& operator=(const xml_document&) = delete;

    void _create() noexcept;
    void _destroy() noexcept;
    void _move(xml_document& rhs) noexcept;

public:
    // Default constructor, makes empty document
    xml_document() noexcept;

    // Destructor, invalidates all node/attribute handles to this document
    ~xml_document();

    // Move semantics support
    xml_document(xml_document&& rhs) noexcept;
    xml_document& operator=(xml_document&& rhs) noexcept;

    // Removes all nodes, leaving the empty document
    void reset() noexcept;

    // Removes all nodes, then copies the entire contents of the specified document
    void reset(const xml_document& proto) noexcept;

    // Load document from stream.
    xml_parse_result load(std::istream& stream,
            uint options = parse_default, xml_encoding e = encoding_utf8) noexcept;

    // (deprecated: use load_string instead) Load document from zero-terminated string.
    PUGIXML_DEPRECATED xml_parse_result load(const char* contents, uint options = parse_default) noexcept;

    // Load document from zero-terminated string.
    xml_parse_result load_string(const char* contents, uint options = parse_default) noexcept;

    // Load document from file
    xml_parse_result load_file(const char* path, uint options = parse_default, xml_encoding e = encoding_utf8) noexcept;

    // Load document from buffer. Copies/converts the buffer, so it may be deleted or changed after the function returns.
    xml_parse_result load_buffer(const void* contents, size_t size, uint options = parse_default, xml_encoding e = encoding_utf8) noexcept;


    // Load document from buffer, using the buffer for in-place parsing
    //   (the buffer is modified and used for storage of document data).
    // You should ensure that buffer data will persist throughout the document's lifetime,
    //   and free the buffer memory manually once document is destroyed.
    xml_parse_result load_buffer_inplace(void* contents, size_t size, uint options = parse_default, xml_encoding e = encoding_utf8) noexcept;


    ////// Load document from buffer, using the buffer for in-place parsing
    ////   (the buffer is modified and used for storage of document data).
    //// You should allocate the buffer with pugixml allocation function;
    ////   document will free the buffer when it is no longer needed (you can't use it anymore).
    //// xml_parse_result load_buffer_inplace_own(void* contents, size_t size, uint options = parse_default, xml_encoding e = encoding_utf8);


    // Save XML to file
    //bool save_file(const char* path, ....)

    // Get document element
    xml_node document_element() const noexcept;
};

#ifndef PUGIXML_NO_XPATH
    // XPath query return type
    enum xpath_value_type
    {
        xpath_type_none,      // Unknown type (query failed to compile)
        xpath_type_node_set,  // Node set (xpath_node_set)
        xpath_type_number,      // Number
        xpath_type_string,      // String
        xpath_type_boolean      // Boolean
    };

    // XPath parsing result
    struct  xpath_parse_result
    {
        // Error message (0 if no error)
        const char* error = nullptr;

        // Last parsed offset (in char units from string start)
        ptrdiff_t offset = 0;

        // Default constructor, initializes object to failed state
        xpath_parse_result() noexcept
            : error("Internal error"), offset(0)
        {
        }

        // Cast to bool operator
        operator bool() const noexcept { return error == 0; }

        // Get error description
        const char* description() const noexcept { return error ? error : "No error"; }
    };

    // A single XPath variable
    class  xpath_variable
    {
        friend class xpath_variable_set;

    protected:
        xpath_value_type _type;
        xpath_variable* _next = nullptr;

        xpath_variable(xpath_value_type type) noexcept;

        // Non-copyable semantics
        xpath_variable(const xpath_variable&) = delete;
        xpath_variable& operator=(const xpath_variable&) = delete;

    public:
        // Get variable name
        const char* name() const noexcept;

        // Get variable type
        xpath_value_type type() const noexcept;

        // Get variable value; no type conversion is performed, default value (false, NaN, empty string, empty node set) is returned on type mismatch error
        bool get_boolean() const noexcept;
        double get_number() const noexcept;
        const char* get_string() const noexcept;
        const xpath_node_set& get_node_set() const noexcept;

        // Set variable value; no type conversion is performed, false is returned on type mismatch error
        bool set(bool value) noexcept;
        bool set(double value) noexcept;
        bool set(const char* value) noexcept;
        bool set(const xpath_node_set& value) noexcept;
    };


// A set of XPath variables
class  xpath_variable_set
{
    xpath_variable* _data[64];

    void _assign(const xpath_variable_set& rhs) noexcept;
    void _swap(xpath_variable_set& rhs) noexcept;

    xpath_variable* _find(const char* name) const noexcept;

    static bool _clone(xpath_variable* var, xpath_variable** out_result) noexcept;
    static void _destroy(xpath_variable* var) noexcept;

public:
    // Default constructor/destructor
    xpath_variable_set() noexcept;
    ~xpath_variable_set();

    // Copy constructor/assignment operator
    xpath_variable_set(const xpath_variable_set& rhs) noexcept;
    xpath_variable_set& operator=(const xpath_variable_set& rhs) noexcept;

    // Move semantics support
    xpath_variable_set(xpath_variable_set&& rhs) noexcept;
    xpath_variable_set& operator=(xpath_variable_set&& rhs) noexcept;

    // Add a new variable or get the existing one, if the types match
    xpath_variable* add(const char* name, xpath_value_type type) noexcept;

    // Set value of an existing variable; no type conversion is performed, false is returned if there is no such variable or if types mismatch
    bool set(const char* name, bool value) noexcept;
    bool set(const char* name, double value) noexcept;
    bool set(const char* name, const char* value) noexcept;
    bool set(const char* name, const xpath_node_set& value) noexcept;

    // Get existing variable by name
    xpath_variable* get(const char* name) noexcept;
    const xpath_variable* get(const char* name) const noexcept;
};

    // A compiled XPath query object
    class  xpath_query
    {
    private:
        void* _impl = nullptr;
        xpath_parse_result _result;

        typedef void (*unspecified_bool_type)(xpath_query***);

        // Non-copyable semantics
        xpath_query(const xpath_query&) = delete;
        xpath_query& operator=(const xpath_query&) = delete;

    public:
        // Construct a compiled object from XPath expression.
        // If PUGIXML_NO_EXCEPTIONS is not defined, throws xpath_exception on compilation errors.
        explicit xpath_query(const char* query, xpath_variable_set* variables = PUGIXML_NULL) noexcept;

        xpath_query() noexcept = default;

        ~xpath_query();

        // Move semantics support
        xpath_query(xpath_query&& rhs) noexcept;
        xpath_query& operator=(xpath_query&& rhs) noexcept;

        // Get query expression return type
        xpath_value_type return_type() const noexcept;

        // Evaluate expression as boolean value in the specified context; performs type conversion if necessary.
        bool evaluate_boolean(const xpath_node& n) const noexcept;

        // Evaluate expression as double value in the specified context; performs type conversion if necessary.
        double evaluate_number(const xpath_node& n) const noexcept;

        // Evaluate expression as string value in the specified context; performs type conversion if necessary.
        std::string evaluate_string(const xpath_node& n) const noexcept;

        // Evaluate expression as string value in the specified context; performs type conversion if necessary.
        // At most capacity characters are written to the destination buffer, full result size is returned (includes terminating zero).
        // If PUGIXML_NO_EXCEPTIONS is defined, returns empty  set instead.
        size_t evaluate_string(char* buffer, size_t capacity, const xpath_node& n) const noexcept;

        // Evaluate expression as node set in the specified context.
        // If PUGIXML_NO_EXCEPTIONS is defined, returns empty node set instead.
        xpath_node_set evaluate_node_set(const xpath_node& n) const noexcept;

        // Evaluate expression as node set in the specified context.
        // Return first node in document order, or empty node if node set is empty.
        // If PUGIXML_NO_EXCEPTIONS is defined, returns empty node instead.
        xpath_node evaluate_node(const xpath_node& n) const noexcept;

        // Get parsing result (used to get compilation errors in PUGIXML_NO_EXCEPTIONS mode)
        const xpath_parse_result& result() const noexcept;

        // Safe bool conversion operator
        operator unspecified_bool_type() const noexcept;

        // Borland C++ workaround
        bool operator!() const noexcept;
    };


#ifndef PUGIXML_NO_EXCEPTIONS
// XPath exception class
class  xpath_exception : public std::exception
{
    xpath_parse_result _result;

public:
    // Construct exception from parse result
    explicit xpath_exception(const xpath_parse_result& result) noexcept;

    // Get error message
    virtual const char* what() const noexcept override;

    // Get parse result
    const xpath_parse_result& result() const noexcept;
};
#endif // PUGIXML_NO_EXCEPTIONS


    // XPath node class (either xml_node or xml_attribute)
    class  xpath_node
    {
    private:
        xml_node _node;
        xml_attribute _attribute;

        typedef void (*unspecified_bool_type)(xpath_node***);

    public:
        // Default constructor; constructs empty XPath node
        xpath_node() noexcept = default;

        // Construct XPath node from XML node/attribute
        xpath_node(const xml_node& node) noexcept;
        xpath_node(const xml_attribute& attribute, const xml_node& parent) noexcept;

        // Get node/attribute, if any
        xml_node node() const noexcept;
        xml_attribute attribute() const noexcept;

        // Get parent of contained node/attribute
        xml_node parent() const noexcept;

        // Safe bool conversion operator
        operator unspecified_bool_type() const noexcept;

        // Borland C++ workaround
        bool operator!() const noexcept;

        // Comparison operators
        bool operator==(const xpath_node& n) const noexcept;
        bool operator!=(const xpath_node& n) const noexcept;
    };


    // A fixed-size collection of XPath nodes
    class  xpath_node_set
    {
    public:
        // Collection type
        enum type_t
        {
            type_unsorted,            // Not ordered
            type_sorted,            // Sorted by document order (ascending)
            type_sorted_reverse        // Sorted by document order (descending)
        };

        // Constant iterator type
        typedef const xpath_node* const_iterator;

        // We define non-constant iterator to be the same as constant iterator so that various generic algorithms (i.e. boost foreach) work
        typedef const xpath_node* iterator;

        // Default constructor. Constructs empty set.
        xpath_node_set();

        // Constructs a set from iterator range; data is not checked for duplicates and is not sorted according to provided type, so be careful
        xpath_node_set(const_iterator begin, const_iterator end, type_t type = type_unsorted);

        // Destructor
        ~xpath_node_set();

        // Copy constructor/assignment operator
        xpath_node_set(const xpath_node_set& ns);
        xpath_node_set& operator=(const xpath_node_set& ns);

        // Move semantics support
        xpath_node_set(xpath_node_set&& rhs) noexcept;
        xpath_node_set& operator=(xpath_node_set&& rhs) noexcept;

        // Get collection type
        type_t type() const noexcept;

        // Get collection size
        size_t size() const noexcept;

        // Indexing operator
        const xpath_node& operator[](size_t index) const noexcept;

        // Collection iterators
        const_iterator begin() const noexcept;
        const_iterator end() const noexcept;

        // Sort the collection in ascending/descending order by document order
        void sort(bool reverse = false) noexcept;

        // Get first node in the collection by document order
        xpath_node first() const noexcept;

        // Check if collection is empty
        bool empty() const noexcept;

    private:
        type_t _type;

        xpath_node _storage[1];

        xpath_node* _begin = nullptr;
        xpath_node* _end = nullptr;

        void _assign(const_iterator begin, const_iterator end, type_t type) noexcept;
        void _move(xpath_node_set& rhs) noexcept;
    };
#endif

} // NS pugRd

namespace std
{
    // Workarounds for (non-standard) iterator category detection for older versions (MSVC7/IC8 and earlier)
    std::bidirectional_iterator_tag PUGIXML_FUNCTION _Iter_cat(const pugRd::xml_node_iterator&);
    std::bidirectional_iterator_tag PUGIXML_FUNCTION _Iter_cat(const pugRd::xml_attribute_iterator&);
    std::bidirectional_iterator_tag PUGIXML_FUNCTION _Iter_cat(const pugRd::xml_named_node_iterator&);
}


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

