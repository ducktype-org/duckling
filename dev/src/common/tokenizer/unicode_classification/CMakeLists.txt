add_library(UnicodeClassification
	src/unicode_classification/classifications.cpp
)

make_module(UnicodeClassification USES unicode Init)