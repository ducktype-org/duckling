#pragma  once

namespace compiler {
	struct Options {
		bool dump_lexer;
		

	};

	void setOpts(Options);
	Options& getOpts();
}
