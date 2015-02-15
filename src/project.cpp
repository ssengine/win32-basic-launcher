#include <ssengine/ssengine.h>
#include <ssengine/log.h>
#include <ssengine/macros.h>
#include "launcher.h"

#include <atlcomcli.h>
#include <MsXml6.h>

#include <Windows.h>
#include "../res/resource.h"

#define NS_SS_SCHEMA_PROJECT_1 L"http://ssengine.org/schema/prj/1/"

#define NS_SS_SCHEMA_USER_1 L"http://ssengine.org/schema/user/1/"


CComPtr<IXMLDOMSchemaCollection> getSchemaCache(int rid, const wchar_t* ns){
	HRESULT hr;

	CComPtr<IXMLDOMDocument> pSchemaDoc;
	pSchemaDoc.CoCreateInstance(__uuidof(DOMDocument60), NULL, CLSCTX_INPROC_SERVER);
	pSchemaDoc->put_async(VARIANT_FALSE);
	//pSchemaDoc->put_validateOnParse(VARIANT_FALSE);

	HRSRC hrSchema = FindResource(NULL, MAKEINTRESOURCE(rid), MAKEINTRESOURCE(IDT_XML_SCHEMA));
	HGLOBAL hData = LoadResource(NULL, hrSchema);

	size_t sz = SizeofResource(NULL, hrSchema);
	const void* resData = LockResource(hData);

	SAFEARRAY* psa = SafeArrayCreateVector(VT_UI1, 0, sz);
	void* pData;
	SafeArrayAccessData(psa, &pData);
	memcpy(pData, resData, sz);
	SafeArrayUnaccessData(psa);

	VARIANT_BOOL        bIsSuccessful;
	hr = pSchemaDoc->load(CComVariant(psa), &bIsSuccessful);
	SafeArrayDestroy(psa);

	if (hr != S_OK){
		CComPtr<IXMLDOMParseError> error;
		pSchemaDoc->get_parseError(&error);
		CComBSTR reason;
		error->get_reason(&reason);

		//TODO: Display a alert.
		SS_WLOGF(L"Parse schema error: %s", BSTR(reason));
		exit(1);
	}

	CComPtr<IXMLDOMSchemaCollection> pSchemaCache;

	pSchemaCache.CoCreateInstance(__uuidof(XMLSchemaCache60), NULL, CLSCTX_INPROC_SERVER);

	pSchemaCache->add(CComBSTR(ns), CComVariant(pSchemaDoc));

	return pSchemaCache;
}

#define ASSUME_SUCCESS(expr) do{HRESULT hr = expr; if (FAILED(hr)){SS_LOGF("%s (%d): COM call failed: (%x)", __FILE__, __LINE__, hr); exit(1);}}while(0)

void load_xml_text(IXMLDOMNode* root,
	const char* name){
	CComBSTR str;
	ASSUME_SUCCESS(root->get_text(&str));

	char* value = wchar_t2char(str);
	ss_macro_define(name, value);
	free(value);
}

void load_xml_text(IXMLDOMNode* root,
					const char* name, 
					const OLECHAR* path){
	CComPtr<IXMLDOMNode> node;
	ASSUME_SUCCESS(root->selectSingleNode(
		CComBSTR(path),
		&node));
	load_xml_text(node, name);
}

std::string BSTRToString(BSTR bstr){
	char* tmp = wchar_t2char(bstr);
	std::string ret = tmp;
	free(tmp);
	return ret;
}

typedef void(*enum_xml_node_callback)(
		int	id,
		const std::string& name,
		IXMLDOMNode* node
	);

void enum_xml_node(
	IXMLDOMNode* root,
	BSTR path,
	BSTR name_path,
	enum_xml_node_callback callback
	)
{
	CComPtr<IXMLDOMNodeList> nodes;
	ASSUME_SUCCESS(root->selectNodes(path, &nodes));

	long length;
	ASSUME_SUCCESS(nodes->get_length(&length));

	for (long i = 0; i < length; i++){
		CComPtr<IXMLDOMNode> node;
		ASSUME_SUCCESS(nodes->get_item(i, &node));
		CComBSTR strName;

		CComPtr<IXMLDOMNode> nName;
		ASSUME_SUCCESS(node->selectSingleNode(name_path, &nName));
		ASSUME_SUCCESS(nName->get_text(&strName));

		std::string  name = BSTRToString(strName);
		callback(i, name, node);
	}
}

static void emulator_enum_callback(int id, const std::string& name, IXMLDOMNode* node)
{
	if (id == 0 && !ss_macro_isdef("EMULATOR")){
		ss_macro_define("EMULATOR", name.c_str());
	}
	std::string macroName = name;
	macroName.insert(0, "EMULATORS(");
	macroName.append(")");

	load_xml_text(node, (macroName + "(width)").c_str(), L"s:Screen/@width");
	load_xml_text(node, (macroName + "(height)").c_str(), L"s:Screen/@height");
	load_xml_text(node, (macroName + "(desity)").c_str(), L"s:Screen/@density");

	load_xml_text(node, (macroName + "(title)").c_str(), L"s:Title");
}

static void script_enum_callback(int id, const std::string& name, IXMLDOMNode* node)
{
	std::string macroName = name;
	macroName.insert(0, "SCRIPTS(");
	macroName.append(")");
	load_xml_text(node, macroName.c_str());
}

static void load_configure_fom_project(IXMLDOMDocument2* pXMLDoc)
{
	pXMLDoc->setProperty(CComBSTR(L"SelectionLanguage"), CComVariant(L"XPath"));
	pXMLDoc->setProperty(CComBSTR(L"SelectionNamespaces"), CComVariant(L"xmlns:s='" NS_SS_SCHEMA_PROJECT_1 L"'"));

	load_xml_text(pXMLDoc, "PROJECT_NAME", L"/s:Project/s:Name");

	enum_xml_node(pXMLDoc, CComBSTR(L"/s:Project/s:Emulators/s:Emulator"), CComBSTR(L"@name"), emulator_enum_callback);
	enum_xml_node(pXMLDoc, CComBSTR(L"/s:Project/s:Scripts/s:Script"), CComBSTR(L"@name"), script_enum_callback);
}

void load_project(const char* path)
{
	CComPtr<IXMLDOMDocument2> pXMLDoc;
	HRESULT hr = pXMLDoc.CoCreateInstance(__uuidof(DOMDocument60), NULL, CLSCTX_INPROC_SERVER);

	if (hr != S_OK){
		SS_WLOGE(L"Failed to load MSXML6");
		exit(1);
	}

	pXMLDoc->setProperty(CComBSTR(L"ProhibitDTD"), CComVariant(false));
	pXMLDoc->put_async(VARIANT_FALSE);
	pXMLDoc->put_validateOnParse(VARIANT_FALSE);

	//TODO: add XML Schema

	pXMLDoc->putref_schemas(CComVariant(getSchemaCache(IDR_PROJECT_SCHEMA, NS_SS_SCHEMA_PROJECT_1)));
	CComVariant varPath(path);
	VARIANT_BOOL bIsSuccessful;
	hr = pXMLDoc->load(varPath, &bIsSuccessful);

	CComPtr<IXMLDOMParseError> error;
	if (hr != S_OK){
		CComPtr<IXMLDOMParseError> error;
		pXMLDoc->get_parseError(&error);
		CComBSTR reason;
		error->get_reason(&reason);

		//TODO: Display a alert.
		SS_WLOGE(L"Parse error: %s", BSTR(reason));
		exit(1);
	}

	hr = pXMLDoc->validate(&error);

	if (hr != S_OK){
		CComBSTR reason;
		error->get_reason(&reason);

		//TODO: Display a alert.
		SS_WLOGE(L"%s", BSTR(reason));
		exit(1);
	}

	SS_DEBUGT("Project XML loaded.");

	load_configure_fom_project(pXMLDoc);
}


void load_configure_fom_user(IXMLDOMDocument2* pXMLDoc){
	pXMLDoc->setProperty(CComBSTR(L"SelectionLanguage"), CComVariant(L"XPath"));
	pXMLDoc->setProperty(CComBSTR(L"SelectionNamespaces"), CComVariant(L"xmlns:s='" NS_SS_SCHEMA_USER_1 L"'"));

	load_xml_text(pXMLDoc, "EMULATOR", L"/s:UserConfiguation/s:Emulator");
	load_xml_text(pXMLDoc, "RENDERER_TYPE", L"/s:UserConfiguation/s:Renderer");
}

void load_user_configure(const char* path)
{
	CComPtr<IXMLDOMDocument2> pXMLDoc;
	HRESULT hr = pXMLDoc.CoCreateInstance(__uuidof(DOMDocument60), NULL, CLSCTX_INPROC_SERVER);

	if (hr != S_OK){
		SS_WLOGE(L"Failed to load MSXML6");
		return;
	}

	pXMLDoc->setProperty(CComBSTR(L"ProhibitDTD"), CComVariant(false));
	pXMLDoc->put_async(VARIANT_FALSE);
	pXMLDoc->put_validateOnParse(VARIANT_FALSE);

	//TODO: add XML Schema

	pXMLDoc->putref_schemas(CComVariant(getSchemaCache(IDR_USER_SCHEMA, NS_SS_SCHEMA_USER_1)));
	CComVariant varPath(path);
	VARIANT_BOOL bIsSuccessful;
	hr = pXMLDoc->load(varPath, &bIsSuccessful);

	CComPtr<IXMLDOMParseError> error;
	if (hr != S_OK){
		CComPtr<IXMLDOMParseError> error;
		pXMLDoc->get_parseError(&error);
		CComBSTR reason;
		error->get_reason(&reason);

		//TODO: Display a alert.
		SS_WLOGE(L"Parse error: %s", BSTR(reason));
		return;
	}

	hr = pXMLDoc->validate(&error);

	if (hr != S_OK){
		CComBSTR reason;
		error->get_reason(&reason);

		//TODO: Display a alert.
		SS_WLOGE(L"%s", BSTR(reason));
		return;
	}

	SS_DEBUGT("User Configuation XML loaded.");

	load_configure_fom_user(pXMLDoc);
}
