#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <dia_app/view_manager/common.hpp>
#include <dia_app/view_manager/view_manager.hpp>
#include <json/json.hpp>

std::string slurp(std::ifstream& in) {
	std::ostringstream sstr;
	sstr << in.rdbuf();
	return sstr.str();
}

void with_file(const std::string& name, const std::string& contents, std::function<void()> f) {
	std::ofstream file;
	file.open(name);
	file << contents;
	file.close();
	try {
		f();
		std::remove(name.c_str());
	} catch (std::exception e) {
		std::remove(name.c_str());
		throw e;
	}
}

void serialization_deserialization() {
	dia::json sample_text = dia::create_text_component_json("Some text");
	dia::json intermediate_node
		= dia::create_intermediate_component_json({ sample_text, sample_text, sample_text });
	dia::json sample_diagnostics = dia::create_intermediate_component_json(
		{ intermediate_node, sample_text, intermediate_node }
	);
	auto file_name = "ser_de_input.in";
	with_file(file_name, to_string(sample_diagnostics), [file_name]() {
		std::ifstream file(file_name);
		std::string   content = slurp(file);
		std::cout << content << std::endl;
		return;
	});
}

int main() { serialization_deserialization(); }
