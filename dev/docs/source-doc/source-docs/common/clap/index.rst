====
Clap
====

Command-Line Argument Parser is our library for parsing user input passed through command line arguments.

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:

    src/clap/index.rst
    tests.rst

Example
=======

.. code-block:: cpp
    :caption: A ``cat`` using clap. Printing happens n times. This example is error prone and enables help message generation.

    // Prints n (n is optional) times contents of a given file(s) to stdin.
    // Usage:   cat <file> [file...]
    // Example: cat foo.txt -n 5

    #include "clap/clap.hpp"
    #include "filesystem/file.hpp"
    #include <iostream>

    void print_file(const fs::FilePath& file) {
        std::cout << file.getContent().view().stdString() << '\n';
    }

    int main(int argc, const char** argv) {
        // Create a clap object and set value parsers.
        auto clap = clap::Clap()
                        .addHelpFlag()
                        .addPositional(clap::FileParser::make())
                        .setDefaultParser(clap::FileParser::make())
                        .add(clap::ParamBuilder::ofValue(clap::IntParser::make())
                                 .optional()  // It's the default
                                 .addShortName('n')
                                 .addLongName("times")
                                 .addShortDesc("How many times to print each content")
                                 .build());
        // .addPositional(FileParser) tells clap to expect at least one file.

        clap::ParsingResult result;
        try {
            // Real parsing happens here. Only this operation may throw clap exception.
            result = clap.parse(argc, argv);

            i64 times = result.getValue<i64>('n').value_or(1);
            while (times--) {
                // Since we are using clap::FileParser, it automatically links
                // specified input to real files!
                auto file1 = result.getPositional<fs::FilePath>(0);
                print_file(file1);

                // Finally, iterate over 'extra' parameters and print them out as well.
                // getExtra returns a base::Optional<T>, but we know it has a value.
                for (usize i = 0; i < result.getExtraParameterCount(); i++)
                    print_file(*result.getExtra<fs::FilePath>(i));
            }
        } catch (clap::exceptions::HelpException& help) {
            // Since we added a built-in help flag, we can catch a help exception.
            // There is a useful generic HelpMessageGenerator.
            std::string help_msg = clap::HelpMessageGenerator::generate(clap, help.parsing_result);
            std::cout << help_msg << '\n';
            return 0;
        } catch (clap::exceptions::ClapException& e) {
            // In case of any other exception, user did something wrong.
            std::cerr << "ERROR! : " << e.what() << '\n';
            return 1;
        }
    }
