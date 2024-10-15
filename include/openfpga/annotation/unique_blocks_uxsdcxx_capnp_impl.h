#pragma once
/*
 * This file is generated by uxsdcxx 0.1.0.
 * https://github.com/duck2/uxsdcxx
 * Modify only if your build process doesn't involve regenerating this file.
 *
 * Cmdline: uxsdcxx/uxsdcap.py /home/serge/bes/13oct/Backend/OpenFPGA/libs/libopenfpgacapnproto/gen/unique_blocks.xsd unique_blocks_capnproto_generate/unique_blocks_uxsdcxx.h unique_blocks_capnproto_generate/unique_blocks_uxsdcxx_capnp.h unique_blocks_capnproto_generate/unique_blocks_uxsdcxx_interface.h /home/serge/bes/13oct/Backend/OpenFPGA/libs/libopenfpgacapnproto/gen
 * Input file: /home/serge/bes/13oct/Backend/OpenFPGA/libs/libopenfpgacapnproto/gen/unique_blocks.xsd
 * md5sum of input file: 1db9d740309076fa51f61413bae1e072
 */

#include <functional>

#include <stdexcept>
#include <vector>
#include "capnp/serialize.h"
#include "unique_blocks_uxsdcxx.capnp.h"
#include "unique_blocks_uxsdcxx_interface.h"

/* All uxsdcxx functions and structs live in this namespace. */
namespace uxsd {


/* Enum conversions from uxsd to ucap */
inline enum_type conv_enum_type(ucap::Type e, const std::function<void(const char *)> * report_error) {
	switch(e) {
	case ucap::Type::UXSD_INVALID:
		return enum_type::UXSD_INVALID;
	case ucap::Type::CBX:
		return enum_type::CBX;
	case ucap::Type::CBY:
		return enum_type::CBY;
	case ucap::Type::SB:
		return enum_type::SB;
	default:
		(*report_error)("Unknown enum_type");
		throw std::runtime_error("Unreachable!");
	}
}

inline ucap::Type conv_to_enum_type(enum_type e) {
	switch(e) {
	case enum_type::UXSD_INVALID:
		return ucap::Type::UXSD_INVALID;
	case enum_type::CBX:
		return ucap::Type::CBX;
	case enum_type::CBY:
		return ucap::Type::CBY;
	case enum_type::SB:
		return ucap::Type::SB;
	default:
		throw std::runtime_error("Unknown enum_type");
	}
}
struct CapnpUniqueBlocksContextTypes : public DefaultUniqueBlocksContextTypes {
	using InstanceReadContext = ucap::Instance::Reader;
	using BlockReadContext = ucap::Block::Reader;
	using UniqueBlocksReadContext = ucap::UniqueBlocks::Reader;
	using InstanceWriteContext = ucap::Instance::Builder;
	using BlockWriteContext = ucap::Block::Builder;
	using UniqueBlocksWriteContext = ucap::UniqueBlocks::Builder;
};

class CapnpUniqueBlocks : public UniqueBlocksBase<CapnpUniqueBlocksContextTypes> {
	public:
	CapnpUniqueBlocks() {}

	void start_load(const std::function<void(const char *)> *report_error_in) override {
		report_error = report_error_in;
	}
	void finish_load() override {}
	void start_write() override {}
	void finish_write() override {}
	void error_encountered(const char * file, int line, const char *message) override {
		std::stringstream msg;		msg << message << " occured at file: " << file << " line: " << line;
		throw std::runtime_error(msg.str());
	}
	inline unsigned int get_instance_x(ucap::Instance::Reader &reader) override {
		return reader.getX();
	}

	inline unsigned int get_instance_y(ucap::Instance::Reader &reader) override {
		return reader.getY();
	}
	inline enum_type get_block_type(ucap::Block::Reader &reader) override {
		return conv_enum_type(reader.getType(), report_error);
	}

	inline unsigned int get_block_x(ucap::Block::Reader &reader) override {
		return reader.getX();
	}

	inline unsigned int get_block_y(ucap::Block::Reader &reader) override {
		return reader.getY();
	}

	inline ucap::Instance::Builder add_block_instance(ucap::Block::Builder &builder, unsigned int x, unsigned int y) override {
		auto instance = capnp::Orphanage::getForMessageContaining(builder).newOrphan<ucap::Instance>();
		instances_.emplace_back(std::move(instance));
		auto child_builder = instances_.back().get();
		child_builder.setX(x);
		child_builder.setY(y);
		return child_builder;
	}

	inline void preallocate_block_instance(ucap::Block::Builder&, size_t size) override {
		instances_.reserve(size);
	}

	inline void finish_block_instance(ucap::Instance::Builder &builder) override {
	}

	inline size_t num_block_instance(ucap::Block::Reader &reader) override {
		return reader.getInstances().size();
	}

	inline ucap::Instance::Reader get_block_instance(int n, ucap::Block::Reader &reader) override {
		return reader.getInstances()[n];
	}
	inline ucap::Block::Builder add_unique_blocks_block(ucap::UniqueBlocks::Builder &builder, enum_type type, unsigned int x, unsigned int y) override {
		auto block = capnp::Orphanage::getForMessageContaining(builder).newOrphan<ucap::Block>();
		blocks_.emplace_back(std::move(block));
		auto child_builder = blocks_.back().get();
		child_builder.setType(conv_to_enum_type(type));
		child_builder.setX(x);
		child_builder.setY(y);
		return child_builder;
	}

	inline void preallocate_unique_blocks_block(ucap::UniqueBlocks::Builder&, size_t size) override {
		blocks_.reserve(size);
	}

	inline void finish_unique_blocks_block(ucap::Block::Builder &builder) override {
		auto instance = builder.initInstances(instances_.size());
		for(size_t i = 0; i < instances_.size(); ++i) {
			instance.adoptWithCaveats(i, std::move(instances_[i]));
		}
		instances_.clear();
	}

	inline size_t num_unique_blocks_block(ucap::UniqueBlocks::Reader &reader) override {
		return reader.getBlocks().size();
	}

	inline ucap::Block::Reader get_unique_blocks_block(int n, ucap::UniqueBlocks::Reader &reader) override {
		return reader.getBlocks()[n];
	}
private:
	const std::function<void(const char *)> *report_error;
	std::vector<capnp::Orphan<ucap::Instance>> instances_;
	std::vector<capnp::Orphan<ucap::Block>> blocks_;
};


} /* namespace uxsd */
