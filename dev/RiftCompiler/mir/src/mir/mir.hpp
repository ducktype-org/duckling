#pragma once


namespace compiler::mir {
	// Few notes: (@TODO: move to sphinx docs)
	/*
		Semantics of mir: 
		"=" -- pass bytes as given, without any calls, etc

		operation:
			create obj = EXPR; // <- creates object with initial value EXPR
			tmp    obj = EXPR; // <- creates temporary object with initial value EXPR
			assign obj = EXPR; // <- reassign object bytes to EXPR, without any additional operations 
			detroy obj;        // <- run obj destructor if its life flag is set
			do 			 EXPR; // <- do EXPR, discard its value

		operation additional flags:
			* move obj; // <- this operation moves given object. Moving marks obj life flag to false

		Example:
			create a = call foo();
					   call f(a) [move a];

		Operations / Expressions:
			call   func, func args...
			vcall  func-ptr, func args...
			get_pointer
			GEP    obj, sym id...
			
			IAdd, ...

		Operations / Terminators:
		 	jmp      Block
		 	branch   bool, Block, Block
			return   value

		todo:
		* tmp == create?
		* is move a special operation?
		* is destroy a flag?
		* what about value categories in things like match(optional)..

	*/


}
