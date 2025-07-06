#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <iostream>
#include <string>

using json = nlohmann::json;

static json yamlNodeToJson(const YAML::Node& node) {
	if (node.IsScalar()) {
		return node.as<std::string>();
	} else if (node.IsSequence()) {
		json arr = json::array();
		for (const auto& it: node) arr.push_back(yamlNodeToJson(it));
		return arr;
	} else if (node.IsMap()) {
		json obj = json::object();
		for (const auto& it: node) obj[it.first.as<std::string>()] = yamlNodeToJson(it.second);
		return obj;
	}
	return nullptr;
}

std::string yamlFileToJson(const std::string& filename) {
	YAML::Node root = YAML::LoadFile(filename);
	json       j    = yamlNodeToJson(root);
	return j.dump(2);
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <plik.yaml>\n";
		return 1;
	}

	const std::string filename = argv[1];
	try {
		std::string jsonOutput = yamlFileToJson(filename);
		std::cout << jsonOutput << "\n";
	} catch (const YAML::Exception& ex) {
		std::cerr << "Parse error: " << ex.what() << "\n";
		return 2;
	} catch (const std::exception& ex) {
		std::cerr << "Error: " << ex.what() << "\n";
		return 3;
	}

	return 0;
}
