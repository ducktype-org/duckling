#include "demangler.hpp"

#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <string>
#include <vector>

/**
 * This is the inverse of mangler.cpp, following the same mangling-scheme.md. It only understands
 * what the mangler actually emits, and it is deliberately best-effort: anything unexpected makes
 * the whole name un-demanglable, so callers fall back to printing the mangled name instead of a
 * half-decoded one.
 */
namespace compiler::helios::mangler {

	namespace {
		/**
		 * @brief Thrown internally as soon as the input stops matching the mangling grammar.
		 * Never escapes tryDemangle().
		 */
		struct NotDemanglable final: std::exception {
			[[nodiscard]]
			const char* what() const noexcept final {
				return "the name is not a demanglable duckling symbol";
			}
		};

		/**
		 * @brief A `<special-symbol-name>` and how it is spelled out for humans.
		 * @note `has_signature` tells whether the tag is followed by a `<function>` in the mangled
		 * name (the compiler-generated methods are, the module/global ctors and dtors are not).
		 */
		struct SpecialSymbol final {
			std::string_view tag;
			std::string_view text;
			bool             has_signature;
		};

		// Note: no tag is a prefix of another one, so a plain linear scan finds the right entry.
		constexpr std::array<SpecialSymbol, 16> SPECIAL_SYMBOLS{ {
			{ .tag = "mc", .text = "<module constructor>", .has_signature = false },
			{ .tag = "md", .text = "<module destructor>", .has_signature = false },
			{ .tag = "gc", .text = "<global constructor>", .has_signature = false },
			{ .tag = "gd", .text = "<global destructor>", .has_signature = false },
			{ .tag = "ic", .text = "<implicit constructor>", .has_signature = true },
			{ .tag = "dc", .text = "<default constructor>", .has_signature = true },
			{ .tag = "cc", .text = "<copy constructor>", .has_signature = true },
			{ .tag = "dd", .text = "<default destructor>", .has_signature = true },
			{ .tag = "toString", .text = "<toString>", .has_signature = true },
			{ .tag = "length", .text = "<length>", .has_signature = true },
			{ .tag = "push", .text = "<push>", .has_signature = true },
			{ .tag = "pop", .text = "<pop>", .has_signature = true },
			{ .tag = "ba", .text = "<box alloc>", .has_signature = true },
			{ .tag = "bf", .text = "<box free>", .has_signature = true },
			{ .tag = "lf", .text = "<list free>", .has_signature = true },
			{ .tag = "bd", .text = "<box destructor>", .has_signature = true },
		} };

		/**
		 * @brief The `<fixed-operator-tag>` table of mangler.cpp's fixedOperatorTag(), reversed.
		 */
		constexpr std::array<std::pair<std::string_view, char>, 18> OPERATOR_TAGS{ {
			{ "nt", '!' },
			{ "rm", '%' },
			{ "an", '&' },
			{ "ml", '*' },
			{ "pl", '+' },
			{ "mi", '-' },
			{ "pd", '.' },
			{ "dv", '/' },
			{ "co", ':' },
			{ "lt", '<' },
			{ "eq", '=' },
			{ "gt", '>' },
			{ "qm", '?' },
			{ "bs", '\\' },
			{ "eo", '^' },
			{ "bt", '`' },
			{ "or", '|' },
			{ "ti", '~' },
		} };

		std::string join(const std::vector<std::string>& parts, const std::string_view separator) {
			std::string ret;
			for (const auto& part: parts) {
				if (!ret.empty()) ret += separator;
				ret += part;
			}
			return ret;
		}

		/**
		 * @brief Recursive-descent parser over a single mangled name.
		 */
		class Demangler final {
		public:
			explicit Demangler(const std::string_view input): input(input) {}

			/**
			 * @brief Parses the whole input, i.e. `<language-prefix> <scheme-version> <encoding>
			 * <opt-metadata>`, and renders it for humans.
			 */
			std::string demangleWholeName() {
				std::string name = takeMangledName();

				// <opt-metadata> ::= "" | <metadata-prefix> <vendor-metadata>
				// The metadata is opaque, so whatever is left belongs to it.
				if (!atEnd()) {
					if (!peekIs('$') && !peekIs('.')) throw NotDemanglable{};
					name += ' ';
					name += input.substr(pos);
					pos = input.size();
				}

				return name;
			}

		private:
			/**
			 * @brief Cap on how deeply types may nest, so that a corrupted name cannot recurse the
			 * parser into a stack overflow. Far above anything a real program produces.
			 */
			static constexpr usize MAX_DEPTH = 256;

			std::string_view input;
			usize            pos   = 0;
			usize            depth = 0;

			/**
			 * @brief Keeps track of the nesting depth for as long as it is alive.
			 */
			class DepthGuard final {
				usize& depth;

			public:
				explicit DepthGuard(usize& depth): depth(depth) {
					if (++this->depth > MAX_DEPTH) throw NotDemanglable{};
				}

				DepthGuard(const DepthGuard&)            = delete;
				DepthGuard& operator=(const DepthGuard&) = delete;

				~DepthGuard() { --depth; }
			};

			// --- primitives ---------------------------------------------------------------------

			[[nodiscard]]
			bool atEnd() const {
				return pos >= input.size();
			}

			[[nodiscard]]
			char peek() const {
				if (atEnd()) throw NotDemanglable{};
				return input[pos];
			}

			[[nodiscard]]
			bool peekIs(const char expected) const {
				return !atEnd() && input[pos] == expected;
			}

			[[nodiscard]]
			bool startsWith(const std::string_view expected) const {
				return input.substr(pos).starts_with(expected);
			}

			char take() {
				const char taken = peek();
				++pos;
				return taken;
			}

			void expect(const char expected) {
				if (take() != expected) throw NotDemanglable{};
			}

			void expect(const std::string_view expected) {
				if (!startsWith(expected)) throw NotDemanglable{};
				pos += expected.size();
			}

			static bool isDigit(const char character) {
				return character >= '0' && character <= '9';
			}

			/**
			 * @brief Whether an `<identifier>` starts here. Identifiers always start with their
			 * byte length (or with the punycode marker), which is what lets us tell them apart from
			 * the letters that terminate identifier lists.
			 */
			[[nodiscard]]
			bool atIdentifier() const {
				return !atEnd() && (isDigit(input[pos]) || input[pos] == 'U');
			}

			/**
			 * @brief Whether a `<function>` starts here, i.e. a (possibly modified) function type.
			 */
			[[nodiscard]]
			bool atSignature() const {
				constexpr std::string_view MODIFIERS = "MLNXR";

				usize at = pos;
				while (at < input.size() && MODIFIERS.contains(input[at])) ++at;
				return at < input.size() && input[at] == 'F';
			}

			/**
			 * @brief Whether a `<path-prefix>` starts at `at`. A path prefix is always followed by
			 * an `<identifier>`, which is how it is distinguished from the same letters used as
			 * `<type-modifier>`s and pointer/slice tags.
			 */
			[[nodiscard]]
			bool isPathPrefixAt(const usize at) const {
				if (at >= input.size()) return false;
				const char marker = input[at];
				if (marker != 'M' && marker != 'S' && marker != 'P' && marker != 'R') return false;
				const usize next = at + 1;
				return next < input.size() && (isDigit(input[next]) || input[next] == 'U');
			}

			/**
			 * @brief Reads a `<compact-number>` (base-62 digits terminated by `_`, biased by one).
			 */
			u64 takeCompactNumber() {
				constexpr std::string_view DIGITS
					= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

				if (peekIs('_')) {
					++pos;
					return 0;
				}

				u64  value = 0;
				bool empty = true;
				while (!peekIs('_')) {
					const auto digit = DIGITS.find(peek());
					if (digit == std::string_view::npos) throw NotDemanglable{};
					if (value > std::numeric_limits<u64>::max() / DIGITS.size() - 1)
						throw NotDemanglable{};
					value = value * DIGITS.size() + static_cast<u64>(digit);
					++pos;
					empty = false;
				}
				if (empty) throw NotDemanglable{};
				expect('_');
				return value + 1;
			}

			/**
			 * @brief Reads a `<base-10-number>`.
			 */
			u64 takeBase10Number() {
				u64  value = 0;
				bool empty = true;
				while (!atEnd() && isDigit(input[pos])) {
					if (value > std::numeric_limits<u64>::max() / 10 - 9) throw NotDemanglable{};
					value = value * 10 + static_cast<u64>(input[pos] - '0');
					++pos;
					empty = false;
				}
				if (empty) throw NotDemanglable{};
				return value;
			}

			/**
			 * @brief Reads a raw run of `length` bytes.
			 */
			std::string_view takeRaw(const u64 length) {
				if (length > input.size() - pos) throw NotDemanglable{};
				const std::string_view raw = input.substr(pos, length);
				pos += length;
				return raw;
			}

			/**
			 * @brief Reads an `<identifier>`, i.e. a length-prefixed name.
			 * @note A punycode-encoded identifier is returned as-is: the mangler does not emit any
			 * yet (`@future` in mangling-scheme.md), so there is nothing to decode against.
			 */
			std::string takeIdentifier() {
				if (peekIs('U')) ++pos;
				return std::string(takeRaw(takeBase10Number()));
			}

			// --- names --------------------------------------------------------------------------

			/**
			 * @brief Reads an `<operator-name>` and rebuilds the source spelling of the operator.
			 */
			std::string takeOperatorName() {
				expect('O');
				const char operatoriness = take();
				const auto translit      = takeRaw(takeBase10Number());

				icu::UnicodeString name;
				for (usize at = 0; at < translit.size();) {
					if (translit[at] == 'x') {
						// "x" <hex-codepoint> "_"
						const usize end = translit.find('_', at);
						if (end == std::string_view::npos) throw NotDemanglable{};
						constexpr std::string_view HEX_DIGITS    = "0123456789abcdef";
						constexpr u64              MAX_CODEPOINT = 0x10'FF'FF;

						u64 codepoint = 0;
						for (const char digit: translit.substr(at + 1, end - at - 1)) {
							const auto value = HEX_DIGITS.find(digit);
							if (value == std::string_view::npos) throw NotDemanglable{};
							codepoint = codepoint * HEX_DIGITS.size() + static_cast<u64>(value);
							if (codepoint > MAX_CODEPOINT) throw NotDemanglable{};
						}
						name.append(static_cast<UChar32>(codepoint));
						at = end + 1;
					} else {
						const std::string_view tag   = translit.substr(at, 2);
						const auto             found = std::ranges::find_if(
                            OPERATOR_TAGS, [&](const auto& entry) { return entry.first == tag; }
                        );
						if (found == OPERATOR_TAGS.end()) throw NotDemanglable{};
						name.append(static_cast<UChar32>(found->second));
						at += tag.size();
					}
				}

				switch (operatoriness) {
				case 'i':
					return base::strConcat("operator", name);
				case 'p':
					return base::strConcat("prefix operator", name);
				case 's':
					return base::strConcat("suffix operator", name);
				default:
					throw NotDemanglable{};
				}
			}

			/**
			 * @brief Reads a `<special-symbol-name>` tag and returns its table entry.
			 */
			const SpecialSymbol& takeSpecialSymbolTag() {
				for (const auto& special: SPECIAL_SYMBOLS)
					if (startsWith(special.tag)) {
						pos += special.tag.size();
						return special;
					}
				throw NotDemanglable{};
			}

			/**
			 * @brief Reads an `<unscoped-name>` (or a `<name-prefix>`, which is a subset of it).
			 * @note Only the signature-less special symbols can show up here; the ones carrying a
			 * `<function>` make up a whole `<encoding>` on their own, see takeGeneratedSymbol().
			 */
			std::string takeUnscopedName() {
				if (peekIs('O')) return takeOperatorName();
				if (peekIs('H')) {
					expect('H');
					const SpecialSymbol& special = takeSpecialSymbolTag();
					if (special.has_signature) throw NotDemanglable{};
					expect('E');
					return std::string(special.text);
				}
				return takeIdentifier();
			}

			/**
			 * @brief Reads a `<path-prefix>`, i.e. the package/module/script chain.
			 */
			std::vector<std::string> takePathPrefix() {
				if (!isPathPrefixAt(pos)) throw NotDemanglable{};
				++pos;

				std::vector<std::string> parts;
				do parts.push_back(takeIdentifier());
				while (atIdentifier());
				return parts;
			}

			/**
			 * @brief Reads a `<symbol-name>`, i.e. the enclosing namespaces/classes plus the name.
			 */
			std::vector<std::string> takeSymbolName() {
				std::vector<std::string> parts;
				switch (take()) {
				case 'G':
				case 'S':
					parts.push_back(takeUnscopedName());
					break;
				case 'N':
				case 'M':
					while (!peekIs('E')) parts.push_back(takeUnscopedName());
					expect('E');
					if (parts.empty()) throw NotDemanglable{};
					break;
				default:
					throw NotDemanglable{};
				}
				return parts;
			}

			/**
			 * @brief Reads a `<path>` and renders it as a dotted duckling path.
			 */
			std::string takePath() {
				std::vector<std::string> parts = takePathPrefix();
				for (auto& part: takeSymbolName()) parts.push_back(std::move(part));
				return join(parts, ".");
			}

			// --- types --------------------------------------------------------------------------

			/**
			 * @brief Reads the `<type-modifier>*` prefix of a type, in the order mangler.cpp emits
			 * them, and spells it the way tsh::SymbolType::toString() would.
			 * @note `MP<type>E` is a genuine ambiguity of the scheme - it is both a many-pointer
			 * and a unique pointer - and is read as the many-pointer here.
			 */
			std::string takeTypeModifiers() {
				std::string modifiers;
				if (peekIs('M') && !startsWith("MP")) {
					++pos;
					modifiers += "unique ";
				}
				if (peekIs('L')) {
					++pos;
					modifiers += "leaking ";
				}
				if (peekIs('N')) {
					++pos;
					modifiers += "const ";
				}
				if (peekIs('X')) {
					++pos;
					modifiers += "box ";
				} else if (peekIs('R')) {
					++pos;
					modifiers += "ref ";
				}
				return modifiers;
			}

			/**
			 * @brief Reads a `<type>` and spells it the way tsh::SymbolType::toString() would.
			 */
			std::string takeType() {
				const DepthGuard  guard(depth);
				const std::string modifiers = takeTypeModifiers();

				// Class types embed the whole mangled name of the class symbol.
				if (peekIs('_')) return modifiers + takeMangledName(true);

				switch (take()) {
				case 'u':
					return modifiers + "()";
				case 'v':
					return modifiers + "void";
				case 'y':
					return modifiers + "byte";
				case 'b':
					return modifiers + "bool";
				case 'c':
					return modifiers + "char";
				case 't':
					return modifiers + "type";
				case 'p':
					return modifiers + "raw_pointer";
				case 'i':
					return base::strConcat(modifiers, "i", takeBase10Number());
				case 'j':
					return base::strConcat(modifiers, "u", takeBase10Number());
				case 'f':
					return base::strConcat(modifiers, "f", takeBase10Number());
				case 'P':
					return base::strConcat(modifiers, "ptr ", takeEnclosedType());
				case 'S':
					return base::strConcat(modifiers, "slice ", takeEnclosedType());
				case 'D':
					return base::strConcat(modifiers, "List[", takeEnclosedType(), "]");
				case 'M':
					expect('P');
					return base::strConcat(modifiers, "manyptr ", takeEnclosedType());
				case 'C':
					expect('P');
					return base::strConcat(modifiers, "cptr ", takeEnclosedType());
				case 'A': {
					const auto size = takeBase10Number();
					return base::strConcat(modifiers, takeEnclosedType(), "[", size, "]");
				}
				case 'T':
					return base::strConcat(modifiers, "Tuple(", join(takeTypeList(), ", "), ")");
				case 'V':
					return base::strConcat(modifiers, "Variant(", join(takeTypeList(), ", "), ")");
				case 'F': {
					// <function-type> ::= "F" <return-type> <argument-type>* "E"
					// @TODO: #2255 <function-qualifier>* are not emitted (nor parsed) yet.
					const auto result = takeType();
					return base::strConcat(
						modifiers, "Function(", join(takeTypeList(), ", "), ") -> (", result, ")"
					);
				}
				default:
					throw NotDemanglable{};
				}
			}

			/**
			 * @brief Reads a single `<type> "E"`, the shape every wrapping type tag uses.
			 */
			std::string takeEnclosedType() {
				std::string inner = takeType();
				expect('E');
				return inner;
			}

			/**
			 * @brief Reads a `<type>*` run terminated by `"E"`.
			 */
			std::vector<std::string> takeTypeList() {
				std::vector<std::string> types;
				while (!peekIs('E')) types.push_back(takeType());
				expect('E');
				return types;
			}

			// --- encodings ----------------------------------------------------------------------

			/**
			 * @brief Reads a `<function>`, i.e. the signature that follows a function's path, and
			 * renders it as `(a: i32, b: f64) -> i32`.
			 * @note The function type is mangled as a symbol type, so it can carry modifiers (the
			 * generated methods are `const`). They say nothing about the symbol itself, so they are
			 * consumed and dropped.
			 */
			std::string takeSignature() {
				takeTypeModifiers();
				expect('F');
				// @TODO: #2255 <function-qualifier>* are not emitted (nor parsed) yet.
				const auto result = takeType();
				const auto types  = takeTypeList();

				std::vector<std::string> names;
				while (atIdentifier()) names.push_back(takeIdentifier());
				expect('E');

				std::vector<std::string> parameters;
				parameters.reserve(types.size());
				for (usize index = 0; index < types.size(); index++)
					parameters.push_back(
						index < names.size() ? base::strConcat(names[index], ": ", types[index])
											 : types[index]
					);

				return base::strConcat("(", join(parameters, ", "), ") -> ", result);
			}

			/**
			 * @brief Reads a compiler-generated symbol, i.e. `"H" <tag> <function> "E"`, optionally
			 * owned by the type that precedes it in the mangled name.
			 */
			std::string takeGeneratedSymbol(const std::string_view owner) {
				expect('H');
				const SpecialSymbol& special = takeSpecialSymbolTag();
				if (!special.has_signature) throw NotDemanglable{};

				const auto signature = takeSignature();
				expect('E');

				if (owner.empty()) return base::strConcat(special.text, signature);
				return base::strConcat(owner, ".", special.text, signature);
			}

			/**
			 * @brief Reads an `<encoding>` - everything between the scheme version and the metadata.
			 * @param as_type Whether the encoding is being read as a type (a class type embeds the
			 * whole mangled name of its symbol), in which case the `class` keyword is left out.
			 */
			std::string takeEncoding(const bool as_type) {
				constexpr std::string_view EXPR_WRAPPER  = "__repl_expr_wrapper_";
				constexpr std::string_view INSTR_WRAPPER = "__repl_instr_wrapper_";

				if (startsWith(EXPR_WRAPPER)) {
					pos += EXPR_WRAPPER.size();
					return base::strConcat("<repl expression ", takeBase10Number(), ">");
				}
				if (startsWith(INSTR_WRAPPER)) {
					pos += INSTR_WRAPPER.size();
					return base::strConcat("<repl instruction ", takeBase10Number(), ">");
				}

				// Generated methods and builtins carry no path at all.
				if (peekIs('H')) return takeGeneratedSymbol({});

				// Classes: "C" <path>.
				if (peekIs('C') && isPathPrefixAt(pos + 1)) {
					++pos;
					return as_type ? takePath() : base::strConcat("class ", takePath());
				}

				// Generated constructors: <type> "H" <tag> <function> "E".
				if (!isPathPrefixAt(pos)) {
					const auto owner = takeType();
					return takeGeneratedSymbol(owner);
				}

				// Variables and constants are just a path, functions add their signature.
				std::string name = takePath();
				if (atSignature()) name += takeSignature();

				// The constructor/destructor of a global variable is its own symbol, mangled as the
				// variable's encoding plus a tag (mangler.cpp emits it without the "H" ... "E").
				if (startsWith("gc")) {
					pos += 2;
					name += ".<global constructor>";
				} else if (startsWith("gd")) {
					pos += 2;
					name += ".<global destructor>";
				}

				return name;
			}

			/**
			 * @brief Reads `<language-prefix> <scheme-version> <encoding>`.
			 * @note The scheme version is dropped: it is noise for a human reader, and a name of an
			 * unknown version would not have parsed this far anyway.
			 */
			std::string takeMangledName(const bool as_type = false) {
				expect("_Q");
				takeCompactNumber();
				return takeEncoding(as_type);
			}
		};
	}

	base::Optional<std::string> tryDemangle(const std::string_view mangled_name) {
		// Anything the mangler decided not to mangle (C linkage, `main`, ...) is readable already.
		if (!mangled_name.starts_with("_Q")) return {};

		try {
			Demangler   demangler(mangled_name);
			std::string demangled = demangler.demangleWholeName();
			if (demangled.empty()) return {};
			return demangled;
		} catch (const NotDemanglable&) { return {}; }
	}

	std::string demangle(const std::string_view mangled_name) {
		return tryDemangle(mangled_name).copyValueOr(std::string(mangled_name));
	}
}
