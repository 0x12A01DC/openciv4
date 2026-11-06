#include "Text.h"

#include <algorithm>
#include <cwctype>
#include <format>
#include <vector>

namespace oc4::host
{
	namespace
	{
		std::wstring widenAscii(const char* s)
		{
			std::wstring w;
			for (; s && *s; ++s)
				w.push_back(static_cast<unsigned char>(*s));
			return w;
		}

		// %D shows the sign of positive values too.
		std::wstring formatInt(long long v, wchar_t kind) { return (kind == L'D' && v > 0 ? L"+" : L"") + std::to_wstring(v); }

		struct Placeholder
		{
			size_t begin = 0, end = 0;
			wchar_t kind = 0; // s d D f F
			int index = 0;    // 1-based
			int form = 1;     // %s1:2: the text's second form
		};

		std::vector<Placeholder> scan(const std::wstring& s)
		{
			std::vector<Placeholder> out;
			for (size_t i = 0; i + 2 < s.size() + 1; ++i)
			{
				if (s[i] != L'%' || i + 2 > s.size())
					continue;
				const wchar_t kind = s[i + 1];
				if (kind != L's' && kind != L'd' && kind != L'D' && kind != L'f' && kind != L'F')
					continue;
				size_t j = i + 2;
				int index = 0;
				while (j < s.size() && std::iswdigit(s[j]))
					index = index * 10 + (s[j++] - L'0');
				if (index == 0)
					continue;
				int form = 1;
				if (kind == L's' && j + 1 < s.size() && s[j] == L':' && std::iswdigit(s[j + 1]))
				{
					form = s[j + 1] - L'0';
					j += 2;
				}
				// Optional "_Name" annotation documents the argument: letters, and more words after
				// underscores ("%d3_pop_max").
				if (j < s.size() && s[j] == L'_')
				{
					size_t k = j + 1;
					while (k < s.size() && (std::iswalpha(s[k]) || (s[k] == L'_' && k + 1 < s.size() && std::iswalpha(s[k + 1])) ||
											   (std::iswdigit(s[k]) && k > j + 1 && std::iswalpha(s[k - 1]))))
						++k;
					if (k > j + 1)
						j = k;
				}
				// ... or straight after the number, a word with an underscore in it ("%s1Luxury_Name",
				// "%s1Civ_Name"; a plain word after it is the text's own, "%d1turns").
				else if (j < s.size() && std::iswalpha(s[j]))
				{
					size_t k = j;
					bool underscore = false;
					while (k < s.size() && (std::iswalpha(s[k]) || (s[k] == L'_' && k + 1 < s.size() && std::iswalpha(s[k + 1]))))
						underscore |= s[k++] == L'_';
					if (underscore)
						j = k;
				}
				out.push_back({ i, j, kind, index, form });
				i = j - 1;
			}
			return out;
		}

		// (a text's forms: split at the separator, but not inside a tag, "[PARAGRAPH:1]")
		std::vector<std::wstring_view> split(std::wstring_view s, wchar_t by)
		{
			std::vector<std::wstring_view> parts;
			size_t start = 0;
			int depth = 0;
			for (size_t i = 0; i < s.size(); ++i)
				if (s[i] == L'[')
					++depth;
				else if (s[i] == L']' && depth > 0)
					--depth;
				else if (s[i] == by && depth == 0)
				{
					parts.push_back(s.substr(start, i - start));
					start = i + 1;
				}
			parts.push_back(s.substr(start));
			return parts;
		}
	}

	void TextManager::add(const char* tag, const wchar_t* text, const wchar_t* gender, const wchar_t* plural)
	{
		m_entries[widenAscii(tag)] = { text ? text : L"", gender ? gender : L"N", plural ? plural : L"false" };
	}

	std::wstring TextManager::raw(const std::wstring& tag) const
	{
		const auto it = m_entries.find(tag);
		return it == m_entries.end() ? tag : it->second.text;
	}

	bool TextManager::resolveTag(std::wstring_view tag, std::wstring& out) const
	{
		if (tag == L"NEWLINE")
			out = L"\n";
		else if (tag == L"TAB")
			out = L"\t";
		else if (tag == L"SPACE")
			out = L" ";
		else if (tag == L"COLOR_REVERT")
			out = L"</color>";
		else if (tag == L"\\LINK")
			out = L"</link>";
		else if (tag.starts_with(L"LINK="))
			out = L"<link=" + std::wstring(tag.substr(5)) + L">";
		// The Civilopedia's styles: as the screens' markup.
		else if (tag == L"BOLD")
			out = L"<b>";
		else if (tag == L"\\BOLD")
			out = L"</b>";
		else if (tag == L"ITALIC")
			out = L"<i>";
		else if (tag == L"\\ITALIC")
			out = L"</i>";
		else if (tag == L"H1")
			out = L"<font=4b>";
		else if (tag == L"H2")
			out = L"<font=3b>";
		else if (tag == L"\\H1" || tag == L"\\H2")
			out = L"</font>";
		// A paragraph: a new line, and a blank one before it for [PARAGRAPH:2].
		else if (tag.starts_with(L"PARAGRAPH:"))
			out.assign(tag.ends_with(L'2') ? 2 : tag.ends_with(L'1') ? 1 : 0, L'\n');
		else
			return m_resolver && m_resolver(tag, out);
		return true;
	}

	std::wstring TextManager::expand(std::wstring_view text) const
	{
		std::wstring out;
		out.reserve(text.size());
		for (size_t i = 0; i < text.size(); ++i)
		{
			if (text[i] == L'%' && i + 1 < text.size() && text[i + 1] == L'%')
			{
				out += L'%';
				++i;
				continue;
			}
			if (text[i] == L'[')
				if (const size_t close = text.find(L']', i + 1); close != std::wstring_view::npos)
					if (std::wstring resolved; resolveTag(text.substr(i + 1, close - i - 1), resolved))
					{
						// (a paragraph after a line break is one line less)
						if (text.substr(i + 1).starts_with(L"PARAGRAPH:") && !resolved.empty() && !out.empty() && out.back() == L'\n')
							resolved.pop_back();
						out += resolved;
						i = close;
						continue;
					}
			out += text[i];
		}
		return out;
	}

	size_t TextManager::forms(const std::wstring& tag) const
	{
		const auto it = m_entries.find(tag);
		if (it == m_entries.end() || it->second.text.find(L'%') != std::wstring::npos)
			return 1;
		return split(it->second.text, L':').size();
	}

	std::wstring TextManager::form(const std::wstring& tag, size_t form) const
	{
		const auto it = m_entries.find(tag);
		if (it == m_entries.end())
			return expand(tag);
		std::wstring_view text = it->second.text;
		if (text.find(L':') != std::wstring_view::npos && text.find(L'%') == std::wstring_view::npos)
		{
			const auto all = split(text, L':');
			text = all[form < all.size() ? form : 0];
		}
		return expand(text);
	}

	std::wstring TextManager::argument(const std::wstring& value, int form) const
	{
		// (any key shaped as one, not only TXT_KEY_...: CoM's corporation is CHANGED_TXT_KEY_CORPORATION_2)
		const bool tag = !value.empty() && std::ranges::all_of(value, [](wchar_t c) { return (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_'; });
		if (!tag || !m_entries.contains(value))
			return value;
		return this->form(value, size_t(std::max(form, 1) - 1));
	}

	size_t TextManager::choose(const Value& v, size_t forms) const
	{
		if (forms <= 1)
			return 0;
		// The forms: each gender's singular, then (when there is an even number of them) the
		// same plurals.
		const size_t genders = forms % 2 == 0 ? forms / 2 : forms;
		size_t gender = 0;
		bool plural = false;
		if (v.kind == Value::Kind::Text)
		{
			if (const auto it = m_entries.find(v.s); it != m_entries.end())
			{
				const std::wstring& g = it->second.gender;
				gender = g.starts_with(L'F') || g.starts_with(L'f') ? 1 : g.starts_with(L'N') || g.starts_with(L'n') ? 2 : 0;
				const std::wstring& p = it->second.plural;
				plural = p == L"1" || p == L"true" || p == L"True";
			}
		}
		else
			plural = (v.kind == Value::Kind::Float ? v.f : double(v.i)) != 1 && (v.kind == Value::Kind::Float ? v.f : double(v.i)) != -1;
		const size_t i = std::min(gender, genders - 1) + (plural && forms % 2 == 0 ? genders : 0);
		return std::min(i, forms - 1);
	}

	std::wstring TextManager::format(const std::wstring& tag, const std::vector<Value>& values) const
	{
		const auto it = m_entries.find(tag);
		if (it == m_entries.end())
			return expand(tag);
		return formatText(it->second.text, values);
	}

	std::wstring TextManager::formatText(const std::wstring& entry, const std::vector<Value>& values) const
	{
		const auto value = [&](int index) -> const Value& {
			static const Value none;
			return index >= 1 && size_t(index) < values.size() ? values[size_t(index)] : none;
		};
		// [NUMn:form:form...]: the form for argument n.
		std::wstring text;
		for (size_t i = 0; i < entry.size(); ++i)
		{
			if (entry.compare(i, 4, L"[NUM") == 0)
			{
				size_t j = i + 4;
				int index = 0;
				while (j < entry.size() && std::iswdigit(entry[j]))
					index = index * 10 + (entry[j++] - L'0');
				if (const size_t close = entry.find(L']', j); j < entry.size() && entry[j] == L':' && close != std::wstring::npos)
				{
					const auto forms = split(std::wstring_view(entry).substr(j + 1, close - j - 1), L':');
					text += forms[choose(value(index), forms.size())];
					i = close;
					continue;
				}
			}
			text += entry[i];
		}

		std::wstring out;
		size_t pos = 0;
		for (const auto& p : scan(text))
		{
			out += expand(std::wstring_view(text).substr(pos, p.begin - pos));
			const Value& v = value(p.index);
			switch (v.kind)
			{
			case Value::Kind::Text:
				out += argument(v.s, p.form);
				break;
			case Value::Kind::Int:
			case Value::Kind::Float:
			{
				const long long n = v.kind == Value::Kind::Int ? v.i : static_cast<long long>(v.f);
				if (p.kind == L'F')
				{
					if (n > 0)
						out += static_cast<wchar_t>(n); // a font symbol
				}
				else if (p.kind == L'f' || (p.kind == L's' && v.kind == Value::Kind::Float))
					out += std::format(L"{:.2f}", v.kind == Value::Kind::Float ? v.f : double(n));
				else
					out += formatInt(n, p.kind);
				break;
			}
			case Value::Kind::None:
				break;
			}
			pos = p.end;
		}
		out += expand(std::wstring_view(text).substr(pos));
		return out;
	}

	std::wstring TextManager::get(const std::wstring& tag, va_list args) const
	{
		const auto it = m_entries.find(tag);
		if (it == m_entries.end())
			return expand(tag);

		// Arguments arrive in index order; their types come from the specifiers.
		int maxIndex = 0;
		std::vector<wchar_t> kinds(32, 0);
		for (const auto& p : scan(it->second.text))
			if (p.index < int(kinds.size()))
			{
				kinds[size_t(p.index)] = p.kind;
				maxIndex = std::max(maxIndex, p.index);
			}
		if (maxIndex == 0 && it->second.text.find(L"[NUM") == std::wstring::npos)
			return expand(it->second.text);
		std::vector<Value> values(size_t(maxIndex) + 1);
		va_list ap;
		va_copy(ap, args);
		for (int i = 1; i <= maxIndex; ++i)
		{
			Value& v = values[size_t(i)];
			switch (kinds[size_t(i)])
			{
			case L's':
			{
				const wchar_t* s = va_arg(ap, const wchar_t*);
				v.kind = Value::Kind::Text;
				v.s = s ? s : L"";
				break;
			}
			case L'f':
				v.kind = Value::Kind::Float;
				v.f = va_arg(ap, double);
				break;
			default: // d D F (a character), and one the text does not use: a number
				v.kind = Value::Kind::Int;
				v.i = va_arg(ap, int);
				break;
			}
		}
		va_end(ap);
		return format(tag, values);
	}

	std::wstring TextManager::get(const std::wstring& tag, const std::vector<Arg>& args) const
	{
		return format(tag, values(args));
	}

	std::wstring TextManager::formatRaw(const std::wstring& text, const std::vector<Arg>& args) const
	{
		const auto it = m_entries.find(text);
		return formatText(it != m_entries.end() ? it->second.text : text, values(args));
	}

	std::vector<TextManager::Value> TextManager::values(const std::vector<Arg>& args)
	{
		std::vector<Value> values(args.size() + 1);
		for (size_t i = 0; i < args.size(); ++i)
		{
			Value& v = values[i + 1];
			if (const auto* s = std::get_if<std::wstring>(&args[i]))
			{
				v.kind = Value::Kind::Text;
				v.s = *s;
			}
			else if (const auto* d = std::get_if<double>(&args[i]))
			{
				v.kind = Value::Kind::Float;
				v.f = *d;
			}
			else
			{
				v.kind = Value::Kind::Int;
				v.i = std::get<long long>(args[i]);
			}
		}
		return values;
	}
}
