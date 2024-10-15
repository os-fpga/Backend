#pragma once
/*
 * This file is generated by uxsdcxx 0.1.0.
 * https://github.com/duck2/uxsdcxx
 * Modify only if your build process doesn't involve regenerating this file.
 *
 * Cmdline: uxsdcxx/uxsdcxx.py /home/serge/bes/13oct/Backend/OpenFPGA/libs/libopenfpgacapnproto/gen/unique_blocks.xsd
 * Input file: /home/serge/bes/13oct/Backend/OpenFPGA/libs/libopenfpgacapnproto/gen/unique_blocks.xsd
 * md5sum of input file: 1db9d740309076fa51f61413bae1e072
 */

#include <functional>


/* All uxsdcxx functions and structs live in this namespace. */

#include <cstdlib>
#include <tuple>

namespace uxsd {

/* Enum tokens generated from XSD enumerations. */

enum class enum_type {UXSD_INVALID = 0, CBX, CBY, SB};

/* Base class for the schema. */
struct DefaultUniqueBlocksContextTypes {
using InstanceReadContext = void *;
	using BlockReadContext = void *;
	using UniqueBlocksReadContext = void *;
using InstanceWriteContext = void *;
	using BlockWriteContext = void *;
	using UniqueBlocksWriteContext = void *;
};

template<typename ContextTypes=DefaultUniqueBlocksContextTypes>
class UniqueBlocksBase {
public:
	virtual ~UniqueBlocksBase() {}
	virtual void start_load(const std::function<void(const char*)> *report_error) = 0;
	virtual void finish_load() = 0;
	virtual void start_write() = 0;
	virtual void finish_write() = 0;
	virtual void error_encountered(const char * file, int line, const char *message) = 0;
	/** Generated for complex type "instance":
	 * <xs:complexType name="instance">
	 *   <xs:attribute name="x" type="xs:unsignedInt" use="required" />
	 *   <xs:attribute name="y" type="xs:unsignedInt" use="required" />
	 * </xs:complexType>
	*/
	virtual inline unsigned int get_instance_x(typename ContextTypes::InstanceReadContext &ctx) = 0;
	virtual inline unsigned int get_instance_y(typename ContextTypes::InstanceReadContext &ctx) = 0;

	/** Generated for complex type "block":
	 * <xs:complexType name="block">
	 * <xs:sequence>
	 *   <xs:element name="instance" type="instance" minOccurs="0" maxOccurs="unbounded" />
	 * </xs:sequence>
	 * <xs:attribute name="type" type="type" use="required" />
	 * <xs:attribute name="x" type="xs:unsignedInt" use="required" />
	 * <xs:attribute name="y" type="xs:unsignedInt" use="required" />
	 * </xs:complexType>
	*/
	virtual inline enum_type get_block_type(typename ContextTypes::BlockReadContext &ctx) = 0;
	virtual inline unsigned int get_block_x(typename ContextTypes::BlockReadContext &ctx) = 0;
	virtual inline unsigned int get_block_y(typename ContextTypes::BlockReadContext &ctx) = 0;
	virtual inline void preallocate_block_instance(typename ContextTypes::BlockWriteContext &ctx, size_t size) = 0;
	virtual inline typename ContextTypes::InstanceWriteContext add_block_instance(typename ContextTypes::BlockWriteContext &ctx, unsigned int x, unsigned int y) = 0;
	virtual inline void finish_block_instance(typename ContextTypes::InstanceWriteContext &ctx) = 0;
	virtual inline size_t num_block_instance(typename ContextTypes::BlockReadContext &ctx) = 0;
	virtual inline typename ContextTypes::InstanceReadContext get_block_instance(int n, typename ContextTypes::BlockReadContext &ctx) = 0;

	/** Generated for complex type "unique_blocks":
	 * <xs:complexType xmlns:xs="http://www.w3.org/2001/XMLSchema">
	 *     <xs:sequence>
	 *       <xs:element name="block" type="block" maxOccurs="unbounded" />
	 *     </xs:sequence>
	 *   </xs:complexType>
	*/
	virtual inline void preallocate_unique_blocks_block(typename ContextTypes::UniqueBlocksWriteContext &ctx, size_t size) = 0;
	virtual inline typename ContextTypes::BlockWriteContext add_unique_blocks_block(typename ContextTypes::UniqueBlocksWriteContext &ctx, enum_type type, unsigned int x, unsigned int y) = 0;
	virtual inline void finish_unique_blocks_block(typename ContextTypes::BlockWriteContext &ctx) = 0;
	virtual inline size_t num_unique_blocks_block(typename ContextTypes::UniqueBlocksReadContext &ctx) = 0;
	virtual inline typename ContextTypes::BlockReadContext get_unique_blocks_block(int n, typename ContextTypes::UniqueBlocksReadContext &ctx) = 0;
};

} /* namespace uxsd */
