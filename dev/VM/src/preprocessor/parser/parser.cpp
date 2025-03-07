

#include <preprocessor/parser/elements.hpp>
#include <token_file/file.hpp>
#include <token_parser_core/tpc.hpp>
#include <deque>
#include "errors.hpp"

namespace vm::parser {
	using lexer::init;

	Box<tokenizer::TokenFile> tokenizeFile(const fs::FilePath& path) {
		init();
		tpc::init();
		lang_def::setKeywordMode(lang_def::KeywordMode::DuckBC);
		return lexer::tokenizeFile(path);
	}

	MBox<ParsedFile> parseFile(Ref<tokenizer::TokenFile> file, dia::Logger& log) {
		const lexer::TokenData& td = file->getTokenData();

		F8ParserState state(
			tpc::TokenStream(td.tokens, td.bof_sentinel, td.eof_sentinel, 0, td.tokens.size()), log
		);

		return ParsedFile::parse(state);
	}

	void defineTypes(Ref<ParsedProgram> program, dia::Logger& log) {
		base::Map<base::StrID, vm::TypeRef> type_map;

		std::deque<Ref<Type>> good_types;

		for (auto& type: program->types) {
			base::StrID name = VISIT(type->datatype, value, return value.name);

			if (type_map.contains(name)) {
				auto msg = makeBox<vm::parser::DuplicatedTypeError>(*type->position);

				auto duplicated_types
					= program->types | std::views::filter([&](auto&& duplicated_type) {
						  base::StrID other_name
							  = VISIT(duplicated_type->datatype, value, return value.name);
						  return name == other_name && (&*duplicated_type != &*type);
					  });

				for (auto& duplicated_type: duplicated_types)
					msg->addNote(makeBox<vm::parser::DuplicatedTypeNote>(*duplicated_type->position)
					);

				log.log(std::move(msg));
			} else {
				vm::Type typ      = vm::Type::declareType(name);
				auto     type_ref = program->type_metadata->addType(std::move(typ));
				type_map.put(name, type_ref);
				good_types.push_back(type.refMut());
			}
		}

		for (auto& type: good_types) {
			variant_match(type->datatype) {
				variant_case(PrimitiveType, data) {
					type_map[data.name]->definePrimitive(data.size);
				}
				variant_case(PointerType, data) {
					type_map[data.name]->definePointer(type_map[data.inner]);
				}
				variant_case(StaticTableType, data) {
					type_map[data.name]->defineStaticTable(type_map[data.inner], data.table_size);
				}
				variant_case(DynamicTableType, data) {
					type_map[data.name]->defineDynamicTable(type_map[data.inner]);
				}
				variant_case(DataType, data) {
					std::vector<std::pair<base::StrID, vm::TypeRef>> fields;
					fields.reserve(data.fields.size());
					for (auto& field: data.fields)
						fields.emplace_back(field.name, type_map[field.type]);
					type_map[data.name]->defineData(fields);
				}
				variant_case(VariantType, data) {
					std::vector<vm::TypeRef> variants;
					variants.reserve(data.variant_alternatives.size());
					for (auto& variant: data.variant_alternatives)
						variants.emplace_back(type_map[variant]);
					type_map[data.name]->defineVariant(variants);
				}
				variant_case(FunctionType, data) {
					std::vector<vm::TypeCRef> parameters;
					parameters.reserve(data.parameters.size());
					for (auto& param: data.parameters) parameters.emplace_back(type_map[param]);
					type_map[data.name]->defineFunction(parameters, type_map[data.result]);
				}
				variant_default { CORE_PANIC("bad type"); }
			}
		}

		program->type_metadata->finalize();
	}

	std::expected<ParsedProgram, std::string> assemble(const std::vector<fs::FilePath>& files) {
		auto          log = dia::Logger();
		ParsedProgram parsed_program;

		for (const auto& file: files) {
			parsed_program.token_files.push_back(tokenizeFile(file));
			auto maybe_parsed = parseFile(parsed_program.token_files.back().refMut(), log).toOptBox();
			
			if (!maybe_parsed || log.bad()) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return std::unexpected(stream.str());
			}

			auto& parsed = maybe_parsed.value();
			parsed_program.files_src_pos.push_back(std::move(parsed->position));

			for (auto& func: parsed->functions) {
				auto func_name = func->name.value;
				if (parsed_program.name_to_func.contains(func_name)) {
					auto msg
						= makeBox<vm::parser::DuplicateFunctionDeclarationError>(*func->position);
					auto dup_func = parsed_program.name_to_func.atMaybe(func_name);
					msg->addNote(makeBox<vm::parser::DuplicatedFunctionDeclarationNote>(
						*dup_func.value()->position
					));
					log.log(std::move(msg));
				}

				parsed_program.functions.push_back(std::move(func));
				parsed_program.name_to_func.put(
					func_name, parsed_program.functions.back().refMut()
				);
			}

			for (auto& type: parsed->types) parsed_program.types.push_back(std::move(type));

		}

		defineTypes(&parsed_program, log);
		if (log.bad()) {
			std::stringstream stream;
			log.dumpLogAndClear(true, stream);
			return std::unexpected(stream.str());
		}

		return parsed_program;
	}
}
