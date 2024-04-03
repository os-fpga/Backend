#pragma once
/*
Original code by Lee Thomason (www.grinninglizard.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/
// Modified by serge-rs.

#ifndef __pln__TINYXML2_INCLUDED_389d92da3824_
#define __pln__TINYXML2_INCLUDED_389d92da3824_

#include "util/pln_log.h"

//   g++ -Wall  -DTINYXML2_DEBUG tinyxml2.cpp xmltest.cpp -o gccxmltest.exe
#define TINYXML2_DEBUG

#define TINYXML2_MAJOR_VERSION 9
#define TINYXML2_MINOR_VERSION 0
#define TINYXML2_PATCH_VERSION 0

namespace tinxml2
{
constexpr int TIXML2_MAJOR_VERSION = 9;
constexpr int TIXML2_MINOR_VERSION = 0;
constexpr int TIXML2_PATCH_VERSION = 0;
constexpr int TINYXML2_MAX_ELEMENT_DEPTH = 800;

class XMLDocument;
class XMLElement;
class XMLAttribute;
class XMLComment;
class XMLText;
class XMLDeclaration;
class XMLUnknown;
class XMLPrinter;


enum XMLError { // WARNING: must match XMLDocument::_errorNames[]
    XML_SUCCESS = 0,
    XML_NO_ATTRIBUTE,
    XML_WRONG_ATTRIBUTE_TYPE,
    XML_ERROR_FILE_NOT_FOUND,
    XML_ERROR_FILE_COULD_NOT_BE_OPENED,
    XML_ERROR_FILE_READ_ERROR,
    XML_ERROR_PARSING_ELEMENT,
    XML_ERROR_PARSING_ATTRIBUTE,
    XML_ERROR_PARSING_TEXT,
    XML_ERROR_PARSING_CDATA,
    XML_ERROR_PARSING_COMMENT,
    XML_ERROR_PARSING_DECLARATION,
    XML_ERROR_PARSING_UNKNOWN,
    XML_ERROR_EMPTY_DOCUMENT,
    XML_ERROR_MISMATCHED_ELEMENT,
    XML_ERROR_PARSING,
    XML_CAN_NOT_CONVERT_TEXT,
    XML_NO_TEXT_NODE,
    XML_ELEMENT_DEPTH_EXCEEDED,

    XML_ERROR_COUNT
};


/*
    A class that wraps strings. Normally stores the start and end
    pointers into the XML file itself, and will apply normalization
    and entity translation if actually read. Can also store (and memory
    manage) a traditional char[]
*/
class StrPair
{
public:
    enum Mode {
        NEEDS_ENTITY_PROCESSING         = 0x01,
        NEEDS_NEWLINE_NORMALIZATION     = 0x02,
        NEEDS_WHITESPACE_COLLAPSING     = 0x04,

        TEXT_ELEMENT                    = NEEDS_ENTITY_PROCESSING | NEEDS_NEWLINE_NORMALIZATION,
        TEXT_ELEMENT_LEAVE_ENTITIES     = NEEDS_NEWLINE_NORMALIZATION,
        ATTRIBUTE_NAME                  = 0,
        ATTRIBUTE_VALUE                 = NEEDS_ENTITY_PROCESSING | NEEDS_NEWLINE_NORMALIZATION,
        ATTRIBUTE_VALUE_LEAVE_ENTITIES  = NEEDS_NEWLINE_NORMALIZATION,
        COMMENT                         = NEEDS_NEWLINE_NORMALIZATION
    };

    StrPair() noexcept
      : _flags( 0 ), _start( 0 ), _end( 0 )
    {}

    ~StrPair();

    void Set( char* start, char* end, int flags ) noexcept {
        assert( start );
        assert( end );
        Reset();
        _start  = start;
        _end    = end;
        _flags  = flags | NEEDS_FLUSH;
    }

    const char* GetStr() noexcept;

    bool Empty() const noexcept {
        return _start == _end;
    }

    void SetInternedStr( const char* str ) noexcept {
        Reset();
        _start = const_cast<char*>(str);
    }

    void SetStr( const char* str, int flags=0 ) noexcept;

    char* ParseText( char* in, const char* endTag, int strFlags, int* curLineNumPtr ) noexcept;
    char* ParseName( char* in ) noexcept;

    void TransferTo( StrPair* other ) noexcept;
    void Reset() noexcept;

private:
    void CollapseWhitespace() noexcept;

    enum {
        NEEDS_FLUSH  = 0x100,
        NEEDS_DELETE = 0x200
    };

    int     _flags = 0;
    char*   _start = 0;
    char*   _end = 0;

    StrPair( const StrPair& other ) = delete;    // not supported
    void operator=( const StrPair& other ) = delete;    // not supported, use TransferTo()
};


/*
    A dynamic array of Plain Old Data. Doesn't support constructors, etc.
    Has a small initial memory pool, so that low or no usage will not
    cause a call to new/delete
*/
template <class T, int INITIAL_SIZE>
class DynArray
{
public:
    DynArray() :
        _mem( _pool ),
        _allocated( INITIAL_SIZE ),
        _size( 0 )
    {
    }

    ~DynArray() {
        if ( _mem != _pool ) {
            delete [] _mem;
        }
    }

    void Clear() {
        _size = 0;
    }

    void Push( T t ) {
        assert( _size < INT_MAX );
        EnsureCapacity( _size+1 );
        _mem[_size] = t;
        ++_size;
    }

    T* PushArr( int count ) {
        assert( count >= 0 );
        assert( _size <= INT_MAX - count );
        EnsureCapacity( _size+count );
        T* ret = &_mem[_size];
        _size += count;
        return ret;
    }

    T Pop() {
        assert( _size > 0 );
        --_size;
        return _mem[_size];
    }

    void PopArr( int count ) {
        assert( _size >= count );
        _size -= count;
    }

    bool Empty() const                    {
        return _size == 0;
    }

    T& operator[](int i)                {
        assert( i>= 0 && i < _size );
        return _mem[i];
    }

    const T& operator[](int i) const    {
        assert( i>= 0 && i < _size );
        return _mem[i];
    }

    const T& PeekTop() const            {
        assert( _size > 0 );
        return _mem[ _size - 1];
    }

    int Size() const                    {
        assert( _size >= 0 );
        return _size;
    }

    int Capacity() const                {
        assert( _allocated >= INITIAL_SIZE );
        return _allocated;
    }

    void SwapRemove(int i) {
        assert(i >= 0 && i < _size);
        assert(_size > 0);
        _mem[i] = _mem[_size - 1];
        --_size;
    }

    const T* Mem() const                {
        assert( _mem );
        return _mem;
    }

    T* Mem() {
        assert( _mem );
        return _mem;
    }

private:
    DynArray( const DynArray& ) = delete; // not supported
    void operator=( const DynArray& ) = delete; // not supported

    void EnsureCapacity( int cap ) {
        assert( cap > 0 );
        if ( cap > _allocated ) {
            assert( cap <= INT_MAX / 2 );
            const int newAllocated = cap * 2;
            T* newMem = new T[newAllocated];
            assert( newAllocated >= _size );
            memcpy( newMem, _mem, sizeof(T)*_size );    // warning: not using constructors, only works for PODs
            if ( _mem != _pool ) {
                delete [] _mem;
            }
            _mem = newMem;
            _allocated = newAllocated;
        }
    }

    T*  _mem = 0;
    T   _pool[INITIAL_SIZE];
    int _allocated = 0;  // objects allocated
    int _size = 0;       // number objects in use
};


/*
    Parent virtual class of a pool for fast allocation
    and deallocation of objects.
*/
class MemPool
{
public:
    MemPool() noexcept = default;
    virtual ~MemPool() {}

    virtual int ItemSize() const noexcept = 0;
    virtual void* Alloc() noexcept = 0;
    virtual void Free( void* ) noexcept = 0;
    virtual void SetTracked() noexcept = 0;
};


/*
    Template child class to create pools of the correct type.
*/
template< int ITEM_SIZE >
class MemPoolT : public MemPool
{
public:
    MemPoolT() noexcept
      : _blockPtrs(), _root(0), _currentAllocs(0), _nAllocs(0), _maxAllocs(0), _nUntracked(0)
    { }

    ~MemPoolT() {
        MemPoolT< ITEM_SIZE >::Clear();
    }

    void Clear() noexcept {
        // Delete the blocks.
        while( !_blockPtrs.Empty()) {
            Block* lastBlock = _blockPtrs.Pop();
            delete lastBlock;
        }
        _root = 0;
        _currentAllocs = 0;
        _nAllocs = 0;
        _maxAllocs = 0;
        _nUntracked = 0;
    }

    virtual int ItemSize() const noexcept { return ITEM_SIZE; }

    int CurrentAllocs() const noexcept { return _currentAllocs; }

    virtual void* Alloc() noexcept {
        if ( !_root ) {
            // Need a new block.
            Block* block = new Block;
            _blockPtrs.Push( block );

            Item* blockItems = block->items;
            for( int i = 0; i < ITEMS_PER_BLOCK - 1; ++i ) {
                blockItems[i].next = &(blockItems[i + 1]);
            }
            blockItems[ITEMS_PER_BLOCK - 1].next = 0;
            _root = blockItems;
        }
        Item* const result = _root;
        assert( result != 0 );
        _root = _root->next;

        ++_currentAllocs;
        if ( _currentAllocs > _maxAllocs ) {
            _maxAllocs = _currentAllocs;
        }
        ++_nAllocs;
        ++_nUntracked;
        return result;
    }

    virtual void Free( void* mem ) noexcept {
        if ( !mem ) {
            return;
        }
        --_currentAllocs;
        Item* item = static_cast<Item*>( mem );
#ifdef TINYXML2_DEBUG
        memset( item, 0xfe, sizeof( *item ) );
#endif
        item->next = _root;
        _root = item;
    }

    void Trace( const char* name ) noexcept {
        printf( "Mempool %s watermark=%d [%dk] current=%d size=%d nAlloc=%d blocks=%d\n",
                name, _maxAllocs, _maxAllocs * ITEM_SIZE / 1024, _currentAllocs,
                ITEM_SIZE, _nAllocs, _blockPtrs.Size() );
    }

    void SetTracked() noexcept { --_nUntracked; }

    int Untracked() const noexcept { return _nUntracked; }

    // This number is perf sensitive. 4k seems like a good tradeoff on my machine.
    // The test file is large, 170k.
    // Release:        VS2010 gcc(no opt)
    //        1k:        4000
    //        2k:        4000
    //        4k:        3900    21000
    //        16k:    5200
    //        32k:    4300
    //        64k:    4000    21000
    // Declared public because some compilers do not accept to use ITEMS_PER_BLOCK
    // in private part if ITEMS_PER_BLOCK is private
    static constexpr int  ITEMS_PER_BLOCK  =  (4 * 1024) / ITEM_SIZE;

private:
    MemPoolT( const MemPoolT& ) = delete; // not supported
    void operator=( const MemPoolT& ) = delete; // not supported

    union Item {
        Item*   next;
        char    itemData[ITEM_SIZE];
    };
    struct Block {
        Item items[ITEMS_PER_BLOCK];
    };

    DynArray< Block*, 10 > _blockPtrs;
    Item* _root = nullptr;

    int _currentAllocs = 0;
    int _nAllocs = 0;
    int _maxAllocs = 0;
    int _nUntracked = 0;
};



/**
    Implements the interface to the "Visitor pattern" (see the Accept() method.)
    If you call the Accept() method, it requires being passed a XMLVisitor
    class to handle callbacks. For nodes that contain other nodes (Document, Element)
    you will get called with a VisitEnter/VisitExit pair. Nodes that are always leafs
    are simply called with Visit().

    If you return 'true' from a Visit method, recursive parsing will continue. If you return
    false, <b>no children of this node or its siblings</b> will be visited.

    All flavors of Visit methods have a default implementation that returns 'true' (continue
    visiting). You need to only override methods that are interesting to you.

    Generally Accept() is called on the XMLDocument, although all nodes support visiting.

    You should never change the document from a callback.

    @sa XMLNode::Accept()
*/
class XMLVisitor
{
public:
    virtual ~XMLVisitor() {}

    /// Visit a document.
    virtual bool VisitEnter( const XMLDocument& /*doc*/ ) noexcept {
        return true;
    }
    /// Visit a document.
    virtual bool VisitExit( const XMLDocument& /*doc*/ ) noexcept {
        return true;
    }

    /// Visit an element.
    virtual bool VisitEnter( const XMLElement& /*element*/, const XMLAttribute* /*firstAttribute*/ ) noexcept {
        return true;
    }
    /// Visit an element.
    virtual bool VisitExit( const XMLElement& /*element*/ ) noexcept {
        return true;
    }

    /// Visit a declaration.
    virtual bool Visit( const XMLDeclaration& /*declaration*/ ) noexcept {
        return true;
    }
    /// Visit a text node.
    virtual bool Visit( const XMLText& /*text*/ ) noexcept {
        return true;
    }
    /// Visit a comment node.
    virtual bool Visit( const XMLComment& /*comment*/ ) noexcept {
        return true;
    }
    /// Visit an unknown node.
    virtual bool Visit( const XMLUnknown& /*unknown*/ ) noexcept {
        return true;
    }
};


/*
    Utility functionality.
*/
struct XMLUtil
{
    static const char* SkipWhiteSpace( const char* p, int* curLineNumPtr ) noexcept
    {
        assert( p );

        while( IsWhiteSpace(*p) ) {
            if (curLineNumPtr && *p == '\n') {
                ++(*curLineNumPtr);
            }
            ++p;
        }
        assert( p );
        return p;
    }
    static char* SkipWhiteSpace( char* const p, int* curLineNumPtr ) noexcept {
        return const_cast<char*>( SkipWhiteSpace( const_cast<const char*>(p), curLineNumPtr ) );
    }

    // Anything in the high order range of UTF-8 is assumed to not be whitespace. This isn't
    // correct, but simple, and usually works.
    static bool IsWhiteSpace( char p )  noexcept  {
        return !IsUTF8Continuation(p) && isspace( static_cast<uint8_t>(p) );
    }

    inline static bool IsNameStartChar( uint8_t ch ) noexcept {
        if ( ch >= 128 ) {
            // This is a heuristic guess in attempt to not implement Unicode-aware isalpha()
            return true;
        }
        if ( isalpha( ch ) ) {
            return true;
        }
        return ch == ':' || ch == '_';
    }

    inline static bool IsNameChar( uint8_t ch ) noexcept {
        return IsNameStartChar( ch )
               || isdigit( ch )
               || ch == '.'
               || ch == '-';
    }

    inline static bool IsPrefixHex( const char* p) noexcept {
        p = SkipWhiteSpace(p, 0);
        return p && *p == '0' && ( *(p + 1) == 'x' || *(p + 1) == 'X');
    }

    inline static bool StringEqual( const char* p, const char* q, int nChar=INT_MAX ) noexcept {
        if ( p == q ) {
            return true;
        }
        assert( p );
        assert( q );
        assert( nChar >= 0 );
        return strncmp( p, q, nChar ) == 0;
    }

    inline static bool IsUTF8Continuation( const char p ) noexcept {
        return ( p & 0x80 ) != 0;
    }

    static const char* ReadBOM( const char* p, bool* hasBOM ) noexcept;

    // p is the starting location,
    // the UTF-8 value of the entity will be placed in value, and length filled in.
    static const char* GetCharacterRef( const char* p, char* value, int* length ) noexcept;
    static void ConvertUTF32ToUTF8( unsigned long input, char* output, int* length ) noexcept;

    // converts primitive types to strings
    static void ToStr( int v, char* buffer, size_t bufferSize ) noexcept;
    static void ToStr( unsigned v, char* buffer, size_t bufferSize ) noexcept;
    static void ToStr( bool v, char* buffer, size_t bufferSize ) noexcept;
    static void ToStr( float v, char* buffer, size_t bufferSize ) noexcept;
    static void ToStr( double v, char* buffer, size_t bufferSize ) noexcept;
    static void ToStr(int64_t v, char* buffer, size_t bufferSize) noexcept;
    static void ToStr(uint64_t v, char* buffer, size_t bufferSize) noexcept;

    // converts strings to primitive types
    static bool    ToInt( const char* str, int* value ) noexcept;
    static bool ToUnsigned( const char* str, unsigned* value ) noexcept;
    static bool    ToBool( const char* str, bool* value ) noexcept;
    static bool    ToFloat( const char* str, float* value ) noexcept;
    static bool ToDouble( const char* str, double* value ) noexcept;
    static bool ToInt64(const char* str, int64_t* value) noexcept;
    static bool ToUnsigned64(const char* str, uint64_t* value) noexcept;

    // Changes what is serialized for a boolean value.
    // Default to "true" and "false". Shouldn't be changed
    // unless you have a special testing or compatibility need.
    // Be careful: static, global, & not thread safe.
    // Be sure to set static const memory as parameters.
    static void SetBoolSerialization(const char* writeTrue, const char* writeFalse) noexcept;

private:
    static const char* writeBoolTrue;
    static const char* writeBoolFalse;
};


/** XMLNode is a base class for every object that is in the
    XML Document Object Model (DOM), except XMLAttributes.
    Nodes have siblings, a parent, and children which can
    be navigated. A node is always in a XMLDocument.
    The type of a XMLNode can be queried, and it can
    be cast to its more defined type.

    A XMLDocument allocates memory for all its Nodes.
    When the XMLDocument gets deleted, all its Nodes
    will also be deleted.

    @verbatim
    A Document can contain:    Element    (container or leaf)
                            Comment (leaf)
                            Unknown (leaf)
                            Declaration( leaf )

    An Element can contain:    Element (container or leaf)
                            Text    (leaf)
                            Attributes (not on tree)
                            Comment (leaf)
                            Unknown (leaf)

    @endverbatim
*/
class XMLNode
{
    friend class XMLDocument;
    friend class XMLElement;
public:

    /// Get the XMLDocument that owns this XMLNode.
    const XMLDocument* GetDocument()  const  noexcept  {
        assert( _document );
        return _document;
    }
    /// Get the XMLDocument that owns this XMLNode.
    XMLDocument* GetDocument()   noexcept    {
        assert( _document );
        return _document;
    }

    /// Safely cast to an Element, or null.
    virtual XMLElement*  ToElement()  noexcept  {
        return 0;
    }
    /// Safely cast to Text, or null.
    virtual XMLText*  ToText()  noexcept  {
        return 0;
    }
    /// Safely cast to a Comment, or null.
    virtual XMLComment*  ToComment()  noexcept  {
        return 0;
    }
    /// Safely cast to a Document, or null.
    virtual XMLDocument*  ToDocument()  noexcept  {
        return 0;
    }
    /// Safely cast to a Declaration, or null.
    virtual XMLDeclaration*  ToDeclaration()  noexcept {
        return 0;
    }
    /// Safely cast to an Unknown, or null.
    virtual XMLUnknown*  ToUnknown()  noexcept  {
        return 0;
    }

    virtual const XMLElement*  ToElement() const noexcept {
        return 0;
    }
    virtual const XMLText*  ToText() const noexcept {
        return 0;
    }
    virtual const XMLComment*  ToComment() const noexcept {
        return 0;
    }
    virtual const XMLDocument*  ToDocument() const noexcept {
        return 0;
    }
    virtual const XMLDeclaration*  ToDeclaration() const noexcept {
        return 0;
    }
    virtual const XMLUnknown*  ToUnknown() const noexcept {
        return 0;
    }

    /** The meaning of 'value' changes for the specific type.
        @verbatim
        Document:    empty (NULL is returned, not an empty string)
        Element:    name of the element
        Comment:    the comment text
        Unknown:    the tag contents
        Text:        the text string
        @endverbatim
    */
    const char* Value() const noexcept;

    /** Set the Value of an XML node.
        @sa Value()
    */
    void SetValue( const char* val, bool staticMem=false ) noexcept;

    /// Gets the line number the node is in, if the document was parsed from a file.
    int GetLineNum() const noexcept { return _parseLineNum; }

    /// Get the parent of this node on the DOM.
    const XMLNode*   Parent() const  noexcept  { return _parent; }

    XMLNode* Parent()  noexcept  { return _parent; }

    /// Returns true if this node has no children.
    bool NoChildren() const   noexcept  { return !_firstChild; }

    /// Get the first child node, or null if none exists.
    const XMLNode*  FirstChild()  const   noexcept    { return _firstChild; }

    XMLNode*  FirstChild()  noexcept   { return _firstChild; }

    /** Get the first child element, or optionally the first child
        element with the specified name.
    */
    const XMLElement* FirstChildElement( const char* name = 0 ) const noexcept;

    XMLElement* FirstChildElement( const char* name = 0 )  noexcept   {
        return const_cast<XMLElement*>(const_cast<const XMLNode*>(this)->FirstChildElement( name ));
    }

    /// Get the last child node, or null if none exists.
    const XMLNode*    LastChild() const   noexcept   { return _lastChild; }

    XMLNode*      LastChild()  noexcept    { return _lastChild; }

    /** Get the last child element or optionally the last child
        element with the specified name.
    */
    const XMLElement* LastChildElement( const char* name = 0 ) const noexcept;

    XMLElement* LastChildElement( const char* name = 0 )  noexcept  {
        return const_cast<XMLElement*>(const_cast<const XMLNode*>(this)->LastChildElement(name) );
    }

    /// Get the previous (left) sibling node of this node.
    const XMLNode*  PreviousSibling() const noexcept { return _prev; }
    XMLNode*  PreviousSibling() noexcept { return _prev; }

    /// Get the previous (left) sibling element of this node, with an optionally supplied name.
    const XMLElement*  PreviousSiblingElement( const char* name = 0 ) const noexcept;

    XMLElement*  PreviousSiblingElement( const char* name = 0 ) noexcept {
        return const_cast<XMLElement*>(const_cast<const XMLNode*>(this)->PreviousSiblingElement( name ) );
    }

    /// Get the next (right) sibling node of this node.
    const XMLNode*   NextSibling()  const noexcept  {
        return _next;
    }

    XMLNode*   NextSibling()  noexcept  {
        return _next;
    }

    /// Get the next (right) sibling element of this node, with an optionally supplied name.
    const XMLElement*  NextSiblingElement( const char* name = 0 ) const noexcept;

    XMLElement*  NextSiblingElement( const char* name = 0 )  noexcept  {
        return const_cast<XMLElement*>(const_cast<const XMLNode*>(this)->NextSiblingElement( name ) );
    }

    /**
        Add a child node as the last (right) child.
        If the child node is already part of the document,
        it is moved from its old location to the new location.
        Returns the addThis argument or 0 if the node does not
        belong to the same document.
    */
    XMLNode* InsertEndChild( XMLNode* addThis ) noexcept;

    XMLNode* LinkEndChild( XMLNode* addThis ) noexcept {
        return InsertEndChild( addThis );
    }

    /**
        Add a child node as the first (left) child.
        If the child node is already part of the document,
        it is moved from its old location to the new location.
        Returns the addThis argument or 0 if the node does not
        belong to the same document.
    */
    XMLNode* InsertFirstChild( XMLNode* addThis ) noexcept;

    /**
        Add a node after the specified child node.
        If the child node is already part of the document,
        it is moved from its old location to the new location.
        Returns the addThis argument or 0 if the afterThis node
        is not a child of this node, or if the node does not
        belong to the same document.
    */
    XMLNode* InsertAfterChild( XMLNode* afterThis, XMLNode* addThis ) noexcept;

    /**
        Delete all the children of this node.
    */
    void DeleteChildren() noexcept;

    /**
        Delete a child of this node.
    */
    void DeleteChild( XMLNode* node ) noexcept;

    /**
        Make a copy of this node, but not its children.
        You may pass in a Document pointer that will be
        the owner of the new Node. If the 'document' is
        null, then the node returned will be allocated
        from the current Document. (this->GetDocument())

        Note: if called on a XMLDocument, this will return null.
    */
    virtual XMLNode* ShallowClone( XMLDocument* document ) const noexcept = 0;

    /**
        Make a copy of this node and all its children.

        If the 'target' is null, then the nodes will
        be allocated in the current document. If 'target'
        is specified, the memory will be allocated is the
        specified XMLDocument.

        NOTE: This is probably not the correct tool to
        copy a document, since XMLDocuments can have multiple
        top level XMLNodes. You probably want to use
        XMLDocument::DeepCopy()
    */
    XMLNode* DeepClone( XMLDocument* target ) const noexcept;

    /**
        Test if 2 nodes are the same, but don't test children.
        The 2 nodes do not need to be in the same Document.

        Note: if called on a XMLDocument, this will return false.
    */
    virtual bool ShallowEqual( const XMLNode* compare ) const noexcept = 0;

    /** Accept a hierarchical visit of the nodes in the TinyXML-2 DOM. Every node in the
        XML tree will be conditionally visited and the host will be called back
        via the XMLVisitor interface.

        This is essentially a SAX interface for TinyXML-2. (Note however it doesn't re-parse
        the XML for the callbacks, so the performance of TinyXML-2 is unchanged by using this
        interface versus any other.)

        The interface has been based on ideas from:

        - http://www.saxproject.org/
        - http://c2.com/cgi/wiki?HierarchicalVisitorPattern

        Which are both good references for "visiting".

        An example of using Accept():
        @verbatim
        XMLPrinter printer;
        tinyxmlDoc.Accept( &printer );
        const char* xmlcstr = printer.CStr();
        @endverbatim
    */
    virtual bool Accept( XMLVisitor* visitor ) const = 0;

    /**
        Set user data into the XMLNode. TinyXML-2 in
        no way processes or interprets user data.
        It is initially 0.
    */
    void SetUserData(void* userData) noexcept { _userData = userData; }

    /**
        Get user data set into the XMLNode. TinyXML-2 in
        no way processes or interprets user data.
        It is initially 0.
    */
    void* GetUserData() const noexcept { return _userData; }

protected:
    explicit XMLNode( XMLDocument* ) noexcept;

    virtual ~XMLNode();

    virtual char* ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr) noexcept;

    XMLDocument*     _document = nullptr;
    XMLNode*         _parent = nullptr;
    mutable StrPair  _value;
    int              _parseLineNum = 0;

    XMLNode*         _firstChild = nullptr;
    XMLNode*         _lastChild = nullptr;

    XMLNode*         _prev = nullptr;
    XMLNode*         _next = nullptr;

    void*            _userData = nullptr;

private:
    MemPool*        _memPool = nullptr;

    void Unlink( XMLNode* child ) noexcept;
    static void DeleteNode( XMLNode* node ) noexcept;
    void InsertChildPreamble( XMLNode* insertThis ) const noexcept;
    const XMLElement* ToElementWithName( const char* name ) const noexcept;

    XMLNode( const XMLNode& ) = delete;    // not supported
    XMLNode& operator=( const XMLNode& ) = delete;    // not supported
}; // XMLNode


/** XML text.

    Note that a text node can have child element nodes, for example:
    @verbatim
    <root>This is <b>bold</b></root>
    @endverbatim

    A text node can have 2 ways to output the next. "normal" output
    and CDATA. It will default to the mode it was parsed from the XML file and
    you generally want to leave it alone, but you can change the output mode with
    SetCData() and query it with CData().
*/
class XMLText : public XMLNode
{
    friend class XMLDocument;
public:
    virtual bool Accept( XMLVisitor* visitor ) const noexcept;

    virtual XMLText* ToText()  noexcept { return this; }
    virtual const XMLText* ToText() const  noexcept { return this; }

    /// Declare whether this should be CDATA or standard text.
    void SetCData(bool isCData) noexcept { _isCData = isCData; }

    /// Returns true if this is a CDATA text element.
    bool CData() const noexcept { return _isCData; }

    virtual XMLNode* ShallowClone( XMLDocument* document ) const noexcept;
    virtual bool ShallowEqual( const XMLNode* compare ) const noexcept;

protected:
    explicit XMLText( XMLDocument* doc ) noexcept
      : XMLNode( doc ), _isCData( false )
    { }

    virtual ~XMLText() { }

    char* ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr ) noexcept;

private:
    bool _isCData = false;

    XMLText( const XMLText& ) = delete;    // not supported
    XMLText& operator=( const XMLText& ) = delete;    // not supported
};


/** An XML Comment. */
class XMLComment : public XMLNode
{
    friend class XMLDocument;
public:
    virtual XMLComment*  ToComment()  noexcept  {
        return this;
    }
    virtual const XMLComment* ToComment() const noexcept  {
        return this;
    }

    virtual bool Accept( XMLVisitor* visitor ) const noexcept;

    virtual XMLNode* ShallowClone( XMLDocument* document ) const noexcept;
    virtual bool ShallowEqual( const XMLNode* compare ) const noexcept;

protected:
    explicit XMLComment( XMLDocument* doc ) noexcept;

    virtual ~XMLComment();

    char* ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr) noexcept;

private:
    XMLComment( const XMLComment& ) = delete;    // not supported
    XMLComment& operator=( const XMLComment& ) = delete;    // not supported
};


/** In correct XML the declaration is the first entry in the file.
    @verbatim
        <?xml version="1.0" standalone="yes"?>
    @endverbatim

    TinyXML-2 will happily read or write files without a declaration,
    however.

    The text of the declaration isn't interpreted. It is parsed
    and written as a string.
*/
class XMLDeclaration : public XMLNode
{
    friend class XMLDocument;
public:
    virtual XMLDeclaration*  ToDeclaration()  noexcept  {
        return this;
    }
    virtual const XMLDeclaration* ToDeclaration() const  noexcept  {
        return this;
    }

    virtual bool Accept( XMLVisitor* visitor ) const noexcept;

    virtual XMLNode* ShallowClone( XMLDocument* document ) const noexcept;
    virtual bool ShallowEqual( const XMLNode* compare ) const noexcept;

protected:
    explicit XMLDeclaration( XMLDocument* doc ) noexcept;

    virtual ~XMLDeclaration();

    char* ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr ) noexcept;

private:
    XMLDeclaration( const XMLDeclaration& ) = delete;    // not supported
    XMLDeclaration& operator=( const XMLDeclaration& ) = delete;    // not supported
};


/** Any tag that TinyXML-2 doesn't recognize is saved as an
    unknown. It is a tag of text, but should not be modified.
    It will be written back to the XML, unchanged, when the file
    is saved.

    DTD tags get thrown into XMLUnknowns.
*/
class XMLUnknown : public XMLNode
{
    friend class XMLDocument;
public:
    virtual XMLUnknown*  ToUnknown()  noexcept  {
        return this;
    }
    virtual const XMLUnknown*  ToUnknown() const noexcept  {
        return this;
    }

    virtual bool Accept( XMLVisitor* visitor ) const noexcept;

    virtual XMLNode* ShallowClone( XMLDocument* document ) const noexcept;
    virtual bool ShallowEqual( const XMLNode* compare ) const noexcept;

protected:
    explicit XMLUnknown( XMLDocument* doc ) noexcept;
    virtual ~XMLUnknown() {}

    char* ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr ) noexcept;

private:
    XMLUnknown( const XMLUnknown& ) = delete;    // not supported
    XMLUnknown& operator=( const XMLUnknown& ) = delete;    // not supported
};



/** An attribute is a name-value pair. Elements have an arbitrary
    number of attributes, each with a unique name.

    @note The attributes are not XMLNodes. You may only query the
    Next() attribute in a list.
*/
class XMLAttribute
{
    friend class XMLElement;
public:
    /// The name of the attribute.
    const char* Name() const noexcept;

    /// The value of the attribute.
    const char* Value() const noexcept;

    /// Gets the line number the attribute is in, if the document was parsed from a file.
    int GetLineNum() const noexcept { return _parseLineNum; }

    /// The next attribute in the list.
    const XMLAttribute* Next() const noexcept {
        return _next;
    }

    /** IntValue interprets the attribute as an integer, and returns the value.
        If the value isn't an integer, 0 will be returned. There is no error checking;
        use QueryIntValue() if you need error checking.
    */
    int IntValue() const noexcept {
        int i = 0;
        QueryIntValue(&i);
        return i;
    }

    int64_t Int64Value() const noexcept {
        int64_t i = 0;
        QueryInt64Value(&i);
        return i;
    }

    uint64_t Unsigned64Value() const noexcept {
        uint64_t i = 0;
        QueryUnsigned64Value(&i);
        return i;
    }

    /// Query as an unsigned integer. See IntValue()
    unsigned UnsignedValue() const noexcept  {
        unsigned i=0;
        QueryUnsignedValue( &i );
        return i;
    }

    /// Query as a boolean. See IntValue()
    bool BoolValue() const noexcept {
        bool b=false;
        QueryBoolValue( &b );
        return b;
    }

    /// Query as a double. See IntValue()
    double DoubleValue() const noexcept {
        double d=0;
        QueryDoubleValue( &d );
        return d;
    }

    /// Query as a float. See IntValue()
    float   FloatValue() const noexcept {
        float f=0;
        QueryFloatValue( &f );
        return f;
    }

    /** QueryIntValue interprets the attribute as an integer, and returns the value
        in the provided parameter. The function will return XML_SUCCESS on success,
        and XML_WRONG_ATTRIBUTE_TYPE if the conversion is not successful.
    */
    XMLError QueryIntValue( int* value ) const noexcept;
    /// See QueryIntValue
    XMLError QueryUnsignedValue( unsigned int* value ) const noexcept;
    /// See QueryIntValue
    XMLError QueryInt64Value(int64_t* value) const noexcept;
    /// See QueryIntValue
    XMLError QueryUnsigned64Value(uint64_t* value) const noexcept;
    /// See QueryIntValue
    XMLError QueryBoolValue( bool* value ) const noexcept;
    /// See QueryIntValue
    XMLError QueryDoubleValue( double* value ) const noexcept;
    /// See QueryIntValue
    XMLError QueryFloatValue( float* value ) const noexcept;

    /// Set the attribute to a string value.
    void SetAttribute( const char* value ) noexcept;
    /// Set the attribute to value.
    void SetAttribute( int value ) noexcept;
    /// Set the attribute to value.
    void SetAttribute( unsigned value ) noexcept;
    /// Set the attribute to value.
    void SetAttribute(int64_t value) noexcept;
    /// Set the attribute to value.
    void SetAttribute(uint64_t value) noexcept;
    /// Set the attribute to value.
    void SetAttribute( bool value ) noexcept;
    /// Set the attribute to value.
    void SetAttribute( double value ) noexcept;
    /// Set the attribute to value.
    void SetAttribute( float value ) noexcept;

private:
    static constexpr int  BUF_SIZE = 200;

    XMLAttribute() noexcept
      : _name(), _value(),_parseLineNum( 0 ), _next( 0 ), _memPool( 0 ) {}

    virtual ~XMLAttribute()   {}

    XMLAttribute( const XMLAttribute& ) = delete;    // not supported
    void operator=( const XMLAttribute& ) = delete;    // not supported

    void SetName( const char* name ) noexcept;

    char* ParseDeep( char* p, bool processEntities, int* curLineNumPtr ) noexcept;

    mutable StrPair  _name;
    mutable StrPair  _value;
    int              _parseLineNum = 0;
    XMLAttribute*    _next = nullptr;
    MemPool*         _memPool = nullptr;
};


/** The element is a container class. It has a value, the element name,
    and can contain other elements, text, comments, and unknowns.
    Elements also contain an arbitrary number of attributes.
*/
class XMLElement : public XMLNode
{
    friend class XMLDocument;
public:

    /// Get the name of an element (which is the Value() of the node.)
    const char* Name() const noexcept { return Value(); }

    /// Set the name of the element.
    void SetName( const char* str, bool staticMem=false ) noexcept {
        SetValue( str, staticMem );
    }

    virtual XMLElement* ToElement() noexcept { return this; }
    virtual const XMLElement* ToElement() const noexcept { return this; }

    virtual bool Accept( XMLVisitor* visitor ) const noexcept;

    /** Given an attribute name, Attribute() returns the value
        for the attribute of that name, or null if none
        exists. For example:

        @verbatim
        const char* value = ele->Attribute( "foo" );
        @endverbatim

        The 'value' parameter is normally null. However, if specified,
        the attribute will only be returned if the 'name' and 'value'
        match. This allow you to write code:

        @verbatim
        if ( ele->Attribute( "foo", "bar" ) ) callFooIsBar();
        @endverbatim

        rather than:
        @verbatim
        if ( ele->Attribute( "foo" ) ) {
            if ( strcmp( ele->Attribute( "foo" ), "bar" ) == 0 ) callFooIsBar();
        }
        @endverbatim
    */
    const char* Attribute( const char* name, const char* value=0 ) const noexcept;

    /** Given an attribute name, IntAttribute() returns the value
        of the attribute interpreted as an integer. The default
        value will be returned if the attribute isn't present,
        or if there is an error. (For a method with error
        checking, see QueryIntAttribute()).
    */
    int IntAttribute(const char* name, int defaultValue = 0) const noexcept;
    /// See IntAttribute()
    unsigned UnsignedAttribute(const char* name, unsigned defaultValue = 0) const noexcept;
    /// See IntAttribute()
    int64_t Int64Attribute(const char* name, int64_t defaultValue = 0) const noexcept;
    /// See IntAttribute()
    uint64_t Unsigned64Attribute(const char* name, uint64_t defaultValue = 0) const noexcept;
    /// See IntAttribute()
    bool BoolAttribute(const char* name, bool defaultValue = false) const noexcept;
    /// See IntAttribute()
    double DoubleAttribute(const char* name, double defaultValue = 0) const noexcept;
    /// See IntAttribute()
    float FloatAttribute(const char* name, float defaultValue = 0) const noexcept;

    /** Given an attribute name, QueryIntAttribute() returns
        XML_SUCCESS, XML_WRONG_ATTRIBUTE_TYPE if the conversion
        can't be performed, or XML_NO_ATTRIBUTE if the attribute
        doesn't exist. If successful, the result of the conversion
        will be written to 'value'. If not successful, nothing will
        be written to 'value'. This allows you to provide default
        value:

        @verbatim
        int value = 10;
        QueryIntAttribute( "foo", &value );        // if "foo" isn't found, value will still be 10
        @endverbatim
    */
    XMLError QueryIntAttribute( const char* name, int* value ) const noexcept {
        const XMLAttribute* a = FindAttribute( name );
        if ( !a ) {
            return XML_NO_ATTRIBUTE;
        }
        return a->QueryIntValue( value );
    }

    /// See QueryIntAttribute()
    XMLError QueryUnsignedAttribute( const char* name, unsigned int* value ) const noexcept {
        const XMLAttribute* a = FindAttribute( name );
        if ( !a ) {
            return XML_NO_ATTRIBUTE;
        }
        return a->QueryUnsignedValue( value );
    }

    /// See QueryIntAttribute()
    XMLError QueryInt64Attribute(const char* name, int64_t* value) const noexcept {
        const XMLAttribute* a = FindAttribute(name);
        if (!a) {
            return XML_NO_ATTRIBUTE;
        }
        return a->QueryInt64Value(value);
    }

    /// See QueryIntAttribute()
    XMLError QueryUnsigned64Attribute(const char* name, uint64_t* value) const noexcept {
        const XMLAttribute* a = FindAttribute(name);
        if(!a) {
            return XML_NO_ATTRIBUTE;
        }
        return a->QueryUnsigned64Value(value);
    }

    /// See QueryIntAttribute()
    XMLError QueryBoolAttribute( const char* name, bool* value ) const noexcept {
        const XMLAttribute* a = FindAttribute( name );
        if ( !a ) {
            return XML_NO_ATTRIBUTE;
        }
        return a->QueryBoolValue( value );
    }
    /// See QueryIntAttribute()
    XMLError QueryDoubleAttribute( const char* name, double* value ) const noexcept {
        const XMLAttribute* a = FindAttribute( name );
        if ( !a ) {
            return XML_NO_ATTRIBUTE;
        }
        return a->QueryDoubleValue( value );
    }
    /// See QueryIntAttribute()
    XMLError QueryFloatAttribute( const char* name, float* value ) const noexcept {
        const XMLAttribute* a = FindAttribute( name );
        if ( !a ) {
            return XML_NO_ATTRIBUTE;
        }
        return a->QueryFloatValue( value );
    }

    /// See QueryIntAttribute()
    XMLError QueryStringAttribute(const char* name, const char** value) const noexcept {
        const XMLAttribute* a = FindAttribute(name);
        if (!a) {
            return XML_NO_ATTRIBUTE;
        }
        *value = a->Value();
        return XML_SUCCESS;
    }


    /** Given an attribute name, QueryAttribute() returns
        XML_SUCCESS, XML_WRONG_ATTRIBUTE_TYPE if the conversion
        can't be performed, or XML_NO_ATTRIBUTE if the attribute
        doesn't exist. It is overloaded for the primitive types,
        and is a generally more convenient replacement of
        QueryIntAttribute() and related functions.

        If successful, the result of the conversion
        will be written to 'value'. If not successful, nothing will
        be written to 'value'. This allows you to provide default
        value:

        @verbatim
        int value = 10;
        QueryAttribute( "foo", &value );        // if "foo" isn't found, value will still be 10
        @endverbatim
    */
    XMLError QueryAttribute( const char* name, int* value ) const noexcept {
        return QueryIntAttribute( name, value );
    }

    XMLError QueryAttribute( const char* name, unsigned int* value ) const noexcept {
        return QueryUnsignedAttribute( name, value );
    }

    XMLError QueryAttribute(const char* name, int64_t* value) const noexcept {
        return QueryInt64Attribute(name, value);
    }

    XMLError QueryAttribute(const char* name, uint64_t* value) const noexcept {
        return QueryUnsigned64Attribute(name, value);
    }

    XMLError QueryAttribute( const char* name, bool* value ) const noexcept {
        return QueryBoolAttribute( name, value );
    }

    XMLError QueryAttribute( const char* name, double* value ) const noexcept {
        return QueryDoubleAttribute( name, value );
    }

    XMLError QueryAttribute( const char* name, float* value ) const noexcept {
        return QueryFloatAttribute( name, value );
    }

    XMLError QueryAttribute(const char* name, const char** value) const noexcept {
        return QueryStringAttribute(name, value);
    }

    /// Sets the named attribute to value.
    void SetAttribute( const char* name, const char* value ) noexcept  {
        XMLAttribute* a = FindOrCreateAttribute( name );
        a->SetAttribute( value );
    }
    /// Sets the named attribute to value.
    void SetAttribute( const char* name, int value ) noexcept {
        XMLAttribute* a = FindOrCreateAttribute( name );
        a->SetAttribute( value );
    }
    /// Sets the named attribute to value.
    void SetAttribute( const char* name, unsigned value ) noexcept {
        XMLAttribute* a = FindOrCreateAttribute( name );
        a->SetAttribute( value );
    }

    /// Sets the named attribute to value.
    void SetAttribute(const char* name, int64_t value) noexcept {
        XMLAttribute* a = FindOrCreateAttribute(name);
        a->SetAttribute(value);
    }

    /// Sets the named attribute to value.
    void SetAttribute(const char* name, uint64_t value) noexcept {
        XMLAttribute* a = FindOrCreateAttribute(name);
        a->SetAttribute(value);
    }

    /// Sets the named attribute to value.
    void SetAttribute( const char* name, bool value ) noexcept {
        XMLAttribute* a = FindOrCreateAttribute( name );
        a->SetAttribute( value );
    }
    /// Sets the named attribute to value.
    void SetAttribute( const char* name, double value ) noexcept {
        XMLAttribute* a = FindOrCreateAttribute( name );
        a->SetAttribute( value );
    }
    /// Sets the named attribute to value.
    void SetAttribute( const char* name, float value ) noexcept {
        XMLAttribute* a = FindOrCreateAttribute( name );
        a->SetAttribute( value );
    }

    /**
        Delete an attribute.
    */
    void DeleteAttribute( const char* name ) noexcept;

    /// Return the first attribute in the list.
    const XMLAttribute* FirstAttribute() const noexcept {
        return _rootAttribute;
    }
    /// Query a specific attribute in the list.
    const XMLAttribute* FindAttribute( const char* name ) const noexcept;

    /** Convenience function for easy access to the text inside an element. Although easy
        and concise, GetText() is limited compared to getting the XMLText child
        and accessing it directly.

        If the first child of 'this' is a XMLText, the GetText()
        returns the character string of the Text node, else null is returned.

        This is a convenient method for getting the text of simple contained text:
        @verbatim
        <foo>This is text</foo>
            const char* str = fooElement->GetText();
        @endverbatim

        'str' will be a pointer to "This is text".

        Note that this function can be misleading. If the element foo was created from
        this XML:
        @verbatim
            <foo><b>This is text</b></foo>
        @endverbatim

        then the value of str would be null. The first child node isn't a text node, it is
        another element. From this XML:
        @verbatim
            <foo>This is <b>text</b></foo>
        @endverbatim
        GetText() will return "This is ".
    */
    const char* GetText() const noexcept;

    /** Convenience function for easy access to the text inside an element. Although easy
        and concise, SetText() is limited compared to creating an XMLText child
        and mutating it directly.

        If the first child of 'this' is a XMLText, SetText() sets its value to
        the given string, otherwise it will create a first child that is an XMLText.

        This is a convenient method for setting the text of simple contained text:
        @verbatim
        <foo>This is text</foo>
            fooElement->SetText( "Hullaballoo!" );
         <foo>Hullaballoo!</foo>
        @endverbatim

        Note that this function can be misleading. If the element foo was created from
        this XML:
        @verbatim
            <foo><b>This is text</b></foo>
        @endverbatim

        then it will not change "This is text", but rather prefix it with a text element:
        @verbatim
            <foo>Hullaballoo!<b>This is text</b></foo>
        @endverbatim

        For this XML:
        @verbatim
            <foo />
        @endverbatim
        SetText() will generate
        @verbatim
            <foo>Hullaballoo!</foo>
        @endverbatim
    */
    void SetText( const char* inText ) noexcept;
    /// Convenience method for setting text inside an element. See SetText() for important limitations.
    void SetText( int value ) noexcept;
    /// Convenience method for setting text inside an element. See SetText() for important limitations.
    void SetText( unsigned value ) noexcept;
    /// Convenience method for setting text inside an element. See SetText() for important limitations.
    void SetText(int64_t value) noexcept;
    /// Convenience method for setting text inside an element. See SetText() for important limitations.
    void SetText(uint64_t value) noexcept;
    /// Convenience method for setting text inside an element. See SetText() for important limitations.
    void SetText( bool value ) noexcept;
    /// Convenience method for setting text inside an element. See SetText() for important limitations.
    void SetText( double value ) noexcept;
    /// Convenience method for setting text inside an element. See SetText() for important limitations.
    void SetText( float value ) noexcept;

    /**
        Convenience method to query the value of a child text node. This is probably best
        shown by example. Given you have a document is this form:
        @verbatim
            <point>
                <x>1</x>
                <y>1.4</y>
            </point>
        @endverbatim

        The QueryIntText() and similar functions provide a safe and easier way to get to the
        "value" of x and y.

        @verbatim
            int x = 0;
            float y = 0;    // types of x and y are contrived for example
            const XMLElement* xElement = pointElement->FirstChildElement( "x" );
            const XMLElement* yElement = pointElement->FirstChildElement( "y" );
            xElement->QueryIntText( &x );
            yElement->QueryFloatText( &y );
        @endverbatim

        @returns XML_SUCCESS (0) on success, XML_CAN_NOT_CONVERT_TEXT if the text cannot be converted
                 to the requested type, and XML_NO_TEXT_NODE if there is no child text to query.

    */
    XMLError QueryIntText( int* ival ) const noexcept;
    /// See QueryIntText()
    XMLError QueryUnsignedText( unsigned* uval ) const noexcept;
    /// See QueryIntText()
    XMLError QueryInt64Text(int64_t* uval) const noexcept;
    /// See QueryIntText()
    XMLError QueryUnsigned64Text(uint64_t* uval) const noexcept;
    /// See QueryIntText()
    XMLError QueryBoolText( bool* bval ) const noexcept;
    /// See QueryIntText()
    XMLError QueryDoubleText( double* dval ) const noexcept;
    /// See QueryIntText()
    XMLError QueryFloatText( float* fval ) const noexcept;

    int IntText(int defaultValue = 0) const noexcept;

    /// See QueryIntText()
    unsigned UnsignedText(unsigned defaultValue = 0) const noexcept;
    /// See QueryIntText()
    int64_t Int64Text(int64_t defaultValue = 0) const noexcept;
    /// See QueryIntText()
    uint64_t Unsigned64Text(uint64_t defaultValue = 0) const noexcept;
    /// See QueryIntText()
    bool BoolText(bool defaultValue = false) const noexcept;
    /// See QueryIntText()
    double DoubleText(double defaultValue = 0) const noexcept;
    /// See QueryIntText()
    float FloatText(float defaultValue = 0) const noexcept;

    /**
        Convenience method to create a new XMLElement and add it as last (right)
        child of this node. Returns the created and inserted element.
    */
    XMLElement* InsertNewChildElement(const char* name) noexcept;
    /// See InsertNewChildElement()
    XMLComment* InsertNewComment(const char* comment) noexcept;
    /// See InsertNewChildElement()
    XMLText* InsertNewText(const char* text) noexcept;
    /// See InsertNewChildElement()
    XMLDeclaration* InsertNewDeclaration(const char* text) noexcept;
    /// See InsertNewChildElement()
    XMLUnknown* InsertNewUnknown(const char* text) noexcept;


    // internal:
    enum ElementClosingType {
        OPEN,        // <foo>
        CLOSED,        // <foo/>
        CLOSING        // </foo>
    };
    ElementClosingType ClosingType() const noexcept {
        return _closingType;
    }
    virtual XMLNode* ShallowClone( XMLDocument* document ) const noexcept;
    virtual bool ShallowEqual( const XMLNode* compare ) const noexcept;

protected:
    char* ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr ) noexcept;

private:
    XMLElement( XMLDocument* doc ) noexcept;

    virtual ~XMLElement();

    XMLElement( const XMLElement& ) = delete;    // not supported
    void operator=( const XMLElement& ) = delete;    // not supported

    XMLAttribute* FindOrCreateAttribute( const char* name ) noexcept;
    char* ParseAttributes( char* p, int* curLineNumPtr ) noexcept;
    static void DeleteAttribute( XMLAttribute* attribute ) noexcept;
    XMLAttribute* CreateAttribute() noexcept;

    static constexpr int BUF_SIZE = 200;

    ElementClosingType _closingType;

    // The attribute list is ordered; there is no 'lastAttribute'
    // because the list needs to be scanned for dupes before adding
    // a new attribute.
    XMLAttribute* _rootAttribute = nullptr;
};


enum Whitespace {
    PRESERVE_WHITESPACE,
    COLLAPSE_WHITESPACE
};


/** A Document binds together all the functionality.
    It can be saved, loaded, and printed to the screen.
    All Nodes are connected and allocated to a Document.
    If the Document is deleted, all its Nodes are also deleted.
*/
class XMLDocument : public XMLNode
{
    friend class XMLElement;
    // Gives access to SetError and Push/PopDepth, but over-access for everything else.
    // Wishing C++ had "internal" scope.
    friend class XMLNode;
    friend class XMLText;
    friend class XMLComment;
    friend class XMLDeclaration;
    friend class XMLUnknown;
public:
    /// constructor
    XMLDocument( bool processEntities = true, Whitespace wsMode = PRESERVE_WHITESPACE ) noexcept;
    ~XMLDocument();

    virtual XMLDocument* ToDocument()  noexcept   {
        assert( this == _document );
        return this;
    }
    virtual const XMLDocument* ToDocument() const noexcept {
        assert( this == _document );
        return this;
    }

    /**
        Parse an XML file from a character string.
        Returns XML_SUCCESS (0) on success, or an errorID.

        You may optionally pass in the 'nBytes', which is
        the number of bytes which will be parsed. If not
        specified, TinyXML will assume 'xml' points to a
        null terminated string.
    */
    XMLError Parse( const char* xml, size_t nBytes=static_cast<size_t>(-1) ) noexcept;

    /**
        Same as Parse(), only the buffer is externally owned.
    */
    XMLError ParseExternal(char* externalBuffer, size_t nBytes) noexcept;

    /**
        Load an XML file from disk.
        Returns XML_SUCCESS (0) on success, or
        an errorID.
    */
    XMLError LoadFile( const char* filename ) noexcept;

    inline XMLError LoadFile( const std::string& fn ) noexcept { return LoadFile(fn.c_str()); }

    /**
        Load an XML file from disk. You are responsible
        for providing and closing the FILE*.

        NOTE: The file should be opened as binary ("rb")
        not text in order for TinyXML-2 to correctly
        do newline normalization.

        Returns XML_SUCCESS (0) on success, or
        an errorID.
    */
    XMLError LoadFile( FILE* ) noexcept;

    /**
        Save the XML file to disk.
        Returns XML_SUCCESS (0) on success, or
        an errorID.
    */
    XMLError SaveFile( const char* filename, bool compact = false ) noexcept;

    /**
        Save the XML file to disk. You are responsible
        for providing and closing the FILE*.

        Returns XML_SUCCESS (0) on success, or
        an errorID.
    */
    XMLError SaveFile( FILE* fp, bool compact = false ) noexcept;

    bool ProcessEntities() const noexcept  {
        return _processEntities;
    }
    Whitespace WhitespaceMode() const noexcept  {
        return _whitespaceMode;
    }

    /**
        Returns true if this document has a leading Byte Order Mark of UTF8.
    */
    bool HasBOM() const noexcept {
        return _writeBOM;
    }
    /** Sets whether to write the BOM when writing the file.
    */
    void SetBOM( bool useBOM ) noexcept {
        _writeBOM = useBOM;
    }

    /** Return the root element of DOM. Equivalent to FirstChildElement().
        To get the first node, use FirstChild().
    */
    XMLElement* RootElement()  noexcept   {
        return FirstChildElement();
    }
    const XMLElement* RootElement() const  noexcept  {
        return FirstChildElement();
    }

    /** Print the Document. If the Printer is not provided, it will
        print to stdout. If you provide Printer, this can print to a file:
        @verbatim
        XMLPrinter printer( fp );
        doc.Print( &printer );
        @endverbatim

        Or you can use a printer to print to memory:
        @verbatim
        XMLPrinter printer;
        doc.Print( &printer );
        // printer.CStr() has a const char* to the XML
        @endverbatim
    */
    void Print( XMLPrinter* streamer=0 ) const noexcept;
    virtual bool Accept( XMLVisitor* visitor ) const noexcept;

    /**
        Create a new Element associated with
        this Document. The memory for the Element
        is managed by the Document.
    */
    XMLElement* NewElement( const char* name ) noexcept;

    /**
        Create a new Comment associated with
        this Document. The memory for the Comment
        is managed by the Document.
    */
    XMLComment* NewComment( const char* comment ) noexcept;

    /**
        Create a new Text associated with
        this Document. The memory for the Text
        is managed by the Document.
    */
    XMLText* NewText( const char* text ) noexcept;

    /**
        Create a new Declaration associated with
        this Document. The memory for the object
        is managed by the Document.

        If the 'text' param is null, the standard
        declaration is used.:
        @verbatim
            <?xml version="1.0" encoding="UTF-8"?>
        @endverbatim
    */
    XMLDeclaration* NewDeclaration( const char* text=0 ) noexcept;

    /**
        Create a new Unknown associated with
        this Document. The memory for the object
        is managed by the Document.
    */
    XMLUnknown* NewUnknown( const char* text ) noexcept;

    /**
        Delete a node associated with this document.
        It will be unlinked from the DOM.
    */
    void DeleteNode( XMLNode* node ) noexcept;

    /// Clears the error flags.
    void ClearError() noexcept;

    /// Return true if there was an error parsing the document.
    bool Error() const noexcept {
        return _errorID != XML_SUCCESS;
    }

    /// Return the errorID.
    XMLError  ErrorID() const noexcept {
        return _errorID;
    }
    const char* ErrorName() const noexcept;

    static const char* ErrorIDToName(XMLError errorID) noexcept;

    /** Returns a "long form" error description. A hopefully helpful
        diagnostic with location, line number, and/or additional info.
    */
    const char* ErrorStr() const noexcept;

    /// A (trivial) utility function that prints the ErrorStr() to stdout.
    void PrintError() const noexcept;

    /// Return the line where the error occurred, or zero if unknown.
    int ErrorLineNum() const noexcept
    {
        return _errorLineNum;
    }

    /// Clear the document, resetting it to the initial state.
    void Clear() noexcept;

    /**
        Copies this document to a target document.
        The target will be completely cleared before the copy.
        If you want to copy a sub-tree, see XMLNode::DeepClone().

        NOTE: that the 'target' must be non-null.
    */
    void DeepCopy(XMLDocument* target) const noexcept;

    // internal
    char* Identify( char* p, XMLNode** node ) noexcept;

    // internal
    void MarkInUse(const XMLNode* const) noexcept;

    virtual XMLNode* ShallowClone( XMLDocument* ) const noexcept { return 0; }
    virtual bool ShallowEqual( const XMLNode* /*compare*/ ) const noexcept { return false; }

    bool IsExternalBuffer() const noexcept { return _externalSize > 0; }

private:
    XMLDocument( const XMLDocument& ) = delete;
    void operator=( const XMLDocument& ) = delete;

    bool            _writeBOM = 0;
    bool            _processEntities = 0;
    XMLError        _errorID;
    Whitespace      _whitespaceMode;

    mutable StrPair  _errorStr;
    int     _errorLineNum = 0;

    char*   _charBuffer = nullptr;

    size_t  _externalSize = 0; // if _charBuffer is not owned by this XMLDocument

    int     _parseCurLineNum = 0;
    int     _parsingDepth = 0;

    // Memory tracking does add some overhead.
    // However, the code assumes that you don't
    // have a bunch of unlinked nodes around.
    // Therefore it takes less memory to track
    // in the document vs. a linked list in the XMLNode,
    // and the performance is the same.
    DynArray<XMLNode*, 10> _unlinked;

    MemPoolT< sizeof(XMLElement) >      _elementPool;
    MemPoolT< sizeof(XMLAttribute) >  _attributePool;
    MemPoolT< sizeof(XMLText) >            _textPool;
    MemPoolT< sizeof(XMLComment) >      _commentPool;

    static const char* _errorNames[XML_ERROR_COUNT];

    void Parse0() noexcept;

    void SetError( XMLError error, int lineNum, const char* format, ... ) noexcept;

    // Something of an obvious security hole, once it was discovered.
    // Either an ill-formed XML or an excessively deep one can overflow
    // the stack. Track stack depth, and error out if needed.
    class DepthTracker {
    public:
        explicit DepthTracker(XMLDocument * document) noexcept
        {
            this->_document = document;
            document->PushDepth();
        }
        ~DepthTracker() {
            _document->PopDepth();
        }
    private:
        XMLDocument * _document = nullptr;
    };
    void PushDepth() noexcept;
    void PopDepth() noexcept;

    template<class NodeType, int PoolElementSize>
    NodeType* CreateUnlinkedNode( MemPoolT<PoolElementSize>& pool ) noexcept
    {
        assert( sizeof( NodeType ) == PoolElementSize );
        assert( sizeof( NodeType ) == pool.ItemSize() );
        NodeType* returnNode = new (pool.Alloc()) NodeType( this );
        assert( returnNode );
        returnNode->_memPool = &pool;

        _unlinked.Push(returnNode);
        return returnNode;
    }

}; // XMLDocument


/**
    A XMLHandle is a class that wraps a node pointer with null checks; this is
    an incredibly useful thing. Note that XMLHandle is not part of the TinyXML-2
    DOM structure. It is a separate utility class.

    Take an example:
    @verbatim
    <Document>
        <Element attributeA = "valueA">
            <Child attributeB = "value1" />
            <Child attributeB = "value2" />
        </Element>
    </Document>
    @endverbatim

    Assuming you want the value of "attributeB" in the 2nd "Child" element, it's very
    easy to write a *lot* of code that looks like:

    @verbatim
    XMLElement* root = document.FirstChildElement( "Document" );
    if ( root )
    {
        XMLElement* element = root->FirstChildElement( "Element" );
        if ( element )
        {
            XMLElement* child = element->FirstChildElement( "Child" );
            if ( child )
            {
                XMLElement* child2 = child->NextSiblingElement( "Child" );
                if ( child2 )
                {
                    // Finally do something useful.
    @endverbatim

    And that doesn't even cover "else" cases. XMLHandle addresses the verbosity
    of such code. A XMLHandle checks for null pointers so it is perfectly safe
    and correct to use:

    @verbatim
    XMLHandle docHandle( &document );
    XMLElement* child2 = docHandle.FirstChildElement( "Document" ).FirstChildElement( "Element" ).FirstChildElement().NextSiblingElement();
    if ( child2 )
    {
        // do something useful
    @endverbatim

    Which is MUCH more concise and useful.

    It is also safe to copy handles - internally they are nothing more than node pointers.
    @verbatim
    XMLHandle handleCopy = handle;
    @endverbatim

    See also XMLConstHandle, which is the same as XMLHandle, but operates on const objects.
*/
class XMLHandle
{
public:
    /// Create a handle from any node (at any depth of the tree.) This can be a null pointer.
    explicit XMLHandle( XMLNode* node ) : _node( node ) {
    }
    /// Create a handle from a node.
    explicit XMLHandle( XMLNode& node ) : _node( &node ) {
    }
    /// Copy constructor
    XMLHandle( const XMLHandle& ref ) : _node( ref._node ) {
    }
    /// Assignment
    XMLHandle& operator=( const XMLHandle& ref )                            {
        _node = ref._node;
        return *this;
    }

    /// Get the first child of this handle.
    XMLHandle FirstChild()                                                     {
        return XMLHandle( _node ? _node->FirstChild() : 0 );
    }
    /// Get the first child element of this handle.
    XMLHandle FirstChildElement( const char* name = 0 )                        {
        return XMLHandle( _node ? _node->FirstChildElement( name ) : 0 );
    }
    /// Get the last child of this handle.
    XMLHandle LastChild()                                                    {
        return XMLHandle( _node ? _node->LastChild() : 0 );
    }
    /// Get the last child element of this handle.
    XMLHandle LastChildElement( const char* name = 0 )                        {
        return XMLHandle( _node ? _node->LastChildElement( name ) : 0 );
    }
    /// Get the previous sibling of this handle.
    XMLHandle PreviousSibling()                                                {
        return XMLHandle( _node ? _node->PreviousSibling() : 0 );
    }
    /// Get the previous sibling element of this handle.
    XMLHandle PreviousSiblingElement( const char* name = 0 )                {
        return XMLHandle( _node ? _node->PreviousSiblingElement( name ) : 0 );
    }
    /// Get the next sibling of this handle.
    XMLHandle NextSibling()                                                    {
        return XMLHandle( _node ? _node->NextSibling() : 0 );
    }
    /// Get the next sibling element of this handle.
    XMLHandle NextSiblingElement( const char* name = 0 )                    {
        return XMLHandle( _node ? _node->NextSiblingElement( name ) : 0 );
    }

    /// Safe cast to XMLNode. This can return null.
    XMLNode* ToNode()                            {
        return _node;
    }
    /// Safe cast to XMLElement. This can return null.
    XMLElement* ToElement()                     {
        return ( _node ? _node->ToElement() : 0 );
    }
    /// Safe cast to XMLText. This can return null.
    XMLText* ToText()                             {
        return ( _node ? _node->ToText() : 0 );
    }
    /// Safe cast to XMLUnknown. This can return null.
    XMLUnknown* ToUnknown()                     {
        return ( _node ? _node->ToUnknown() : 0 );
    }
    /// Safe cast to XMLDeclaration. This can return null.
    XMLDeclaration* ToDeclaration()             {
        return ( _node ? _node->ToDeclaration() : 0 );
    }

private:
    XMLNode* _node = nullptr;
};


/**
    A variant of the XMLHandle class for working with const XMLNodes and Documents. It is the
    same in all regards, except for the 'const' qualifiers. See XMLHandle for API.
*/
class XMLConstHandle
{
public:
    explicit XMLConstHandle( const XMLNode* node ) : _node( node ) {
    }
    explicit XMLConstHandle( const XMLNode& node ) : _node( &node ) {
    }

    XMLConstHandle( const XMLConstHandle& ) noexcept = default;

    XMLConstHandle& operator = ( const XMLConstHandle& ) noexcept = default;

    const XMLConstHandle FirstChild() const                                            {
        return XMLConstHandle( _node ? _node->FirstChild() : 0 );
    }
    const XMLConstHandle FirstChildElement( const char* name = 0 ) const                {
        return XMLConstHandle( _node ? _node->FirstChildElement( name ) : 0 );
    }
    const XMLConstHandle LastChild()    const                                        {
        return XMLConstHandle( _node ? _node->LastChild() : 0 );
    }
    const XMLConstHandle LastChildElement( const char* name = 0 ) const                {
        return XMLConstHandle( _node ? _node->LastChildElement( name ) : 0 );
    }
    const XMLConstHandle PreviousSibling() const                                    {
        return XMLConstHandle( _node ? _node->PreviousSibling() : 0 );
    }
    const XMLConstHandle PreviousSiblingElement( const char* name = 0 ) const        {
        return XMLConstHandle( _node ? _node->PreviousSiblingElement( name ) : 0 );
    }
    const XMLConstHandle NextSibling() const                                        {
        return XMLConstHandle( _node ? _node->NextSibling() : 0 );
    }
    const XMLConstHandle NextSiblingElement( const char* name = 0 ) const            {
        return XMLConstHandle( _node ? _node->NextSiblingElement( name ) : 0 );
    }


    const XMLNode* ToNode() const noexcept  {
        return _node;
    }
    const XMLElement* ToElement() const noexcept  {
        return ( _node ? _node->ToElement() : 0 );
    }
    const XMLText* ToText() const noexcept  {
        return ( _node ? _node->ToText() : 0 );
    }
    const XMLUnknown* ToUnknown() const noexcept  {
        return ( _node ? _node->ToUnknown() : 0 );
    }
    const XMLDeclaration* ToDeclaration() const noexcept  {
        return ( _node ? _node->ToDeclaration() : 0 );
    }

private:
    const XMLNode* _node = 0;
};


/**
    Printing functionality. The XMLPrinter gives you more
    options than the XMLDocument::Print() method.

    It can:
    -# Print to memory.
    -# Print to a file you provide.
    -# Print XML without a XMLDocument.

    Print to Memory

    @verbatim
    XMLPrinter printer;
    doc.Print( &printer );
    SomeFunction( printer.CStr() );
    @endverbatim

    Print to a File

    You provide the file pointer.
    @verbatim
    XMLPrinter printer( fp );
    doc.Print( &printer );
    @endverbatim

    Print without a XMLDocument

    When loading, an XML parser is very useful. However, sometimes
    when saving, it just gets in the way. The code is often set up
    for streaming, and constructing the DOM is just overhead.

    The Printer supports the streaming case. The following code
    prints out a trivially simple XML file without ever creating
    an XML document.

    @verbatim
    XMLPrinter printer( fp );
    printer.OpenElement( "foo" );
    printer.PushAttribute( "foo", "bar" );
    printer.CloseElement();
    @endverbatim
*/
class XMLPrinter : public XMLVisitor
{
public:
    /** Construct the printer. If the FILE* is specified,
        this will print to the FILE. Else it will print
        to memory, and the result is available in CStr().
        If 'compact' is set to true, then output is created
        with only required whitespace and newlines.
    */
    XMLPrinter( FILE* file=0, bool compact = false, int depth = 0 ) noexcept;

    virtual ~XMLPrinter()  {}

    /** If streaming, write the BOM and declaration. */
    void PushHeader( bool writeBOM, bool writeDeclaration ) noexcept;
    /** If streaming, start writing an element.
        The element must be closed with CloseElement()
    */
    void OpenElement( const char* name, bool compactMode=false ) noexcept;
    /// If streaming, add an attribute to an open element.
    void PushAttribute( const char* name, const char* value ) noexcept;
    void PushAttribute( const char* name, int value ) noexcept;
    void PushAttribute( const char* name, unsigned value ) noexcept;
    void PushAttribute( const char* name, int64_t value ) noexcept;
    void PushAttribute( const char* name, uint64_t value ) noexcept;
    void PushAttribute( const char* name, bool value ) noexcept;
    void PushAttribute( const char* name, double value ) noexcept;
    /// If streaming, close the Element.
    virtual void CloseElement( bool compactMode=false ) noexcept;

    /// Add a text node.
    void PushText( const char* text, bool cdata=false ) noexcept;
    /// Add a text node from an integer.
    void PushText( int value ) noexcept;
    /// Add a text node from an unsigned.
    void PushText( unsigned value ) noexcept;
    /// Add a text node from a signed 64bit integer.
    void PushText( int64_t value ) noexcept;
    /// Add a text node from an unsigned 64bit integer.
    void PushText( uint64_t value ) noexcept;
    /// Add a text node from a bool.
    void PushText( bool value ) noexcept;
    /// Add a text node from a float.
    void PushText( float value ) noexcept;
    /// Add a text node from a double.
    void PushText( double value ) noexcept;

    /// Add a comment
    void PushComment( const char* comment ) noexcept;

    void PushDeclaration( const char* value ) noexcept;
    void PushUnknown( const char* value ) noexcept;

    virtual bool VisitEnter( const XMLDocument& /*doc*/ ) noexcept;
    virtual bool VisitExit( const XMLDocument& /*doc*/ )  noexcept  {
        return true;
    }

    virtual bool VisitEnter( const XMLElement& element, const XMLAttribute* attribute ) noexcept;
    virtual bool VisitExit( const XMLElement& element ) noexcept;

    virtual bool Visit( const XMLText& text ) noexcept;
    virtual bool Visit( const XMLComment& comment ) noexcept;
    virtual bool Visit( const XMLDeclaration& declaration ) noexcept;
    virtual bool Visit( const XMLUnknown& unknown ) noexcept;

    /**
        If in print to memory mode, return a pointer to
        the XML file in memory.
    */
    const char* CStr() const noexcept {
        return _buffer.Mem();
    }

    /**
        If in print to memory mode, return the size
        of the XML file in memory. (Note the size returned
        includes the terminating null.)
    */
    int CStrSize() const noexcept {
        return _buffer.Size();
    }

    /**
        If in print to memory mode, reset the buffer to the
        beginning.
    */
    void ClearBuffer( bool resetToFirstElement = true ) noexcept {
        _buffer.Clear();
        _buffer.Push(0);
        _firstElement = resetToFirstElement;
    }

protected:
    virtual bool CompactMode( const XMLElement& ) noexcept  { return _compactMode; }

    /** Prints out the space before an element. You may override to change
        the space and tabs used. A PrintSpace() override should call Print().
    */
    virtual void PrintSpace( int depth ) noexcept;
    virtual void Print( const char* format, ... ) noexcept;
    virtual void Write( const char* data, size_t size ) noexcept;
    virtual void Putc( char ch ) noexcept;

    inline void Write(const char* data) { Write(data, strlen(data)); }

    void SealElementIfJustOpened() noexcept;

    bool _elementJustOpened;
    DynArray< const char*, 10 > _stack;

private:
    /**
       Prepares to write a new node. This includes sealing an element that was
       just opened, and writing any whitespace necessary if not in compact mode.
     */
    void PrepareForNewNode( bool compactMode ) noexcept;
    void PrintString( const char*, bool restrictedEntitySet ) noexcept;  // prints out, after detecting entities.

    bool _firstElement = 0;
    FILE* _fp = 0;
    int _depth = 0;
    int _textDepth = 0;
    bool _processEntities = 0;
    bool _compactMode = 0;

    static constexpr int   ENTITY_RANGE = 64;
    static constexpr int   BUF_SIZE = 200;

    bool _entityFlag[ENTITY_RANGE];
    bool _restrictedEntityFlag[ENTITY_RANGE];

    DynArray< char, 20 > _buffer;

    XMLPrinter( const XMLPrinter& ) = delete;
    XMLPrinter& operator=( const XMLPrinter& ) = delete;
};


} // NS tinxml2


#endif

