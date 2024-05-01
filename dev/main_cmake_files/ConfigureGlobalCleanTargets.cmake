define_property(TARGET PROPERTY MODULE_DIRECTORIES BRIEF_DOCS "Used for clean-modules target" FULL_DOCS "Used for clean-modules target")
add_custom_target(clean-modules
	echo "Deleting following directories:"
	COMMAND echo -e "$<JOIN:$<TARGET_PROPERTY:clean-modules,MODULE_DIRECTORIES>,\\n>"
	COMMAND rm -rfI "$<TARGET_PROPERTY:clean-modules,MODULE_DIRECTORIES>" || "true"
	USES_TERMINAL
	COMMAND_EXPAND_LISTS
	VERBATIM)
