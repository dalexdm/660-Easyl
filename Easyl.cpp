#include <maya\MFnPlugin.h>
#include <maya\MGlobal.h>
#include "paintContextCmd.h"

//////////////////////////////////////////////
// plugin initialization
//////////////////////////////////////////////
MStatus initializePlugin(MObject obj)
{
	MStatus		status;
	MFnPlugin	plugin(obj, PLUGIN_COMPANY, "12.0", "Any");

	//auto-load the UI path and add it to MAYA_SCRIPT_PATH
	MString pathPrefix = plugin.loadPath();
	pathPrefix += "/mel";
	MGlobal::executeCommand("string $paat = \"" + pathPrefix + "\";");
	MGlobal::executeCommand("string $compat = $paat + \";\" + getenv(\"MAYA_SCRIPT_PATH\");");
	MGlobal::executeCommand("putenv \"MAYA_SCRIPT_PATH\" $compat;");

	status = plugin.registerContextCommand("paintContext",
		paintContextCmd::creator);
	status = plugin.registerUI("EasylUICreator", "EasylUIDestroyer");

	return status;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus		status;
	MFnPlugin	plugin(obj);

	status = plugin.deregisterContextCommand("paintContext");

	return status;
}
