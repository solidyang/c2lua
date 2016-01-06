// c2lua.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <set>
#include <sstream>
extern "C" {
#include <pcre2.h>
};

#pragma comment(lib, "d:/library/pcre/src/Debug/pcre.lib")

void read_file(const char *sFn, std::string &sContent)
{
	FILE *f = fopen(sFn, "r");
	if(NULL == f)
		return;

	fseek(f, 0, SEEK_END);
	unsigned size = ftell(f);
	fseek(f, 0, SEEK_SET);

	sContent.resize(size);
	fread(&sContent[0], 1, size, f);

	if (size >= 3 
		&& (unsigned char)sContent[0] == 0xEF 
		&& (unsigned char)sContent[1] == 0xBB 
		&& (unsigned char)sContent[2] == 0xBF) {
		sContent.erase(0, 3);
	}

	fclose(f);
	f = NULL;
}

struct RegexResult {
	int nFrom, nTo;
};

int find_by_regex(const std::string &src, const char *pattern, std::vector<RegexResult> &result)
{
	int error;
	PCRE2_SIZE erroffset;
	pcre2_compile_context *ccontext = pcre2_compile_context_create(NULL);
	pcre2_set_newline(ccontext, PCRE2_NEWLINE_ANYCRLF);
	int ops = 0;
	if (src.find('\n') != std::string::npos) {
		ops = PCRE2_MULTILINE;
	}	
	/*
	PCRE2_EXP_DEFN pcre2_code * PCRE2_CALL_CONVENTION
		pcre2_compile(PCRE2_SPTR pattern, PCRE2_SIZE patlen, uint32_t options,
		int *errorptr, PCRE2_SIZE *erroroffset, pcre2_compile_context *ccontext);
	*/
	pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern, strlen(pattern), ops, &error, &erroffset, ccontext);
	if (re == NULL) {
		//printf("PCRE compilation failed(%d) at offset %d\n", error, erroffset);
		return 0;
	}

	//rc = pcre2_exec(re, NULL, src, strlen(src), 0, 0, ovector, OVECCOUNT);
	/*
	PCRE2_EXP_DEFN int PCRE2_CALL_CONVENTION
		pcre2_match(const pcre2_code *code, PCRE2_SPTR subject, PCRE2_SIZE length,
		PCRE2_SIZE start_offset, uint32_t options, pcre2_match_data *match_data,
		pcre2_match_context *mcontext);
	*/

	pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);	
	int rc = pcre2_match(re, (PCRE2_SPTR8)src.c_str(), src.size(), 0, 0, match_data, NULL);
	if (rc < 0) 
	{
		//if (rc == PCRE2_ERROR_NOMATCH) 
		//	printf("Sorry, no match ...\n");
		//else 
		//	printf("Matching error %d\n", rc);

		pcre2_code_free(re);
		return 0;
	}

	int oveccount = pcre2_get_ovector_count(match_data);
	PCRE2_SIZE *ovec = pcre2_get_ovector_pointer(match_data);
	result.resize(rc);	
	for (int i = 0; i < rc; i++) {
		result[i].nFrom = ovec[2*i];
		result[i].nTo = ovec[2*i+1];
	}
	
	pcre2_code_free(re);
	pcre2_match_data_free(match_data);
	return rc;
}

int find_char(const char *src, int from, char ch, char pair_l = 0)
{
	int ret = -1;
	int depth = 0;
	for (int i = from; src[i] != 0; ++i)
	{
		if (pair_l && src[i] == pair_l) {
			++depth;
			continue;
		}

		if (src[i] == ch)
		{
			if (depth) {
				--depth;
				continue;
			}

			ret = i;
			break;
		}
	};

	return ret;
}

void erase_by_regex(std::string &src, const char *pattern)
{
	std::vector<RegexResult> result;
	while (1)
	{
		int count = find_by_regex(src, pattern, result);
		if (0 == count) {
			break;
		}

		src.erase(result[0].nFrom, result[0].nTo - result[0].nFrom);
	};
}

void parse_vars(const std::string &line, std::vector<std::pair<std::string, std::string> > &vars)
{
	std::vector<RegexResult> result;
	int count = find_by_regex(line, "(\\w+)\\s+([\\w\\s\\:\\+\\[\\]]+)(?:\\s*,\\s*([\\w\\s\\:\\+\\[\\]]+))*;", result);
	if (count < 3) {
		return;
	}

	std::string t;
	t.assign(line.c_str(), result[1].nFrom, result[1].nTo - result[1].nFrom);

	for (int i = 2; i < count; ++i)
	{
		std::string v;
		v.assign(line.c_str(), result[i].nFrom, result[i].nTo - result[i].nFrom);
		vars.push_back(std::pair<std::string, std::string>(t, v));
	}
}

void parse_enums(const std::string &line, std::vector<std::pair<std::string, std::string> > &enums)
{
	std::vector<RegexResult> result;
	int count = find_by_regex(line, "(\\w+)\\s*(?:=\\s*(\\w+\\s*[<|-|+]*\\s*\\w*\\s*[-|+]*\\s*\\w*)\\s*)*[,]*", result);
	if (count < 2) {
		return;
	}

	std::string t;
	t.assign(line.c_str(), result[1].nFrom, result[1].nTo - result[1].nFrom);

	std::string v;
	if (count > 2) {
		v.assign(line.c_str(), result[2].nFrom, result[2].nTo - result[2].nFrom);
	}

	enums.push_back(std::pair<std::string, std::string>(t, v));
}

static void output(std::string &out, const char *fmt, ...)
{
	char buffer[2048];
	va_list arg;
	va_start(arg, fmt);
	int nRet = vsnprintf(buffer, 2047, fmt, arg);
	va_end(arg);

	out += "\n";
	out += buffer;
}

static void output_test(std::string &out, const char *test_type, std::string &name, int &test_counter, const char *type_name = NULL)
{	
	char tmp[512];
	if (strcmp(test_type, "READ_TEST_Struct") == 0 && type_name) 
	{
		const char *naked_type = type_name;
		const char *ns = strrchr(type_name, ':');
		if (ns) {
			naked_type = ns + 1;
		}
		sprintf(tmp, "%s(%s, %s)", test_type, name.c_str(), naked_type);
	}
	else {
		sprintf(tmp, "%s(%s)", test_type, name.c_str());
	}

	out += (test_counter == 0 ? "\n\t" : "\n\telse ");
	out += tmp;

	++test_counter;
}

void save_file(const char *fn, const std::string &out)
{
	FILE *f = fopen(fn, "w");
	if (f)
	{
		fwrite(out.c_str(), out.size(), 1, f);
		fclose(f);
	}
}

void split_string(const char *str, std::vector<std::string> &v)
{
	std::string t;
	const char *from = NULL;
	for (const char *i = str; ; ++i)
	{
		if (*i == ' ' || *i == '\t' || *i == '\r' || *i == '\n' || *i == 0) 
		{
			if (from) {
				t.assign(from, 0, i - from);
				v.push_back(t);
				from = NULL;
			}
		}
		else if (NULL == from) {
			from = i;
		}

		if (*i == 0) {
			break;
		}
	}
}

bool start_with(std::string str, std::string start) {
	return str.compare(0, start.size(), start) == 0;
}

int read_line(const std::string &src, int from, std::string &line)
{
	int line_end = find_char(src.c_str(), from, '\n');
	if (line_end < 0){
		line_end = src.size();
	}		

	if (line_end > from) {
		std::string line_tmp;
		line_tmp.assign(src.c_str(), from, line_end - from);
		if (find_char(line_tmp.c_str(), 0, '(') >= 0) {
			const char *s = src.c_str() + from;

			int count = 0;
			bool bHasSeg = false;
			while (s) {
				if (*s == '{') {
					count++;
					bHasSeg = true;
				}
				if (*s == '}') {
					count--;
				}

				if ((!bHasSeg && *s == ';') || (bHasSeg && count == 0)) {
					break;
				}
				from++;
				s++;
			}

			line_end = find_char(src.c_str(), from, '\n');
			if (line_end < 0){
				line_end = src.size();
			}

			line.clear();
		}else  {
			line.assign(src.c_str(), from, line_end - from);
		}
	}		
	else {
		line.clear();
	}

	return line_end + 1;
}

struct StructMemberInfo
{
	std::string sName;
	std::string sType;
	std::string sArrayLen;
	std::string sArrayLen1;
	std::string sLuaDataFunc;
	std::string sLen;
	void setType(std::string typ)
	{
		sType = typ;
		sLen = "0";
		if ((sType == "char" && sArrayLen.empty()) || sType == "Sint8") {
			sLuaDataFunc = "GetSint8";
			sLen = "1";
		}
		else if (sType == "Uint8") {
			sLuaDataFunc = "GetUint8";
			sLen = "1";
		}
		else if (sType == "Sint16") {
			sLuaDataFunc = "GetSint16";
			sLen = "2";
		}
		else if (sType == "Uint16" || sType == "DataHead") {
			sLuaDataFunc = "GetUint16";
			sLen = "2";
		}
		else if (sType == "Sint32") {
			sLuaDataFunc = "GetSint32";
			sLen = "4";
		}
		else if (sType == "Uint32") {
			sLuaDataFunc = "GetUint32";
			sLen = "4";
		}
		else if (sType == "float") {
			sLuaDataFunc = "GetFloat";
			sLen = "4";
		}
		else if (sType == "double") {
			sLuaDataFunc = "GetDouble";
			sLen = "4";
		}
		else if (sType == "char" && !sArrayLen.empty()) {
			sLuaDataFunc = "GetString";
		}
		else if (sType == "bool") {
			sLuaDataFunc = "GetBoolean";
			sLen = "1";
		}
		else {
			std::ostringstream o;
			o << "_G.C2Lua.readData(t, \\\"" << sType <<"\\\")";
			sLuaDataFunc = o.str();

			o.str("");
			o << "_G.C2Lua.sizeof(\\\"" << sType <<"\\\")";
			sLen = o.str();
		}
	}
	int getType() const
	{
		int typ = -1;
		bool bArray = !sArrayLen.empty();

		if (sType == "char" || sType == "Sint8") {
			typ = 0;
		}
		else if (sType == "Uint8") {
			typ = 1;
		}
		else if (sType == "Sint16") {
			typ = 2;
		}
		else if (sType == "Uint16" || sType == "DataHead") {
			typ = 3;
		}
		else if (sType == "Sint32") {
			typ = 4;
		}
		else if (sType == "Uint32") {
			typ = 5;
		}
		else if (sType == "float") {
			typ = 6;
		}
		else if (sType == "double") {
			typ = 7;
		}
		else if (sType == "bool") {
			typ = 10;
		}

		if (bArray) {
			if (sType == "char") {
				typ = 8;
			}else {
				typ += 100;
			}
		}
				
		return typ;
	}
};

struct StructInfo
{
	std::string sName, sNamespaceStruct, sNamespace;
	std::vector<StructMemberInfo> vMember;
};

int parse_struct_definition(std::set<std::string> &vHeader, std::vector<StructInfo> &vStructs, const char *sNamespaceStruct, const char *sHeader, const std::set<std::string> &filter_vars)
{
	printf("Export %s in %s to lua...\n", sNamespaceStruct, sHeader);

	// extract struct name
	std::string struct_name = sNamespaceStruct;
	std::string struct_namespace;
	const char *namespace_end = strrchr(sNamespaceStruct, ':');
	if (namespace_end) {
		struct_name.assign(namespace_end+1, 0, sNamespaceStruct + strlen(sNamespaceStruct) - (namespace_end+1));
		struct_namespace.assign(sNamespaceStruct, 0, namespace_end - sNamespaceStruct - 1);
	}
	

	// extract header
	std::string header = sHeader;
	const char *slash = NULL;
	const char *have_include = strstr(sHeader, "/include/");
	if (have_include) {
		slash = strchr(have_include + 10, '/');
	}	
	else {
		slash = strrchr(sHeader, '/');
	}
	if (slash) {
		header.assign(slash+1, 0, sHeader + strlen(sHeader) - (slash+1));
	}
	

	// load header
	std::string src;
	read_file(sHeader, src);

	// lookup struct definition
	std::vector<RegexResult> result;
	char pattern[256];
	_snprintf(pattern, 255, "\\bstruct\\s*(?:SU_LIB_COMMON)?\\s*%s\\s*\\{", struct_name.c_str());	
	int count = find_by_regex(src, pattern, result);
	if (0 == count) {
		_snprintf(pattern, 255, "\\bclass\\s*(?:SU_LIB_COMMON)?\\s*%s\\s*\\{", struct_name.c_str());	
		count = find_by_regex(src, pattern, result);
	}

	if (0 == count) {
		_snprintf(pattern, 255, "\\bstruct\\s*(?:SU_LIB_COMMON)?\\s*%s\\s*:\\s*(?:public|private|protected)?\\s*(?:\\w*::)*(\\w+)\\s*\\{", struct_name.c_str());	
		count = find_by_regex(src, pattern, result);
	}

	if (0 == count) {
		_snprintf(pattern, 255, "\\bclass\\s*(?:SU_LIB_COMMON)?\\s*%s\\s*:\\s*(?:public|private|protected)?\\s*(?:\\w*::)*(\\w+)\\s*\\{", struct_name.c_str());	
		count = find_by_regex(src, pattern, result);
	}

	if (0 == count) {
		printf("Can't find %s definition in %s\r\n", sNamespaceStruct, sHeader);
		return 0;
	}

	int struct_end = find_char(src.c_str(), result[0].nTo + 1, '}', '{');
	if (struct_end < 0)
	{
		printf("Can't find %s definition-end\r\n", pattern);
		return 0;
	}	
	std::string struct_content;
	struct_content.assign(src.c_str() + result[0].nTo, struct_end - 1 - result[0].nTo);

	//struct inherit
	std::string sNamespaceStructInherit;
	std::string sStructInherit;
	if (count > 1) {
		sNamespaceStructInherit.assign(src.c_str(), result[1].nFrom, result[1].nTo - result[1].nFrom);
		const char *namespace_end = strrchr(sNamespaceStructInherit.c_str(), ':');
		if (namespace_end) {
			sStructInherit.assign(namespace_end+1, 0, sNamespaceStruct + strlen(sNamespaceStruct) - (namespace_end+1));
		}
	}

	//vStructs.push_back(struct_name);
	vHeader.insert(header);
	StructInfo si;
	si.sName = struct_name;
	si.sNamespaceStruct = sNamespaceStruct;
	si.sNamespace = struct_namespace;

	// 去除注释
	erase_by_regex(struct_content, "\\/\\*(.*\\s*)+?\\*\\/");
	erase_by_regex(struct_content, "\\/\\/.*");

	// 读入每行并分析
	std::vector<std::pair<std::string, std::string> > vars;
	int line_from = 0;
	std::string line;
	do
	{		
		line_from = read_line(struct_content, line_from, line);
		if (!line.empty()) {
			parse_vars(line, vars);	
		}
	} while (line_from < (int)struct_content.size());

	for (unsigned i = 0; i < vars.size(); ++i)
	{
		StructMemberInfo mi;
		//mi.sType = vars[i].first;
		
		std::string &v = vars[i].second;
		std::string::size_type f = v.find('[');
		if (f != std::string::npos) {
			mi.sName.assign(v.c_str(), 0, f);

			std::string sArrayLen;
			sArrayLen.assign(v.c_str(), f+1, v.size() - f - 2);
			std::string::size_type af = sArrayLen.find(']');
			if (af != std::string::npos)
			{
				mi.sArrayLen.assign(sArrayLen.c_str(), 0, af);
				mi.sArrayLen1.assign(sArrayLen.c_str(), af + 2, sArrayLen.length() - af - 1);
			}
			else
			{
				mi.sArrayLen = sArrayLen;
				mi.sArrayLen1.clear();
			}
		}
		else {
			mi.sName = v;
		}

		mi.setType(vars[i].first);

		if (filter_vars.empty() || filter_vars.find(mi.sName) != filter_vars.end()) {
			si.vMember.push_back(mi);
		}		
	}

	if (!sNamespaceStructInherit.empty()) {
		std::vector<StructInfo>::iterator it = vStructs.begin();
		for (;it != vStructs.end(); ++it) {
			if (it->sNamespaceStruct.compare(sNamespaceStructInherit) == 0
				|| it->sName.compare(sNamespaceStructInherit) == 0) {
				for (unsigned i = 0; i < it->vMember.size(); ++i) {
					si.vMember.push_back(it->vMember[i]);
				}
				break;
			}
		}
	}
	
	vStructs.push_back(si);

/*
	// export to cpp
	//luaSizeofStruct
	output(out, "int luaSizeof%s(lua_State *L_) {", struct_name.c_str());
	output(out, "\tlua_pushinteger(L_, sizeof(%s));", sNamespaceStruct);
	output(out, "\treturn 1;");
	output(out, "}\n");

	//luaReadStruct
	output(out, "int luaRead%s(lua_State *L_) {", struct_name.c_str());
	output(out, "\tC2LuaValue val;");
	output(out, "\t%s *data = (%s*)getC2LuaStructPtr(L_, val, 1);", sNamespaceStruct, sNamespaceStruct);
	output(out, "\tconst char *member = luaL_checkstring(L_, 2);\n");	

	int test_count = 0;
	for (unsigned i = 0; i < vars.size(); ++i)
	{
		if (!filter_vars.empty() && filter_vars.find(vars[i].second) == filter_vars.end()) {
			continue;
		}

		if (vars[i].first == "char" || vars[i].first == "Sint8") {
			output_test(out, "READ_TEST_Sint8", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Uint8") {
			output_test(out, "READ_TEST_Uint8", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Sint16") {
			output_test(out, "READ_TEST_Sint16", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Uint16" || vars[i].first == "DataHead") {
			output_test(out, "READ_TEST_Uint16", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Sint32") {
			output_test(out, "READ_TEST_Sint32", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Uint32") {
			output_test(out, "READ_TEST_Uint32", vars[i].second, test_count);				
		}
		else if (vars[i].first == "float") {
			output_test(out, "READ_TEST_float", vars[i].second, test_count);				
		}
		else if (vars[i].first == "double") {
			output_test(out, "READ_TEST_double", vars[i].second, test_count);				
		}
		else 
		{
			if (vars[i].first.find("[]") != std::string::npos) 
			{
				if (vars[i].first == "char[]") {
					output_test(out, "READ_TEST_String", vars[i].second, test_count);
				}
				else {
					output_test(out, "READ_TEST_Array", vars[i].second, test_count);
				}
			}			
			else {
				output_test(out, "READ_TEST_Struct", vars[i].second, test_count, vars[i].first.c_str());
			}
		}
	}

	output(out, "\treturn val.tolua(L_);\n}\n");

	//luaWriteStruct
	output(out, "int luaWrite%s(lua_State *L_) {", struct_name.c_str());
	output(out, "\tC2LuaValue val;");
	output(out, "\t%s *data = (%s*)getC2LuaStructPtr(L_, val, 1);", sNamespaceStruct, sNamespaceStruct);
	output(out, "\tconst char *member = luaL_checkstring(L_, 2);\n");	

	test_count = 0;
	for (unsigned i = 0; i < vars.size(); ++i)
	{
		if (!filter_vars.empty() && filter_vars.find(vars[i].second) == filter_vars.end()) {
			continue;
		}

		if (vars[i].first == "char" || vars[i].first == "Sint8") {
			output_test(out, "WRITE_TEST_Sint8", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Uint8") {
			output_test(out, "WRITE_TEST_Uint8", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Sint16") {
			output_test(out, "WRITE_TEST_Sint16", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Uint16" || vars[i].first == "DataHead") {
			output_test(out, "WRITE_TEST_Uint16", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Sint32") {
			output_test(out, "WRITE_TEST_Sint32", vars[i].second, test_count);				
		}
		else if (vars[i].first == "Uint32") {
			output_test(out, "WRITE_TEST_Uint32", vars[i].second, test_count);				
		}
		else if (vars[i].first == "float") {
			output_test(out, "WRITE_TEST_float", vars[i].second, test_count);				
		}
		else if (vars[i].first == "double") {
			output_test(out, "WRITE_TEST_double", vars[i].second, test_count);				
		}
		else if (vars[i].first == "char[]") {
			output_test(out, "WRITE_TEST_String", vars[i].second, test_count);
		}
	}

	output(out, "\tval.toc();");
	output(out, "\treturn 0;\n}\n");
*/
	return 0;
}

struct EnumMemberInfo
{
	std::string sName;
	std::string sExpression;
	int nType;
};

struct EnumInfo
{
	std::string sName, sNamespaceEnum, sNamespace;
	std::vector<EnumMemberInfo> vMember;
};

int parse_enum_definition(std::set<std::string> &vHeader, std::vector<EnumInfo> &vEnums, const char *sNamespaceEnum, const char *sHeader)
{
	printf("Export %s in %s to lua...\n", sNamespaceEnum, sHeader);

	// extract struct name
	std::string enum_name = sNamespaceEnum;
	std::string enum_namespace;
	const char *namespace_end = strrchr(sNamespaceEnum, ':');
	if (namespace_end) {
		enum_name.assign(namespace_end+1, 0, sNamespaceEnum + strlen(sNamespaceEnum) - (namespace_end+1));
		enum_namespace.assign(sNamespaceEnum, 0, namespace_end - sNamespaceEnum - 1);
	}


	// extract header
	std::string header = sHeader;
	const char *slash = NULL;
	const char *have_include = strstr(sHeader, "/include/");
	if (have_include) {
		slash = strchr(have_include + 10, '/');
	}	
	else {
		slash = strrchr(sHeader, '/');
	}
	if (slash) {
		header.assign(slash+1, 0, sHeader + strlen(sHeader) - (slash+1));
	}


	// load header
	std::string src;
	read_file(sHeader, src);

	// lookup struct definition
	std::vector<RegexResult> result;
	char pattern[256];
	_snprintf(pattern, 255, "\\benum\\s*%s\\s*\\{", enum_name.c_str());	
	int count = find_by_regex(src, pattern, result);
	if (0 == count) {
		printf("Can't find %s definition in %s\r\n", sNamespaceEnum, sHeader);
		return 0;
	}

	int struct_end = find_char(src.c_str(), result[0].nTo + 1, '}', '{');
	if (struct_end < 0)
	{
		printf("Can't find %s definition-end\r\n", pattern);
		return 0;
	}	
	std::string struct_content;
	struct_content.assign(src.c_str() + result[0].nTo, struct_end - 1 - result[0].nTo);

	//vStructs.push_back(struct_name);
	vHeader.insert(header);
	EnumInfo ei;
	ei.sName = enum_name;
	ei.sNamespaceEnum = sNamespaceEnum;
	ei.sNamespace = enum_namespace;

	// 去除注释
	erase_by_regex(struct_content, "\\/\\*(.*\\s*)+?\\*\\/");
	erase_by_regex(struct_content, "\\/\\/.*");

	// 读入每行并分析
	std::vector<std::pair<std::string, std::string> > enums;
	int line_from = 0;
	std::string line;
	do
	{		
		line_from = read_line(struct_content, line_from, line);
		if (!line.empty()) {
			parse_enums(line, enums);	
		}
	} while (line_from < (int)struct_content.size());

	for (unsigned i = 0; i < enums.size(); ++i)
	{
		EnumMemberInfo mi;
		//mi.sType = vars[i].first;

		std::string &v = enums[i].second;
		mi.sName = enums[i].first;
		mi.sExpression = enums[i].second;
		mi.nType = 0;

		if (!mi.sExpression.empty()) {
			std::vector<RegexResult> r;
			int count = find_by_regex(mi.sExpression, "^[0-9]+$", r);
			if (0 == count) {
				mi.nType = 1;
			} else {
				mi.nType = 2;
			}
		}

		ei.vMember.push_back(mi);
	}

	vEnums.push_back(ei);

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2) {
		printf("Usage: c2lua.exe /path/to/c2lua_list.txt\r\n");
		printf("\t c2lua_list.txt content should like this:\n");
		printf("\t ../../include/summer/common/nmsgdefine.h summer::ActTimeRange\n");
		printf("\t ../../include/summer/common/nmsgother.h summer::PlayerData nDBID szRoleName nIcon\n");
		printf("\t ../../include/summer/common/nmsgother.h summer::GMM2C_Login data nMipVip marketingOpen marketingShow peakPVPRestHour\n");
		printf("\t ../../include/summer/common/nmsgguild.h summer::GMM2C_Guild_Announce sAnnoucement\n");
		return 0;
	}

	std::set<std::string> headers;		//用到的头文件列表
	std::vector<StructInfo> structs;	//导出的结构名列表
	std::vector<EnumInfo> enums;      //导出的枚举列表
	// for each-line in export_list.c2lua 
	std::string export_list_src;
	read_file(argv[1], export_list_src);

	int line_from = 0;
	std::string line;
	do
	{		
		line_from = read_line(export_list_src, line_from, line);
		if (!line.empty()) 
		{
			std::vector<std::string> strs;
			split_string(line.c_str(), strs);		
			if (strs.size() >= 2)
			{
				if (start_with(line, "enum:"))
				{
					parse_enum_definition(headers, enums, strs[1].c_str(), strs[0].substr(5).c_str());
				} 
				else {
					std::set<std::string> filter;
					for (unsigned i = 2; i < strs.size(); ++i) {
						filter.insert(strs[i]);
					}
					parse_struct_definition(headers, structs, strs[1].c_str(), strs[0].c_str(), filter);
				}
				
			}
		}
	} while (line_from < (int)export_list_src.size());

	
	// export to cpp
	// head
	std::string out = "//This file is auto-generated by c2lua.\n//Don't modify anything manually!\n";
	output(out, "#include \"stdafx.h\"");
	output(out, "#include <kernel/logger.h>");
	output(out, "#include <kernel/autofile.h>");
	{
		std::set<std::string>::iterator i = headers.begin(), iend = headers.end();
		for (; i != iend; ++i) {
			output(out, "#include \"%s\"", i->c_str());
		}
	}	
	output(out, "\nusing namespace new3d::kernel;");
	output(out, "using namespace summer;\n");

	output(out, "static void output(std::string &out, const char *fmt, ...) {");
	output(out, "\tchar buffer[2048];");
	output(out, "\tva_list arg;");
	output(out, "\tva_start(arg, fmt);");
	output(out, "\tint nRet = vsnprintf(buffer, 2047, fmt, arg);");
	output(out, "\tva_end(arg);");
	output(out, "\tout += \"\\r\\n\";");
	output(out, "\tout += buffer;");
	output(out, "}\n");

	output(out, "void output_c2luaimpl_lua() {");
	output(out, "\tstd::string out;");
	// for each struct.
	for (unsigned i = 0; i < structs.size(); ++i)
	{
		const StructInfo &si = structs[i];		
		output(out, "\tout += \"function C2Lua.Sizeof%s()\";", si.sName.c_str());
		output(out, "\toutput(out, \"\\treturn %%d;\\n\", sizeof(%s));", si.sNamespaceStruct.c_str());
		output(out, "\tout += \"end;\\r\\n\\r\\n\";\n");

		output(out, "\tout += \"function C2Lua.Offset%s(sVarName)\\r\\n\";", si.sName.c_str());
		output(out, "\tout += \"\\tlocal typ, off = 0;\\r\\n\";");
		for (unsigned j = 0; j < si.vMember.size(); ++j)
		{
			const StructMemberInfo &mi = si.vMember[j];
			if (0 == j) {
				output(out, "\tout += \"\\tif \\\"%s\\\" == sVarName then\\r\\n\";", mi.sName.c_str());
			}
			else {
				output(out, "\tout += \"\\telseif \\\"%s\\\" == sVarName then\\r\\n\";", mi.sName.c_str());
			}

			int typ = mi.getType();
			if (typ >= 0) {
				output(out, "\tout += \"\\t\\ttyp = %d;\";", typ);
			}
			else {
				output(out, "\tout += \"\\t\\ttyp = \\\"%s\\\";\";", mi.sType.c_str());
			}			
			output(out, "\toutput(out, \"\\t\\toff = %%d;\\r\\n\", (int)&((%s*)0)->%s);", si.sNamespaceStruct.c_str(), mi.sName.c_str());			
		}
		output(out, "\tout += \"\\tend;\\r\\n\";");
		output(out, "\tout += \"\\treturn typ, off;\\r\\n\";");
		output(out, "\tout += \"end;\\r\\n\\r\\n\";\n");


		output(out, "\tout += \"function C2Lua.GetArrayData%s(buff, offset, sVarName)\\r\\n\";", si.sName.c_str());
		output(out, "\tout += \"\\tlocal ret = {};\\r\\n\";");
		unsigned count = 0;
		for (unsigned j = 0; j < si.vMember.size(); ++j)
		{
			const StructMemberInfo &mi = si.vMember[j];
			if (mi.sArrayLen.empty())
				continue;

			if (0 == count) {
				output(out, "\tout += \"\\tif \\\"%s\\\" == sVarName then\\r\\n\";", mi.sName.c_str());
			}
			else {
				output(out, "\tout += \"\\telseif \\\"%s\\\" == sVarName then\\r\\n\";", mi.sName.c_str());
			}

			if (mi.getType() == 8)
			{
				if (mi.sArrayLen1.empty()) {
					output(out, "\tout += \"\\t\\treturn buff:%s(offset);\\r\\n\";", mi.sLuaDataFunc.c_str());
				}
				else {
					output(out, "\toutput(out, \"\\t\\tfor i = 0,%%d do\\r\\n\", %s - 1);", mi.sArrayLen.c_str());
					output(out, "\toutput(out, \"\\t\\t\\t\\tret[i] = buff:%s(offset + i*%%d);\\r\\n\", %s);", mi.sLuaDataFunc.c_str(), mi.sArrayLen1.c_str());
					output(out, "\tout += \"\\t\\tend;\\r\\n\";");
				}
			
			}else {
				output(out, "\toutput(out, \"\\t\\tfor i = 0,%%d do\\r\\n\", %s - 1);", mi.sArrayLen.c_str());
				if (mi.sArrayLen1.empty()) {
					std::string::size_type pos = 0;
					if ((pos = mi.sLuaDataFunc.find("_G.C2Lua", pos)) != std::string::npos) {
						output(out, "\tout += \"\\t\\t\\tret[i] = C2Lua.locate(buff, offset + i*%s, \\\"%s\\\");\\r\\n\";", mi.sLen.c_str(), mi.sType.c_str());
					} else {
						output(out, "\tout += \"\\t\\t\\tret[i] = buff:%s(offset + i*%s);\\r\\n\";", mi.sLuaDataFunc.c_str(), mi.sLen.c_str());
					}
				} else {
					output(out, "\tout += \"\\t\\t\\tlocal item = {};\\r\\n\";");
					output(out, "\toutput(out, \"\\t\\t\\tfor j = 0,%%d do\\r\\n\", %s - 1);", mi.sArrayLen1.c_str());
					std::string::size_type pos = 0;
					if ((pos = mi.sLuaDataFunc.find("_G.C2Lua", pos)) != std::string::npos) {
						output(out, "\toutput(out, \"\\t\\t\\t\\titem[i] = C2Lua.locate(buff, offset + i*%%d + j*%s, \\\" %s\\\");\\r\\n\", sizeof(((%s*)0)->%s[0]));", mi.sLen.c_str(), mi.sType.c_str(), si.sNamespaceStruct.c_str(), mi.sName.c_str());
					} else {
						output(out, "\toutput(out, \"\\t\\t\\t\\titem[i] = buff:%s(offset + i*%%d + j*%s);\\r\\n\", sizeof(((%s*)0)->%s[0]));", mi.sLuaDataFunc.c_str(), mi.sLen.c_str(), si.sNamespaceStruct.c_str(), mi.sName.c_str());
					}
					output(out, "\tout += \"\\t\\t\\tend;\\r\\n\";");
					output(out, "\tout += \"\\t\\t\\tret[i] = item;\\r\\n\";");
				}
				output(out, "\tout += \"\\t\\tend;\\r\\n\";");
			}

			count++;
		}
		if (count) {
			output(out, "\tout += \"\\telse\\r\\n\";");
			output(out, "\tout += \"\\t\\t_G.LogInfo(sVarName..\\\" is not a array in %s\\\");\\r\\n\";", si.sName.c_str());
			output(out, "\tout += \"\\tend;\\r\\n\";");
		}

		output(out, "\tout += \"\\treturn ret;\\r\\n\";");
		output(out, "\tout += \"end;\\r\\n\\r\\n\";\n");
	}

	// for each enums.
	output(out, "\tint nIndex;");
	for (unsigned i = 0; i < enums.size(); ++i)
	{
		const EnumInfo &ei = enums[i];
		output(out, "\tnIndex = -1;");
		output(out, "\tout += \"\\r\\n\\r\\n\";");
		output(out, "\tout += \"%s = {};\";", ei.sName.c_str());
		for (unsigned j = 0; j < ei.vMember.size(); ++j) {
			const EnumMemberInfo &mi = ei.vMember[j];
			if (mi.nType) {
				output(out, "\tnIndex = %s;", mi.sExpression.c_str());
			} else {
				output(out, "\tnIndex++;");
			}
			output(out, "\toutput(out, \"%s.%s = %%d;\", nIndex);", ei.sName.c_str(), mi.sName.c_str());
		}
		output(out, "");
	}

	output(out, "");
	output(out, "\tAutoFile f(\"/script/client/util/c2luaimpl.lua\", L3_O_WRITE);");
	output(out, "\tif (f) {");
	output(out, "\t\tf->Write(out.c_str(), out.size());");
	output(out, "\t}");
	output(out, "}");
	/*
	//luaRegStruct
	output(out, "void luaRegC2Lua(lua_State *L_) {");
	output(out, "\tconst luaL_Reg funcs[] = {");
	for (unsigned i = 0; i < structs.size(); ++i)
	{
		output(out, "\t\t{\"Sizeof%s\", &luaSizeof%s},", structs[i].c_str(), structs[i].c_str());
		output(out, "\t\t{\"Read%s\", &luaRead%s},", structs[i].c_str(), structs[i].c_str());
		output(out, "\t\t{\"Write%s\", &luaWrite%s},", structs[i].c_str(), structs[i].c_str());
	}	
	output(out, "\t\t{NULL, NULL},");
	output(out, "\t};");
	output(out, "\tluaL_register(L_, \"_G\", funcs);");
	output(out, "}\n");
	*/
	save_file("c2luaimpl.cpp", out);
	return 0;
}