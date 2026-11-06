#pragma once

// Civ4 keeps its translated text in the engine: the DLL feeds it entries while
// loading xml/text/*.xml (addText) and asks for formatted strings (getText).

#include <cstdarg>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace oc4::host
{
	class TextManager
	{
	public:
		void add(const char* tag, const wchar_t* text, const wchar_t* gender, const wchar_t* plural);

		// Replaces %s1 / %d1 / %D1 / %F1 ... (optionally followed by a _Name hint) with the
		// numbered arguments, whose types follow from the specifiers: %F is a character (a font
		// symbol), %D a number with its sign. Arguments that are text keys (TXT_KEY_..., as the
		// DLL passes a unit's or a civilization's) become their text: its first grammatical
		// form ("American:Americans"), or the one %s1:2 asks for. [NUM1:Turn:Turns] is the
		// form for argument 1: by its number (singular, plural), or for a text by its gender
		// and number (the forms are masculine, feminine[, neuter], then the same plural).
		// Tags are resolved (expand). A tag that is no entry comes back expanded.
		std::wstring get(const std::wstring& tag, va_list args) const;
		// Same with typed arguments (CyTranslator.getText from Python).
		using Arg = std::variant<long long, double, std::wstring>;
		std::wstring get(const std::wstring& tag, const std::vector<Arg>& args) const;
		// A text key's text, or a text that is no entry, formatted the same way (a leader's
		// words in diplomacy: the script picks their key, the engine puts the arguments in).
		std::wstring formatRaw(const std::wstring& text, const std::vector<Arg>& args) const;
		std::wstring raw(const std::wstring& tag) const;
		// An entry's grammatical forms ("Warrior:Warriors"; one if it has a placeholder) and
		// form `form` of them (from 0; the first if there are fewer), expanded.
		size_t forms(const std::wstring& tag) const;
		std::wstring form(const std::wstring& tag, size_t form) const;

		// The markup the original engine resolves in its texts: [NEWLINE], [TAB], [SPACE],
		// [LINK=x]...[\LINK], [COLOR_REVERT], "%%" for a percent sign, and through the resolver
		// the rest ([COLOR_x], [ICON_x]). Unknown tags stay.
		std::wstring expand(std::wstring_view text) const;
		void setTagResolver(std::function<bool(std::wstring_view tag, std::wstring& out)> resolver) { m_resolver = std::move(resolver); }
		bool has(const std::wstring& tag) const { return m_entries.contains(tag); }
		size_t size() const { return m_entries.size(); }

	private:
		// An argument of a text (values[0] is unused: they count from 1).
		struct Value
		{
			enum class Kind : uint8_t
			{
				None, Int, Float, Text
			};
			Kind kind = Kind::None;
			long long i = 0;
			double f = 0;
			std::wstring s;
		};
		std::wstring format(const std::wstring& tag, const std::vector<Value>& values) const;
		std::wstring formatText(const std::wstring& text, const std::vector<Value>& values) const;
		static std::vector<Value> values(const std::vector<Arg>& args);
		// Form 1.. of a text key's text (the first if it has fewer); other values as they are.
		std::wstring argument(const std::wstring& value, int form = 1) const;
		// Which of `forms` a [NUMn:...] tag shows for the value.
		size_t choose(const Value& v, size_t forms) const;
		bool resolveTag(std::wstring_view tag, std::wstring& out) const;

		struct Entry
		{
			std::wstring text, gender, plural;
		};
		std::unordered_map<std::wstring, Entry> m_entries;
		std::function<bool(std::wstring_view tag, std::wstring& out)> m_resolver;
	};
}
