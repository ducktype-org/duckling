#include "config.hpp"

#include <diagnostic/logger.hpp>
#include <lexer/lexer_class.hpp>

namespace config {
	clap::Clap standardOptions() {
		return clap::Clap()
		    .addHelpFlag()
		    .add(clap::ParamBuilder::ofFlag()
		             .addLongName("logger-cerr")
		             .addShortDesc(
						 "If set, Logger class will immediately print its messages to cerr. "
						 "Useful for debugging."
					 )
		             .build())
		    .add(clap::ParamBuilder::ofFlag()
		             .addLongName("lexer-cerr")
		             .addShortDesc(
						 "If set, Lexer class will immediately print parsed tokens to cerr. "
						 "Useful for debugging."
					 )
		             .build());
	}

	clap::ParsingResult configureWith(clap::Clap& clap, clap::CLIArgs args) {
		auto res = clap.parse(args);

		dia::Logger::setImmediatelyDump(res.isFlag("logger-cerr"));
		lexer::Lexer::setTokenMessages(res.isFlag("lexer-cerr"));

		return res;
	}
}
