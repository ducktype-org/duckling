#pragma once


namespace compiler::mir {
	// Few notes: (@TODO: move to sphinx docs)
	/*
		Semantics of mir: 
		"=" -- pass bytes as given, without any calls, etc

		operation:
			obj = EXPR; // <- creates or assigns local object with initial value EXPR
				  EXPR; // <- does EXPR
			

			
		operation additional flags:
			* move obj; // <- this operation moves given object. Moving marks obj life flag to false
			* construct obj;
			* destroy obj;

		Example:
			a = call foo(); [construct a]
				call f(a);  [move a]

		Operations / Expressions:
			call   func, func args...
			vcall  func-ptr, func args...
			get_pointer
			GEP    obj, sym id...
			
			IAdd, ...

			destroy
			destroy_if

		Operations / Terminators:
		 	jmp      Block
		 	branch   bool, Block, Block
			return   value

		todo:
		* tmp == create? -- yes
		* is move a special operation? -- no
		* is destroy a flag? -- yes
		* what about value categories in things like match(optional)..

		CFG enriched with Scope data:
			

	*/


}
