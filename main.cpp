#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return {data, data + sz};
}

void PrintIncludeError(const string& include_file, const path& file_path, size_t line_number) {
    cout << "unknown include file " << include_file
         << " at file " << file_path.string()
         << " at line " << line_number + 1 << endl;
}

bool ProcessFile(const path& file_path, ostream& out, const vector<path>& include_directories, size_t& line_number) {
    ifstream input(file_path);
    if (!input.is_open()) {
        return false;
    }

    string line;
    regex include_quoted(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    regex include_angle(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch match;

    for (string current_line; getline(input, current_line); ++line_number) {
        if (regex_match(current_line, match, include_quoted)) {
            string include_file = match[1];
            path include_path = file_path.parent_path() / include_file;

            if (!filesystem::exists(include_path)) {
                bool found = false;
                for (const path& dir : include_directories) {
                    path try_path = dir / include_file;
                    if (filesystem::exists(try_path)) {
                        include_path = try_path;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    PrintIncludeError(include_file, file_path, line_number);
                    return false;
                }
            }

            size_t nested_line_number = 0;
            if (!ProcessFile(include_path, out, include_directories, nested_line_number)) {
                return false;
            }

        } else if (regex_match(current_line, match, include_angle)) {
            string include_file = match[1];
            path include_path;
            bool found = false;
            for (const path& dir : include_directories) {
                path try_path = dir / include_file;
                if (filesystem::exists(try_path)) {
                    include_path = try_path;
                    found = true;
                    break;
                }
            }
            if (!found) {
                PrintIncludeError(include_file, file_path, line_number);
                return false;
            }

            size_t nested_line_number = 0;
            if (!ProcessFile(include_path, out, include_directories, nested_line_number)) {
                return false;
            }

        } else {
            out << current_line << '\n';
        }
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream test_open(in_file);
    if (!test_open.is_open()) {
        return false;
    }
    test_open.close();

    ofstream out(out_file);
    if (!out.is_open()) {
        return false;
    }

    size_t line_number = 0;
    return ProcessFile(in_file, out, include_directories, line_number);
}

string GetFileContents(const string& file) {
    ifstream stream(file);

    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                        {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
