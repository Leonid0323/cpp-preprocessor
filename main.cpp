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
    return path(data, data + sz);
}

bool PreprocessInner(const path& file_path, istream& input, ostream& output, const vector<path>& include_directories){
    static regex include_quotes(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex include_brackets(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    size_t line_cnt = 0;
    string str;
    while(getline(input, str)){
        ifstream input_internal;
        smatch m1, m2;
        ++line_cnt;
        if(!regex_match(str, m1, include_quotes) && !regex_match(str, m2, include_brackets)){
            output << str << endl;
        }
        else{
            bool is_known = false;
            path p;
            path p1;
            if((regex_match(str, m1, include_quotes))){
                p = file_path.parent_path() / string(m1[1]);
                if (filesystem::exists(p)) {
                    input_internal.open(p);
                    is_known = true;
                } else{
                    for(const path& dir : include_directories){
                        p = dir / string(m1[1]);
                        p1 = string(m1[1]);
                        if (input_internal.is_open()){
                            break;
                        }
                        if (filesystem::exists(p)) {
                            input_internal.open(p);
                            is_known = true;
                        }
                    }
                }
            } else {
                for(const path& dir : include_directories){
                    p = dir / string(m2[1]);
                    p1 = string(m2[1]);
                    if (input_internal.is_open()){
                        break;
                    }
                    else if(filesystem::exists(p)){
                        input_internal.open(p);
                        is_known = true;
                    } 
                }
            }
            if(is_known){
                PreprocessInner(p, input_internal, output, include_directories);
            } else {
            cout << "unknown include file " << p1.string() << " at file " << file_path.string() << " at line " << line_cnt << endl;
                return false;
            }
        }
    }
    return true;
}
 
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories){
    ifstream input(in_file);
    if (!input.is_open()){
        cerr << "Error: Input file not found" << std::endl;
        return false;
    } 
    ofstream output(out_file, ios::out | ios::app);
    if (!output.is_open()){
        cerr << "Error: Output file not found" << std::endl;
        return false;
    }
    return PreprocessInner(in_file, input, output, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
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