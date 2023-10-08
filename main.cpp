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

path operator""_p(const char* data, std::size_t sz) 
{
    return path(data, data + sz);
}

const regex LOCAL(R"(\s*#\s*include\s*\"([^"]*)\"\s*)");
const regex GLOBAL(R"(\s*#\s*include\s*<([^>]*)>\s*)");
smatch m;

bool SomePreprocessor(const path& file, vector<string>& alllines, const vector<path>& include_directories) 
{
    int line_nubmer = 0;

    ifstream instream(file, ios::in | ios::binary);
    if (!instream) 
    {
        return false;
    }

    while (instream) 
    {
        string line;

        if (!getline(instream, line))
        {
            break;
        }

        ++line_nubmer;
        if (regex_match(line, m, LOCAL)) {
            path p1 = file.parent_path().string() + "/" + string(m[1]);

            if (!SomePreprocessor(p1, alllines, include_directories)) 
            {
                int dif_path = static_cast<int>(include_directories.size());
                int checked_path = 0;
                for (auto inc : include_directories) 
                {
                    path p = inc.string() + "/" + string(m[1]);
                    if (!SomePreprocessor(p, alllines, include_directories)) 
                    {
                        ++checked_path;
                    }
                    if (checked_path == dif_path) 
                    {
                        cout << "unknown include file " << m[1] << " at file " << file.string() << " at line " << line_nubmer << endl;
                        return false;
                    }
                }
            }
        }
        else if (regex_match(line, m, GLOBAL)) 
        {
            int dif_path = static_cast<int>(include_directories.size());
            int checked_path = 0;
            for (auto inc : include_directories) {
                path p = inc.string() + "/" + string(m[1]);

                if (!SomePreprocessor(p, alllines, include_directories)) 
                {
                    ++checked_path;
                }
                if (checked_path == dif_path) 
                {
                    cout << "unknown include file " << m[1] << " at file " << file.string() << " at line " << line_nubmer << endl;
                    return false;
                }
            }

        }
        else if (!instream.eof() || !line.empty())
        {
            alllines.push_back(line);
        }
    }

    return true;
}

void VectorToFile(const path& out_file, vector<string>& lines) 
{
    ofstream ostr(out_file, ios::out | ios::binary);
    if (!ostr) 
    {
        return;
    }

    for (auto r : lines) 
    {
        ostr << r << endl;
    }

}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) 
{

    ifstream istr(in_file, ios::in | ios::binary);
    if (!istr) 
    {
        return false;
    }

    vector<string> result;

    if (!SomePreprocessor(in_file, result, include_directories)) 
    {
        VectorToFile(out_file, result);
        return false;
    }

    VectorToFile(out_file, result);

    return true;
}

string GetFileContents(string file) 
{
    ifstream stream(file);

    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() 
{
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
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

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

int main() 
{
    Test();
}
