function testcase {
	local_count: 10;
	arg_count: 2;

	define z1 = local[0];
	define z2 = local[1];

	code: {
		mov z1, arg[0];
		mov z2, arg[1];
	  label: while_label;
	  	test_eq z1, z2;
		jmp_if return_label;
		cmp_geq z1, z2;
		jmp_if sub_label;
		swap z1, z2;
	  label: sub_label;
	    sub z1, z2;
		jmp while_label;
	  label: return_label;
	  	ret_imm z1;
	};
}
