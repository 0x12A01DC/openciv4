// openciv4: the Windows tools' wmain as a program entry point off Windows.
#include <windows.h>

#include <string>
#include <vector>

int wmain(int argc, wchar_t** argv);

int main(int argc, char** argv)
{
	std::vector<std::wstring> args;
	std::vector<wchar_t*> wide;
	for (int i = 0; i < argc; ++i)
	{
		const int length = static_cast<int>(std::strlen(argv[i]));
		std::wstring arg(length + 1, L'\0');
		arg.resize(length ? MultiByteToWideChar(CP_UTF8, 0, argv[i], length, arg.data(), length + 1) : 0);
		args.push_back(std::move(arg));
	}
	for (std::wstring& arg : args)
		wide.push_back(arg.data());
	wide.push_back(nullptr);
	return wmain(argc, wide.data());
}
