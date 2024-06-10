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

#include "file_readers/pln_tinyxml2.h"
#include "file_readers/pln_Fio.h"

#include <new>
#include <cstddef>
#include <cstdarg>
#include <unistd.h>

namespace tinxml2
{

static inline int TIXML_VSCPRINTF( const char* format, va_list va ) noexcept
{
    int len = vsnprintf( 0, 0, format, va );
    assert( len >= 0 );
    return len;
}

static constexpr char LINE_FEED              = static_cast<char>(0x0a);      // all line endings are normalized to LF
static constexpr char LF = LINE_FEED;
static constexpr char CARRIAGE_RETURN        = static_cast<char>(0x0d);      // CR gets filtered out
static constexpr char CR = CARRIAGE_RETURN;
static constexpr char SINGLE_QUOTE            = '\'';
static constexpr char DOUBLE_QUOTE            = '\"';

// Bunch of unicode info at:
//        http://www.unicode.org/faq/utf_bom.html
//    ef bb bf (Microsoft "lead bytes") - designates UTF-8

static constexpr uint8_t TIXML_UTF_LEAD_0 = 0xefU;
static constexpr uint8_t TIXML_UTF_LEAD_1 = 0xbbU;
static constexpr uint8_t TIXML_UTF_LEAD_2 = 0xbfU;

struct Entity {
    const char* pattern;
    int length;
    char value;
};

static constexpr int NUM_ENTITIES = 5;

static const Entity s_entities[NUM_ENTITIES] = {
    { "quot",  4,    DOUBLE_QUOTE },
    { "amp",   3,        '&'  },
    { "apos",  4,    SINGLE_QUOTE },
    { "lt",    2,        '<'     },
    { "gt",    2,        '>'     }
};


StrPair::~StrPair()
{
    Reset();
}

void StrPair::TransferTo( StrPair* other ) noexcept
{
    if ( this == other ) {
        return;
    }
    // This in effect implements the assignment operator by "moving"
    // ownership (as in auto_ptr).

    assert( other != 0 );
    assert( other->_flags == 0 );
    assert( other->_start == 0 );
    assert( other->_end == 0 );

    other->Reset();

    other->_flags = _flags;
    other->_start = _start;
    other->_end = _end;

    _flags = 0;
    _start = 0;
    _end = 0;
}

void StrPair::Reset() noexcept
{
    if ( _flags & NEEDS_DELETE ) {
        delete [] _start;
    }
    _flags = 0;
    _start = 0;
    _end = 0;
}

void StrPair::SetStr( const char* str, int flags ) noexcept
{
    assert( str );
    Reset();
    size_t len = strlen( str );
    assert( _start == 0 );
    _start = new char[ len+1 ];
    assert(_start);
    memcpy( _start, str, len+1 );
    _end = _start + len;
    _flags = flags | NEEDS_DELETE;
}

char* StrPair::ParseText( char* p, const char* endTag, int strFlags, int* curLineNumPtr ) noexcept
{
    assert( p );
    assert( endTag && *endTag );
    assert(curLineNumPtr);

    char* start = p;
    const char  endChar = *endTag;
    size_t length = strlen( endTag );

    // Inner loop of text parsing.
    while ( *p ) {
        if ( *p == endChar && strncmp( p, endTag, length ) == 0 ) {
            Set( start, p, strFlags );
            return p + length;
        } else if (*p == '\n') {
            ++(*curLineNumPtr);
        }
        ++p;
        assert( p );
    }
    return 0;
}

char* StrPair::ParseName( char* p ) noexcept
{
    if ( !p || !(*p) ) {
        return 0;
    }
    if ( !XMLUtil::IsNameStartChar( (uint8_t) *p ) ) {
        return 0;
    }

    char* const start = p;
    ++p;
    while ( *p && XMLUtil::IsNameChar( (uint8_t) *p ) ) {
        ++p;
    }

    Set( start, p, 0 );
    return p;
}

void StrPair::CollapseWhitespace() noexcept
{
    // Adjusting _start would cause undefined behavior on delete[]
    assert( ( _flags & NEEDS_DELETE ) == 0 );
    // Trim leading space.
    _start = XMLUtil::SkipWhiteSpace( _start, 0 );

    if ( *_start ) {
        const char* p = _start;    // the read pointer
        char* q = _start;    // the write pointer

        while( *p ) {
            if ( XMLUtil::IsWhiteSpace( *p )) {
                p = XMLUtil::SkipWhiteSpace( p, 0 );
                if ( *p == 0 ) {
                    break;    // don't write to q; this trims the trailing space.
                }
                *q = ' ';
                ++q;
            }
            *q = *p;
            ++q;
            ++p;
        }
        *q = 0;
    }
}

const char* StrPair::GetStr() noexcept
{
    assert( _start );
    assert( _end );
    if ( _flags & NEEDS_FLUSH ) {
        *_end = 0;
        _flags ^= NEEDS_FLUSH;

        if ( _flags ) {
            const char* p = _start;    // the read pointer
            char* q = _start;    // the write pointer

            while( p < _end ) {
                if ( (_flags & NEEDS_NEWLINE_NORMALIZATION) && *p == CR ) {
                    // CR-LF pair becomes LF
                    // CR alone becomes LF
                    // LF-CR becomes LF
                    if ( *(p+1) == LF ) {
                        p += 2;
                    }
                    else {
                        ++p;
                    }
                    *q = LF;
                    ++q;
                }
                else if ( (_flags & NEEDS_NEWLINE_NORMALIZATION) && *p == LF ) {
                    if ( *(p+1) == CR ) {
                        p += 2;
                    }
                    else {
                        ++p;
                    }
                    *q = LF;
                    ++q;
                }
                else if ( (_flags & NEEDS_ENTITY_PROCESSING) && *p == '&' ) {
                    // Entities handled by tinyXML2:
                    // - special s_entities in the entity table [in/out]
                    // - numeric character reference [in]
                    //   &#20013; or &#x4e2d;

                    if ( *(p+1) == '#' ) {
                        const int buflen = 10;
                        char buf[buflen] = { 0 };
                        int len = 0;
                        const char* adjusted = const_cast<char*>( XMLUtil::GetCharacterRef( p, buf, &len ) );
                        if ( adjusted == 0 ) {
                            *q = *p;
                            ++p;
                            ++q;
                        }
                        else {
                            assert( 0 <= len && len <= buflen );
                            assert( q + len <= adjusted );
                            p = adjusted;
                            memcpy( q, buf, len );
                            q += len;
                        }
                    }
                    else {
                        bool entityFound = false;
                        for( int i = 0; i < NUM_ENTITIES; ++i ) {
                            const Entity& entity = s_entities[i];
                            if ( strncmp( p + 1, entity.pattern, entity.length ) == 0
                                    && *( p + entity.length + 1 ) == ';' ) {
                                // Found an entity - convert.
                                *q = entity.value;
                                ++q;
                                p += entity.length + 2;
                                entityFound = true;
                                break;
                            }
                        }
                        if ( !entityFound ) {
                            // fixme: treat as error?
                            ++p;
                            ++q;
                        }
                    }
                }
                else {
                    *q = *p;
                    ++p;
                    ++q;
                }
            }
            *q = 0;
        }
        // The loop below has plenty going on, and this
        // is a less useful mode. Break it out.
        if ( _flags & NEEDS_WHITESPACE_COLLAPSING ) {
            CollapseWhitespace();
        }
        _flags = (_flags & NEEDS_DELETE);
    }
    assert( _start );
    return _start;
}



// --------- XMLUtil ----------- //

const char* XMLUtil::writeBoolTrue  = "true";
const char* XMLUtil::writeBoolFalse = "false";

void XMLUtil::SetBoolSerialization(const char* writeTrue, const char* writeFalse) noexcept
{
    static const char* defTrue  = "true";
    static const char* defFalse = "false";

    writeBoolTrue = (writeTrue) ? writeTrue : defTrue;
    writeBoolFalse = (writeFalse) ? writeFalse : defFalse;
}

const char* XMLUtil::ReadBOM( const char* p, bool* bom ) noexcept
{
    assert( p );
    assert( bom );
    *bom = false;
    const uint8_t* pu = reinterpret_cast<const uint8_t*>(p);
    // Check for BOM:
    if (    *(pu+0) == TIXML_UTF_LEAD_0
            && *(pu+1) == TIXML_UTF_LEAD_1
            && *(pu+2) == TIXML_UTF_LEAD_2 ) {
        *bom = true;
        p += 3;
    }
    assert( p );
    return p;
}

void XMLUtil::ConvertUTF32ToUTF8( uint64_t input, char* output, int* length ) noexcept
{
    constexpr uint64_t BYTE_MASK = 0xBF;
    constexpr uint64_t BYTE_MARK = 0x80;
    const uint64_t FIRST_BYTE_MARK[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

    if (input < 0x80) {
        *length = 1;
    }
    else if ( input < 0x800 ) {
        *length = 2;
    }
    else if ( input < 0x10000 ) {
        *length = 3;
    }
    else if ( input < 0x200000 ) {
        *length = 4;
    }
    else {
        *length = 0;    // This code won't convert this correctly anyway.
        return;
    }

    output += *length;

    // Scary scary fall throughs are annotated with carefully designed comments
    // to suppress compiler warnings such as -Wimplicit-fallthrough in gcc
    switch (*length) {
        case 4:
            --output;
            *output = static_cast<char>((input | BYTE_MARK) & BYTE_MASK);
            input >>= 6;
            //fall through
        case 3:
            --output;
            *output = static_cast<char>((input | BYTE_MARK) & BYTE_MASK);
            input >>= 6;
            //fall through
        case 2:
            --output;
            *output = static_cast<char>((input | BYTE_MARK) & BYTE_MASK);
            input >>= 6;
            //fall through
        case 1:
            --output;
            *output = static_cast<char>(input | FIRST_BYTE_MARK[*length]);
            break;
        default:
            assert( false );
    }
}

const char* XMLUtil::GetCharacterRef( const char* p, char* value, int* length ) noexcept
{
    // Presume an entity, and pull it out.
    *length = 0;

    if ( *(p+1) == '#' && *(p+2) ) {
        uint64_t ucs = 0;
        assert( sizeof( ucs ) >= 4 );
        ptrdiff_t delta = 0;
        uint mult = 1;
        static const char SEMICOLON = ';';

        if ( *(p+2) == 'x' ) {
            // Hexadecimal.
            const char* q = p+3;
            if ( !(*q) ) {
                return 0;
            }

            q = strchr( q, SEMICOLON );

            if ( !q ) {
                return 0;
            }
            assert( *q == SEMICOLON );

            delta = q-p;
            --q;

            while ( *q != 'x' ) {
                uint digit = 0;

                if ( *q >= '0' && *q <= '9' ) {
                    digit = *q - '0';
                }
                else if ( *q >= 'a' && *q <= 'f' ) {
                    digit = *q - 'a' + 10;
                }
                else if ( *q >= 'A' && *q <= 'F' ) {
                    digit = *q - 'A' + 10;
                }
                else {
                    return 0;
                }
                assert( digit < 16 );
                assert( digit == 0 || mult <= UINT_MAX / digit );
                const uint digitScaled = mult * digit;
                assert( ucs <= ULONG_MAX - digitScaled );
                ucs += digitScaled;
                assert( mult <= UINT_MAX / 16 );
                mult *= 16;
                --q;
            }
        }
        else {
            // Decimal.
            const char* q = p+2;
            if ( !(*q) ) {
                return 0;
            }

            q = strchr( q, SEMICOLON );

            if ( !q ) {
                return 0;
            }
            assert( *q == SEMICOLON );

            delta = q-p;
            --q;

            while ( *q != '#' ) {
                if ( *q >= '0' && *q <= '9' ) {
                    const uint digit = *q - '0';
                    assert( digit < 10 );
                    assert( digit == 0 || mult <= UINT_MAX / digit );
                    const uint digitScaled = mult * digit;
                    assert( ucs <= ULONG_MAX - digitScaled );
                    ucs += digitScaled;
                }
                else {
                    return 0;
                }
                assert( mult <= UINT_MAX / 10 );
                mult *= 10;
                --q;
            }
        }
        // convert the UCS to UTF-8
        ConvertUTF32ToUTF8( ucs, value, length );
        return p + delta + 1;
    }
    return p+1;
}

void XMLUtil::ToStr( int v, char* buffer, size_t bufferSize ) noexcept
{
    snprintf( buffer, bufferSize, "%d", v );
}

void XMLUtil::ToStr( uint v, char* buffer, size_t bufferSize ) noexcept
{
    snprintf( buffer, bufferSize, "%u", v );
}

void XMLUtil::ToStr( bool v, char* buffer, size_t bufferSize ) noexcept
{
    snprintf( buffer, bufferSize, "%s", v ? writeBoolTrue : writeBoolFalse);
}

/*
    ToStr() of a number is a very tricky topic.
    https://github.com/leethomason/tinyxml2/issues/106
*/
void XMLUtil::ToStr( float v, char* buffer, size_t bufferSize ) noexcept
{
    snprintf( buffer, bufferSize, "%.8g", v );
}

void XMLUtil::ToStr( double v, char* buffer, size_t bufferSize ) noexcept
{
    snprintf( buffer, bufferSize, "%.17g", v );
}

void XMLUtil::ToStr( int64_t v, char* buffer, size_t bufferSize ) noexcept
{
    snprintf(buffer, bufferSize, "%ld", v);
}

void XMLUtil::ToStr( uint64_t v, char* buffer, size_t bufferSize ) noexcept
{
    snprintf(buffer, bufferSize, "%zu", v);
}

bool XMLUtil::ToInt(const char* str, int* value) noexcept
{
    if (IsPrefixHex(str)) {
        uint v = 0;
        if (sscanf(str, "%x", &v) == 1) {
            *value = static_cast<int>(v);
            return true;
        }
    }
    else {
        if (fio::is_integer(str)) {
            *value = ::atoi(str);
            return true;
        }
    }
    return false;
}

bool XMLUtil::ToUnsigned(const char* str, uint* value) noexcept
{
    if (IsPrefixHex(str)) {
        uint v = 0;
        if (sscanf(str, "%x", &v) == 1) {
            *value = v;
            return true;
        }
    }
    else {
        if (fio::is_uint(str)) {
            int64_t v = ::atol(str);
            *value = (v >= int64_t(UINT_MAX) ? UINT_MAX : v);
            return true;
        }
    }
    return false;
}

bool XMLUtil::ToBool( const char* str, bool* value ) noexcept
{
    int ival = 0;
    if ( ToInt( str, &ival )) {
        *value = (ival==0) ? false : true;
        return true;
    }
    static const char* TRUE_VALS[] = { "true", "True", "TRUE", 0 };
    static const char* FALSE_VALS[] = { "false", "False", "FALSE", 0 };

    for (int i = 0; TRUE_VALS[i]; ++i) {
        if (StringEqual(str, TRUE_VALS[i])) {
            *value = true;
            return true;
        }
    }
    for (int i = 0; FALSE_VALS[i]; ++i) {
        if (StringEqual(str, FALSE_VALS[i])) {
            *value = false;
            return true;
        }
    }
    return false;
}

bool XMLUtil::ToFloat( const char* str, float* value ) noexcept
{
    if ( sscanf( str, "%f", value ) == 1 ) {
        return true;
    }
    return false;
}

bool XMLUtil::ToDouble( const char* str, double* value ) noexcept
{
    if ( sscanf( str, "%lf", value ) == 1 ) {
        return true;
    }
    return false;
}

bool XMLUtil::ToInt64(const char* str, int64_t* value) noexcept
{
    if (IsPrefixHex(str)) {
        uint64_t v = 0;
        if (sscanf(str, "%lx", &v) == 1) {
            *value = v;
            return true;
        }
    }
    else {
        if (fio::is_integer(str)) {
            *value = ::atol(str);
            return true;
        }
    }

    return false;
}

bool XMLUtil::ToUnsigned64(const char* str, uint64_t* value) noexcept
{
    uint64_t v = 0;
    if (sscanf(str, IsPrefixHex(str) ? "%lx" : "%lu", &v) == 1) {
        *value = (uint64_t)v;
        return true;
    }
    return false;
}

char* XMLDocument::Identify( char* p, XMLNode** node ) noexcept
{
    assert( node );
    assert( p );
    char* start = p;
    int const startLine = _parseCurLineNum;
    p = XMLUtil::SkipWhiteSpace( p, &_parseCurLineNum );
    if( !*p ) {
        *node = 0;
        assert( p );
        return p;
    }

    // These strings define the matching patterns:
    static const char* xmlHeader        = { "<?" };
    static const char* commentHeader    = { "<!--" };
    static const char* cdataHeader      = { "<![CDATA[" };
    static const char* dtdHeader        = { "<!" };
    static const char* elementHeader    = { "<" };    // and a header for everything else; check last.

    constexpr int xmlHeaderLen        = 2;
    constexpr int commentHeaderLen    = 4;
    constexpr int cdataHeaderLen      = 9;
    constexpr int dtdHeaderLen        = 2;
    constexpr int elementHeaderLen    = 1;

    assert( sizeof( XMLComment ) == sizeof( XMLUnknown ) );        // use same memory pool
    assert( sizeof( XMLComment ) == sizeof( XMLDeclaration ) );    // use same memory pool
    XMLNode* returnNode = 0;
    if ( XMLUtil::StringEqual( p, xmlHeader, xmlHeaderLen ) ) {
        returnNode = CreateUnlinkedNode<XMLDeclaration>( _commentPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += xmlHeaderLen;
    }
    else if ( XMLUtil::StringEqual( p, commentHeader, commentHeaderLen ) ) {
        returnNode = CreateUnlinkedNode<XMLComment>( _commentPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += commentHeaderLen;
    }
    else if ( XMLUtil::StringEqual( p, cdataHeader, cdataHeaderLen ) ) {
        XMLText* text = CreateUnlinkedNode<XMLText>( _textPool );
        returnNode = text;
        returnNode->_parseLineNum = _parseCurLineNum;
        p += cdataHeaderLen;
        text->SetCData( true );
    }
    else if ( XMLUtil::StringEqual( p, dtdHeader, dtdHeaderLen ) ) {
        returnNode = CreateUnlinkedNode<XMLUnknown>( _commentPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += dtdHeaderLen;
    }
    else if ( XMLUtil::StringEqual( p, elementHeader, elementHeaderLen ) ) {
        returnNode =  CreateUnlinkedNode<XMLElement>( _elementPool );
        returnNode->_parseLineNum = _parseCurLineNum;
        p += elementHeaderLen;
    }
    else {
        returnNode = CreateUnlinkedNode<XMLText>( _textPool );
        returnNode->_parseLineNum = _parseCurLineNum; // Report line of first non-whitespace character
        p = start;    // Back it up, all the text counts.
        _parseCurLineNum = startLine;
    }

    assert( returnNode );
    assert( p );
    *node = returnNode;
    return p;
}

bool XMLDocument::Accept( XMLVisitor* visitor ) const noexcept
{
    assert( visitor );
    if ( visitor->VisitEnter( *this ) ) {
        for ( const XMLNode* node=FirstChild(); node; node=node->NextSibling() ) {
            if ( !node->Accept( visitor ) ) {
                break;
            }
        }
    }
    return visitor->VisitExit( *this );
}


// --------- XMLNode ----------- //

XMLNode::XMLNode( XMLDocument* doc ) noexcept
 :  _document( doc ),
    _parent( 0 ),
    _value(),
    _parseLineNum( 0 ),
    _firstChild( 0 ), _lastChild( 0 ),
    _prev( 0 ), _next( 0 ),
    _userData( 0 ),
    _memPool( 0 )
{
}

XMLNode::~XMLNode()
{
    DeleteChildren();
    if ( _parent ) {
        _parent->Unlink( this );
    }
}

const char* XMLNode::Value() const noexcept
{
    // Edge case: XMLDocuments don't have a Value. Return null.
    if ( this->ToDocument() )
        return 0;
    return _value.GetStr();
}

void XMLNode::SetValue( const char* str, bool staticMem ) noexcept
{
    if ( staticMem ) {
        _value.SetInternedStr( str );
    }
    else {
        _value.SetStr( str );
    }
}

XMLNode* XMLNode::DeepClone(XMLDocument* target) const noexcept
{
    XMLNode* clone = this->ShallowClone(target);
    if (!clone) return 0;

    for (const XMLNode* child = this->FirstChild(); child; child = child->NextSibling()) {
        XMLNode* childClone = child->DeepClone(target);
        assert(childClone);
        clone->InsertEndChild(childClone);
    }
    return clone;
}

void XMLNode::DeleteChildren() noexcept
{
    while( _firstChild ) {
        assert( _lastChild );
        DeleteChild( _firstChild );
    }
    _firstChild = _lastChild = 0;
}

void XMLNode::Unlink( XMLNode* child ) noexcept
{
    assert( child );
    assert( child->_document == _document );
    assert( child->_parent == this );
    if ( child == _firstChild ) {
        _firstChild = _firstChild->_next;
    }
    if ( child == _lastChild ) {
        _lastChild = _lastChild->_prev;
    }

    if ( child->_prev ) {
        child->_prev->_next = child->_next;
    }
    if ( child->_next ) {
        child->_next->_prev = child->_prev;
    }
    child->_next = 0;
    child->_prev = 0;
    child->_parent = 0;
}

void XMLNode::DeleteChild( XMLNode* node ) noexcept
{
    assert( node );
    assert( node->_document == _document );
    assert( node->_parent == this );
    Unlink( node );
    assert(node->_prev == 0);
    assert(node->_next == 0);
    assert(node->_parent == 0);
    DeleteNode( node );
}

XMLNode* XMLNode::InsertEndChild( XMLNode* addThis ) noexcept
{
    assert( addThis );
    if ( addThis->_document != _document ) {
        assert( false );
        return 0;
    }
    InsertChildPreamble( addThis );

    if ( _lastChild ) {
        assert( _firstChild );
        assert( _lastChild->_next == 0 );
        _lastChild->_next = addThis;
        addThis->_prev = _lastChild;
        _lastChild = addThis;

        addThis->_next = 0;
    }
    else {
        assert( _firstChild == 0 );
        _firstChild = _lastChild = addThis;

        addThis->_prev = 0;
        addThis->_next = 0;
    }
    addThis->_parent = this;
    return addThis;
}

XMLNode* XMLNode::InsertFirstChild( XMLNode* addThis ) noexcept
{
    assert( addThis );
    if ( addThis->_document != _document ) {
        assert( false );
        return 0;
    }
    InsertChildPreamble( addThis );

    if ( _firstChild ) {
        assert( _lastChild );
        assert( _firstChild->_prev == 0 );

        _firstChild->_prev = addThis;
        addThis->_next = _firstChild;
        _firstChild = addThis;

        addThis->_prev = 0;
    }
    else {
        assert( _lastChild == 0 );
        _firstChild = _lastChild = addThis;

        addThis->_prev = 0;
        addThis->_next = 0;
    }
    addThis->_parent = this;
    return addThis;
}

XMLNode* XMLNode::InsertAfterChild( XMLNode* afterThis, XMLNode* addThis ) noexcept
{
    assert( addThis );
    if ( addThis->_document != _document ) {
        assert( false );
        return 0;
    }

    assert( afterThis );

    if ( afterThis->_parent != this ) {
        assert( false );
        return 0;
    }
    if ( afterThis == addThis ) {
        // Current state: BeforeThis -> AddThis -> OneAfterAddThis
        // Now AddThis must disappear from it's location and then
        // reappear between BeforeThis and OneAfterAddThis.
        // So just leave it where it is.
        return addThis;
    }

    if ( afterThis->_next == 0 ) {
        // The last node or the only node.
        return InsertEndChild( addThis );
    }
    InsertChildPreamble( addThis );
    addThis->_prev = afterThis;
    addThis->_next = afterThis->_next;
    afterThis->_next->_prev = addThis;
    afterThis->_next = addThis;
    addThis->_parent = this;
    return addThis;
}

const XMLElement* XMLNode::FirstChildElement( const char* name ) const noexcept
{
    for( const XMLNode* node = _firstChild; node; node = node->_next ) {
        const XMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}

const XMLElement* XMLNode::LastChildElement( const char* name ) const noexcept
{
    for( const XMLNode* node = _lastChild; node; node = node->_prev ) {
        const XMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}

const XMLElement* XMLNode::NextSiblingElement( const char* name ) const noexcept
{
    for( const XMLNode* node = _next; node; node = node->_next ) {
        const XMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}

const XMLElement* XMLNode::PreviousSiblingElement( const char* name ) const noexcept
{
    for( const XMLNode* node = _prev; node; node = node->_prev ) {
        const XMLElement* element = node->ToElementWithName( name );
        if ( element ) {
            return element;
        }
    }
    return 0;
}

char* XMLNode::ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr ) noexcept
{
    // This is a recursive method, but thinking about it "at the current level"
    // it is a pretty simple flat list:
    //        <foo/>
    //        <!-- comment -->
    //
    // With a special case:
    //        <foo>
    //        </foo>
    //        <!-- comment -->
    //
    // Where the closing element (/foo) *must* be the next thing after the opening
    // element, and the names must match. BUT the tricky bit is that the closing
    // element will be read by the child.
    //
    // 'endTag' is the end tag for this node, it is returned by a call to a child.
    // 'parentEnd' is the end tag for the parent, which is filled in and returned.

    XMLDocument::DepthTracker tracker(_document);
    if (_document->Error())
        return 0;

    while( p && *p ) {
        XMLNode* node = 0;

        p = _document->Identify( p, &node );
        assert( p );
        if ( node == 0 ) {
            break;
        }

       const int initialLineNum = node->_parseLineNum;

        StrPair endTag;
        p = node->ParseDeep( p, &endTag, curLineNumPtr );
        if ( !p ) {
            _document->DeleteNode( node );
            if ( !_document->Error() ) {
                _document->SetError( XML_ERROR_PARSING, initialLineNum, 0);
            }
            break;
        }

        const XMLDeclaration* const decl = node->ToDeclaration();
        if ( decl ) {
            // Declarations are only allowed at document level
            //
            // Multiple declarations are allowed but all declarations
            // must occur before anything else.
            //
            // Optimized due to a security test case. If the first node is
            // a declaration, and the last node is a declaration, then only
            // declarations have so far been added.
            bool wellLocated = false;

            if (ToDocument()) {
                if (FirstChild()) {
                    wellLocated =
                        FirstChild() &&
                        FirstChild()->ToDeclaration() &&
                        LastChild() &&
                        LastChild()->ToDeclaration();
                }
                else {
                    wellLocated = true;
                }
            }
            if ( !wellLocated ) {
                _document->SetError( XML_ERROR_PARSING_DECLARATION, initialLineNum, "XMLDeclaration value=%s", decl->Value());
                _document->DeleteNode( node );
                break;
            }
        }

        XMLElement* ele = node->ToElement();
        if ( ele ) {
            // We read the end tag. Return it to the parent.
            if ( ele->ClosingType() == XMLElement::CLOSING ) {
                if ( parentEndTag ) {
                    ele->_value.TransferTo( parentEndTag );
                }
                node->_memPool->SetTracked();   // created and then immediately deleted.
                DeleteNode( node );
                return p;
            }

            // Handle an end tag returned to this level.
            // And handle a bunch of annoying errors.
            bool mismatch = false;
            if ( endTag.Empty() ) {
                if ( ele->ClosingType() == XMLElement::OPEN ) {
                    mismatch = true;
                }
            }
            else {
                if ( ele->ClosingType() != XMLElement::OPEN ) {
                    mismatch = true;
                }
                else if ( !XMLUtil::StringEqual( endTag.GetStr(), ele->Name() ) ) {
                    mismatch = true;
                }
            }
            if ( mismatch ) {
                _document->SetError( XML_ERROR_MISMATCHED_ELEMENT, initialLineNum, "XMLElement name=%s", ele->Name());
                _document->DeleteNode( node );
                break;
            }
        }
        InsertEndChild( node );
    }
    return 0;
}

/*static*/ void XMLNode::DeleteNode( XMLNode* node ) noexcept
{
    if ( node == 0 ) {
        return;
    }
    assert(node->_document);
    if (!node->ToDocument()) {
        node->_document->MarkInUse(node);
    }

    MemPool* pool = node->_memPool;
    node->~XMLNode();
    pool->Free( node );
}

void XMLNode::InsertChildPreamble( XMLNode* insertThis ) const noexcept
{
    assert( insertThis );
    assert( insertThis->_document == _document );

    if (insertThis->_parent) {
        insertThis->_parent->Unlink( insertThis );
    }
    else {
        insertThis->_document->MarkInUse(insertThis);
        insertThis->_memPool->SetTracked();
    }
}

const XMLElement* XMLNode::ToElementWithName( const char* name ) const noexcept
{
    const XMLElement* element = this->ToElement();
    if ( element == 0 ) {
        return 0;
    }
    if ( name == 0 ) {
        return element;
    }
    if ( XMLUtil::StringEqual( element->Name(), name ) ) {
       return element;
    }
    return 0;
}

// --------- XMLText ---------- //
char* XMLText::ParseDeep( char* p, StrPair*, int* curLineNumPtr ) noexcept
{
    if ( this->CData() ) {
        p = _value.ParseText( p, "]]>", StrPair::NEEDS_NEWLINE_NORMALIZATION, curLineNumPtr );
        if ( !p ) {
            _document->SetError( XML_ERROR_PARSING_CDATA, _parseLineNum, 0 );
        }
        return p;
    }
    else {
        int flags = _document->ProcessEntities() ? StrPair::TEXT_ELEMENT : StrPair::TEXT_ELEMENT_LEAVE_ENTITIES;
        if ( _document->WhitespaceMode() == COLLAPSE_WHITESPACE ) {
            flags |= StrPair::NEEDS_WHITESPACE_COLLAPSING;
        }

        p = _value.ParseText( p, "<", flags, curLineNumPtr );
        if ( p && *p ) {
            return p-1;
        }
        if ( !p ) {
            _document->SetError( XML_ERROR_PARSING_TEXT, _parseLineNum, 0 );
        }
    }
    return 0;
}

XMLNode* XMLText::ShallowClone( XMLDocument* doc ) const noexcept
{
    if ( !doc ) {
        doc = _document;
    }
    XMLText* text = doc->NewText( Value() );    // fixme: this will always allocate memory. Intern?
    text->SetCData( this->CData() );
    return text;
}

bool XMLText::ShallowEqual( const XMLNode* compare ) const noexcept
{
    assert( compare );
    const XMLText* text = compare->ToText();
    return ( text && XMLUtil::StringEqual( text->Value(), Value() ) );
}

bool XMLText::Accept( XMLVisitor* visitor ) const noexcept
{
    assert( visitor );
    return visitor->Visit( *this );
}


// --------- XMLComment ---------- //

XMLComment::XMLComment( XMLDocument* doc ) noexcept
  : XMLNode( doc )
{
}

XMLComment::~XMLComment()
{
}

char* XMLComment::ParseDeep( char* p, StrPair*, int* curLineNumPtr ) noexcept
{
    // Comment parses as text.
    p = _value.ParseText( p, "-->", StrPair::COMMENT, curLineNumPtr );
    if ( p == 0 ) {
        _document->SetError( XML_ERROR_PARSING_COMMENT, _parseLineNum, 0 );
    }
    return p;
}

XMLNode* XMLComment::ShallowClone( XMLDocument* doc ) const noexcept
{
    if ( !doc ) {
        doc = _document;
    }
    XMLComment* comment = doc->NewComment( Value() );    // fixme: this will always allocate memory. Intern?
    return comment;
}

bool XMLComment::ShallowEqual( const XMLNode* compare ) const noexcept
{
    assert( compare );
    const XMLComment* comment = compare->ToComment();
    return ( comment && XMLUtil::StringEqual( comment->Value(), Value() ));
}

bool XMLComment::Accept( XMLVisitor* visitor ) const noexcept
{
    assert( visitor );
    return visitor->Visit( *this );
}


// --------- XMLDeclaration ---------- //

XMLDeclaration::XMLDeclaration( XMLDocument* doc ) noexcept
  : XMLNode( doc )
{
}

XMLDeclaration::~XMLDeclaration()
{
    //printf( "~XMLDeclaration\n" );
}

char* XMLDeclaration::ParseDeep( char* p, StrPair*, int* curLineNumPtr ) noexcept
{
    // Declaration parses as text.
    p = _value.ParseText( p, "?>", StrPair::NEEDS_NEWLINE_NORMALIZATION, curLineNumPtr );
    if ( p == 0 ) {
        _document->SetError( XML_ERROR_PARSING_DECLARATION, _parseLineNum, 0 );
    }
    return p;
}

XMLNode* XMLDeclaration::ShallowClone( XMLDocument* doc ) const noexcept
{
    if ( !doc ) {
        doc = _document;
    }
    XMLDeclaration* dec = doc->NewDeclaration( Value() );    // fixme: this will always allocate memory. Intern?
    return dec;
}

bool XMLDeclaration::ShallowEqual( const XMLNode* compare ) const noexcept
{
    assert( compare );
    const XMLDeclaration* declaration = compare->ToDeclaration();
    return ( declaration && XMLUtil::StringEqual( declaration->Value(), Value() ));
}

bool XMLDeclaration::Accept( XMLVisitor* visitor ) const noexcept
{
    assert( visitor );
    return visitor->Visit( *this );
}


// --------- XMLUnknown ---------- //

XMLUnknown::XMLUnknown( XMLDocument* doc ) noexcept
  : XMLNode( doc )
{
}

char* XMLUnknown::ParseDeep( char* p, StrPair*, int* curLineNumPtr ) noexcept
{
    // Unknown parses as text.
    p = _value.ParseText( p, ">", StrPair::NEEDS_NEWLINE_NORMALIZATION, curLineNumPtr );
    if ( !p ) {
        _document->SetError( XML_ERROR_PARSING_UNKNOWN, _parseLineNum, 0 );
    }
    return p;
}

XMLNode* XMLUnknown::ShallowClone( XMLDocument* doc ) const noexcept
{
    if ( !doc ) {
        doc = _document;
    }
    XMLUnknown* text = doc->NewUnknown( Value() );    // fixme: this will always allocate memory. Intern?
    return text;
}

bool XMLUnknown::ShallowEqual( const XMLNode* compare ) const noexcept
{
    assert( compare );
    const XMLUnknown* unknown = compare->ToUnknown();
    return ( unknown && XMLUtil::StringEqual( unknown->Value(), Value() ));
}

bool XMLUnknown::Accept( XMLVisitor* visitor ) const noexcept
{
    assert( visitor );
    return visitor->Visit( *this );
}


// --------- XMLAttribute ---------- //

const char* XMLAttribute::Name() const noexcept
{
    return _name.GetStr();
}

const char* XMLAttribute::Value() const noexcept
{
    return _value.GetStr();
}

char* XMLAttribute::ParseDeep( char* p, bool processEntities, int* curLineNumPtr ) noexcept
{
    // Parse using the name rules: bug fix, was using ParseText before
    p = _name.ParseName( p );
    if ( !p || !*p ) {
        return 0;
    }

    // Skip white space before =
    p = XMLUtil::SkipWhiteSpace( p, curLineNumPtr );
    if ( *p != '=' ) {
        return 0;
    }

    ++p;    // move up to opening quote
    p = XMLUtil::SkipWhiteSpace( p, curLineNumPtr );
    if ( *p != '\"' && *p != '\'' ) {
        return 0;
    }

    const char endTag[2] = { *p, 0 };
    ++p;    // move past opening quote

    p = _value.ParseText( p, endTag, processEntities ? StrPair::ATTRIBUTE_VALUE : StrPair::ATTRIBUTE_VALUE_LEAVE_ENTITIES, curLineNumPtr );
    return p;
}

void XMLAttribute::SetName( const char* n ) noexcept
{
    _name.SetStr( n );
}

XMLError XMLAttribute::QueryIntValue( int* value ) const noexcept
{
    if ( XMLUtil::ToInt( Value(), value )) {
        return XML_SUCCESS;
    }
    return XML_WRONG_ATTRIBUTE_TYPE;
}

XMLError XMLAttribute::QueryUnsignedValue( uint* value ) const noexcept
{
    if ( XMLUtil::ToUnsigned( Value(), value )) {
        return XML_SUCCESS;
    }
    return XML_WRONG_ATTRIBUTE_TYPE;
}

XMLError XMLAttribute::QueryInt64Value(int64_t* value) const noexcept
{
    if (XMLUtil::ToInt64(Value(), value)) {
        return XML_SUCCESS;
    }
    return XML_WRONG_ATTRIBUTE_TYPE;
}

XMLError XMLAttribute::QueryUnsigned64Value(uint64_t* value) const noexcept
{
    if(XMLUtil::ToUnsigned64(Value(), value)) {
        return XML_SUCCESS;
    }
    return XML_WRONG_ATTRIBUTE_TYPE;
}

XMLError XMLAttribute::QueryBoolValue( bool* value ) const noexcept
{
    if ( XMLUtil::ToBool( Value(), value )) {
        return XML_SUCCESS;
    }
    return XML_WRONG_ATTRIBUTE_TYPE;
}

XMLError XMLAttribute::QueryFloatValue( float* value ) const noexcept
{
    if ( XMLUtil::ToFloat( Value(), value )) {
        return XML_SUCCESS;
    }
    return XML_WRONG_ATTRIBUTE_TYPE;
}

XMLError XMLAttribute::QueryDoubleValue( double* value ) const noexcept
{
    if ( XMLUtil::ToDouble( Value(), value )) {
        return XML_SUCCESS;
    }
    return XML_WRONG_ATTRIBUTE_TYPE;
}

void XMLAttribute::SetAttribute( const char* v ) noexcept
{
    _value.SetStr( v );
}

void XMLAttribute::SetAttribute( int v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}

void XMLAttribute::SetAttribute( uint v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}

void XMLAttribute::SetAttribute(int64_t v) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr(v, buf, BUF_SIZE);
    _value.SetStr(buf);
}

void XMLAttribute::SetAttribute(uint64_t v) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr(v, buf, BUF_SIZE);
    _value.SetStr(buf);
}

void XMLAttribute::SetAttribute( bool v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}

void XMLAttribute::SetAttribute( double v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}

void XMLAttribute::SetAttribute( float v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    _value.SetStr( buf );
}


// --------- XMLElement ---------- //

XMLElement::XMLElement( XMLDocument* doc ) noexcept
 : XMLNode( doc ),
    _closingType( OPEN ),
    _rootAttribute( 0 )
{
}

XMLElement::~XMLElement()
{
    while( _rootAttribute ) {
        XMLAttribute* next = _rootAttribute->_next;
        DeleteAttribute( _rootAttribute );
        _rootAttribute = next;
    }
}

const XMLAttribute* XMLElement::FindAttribute( const char* name ) const noexcept
{
    for( XMLAttribute* a = _rootAttribute; a; a = a->_next ) {
        if ( XMLUtil::StringEqual( a->Name(), name ) ) {
            return a;
        }
    }
    return 0;
}

const char* XMLElement::Attribute( const char* name, const char* value ) const noexcept
{
    const XMLAttribute* a = FindAttribute( name );
    if ( !a ) {
        return 0;
    }
    if ( !value || XMLUtil::StringEqual( a->Value(), value )) {
        return a->Value();
    }
    return 0;
}

int XMLElement::IntAttribute(const char* name, int defaultValue) const noexcept
{
    int i = defaultValue;
    QueryIntAttribute(name, &i);
    return i;
}

uint XMLElement::UnsignedAttribute(const char* name, uint defaultValue) const noexcept
{
    uint i = defaultValue;
    QueryUnsignedAttribute(name, &i);
    return i;
}

int64_t XMLElement::Int64Attribute(const char* name, int64_t defaultValue) const noexcept
{
    int64_t i = defaultValue;
    QueryInt64Attribute(name, &i);
    return i;
}

uint64_t XMLElement::Unsigned64Attribute(const char* name, uint64_t defaultValue) const noexcept
{
    uint64_t i = defaultValue;
    QueryUnsigned64Attribute(name, &i);
    return i;
}

bool XMLElement::BoolAttribute(const char* name, bool defaultValue) const noexcept
{
    bool b = defaultValue;
    QueryBoolAttribute(name, &b);
    return b;
}

double XMLElement::DoubleAttribute(const char* name, double defaultValue) const noexcept
{
    double d = defaultValue;
    QueryDoubleAttribute(name, &d);
    return d;
}

float XMLElement::FloatAttribute(const char* name, float defaultValue) const noexcept
{
    float f = defaultValue;
    QueryFloatAttribute(name, &f);
    return f;
}

const char* XMLElement::GetText() const noexcept
{
    /* skip comment node */
    const XMLNode* node = FirstChild();
    while (node) {
        if (node->ToComment()) {
            node = node->NextSibling();
            continue;
        }
        break;
    }

    if ( node && node->ToText() ) {
        return node->Value();
    }
    return 0;
}

void  XMLElement::SetText( const char* inText ) noexcept
{
    if ( FirstChild() && FirstChild()->ToText() )
        FirstChild()->SetValue( inText );
    else {
        XMLText*    theText = GetDocument()->NewText( inText );
        InsertFirstChild( theText );
    }
}

void XMLElement::SetText( int v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}

void XMLElement::SetText( uint v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}

void XMLElement::SetText(int64_t v) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr(v, buf, BUF_SIZE);
    SetText(buf);
}

void XMLElement::SetText(uint64_t v) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr(v, buf, BUF_SIZE);
    SetText(buf);
}

void XMLElement::SetText( bool v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}

void XMLElement::SetText( float v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}

void XMLElement::SetText( double v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    SetText( buf );
}

XMLError XMLElement::QueryIntText( int* ival ) const noexcept
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( XMLUtil::ToInt( t, ival ) ) {
            return XML_SUCCESS;
        }
        return XML_CAN_NOT_CONVERT_TEXT;
    }
    return XML_NO_TEXT_NODE;
}

XMLError XMLElement::QueryUnsignedText( uint* uval ) const noexcept
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( XMLUtil::ToUnsigned( t, uval ) ) {
            return XML_SUCCESS;
        }
        return XML_CAN_NOT_CONVERT_TEXT;
    }
    return XML_NO_TEXT_NODE;
}

XMLError XMLElement::QueryInt64Text(int64_t* ival) const noexcept
{
    if (FirstChild() && FirstChild()->ToText()) {
        const char* t = FirstChild()->Value();
        if (XMLUtil::ToInt64(t, ival)) {
            return XML_SUCCESS;
        }
        return XML_CAN_NOT_CONVERT_TEXT;
    }
    return XML_NO_TEXT_NODE;
}

XMLError XMLElement::QueryUnsigned64Text(uint64_t* uval) const noexcept
{
    if(FirstChild() && FirstChild()->ToText()) {
        const char* t = FirstChild()->Value();
        if(XMLUtil::ToUnsigned64(t, uval)) {
            return XML_SUCCESS;
        }
        return XML_CAN_NOT_CONVERT_TEXT;
    }
    return XML_NO_TEXT_NODE;
}

XMLError XMLElement::QueryBoolText( bool* bval ) const noexcept
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( XMLUtil::ToBool( t, bval ) ) {
            return XML_SUCCESS;
        }
        return XML_CAN_NOT_CONVERT_TEXT;
    }
    return XML_NO_TEXT_NODE;
}

XMLError XMLElement::QueryDoubleText( double* dval ) const noexcept
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( XMLUtil::ToDouble( t, dval ) ) {
            return XML_SUCCESS;
        }
        return XML_CAN_NOT_CONVERT_TEXT;
    }
    return XML_NO_TEXT_NODE;
}

XMLError XMLElement::QueryFloatText( float* fval ) const noexcept
{
    if ( FirstChild() && FirstChild()->ToText() ) {
        const char* t = FirstChild()->Value();
        if ( XMLUtil::ToFloat( t, fval ) ) {
            return XML_SUCCESS;
        }
        return XML_CAN_NOT_CONVERT_TEXT;
    }
    return XML_NO_TEXT_NODE;
}

int XMLElement::IntText(int defaultValue) const noexcept
{
    int i = defaultValue;
    QueryIntText(&i);
    return i;
}

uint XMLElement::UnsignedText(uint defaultValue) const noexcept
{
    uint i = defaultValue;
    QueryUnsignedText(&i);
    return i;
}

int64_t XMLElement::Int64Text(int64_t defaultValue) const noexcept
{
    int64_t i = defaultValue;
    QueryInt64Text(&i);
    return i;
}

uint64_t XMLElement::Unsigned64Text(uint64_t defaultValue) const noexcept
{
    uint64_t i = defaultValue;
    QueryUnsigned64Text(&i);
    return i;
}

bool XMLElement::BoolText(bool defaultValue) const noexcept
{
    bool b = defaultValue;
    QueryBoolText(&b);
    return b;
}

double XMLElement::DoubleText(double defaultValue) const noexcept
{
    double d = defaultValue;
    QueryDoubleText(&d);
    return d;
}

float XMLElement::FloatText(float defaultValue) const noexcept
{
    float f = defaultValue;
    QueryFloatText(&f);
    return f;
}

XMLAttribute* XMLElement::FindOrCreateAttribute( const char* name ) noexcept
{
    XMLAttribute* last = 0;
    XMLAttribute* attrib = 0;
    for( attrib = _rootAttribute;
            attrib;
            last = attrib, attrib = attrib->_next ) {
        if ( XMLUtil::StringEqual( attrib->Name(), name ) ) {
            break;
        }
    }
    if ( !attrib ) {
        attrib = CreateAttribute();
        assert( attrib );
        if ( last ) {
            assert( last->_next == 0 );
            last->_next = attrib;
        }
        else {
            assert( _rootAttribute == 0 );
            _rootAttribute = attrib;
        }
        attrib->SetName( name );
    }
    return attrib;
}

void XMLElement::DeleteAttribute( const char* name ) noexcept
{
    XMLAttribute* prev = 0;
    for( XMLAttribute* a=_rootAttribute; a; a=a->_next ) {
        if ( XMLUtil::StringEqual( name, a->Name() ) ) {
            if ( prev ) {
                prev->_next = a->_next;
            }
            else {
                _rootAttribute = a->_next;
            }
            DeleteAttribute( a );
            break;
        }
        prev = a;
    }
}

char* XMLElement::ParseAttributes( char* p, int* curLineNumPtr ) noexcept
{
    XMLAttribute* prevAttribute = 0;

    // Read the attributes.
    while( p ) {
        p = XMLUtil::SkipWhiteSpace( p, curLineNumPtr );
        if ( !(*p) ) {
            _document->SetError( XML_ERROR_PARSING_ELEMENT, _parseLineNum, "XMLElement name=%s", Name() );
            return 0;
        }

        // attribute.
        if (XMLUtil::IsNameStartChar( (uint8_t) *p ) ) {
            XMLAttribute* attrib = CreateAttribute();
            assert( attrib );
            attrib->_parseLineNum = _document->_parseCurLineNum;

            const int attrLineNum = attrib->_parseLineNum;

            p = attrib->ParseDeep( p, _document->ProcessEntities(), curLineNumPtr );
            if ( !p || Attribute( attrib->Name() ) ) {
                DeleteAttribute( attrib );
                _document->SetError( XML_ERROR_PARSING_ATTRIBUTE, attrLineNum, "XMLElement name=%s", Name() );
                return 0;
            }
            // There is a minor bug here: if the attribute in the source xml
            // document is duplicated, it will not be detected and the
            // attribute will be doubly added. However, tracking the 'prevAttribute'
            // avoids re-scanning the attribute list. Preferring performance for
            // now, may reconsider in the future.
            if ( prevAttribute ) {
                assert( prevAttribute->_next == 0 );
                prevAttribute->_next = attrib;
            }
            else {
                assert( _rootAttribute == 0 );
                _rootAttribute = attrib;
            }
            prevAttribute = attrib;
        }
        // end of the tag
        else if ( *p == '>' ) {
            ++p;
            break;
        }
        // end of the tag
        else if ( *p == '/' && *(p+1) == '>' ) {
            _closingType = CLOSED;
            return p+2;    // done; sealed element.
        }
        else {
            _document->SetError( XML_ERROR_PARSING_ELEMENT, _parseLineNum, 0 );
            return 0;
        }
    }
    return p;
}

void XMLElement::DeleteAttribute( XMLAttribute* attribute ) noexcept
{
    if ( attribute == 0 ) {
        return;
    }
    MemPool* pool = attribute->_memPool;
    attribute->~XMLAttribute();
    pool->Free( attribute );
}

XMLAttribute* XMLElement::CreateAttribute() noexcept
{
    assert( sizeof( XMLAttribute ) == _document->_attributePool.ItemSize() );
    XMLAttribute* attrib = new (_document->_attributePool.Alloc() ) XMLAttribute();
    assert( attrib );
    attrib->_memPool = &_document->_attributePool;
    attrib->_memPool->SetTracked();
    return attrib;
}

XMLElement* XMLElement::InsertNewChildElement(const char* name) noexcept
{
    XMLElement* node = _document->NewElement(name);
    return InsertEndChild(node) ? node : 0;
}

XMLComment* XMLElement::InsertNewComment(const char* comment) noexcept
{
    XMLComment* node = _document->NewComment(comment);
    return InsertEndChild(node) ? node : 0;
}

XMLText* XMLElement::InsertNewText(const char* text) noexcept
{
    XMLText* node = _document->NewText(text);
    return InsertEndChild(node) ? node : 0;
}

XMLDeclaration* XMLElement::InsertNewDeclaration(const char* text) noexcept
{
    XMLDeclaration* node = _document->NewDeclaration(text);
    return InsertEndChild(node) ? node : 0;
}

XMLUnknown* XMLElement::InsertNewUnknown(const char* text) noexcept
{
    XMLUnknown* node = _document->NewUnknown(text);
    return InsertEndChild(node) ? node : 0;
}


//
//    <ele></ele>
//    <ele>foo<b>bar</b></ele>
//
char* XMLElement::ParseDeep( char* p, StrPair* parentEndTag, int* curLineNumPtr ) noexcept
{
    // Read the element name.
    p = XMLUtil::SkipWhiteSpace( p, curLineNumPtr );

    // The closing element is the </element> form. It is
    // parsed just like a regular element then deleted from
    // the DOM.
    if ( *p == '/' ) {
        _closingType = CLOSING;
        ++p;
    }

    p = _value.ParseName( p );
    if ( _value.Empty() ) {
        return 0;
    }

    p = ParseAttributes( p, curLineNumPtr );
    if ( !p || !*p || _closingType != OPEN ) {
        return p;
    }

    p = XMLNode::ParseDeep( p, parentEndTag, curLineNumPtr );
    return p;
}

XMLNode* XMLElement::ShallowClone( XMLDocument* doc ) const noexcept
{
    if ( !doc ) {
        doc = _document;
    }
    XMLElement* element = doc->NewElement( Value() );                    // fixme: this will always allocate memory. Intern?
    for( const XMLAttribute* a=FirstAttribute(); a; a=a->Next() ) {
        element->SetAttribute( a->Name(), a->Value() );                    // fixme: this will always allocate memory. Intern?
    }
    return element;
}

bool XMLElement::ShallowEqual( const XMLNode* compare ) const noexcept
{
    assert( compare );
    const XMLElement* other = compare->ToElement();
    if ( other && XMLUtil::StringEqual( other->Name(), Name() )) {

        const XMLAttribute* a=FirstAttribute();
        const XMLAttribute* b=other->FirstAttribute();

        while ( a && b ) {
            if ( !XMLUtil::StringEqual( a->Value(), b->Value() ) ) {
                return false;
            }
            a = a->Next();
            b = b->Next();
        }
        if ( a || b ) {
            // different count
            return false;
        }
        return true;
    }
    return false;
}

bool XMLElement::Accept( XMLVisitor* visitor ) const noexcept
{
    assert( visitor );
    if ( visitor->VisitEnter( *this, _rootAttribute ) ) {
        for ( const XMLNode* node=FirstChild(); node; node=node->NextSibling() ) {
            if ( !node->Accept( visitor ) ) {
                break;
            }
        }
    }
    return visitor->VisitExit( *this );
}


// --------- XMLDocument ----------- //

// Warning: List must match 'enum XMLError'
const char* XMLDocument::_errorNames[XML_ERROR_COUNT] = {
    "XML_SUCCESS",
    "XML_NO_ATTRIBUTE",
    "XML_WRONG_ATTRIBUTE_TYPE",
    "XML_ERROR_FILE_NOT_FOUND",
    "XML_ERROR_FILE_COULD_NOT_BE_OPENED",
    "XML_ERROR_FILE_READ_ERROR",
    "XML_ERROR_PARSING_ELEMENT",
    "XML_ERROR_PARSING_ATTRIBUTE",
    "XML_ERROR_PARSING_TEXT",
    "XML_ERROR_PARSING_CDATA",
    "XML_ERROR_PARSING_COMMENT",
    "XML_ERROR_PARSING_DECLARATION",
    "XML_ERROR_PARSING_UNKNOWN",
    "XML_ERROR_EMPTY_DOCUMENT",
    "XML_ERROR_MISMATCHED_ELEMENT",
    "XML_ERROR_PARSING",
    "XML_CAN_NOT_CONVERT_TEXT",
    "XML_NO_TEXT_NODE",
    "XML_ELEMENT_DEPTH_EXCEEDED"
};

XMLDocument::XMLDocument( bool processEntities, Whitespace whitespaceMode ) noexcept
 :
    XMLNode( 0 ),
    _writeBOM( false ),
    _processEntities( processEntities ),
    _errorID(XML_SUCCESS),
    _whitespaceMode( whitespaceMode ),
    _errorStr(),
    _errorLineNum( 0 ),
    _charBuffer( nullptr ),
    _parseCurLineNum( 0 ),
    _parsingDepth(0),
    _unlinked(),
    _elementPool(),
    _attributePool(),
    _textPool(),
    _commentPool()
{
    // avoid VC++ C4355 warning about 'this' in initializer list (C4355 is off by default in VS2012+)
    _document = this;
}

XMLDocument::~XMLDocument() noexcept
{
    Clear();
}

void XMLDocument::MarkInUse(const XMLNode* const node) noexcept
{
    assert(node);
    assert(node->_parent == 0);

    for (int i = 0; i < _unlinked.Size(); ++i) {
        if (node == _unlinked[i]) {
            _unlinked.SwapRemove(i);
            break;
        }
    }
}

void XMLDocument::Clear() noexcept
{
    DeleteChildren();
    while( _unlinked.Size()) {
        DeleteNode(_unlinked[0]);    // Will remove from _unlinked as part of delete.
    }

#ifdef TINYXML2_DEBUG
    const bool hadError = Error();
#endif
    ClearError();

    if (not IsExternalBuffer())
        delete [] _charBuffer;

    _charBuffer = nullptr;

    _parsingDepth = 0;

#if 0
    _textPool.Trace( "text" );
    _elementPool.Trace( "element" );
    _commentPool.Trace( "comment" );
    _attributePool.Trace( "attribute" );
#endif

#ifdef TINYXML2_DEBUG
    if ( !hadError ) {
        assert( _elementPool.CurrentAllocs()   == _elementPool.Untracked() );
        assert( _attributePool.CurrentAllocs() == _attributePool.Untracked() );
        assert( _textPool.CurrentAllocs()      == _textPool.Untracked() );
        assert( _commentPool.CurrentAllocs()   == _commentPool.Untracked() );
    }
#endif
}

void XMLDocument::DeepCopy(XMLDocument* target) const noexcept
{
    assert(target);
    if (target == this) {
        return; // technically success - a no-op.
    }

    target->Clear();
    for (const XMLNode* node = this->FirstChild(); node; node = node->NextSibling()) {
        target->InsertEndChild(node->DeepClone(target));
    }
}

XMLElement* XMLDocument::NewElement( const char* name ) noexcept
{
    XMLElement* ele = CreateUnlinkedNode<XMLElement>( _elementPool );
    ele->SetName( name );
    return ele;
}

XMLComment* XMLDocument::NewComment( const char* str ) noexcept
{
    XMLComment* comment = CreateUnlinkedNode<XMLComment>( _commentPool );
    comment->SetValue( str );
    return comment;
}

XMLText* XMLDocument::NewText( const char* str ) noexcept
{
    XMLText* text = CreateUnlinkedNode<XMLText>( _textPool );
    text->SetValue( str );
    return text;
}

XMLDeclaration* XMLDocument::NewDeclaration( const char* str ) noexcept
{
    XMLDeclaration* dec = CreateUnlinkedNode<XMLDeclaration>( _commentPool );
    dec->SetValue( str ? str : "xml version=\"1.0\" encoding=\"UTF-8\"" );
    return dec;
}

XMLUnknown* XMLDocument::NewUnknown( const char* str ) noexcept
{
    XMLUnknown* unk = CreateUnlinkedNode<XMLUnknown>( _commentPool );
    unk->SetValue( str );
    return unk;
}

static FILE* callfopen( const char* filepath, const char* mode ) noexcept
{
    assert( filepath );
    assert( mode );

    FILE* fp = fopen( filepath, mode );

    return fp;
}

void XMLDocument::DeleteNode( XMLNode* node ) noexcept
{
    assert( node );
    assert(node->_document == this );
    if (node->_parent) {
        node->_parent->DeleteChild( node );
    }
    else {
        // Isn't in the tree.
        // Use the parent delete.
        // Also, we need to mark it tracked: we 'know'
        // it was never used.
        node->_memPool->SetTracked();
        // Call the static XMLNode version:
        XMLNode::DeleteNode(node);
    }
}

XMLError XMLDocument::LoadFile( const char* filename ) noexcept
{
    if ( !filename ) {
        assert( false );
        SetError( XML_ERROR_FILE_COULD_NOT_BE_OPENED, 0, "filename=<null>" );
        return _errorID;
    }

    Clear();
    FILE* fp = callfopen( filename, "rb" );
    if ( !fp ) {
        perror("fopen");
        SetError( XML_ERROR_FILE_NOT_FOUND, 0, "filename=%s", filename );
        return _errorID;
    }
    LoadFile( fp );
    fclose( fp );
    return _errorID;
}

XMLError XMLDocument::LoadFile( FILE* fp ) noexcept
{
    Clear();

    assert(fp);
    fseek( fp, 0, SEEK_SET );
    if ( fgetc( fp ) == EOF && ferror( fp ) != 0 ) {
        SetError( XML_ERROR_FILE_READ_ERROR, 0, 0 );
        return _errorID;
    }

    fseek( fp, 0, SEEK_END );

    size_t file_size;
    {
        int64_t isz = ftell( fp );
        if ( isz < 0 ) {
            perror("ftell");
            SetError( XML_ERROR_FILE_READ_ERROR, 0, 0 );
            fseek( fp, 0, SEEK_SET );
            return _errorID;
        }
        fseek( fp, 0, SEEK_SET );
        file_size = static_cast<size_t>(isz);
    }

    constexpr size_t maxFileSZ = size_t(UINT_MAX) * 4; // 16 GiB
    if ( file_size >= maxFileSZ ) {
        SetError( XML_ERROR_FILE_READ_ERROR, 0, 0 );
        return _errorID;
    }

    if ( file_size == 0 ) {
        SetError( XML_ERROR_EMPTY_DOCUMENT, 0, 0 );
        return _errorID;
    }

    assert( ! _charBuffer );
    assert( not IsExternalBuffer() );

    _charBuffer = new char[file_size + 1];

    size_t read = ::fread( _charBuffer, 1, file_size, fp );
    if ( read != file_size ) {
        perror("fread");
        SetError( XML_ERROR_FILE_READ_ERROR, 0, 0 );
        return _errorID;
    }

    _charBuffer[file_size] = 0;

    Parse0();
    return _errorID;
}

XMLError XMLDocument::SaveFile( const char* filename, bool compact ) noexcept
{
    if ( !filename ) {
        assert( false );
        SetError( XML_ERROR_FILE_COULD_NOT_BE_OPENED, 0, "filename=<null>" );
        return _errorID;
    }

    FILE* fp = callfopen( filename, "w" );
    if ( !fp ) {
        SetError( XML_ERROR_FILE_COULD_NOT_BE_OPENED, 0, "filename=%s", filename );
        return _errorID;
    }
    SaveFile(fp, compact);
    fclose( fp );
    return _errorID;
}

XMLError XMLDocument::SaveFile( FILE* fp, bool compact ) noexcept
{
    // Clear any error from the last save, otherwise it will get reported
    // for *this* call.
    ClearError();
    XMLPrinter stream( fp, compact );
    Print( &stream );
    return _errorID;
}

XMLError XMLDocument::Parse( const char* xml, size_t nBytes ) noexcept
{
    Clear();

    if ( nBytes == 0 || !xml || !*xml ) {
        SetError( XML_ERROR_EMPTY_DOCUMENT, 0, 0 );
        return _errorID;
    }
    if ( nBytes == static_cast<size_t>(-1) ) {
        nBytes = strlen( xml );
    }
    assert( _charBuffer == nullptr );
    assert( not IsExternalBuffer() );

    _charBuffer = new char[ nBytes + 1 ];
    memcpy( _charBuffer, xml, nBytes );
    _charBuffer[nBytes] = 0;

    Parse0();

    if ( Error() ) {
        // clean up now essentially dangling memory.
        // and the parse fail can put objects in the
        // pools that are dead and inaccessible.
        DeleteChildren();
        _elementPool.Clear();
        _attributePool.Clear();
        _textPool.Clear();
        _commentPool.Clear();
    }
    return _errorID;
}

XMLError XMLDocument::ParseExternal(char* externalBuffer, size_t nBytes) noexcept
{
    Clear();

    if ( nBytes == 0 || !externalBuffer || !*externalBuffer ) {
        SetError( XML_ERROR_EMPTY_DOCUMENT, 0, 0 );
        return _errorID;
    }

    _externalSize = nBytes;

    assert( _charBuffer == nullptr );
    assert( IsExternalBuffer() );

    _charBuffer = externalBuffer;

    _charBuffer[nBytes] = 0;

    Parse0();

    if ( Error() ) {
        // clean up now essentially dangling memory.
        // and the parse fail can put objects in the
        // pools that are dead and inaccessible.
        DeleteChildren();
        _elementPool.Clear();
        _attributePool.Clear();
        _textPool.Clear();
        _commentPool.Clear();
    }
    return _errorID;
}

void XMLDocument::Print( XMLPrinter* streamer ) const noexcept
{
    if ( streamer ) {
        Accept( streamer );
    }
    else {
        XMLPrinter stdoutStreamer( stdout );
        Accept( &stdoutStreamer );
    }
}

void XMLDocument::ClearError() noexcept
{
    _errorID = XML_SUCCESS;
    _errorLineNum = 0;
    _errorStr.Reset();
}

void XMLDocument::SetError( XMLError error, int lineNum, const char* format, ... ) noexcept
{
    assert( error >= 0 && error < XML_ERROR_COUNT );
    _errorID = error;
    _errorLineNum = lineNum;
    _errorStr.Reset();

    constexpr size_t BUFFER_SIZE = 5000; // unix file names can be up to 4K long
    char buffer[BUFFER_SIZE + 2];
    buffer[0] = 0;
    buffer[1] = 0;
    buffer[BUFFER_SIZE - 1] = 0;
    buffer[BUFFER_SIZE] = 0;

    assert(sizeof(error) <= sizeof(int));
    snprintf(buffer, BUFFER_SIZE, "Error=%s ErrorID=%d (0x%x) Line number=%d",
                ErrorIDToName(error), int(error), int(error), lineNum);

    if (format) {
        size_t len = strlen(buffer);
        snprintf(buffer + len, BUFFER_SIZE - len, ": ");
        len = strlen(buffer);

        va_list va;
        va_start(va, format);
        vsnprintf(buffer + len, BUFFER_SIZE - len, format, va);
        va_end(va);
    }
    _errorStr.SetStr(buffer);
}

/*static*/ const char* XMLDocument::ErrorIDToName(XMLError errorID) noexcept
{
    assert( errorID >= 0 && errorID < XML_ERROR_COUNT );
    const char* errorName = _errorNames[errorID];
    assert( errorName && errorName[0] );
    return errorName;
}

const char* XMLDocument::ErrorStr() const noexcept
{
    return _errorStr.Empty() ? "" : _errorStr.GetStr();
}

void XMLDocument::PrintError() const noexcept
{
    printf("%s\n", ErrorStr());
}

const char* XMLDocument::ErrorName() const noexcept
{
    return ErrorIDToName(_errorID);
}

void XMLDocument::Parse0() noexcept
{
    assert( NoChildren() ); // Clear() must have been called previously
    assert( _charBuffer );
    _parseCurLineNum = 1;
    _parseLineNum = 1;
    char* p = _charBuffer;
    p = XMLUtil::SkipWhiteSpace( p, &_parseCurLineNum );
    p = const_cast<char*>( XMLUtil::ReadBOM( p, &_writeBOM ) );
    if ( !*p ) {
        SetError( XML_ERROR_EMPTY_DOCUMENT, 0, 0 );
        return;
    }
    ParseDeep(p, 0, &_parseCurLineNum );
}

void XMLDocument::PushDepth() noexcept
{
    _parsingDepth++;
    if (_parsingDepth == TINYXML2_MAX_ELEMENT_DEPTH) {
        SetError(XML_ELEMENT_DEPTH_EXCEEDED, _parseCurLineNum, "Element nesting is too deep." );
    }
}

void XMLDocument::PopDepth() noexcept
{
    assert(_parsingDepth > 0);
    --_parsingDepth;
}

XMLPrinter::XMLPrinter( FILE* file, bool compact, int depth ) noexcept
  : _elementJustOpened( false ),
    _stack(),
    _firstElement( true ),
    _fp( file ),
    _depth( depth ),
    _textDepth( -1 ),
    _processEntities( true ),
    _compactMode( compact ),
    _buffer()
{
    for( int i=0; i<ENTITY_RANGE; ++i ) {
        _entityFlag[i] = false;
        _restrictedEntityFlag[i] = false;
    }
    for( int i=0; i<NUM_ENTITIES; ++i ) {
        const char entityValue = s_entities[i].value;
        const uint8_t flagIndex = static_cast<uint8_t>(entityValue);
        assert( flagIndex < ENTITY_RANGE );
        _entityFlag[flagIndex] = true;
    }
    _restrictedEntityFlag[static_cast<uint8_t>('&')] = true;
    _restrictedEntityFlag[static_cast<uint8_t>('<')] = true;
    _restrictedEntityFlag[static_cast<uint8_t>('>')] = true;    // not required, but consistency is nice
    _buffer.Push( 0 );
}

void XMLPrinter::Print( const char* format, ... ) noexcept
{
    va_list     va;
    va_start( va, format );

    if ( _fp ) {
        vfprintf( _fp, format, va );
    }
    else {
        int len = TIXML_VSCPRINTF( format, va );
        // Close out and re-start the va-args
        va_end( va );
        assert( len >= 0 );
        va_start( va, format );
        assert( _buffer.Size() > 0 && _buffer[_buffer.Size() - 1] == 0 );
        char* p = _buffer.PushArr( len ) - 1;    // back up over the null terminator.
        vsnprintf( p, len+1, format, va );
    }
    va_end( va );
}

void XMLPrinter::Write( const char* data, size_t size ) noexcept
{
    if ( _fp ) {
        fwrite ( data , sizeof(char), size, _fp);
    }
    else {
        char* p = _buffer.PushArr( static_cast<int>(size) ) - 1;   // back up over the null terminator.
        memcpy( p, data, size );
        p[size] = 0;
    }
}

void XMLPrinter::Putc( char ch ) noexcept
{
    if ( _fp ) {
        fputc ( ch, _fp);
    }
    else {
        char* p = _buffer.PushArr( sizeof(char) ) - 1;   // back up over the null terminator.
        p[0] = ch;
        p[1] = 0;
    }
}

void XMLPrinter::PrintSpace( int depth ) noexcept
{
    for( int i=0; i<depth; ++i ) {
        Write( "    " );
    }
}

void XMLPrinter::PrintString( const char* p, bool restricted ) noexcept
{
    // Look for runs of bytes between s_entities to print.
    const char* q = p;

    if ( _processEntities ) {
        const bool* flag = restricted ? _restrictedEntityFlag : _entityFlag;
        while ( *q ) {
            assert( p <= q );
            // Remember, char is sometimes signed. (How many times has that bitten me?)
            if ( *q > 0 && *q < ENTITY_RANGE ) {
                // Check for s_entities. If one is found, flush
                // the stream up until the entity, write the
                // entity, and keep looking.
                if ( flag[static_cast<uint8_t>(*q)] ) {
                    while ( p < q ) {
                        size_t delta = q - p;
                        int toPrint = ( INT_MAX < delta ) ? INT_MAX : static_cast<int>(delta);
                        Write( p, toPrint );
                        p += toPrint;
                    }
                    bool entityPatternPrinted = false;
                    for( int i=0; i<NUM_ENTITIES; ++i ) {
                        if ( s_entities[i].value == *q ) {
                            Putc( '&' );
                            Write( s_entities[i].pattern, s_entities[i].length );
                            Putc( ';' );
                            entityPatternPrinted = true;
                            break;
                        }
                    }
                    if ( !entityPatternPrinted ) {
                        // assert( entityPatternPrinted ) causes gcc -Wunused-but-set-variable in release
                        assert( false );
                    }
                    ++p;
                }
            }
            ++q;
            assert( p <= q );
        }
        // Flush the remaining string. This will be the entire
        // string if an entity wasn't found.
        if ( p < q ) {
            size_t delta = q - p;
            int toPrint = ( INT_MAX < delta ) ? INT_MAX : static_cast<int>(delta);
            Write( p, toPrint );
        }
    }
    else {
        Write( p );
    }
}

void XMLPrinter::PushHeader( bool writeBOM, bool writeDec ) noexcept
{
    if ( writeBOM ) {
        static const uint8_t bom[] = { TIXML_UTF_LEAD_0, TIXML_UTF_LEAD_1, TIXML_UTF_LEAD_2, 0 };
        Write( reinterpret_cast< const char* >( bom ) );
    }
    if ( writeDec ) {
        PushDeclaration( "xml version=\"1.0\"" );
    }
}

void XMLPrinter::PrepareForNewNode( bool compactMode ) noexcept
{
    SealElementIfJustOpened();

    if ( compactMode ) {
        return;
    }

    if ( _firstElement ) {
        PrintSpace (_depth);
    } else if ( _textDepth < 0) {
        Putc( '\n' );
        PrintSpace( _depth );
    }

    _firstElement = false;
}

void XMLPrinter::OpenElement( const char* name, bool compactMode ) noexcept
{
    PrepareForNewNode( compactMode );
    _stack.Push( name );

    Write ( "<" );
    Write ( name );

    _elementJustOpened = true;
    ++_depth;
}

void XMLPrinter::PushAttribute( const char* name, const char* value ) noexcept
{
    assert( _elementJustOpened );
    Putc ( ' ' );
    Write( name );
    Write( "=\"" );
    PrintString( value, false );
    Putc ( '\"' );
}

void XMLPrinter::PushAttribute( const char* name, int v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}

void XMLPrinter::PushAttribute( const char* name, uint v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}

void XMLPrinter::PushAttribute(const char* name, int64_t v) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr(v, buf, BUF_SIZE);
    PushAttribute(name, buf);
}

void XMLPrinter::PushAttribute(const char* name, uint64_t v) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr(v, buf, BUF_SIZE);
    PushAttribute(name, buf);
}

void XMLPrinter::PushAttribute( const char* name, bool v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}

void XMLPrinter::PushAttribute( const char* name, double v ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( v, buf, BUF_SIZE );
    PushAttribute( name, buf );
}

void XMLPrinter::CloseElement( bool compactMode ) noexcept
{
    --_depth;
    const char* name = _stack.Pop();

    if ( _elementJustOpened ) {
        Write( "/>" );
    }
    else {
        if ( _textDepth < 0 && !compactMode) {
            Putc( '\n' );
            PrintSpace( _depth );
        }
        Write ( "</" );
        Write ( name );
        Write ( ">" );
    }

    if ( _textDepth == _depth ) {
        _textDepth = -1;
    }
    if ( _depth == 0 && !compactMode) {
        Putc( '\n' );
    }
    _elementJustOpened = false;
}

void XMLPrinter::SealElementIfJustOpened() noexcept
{
    if ( !_elementJustOpened ) {
        return;
    }
    _elementJustOpened = false;
    Putc( '>' );
}

void XMLPrinter::PushText( const char* text, bool cdata ) noexcept
{
    _textDepth = _depth-1;

    SealElementIfJustOpened();
    if ( cdata ) {
        Write( "<![CDATA[" );
        Write( text );
        Write( "]]>" );
    }
    else {
        PrintString( text, true );
    }
}

void XMLPrinter::PushText( int64_t value ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}

void XMLPrinter::PushText( uint64_t value ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr(value, buf, BUF_SIZE);
    PushText(buf, false);
}

void XMLPrinter::PushText( int value ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}

void XMLPrinter::PushText( uint value ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}

void XMLPrinter::PushText( bool value ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}

void XMLPrinter::PushText( float value ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}

void XMLPrinter::PushText( double value ) noexcept
{
    char buf[BUF_SIZE];
    XMLUtil::ToStr( value, buf, BUF_SIZE );
    PushText( buf, false );
}

void XMLPrinter::PushComment( const char* comment ) noexcept
{
    PrepareForNewNode( _compactMode );

    Write( "<!--" );
    Write( comment );
    Write( "-->" );
}

void XMLPrinter::PushDeclaration( const char* value ) noexcept
{
    PrepareForNewNode( _compactMode );

    Write( "<?" );
    Write( value );
    Write( "?>" );
}

void XMLPrinter::PushUnknown( const char* value ) noexcept
{
    PrepareForNewNode( _compactMode );

    Write( "<!" );
    Write( value );
    Putc( '>' );
}

bool XMLPrinter::VisitEnter( const XMLDocument& doc ) noexcept
{
    _processEntities = doc.ProcessEntities();
    if ( doc.HasBOM() ) {
        PushHeader( true, false );
    }
    return true;
}

bool XMLPrinter::VisitEnter( const XMLElement& element, const XMLAttribute* attribute ) noexcept
{
    const XMLElement* parentElem = 0;
    if ( element.Parent() ) {
        parentElem = element.Parent()->ToElement();
    }
    const bool compactMode = parentElem ? CompactMode( *parentElem ) : _compactMode;
    OpenElement( element.Name(), compactMode );
    while ( attribute ) {
        PushAttribute( attribute->Name(), attribute->Value() );
        attribute = attribute->Next();
    }
    return true;
}

bool XMLPrinter::VisitExit( const XMLElement& element ) noexcept
{
    CloseElement( CompactMode(element) );
    return true;
}

bool XMLPrinter::Visit( const XMLText& text ) noexcept
{
    PushText( text.Value(), text.CData() );
    return true;
}

bool XMLPrinter::Visit( const XMLComment& comment ) noexcept
{
    PushComment( comment.Value() );
    return true;
}

bool XMLPrinter::Visit( const XMLDeclaration& declaration ) noexcept
{
    PushDeclaration( declaration.Value() );
    return true;
}

bool XMLPrinter::Visit( const XMLUnknown& unknown ) noexcept
{
    PushUnknown( unknown.Value() );
    return true;
}

} // NS tinxml2

