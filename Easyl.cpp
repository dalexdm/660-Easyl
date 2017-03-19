#include <maya/MIOStream.h>
#include <math.h>
#include <stdlib.h>

#include <maya/MFnPlugin.h>
#include <maya/MString.h>
#include <maya/MGlobal.h>
#include <maya/M3dView.h>
#include <maya/MDagPath.h>
#include <maya/MPointArray.h>
#include <maya/MItDag.h>
#include <maya/MFnMesh.h>
#include <vector>

#include <maya/MItSelectionList.h>
#include <maya/MSelectionList.h>

#include <maya/MPxContextCommand.h>
#include <maya/MPxContext.h>
#include <maya/MEvent.h>

#include <maya/MUIDrawManager.h>
#include <maya/MFrameContext.h>
#include <maya/MPoint.h>
#include <maya/MColor.h>

#define kStartLevelSetFlag "-r"
#define kStartLevelSetFlagLong "-root"

//////////////////////////////////////////////
// The user Context
//////////////////////////////////////////////
const char helpString[] =
			"Drag with the left mouse button to paint";
const float kMaxError = 0.1;

class paintContext : public MPxContext
{
public:
	paintContext();
	virtual void	toolOnSetup( MEvent & event );

	// Catch-all methods for use in any viewport renderer - each will be routed to their 'common' method
	virtual MStatus	doPress( MEvent & event );
	virtual MStatus	doDrag( MEvent & event );
	virtual MStatus	doRelease( MEvent & event );
	virtual MStatus	doPress ( MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doRelease( MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doDrag ( MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);

	//tool Settings methods
	void setStartLevel(int level);
	int getStartLevel() { return startLevel; };

	//temporary
	bool isFeather = false;

private:
	// Press and Release shared operations (agnostic of viewport)
	void doPressCommon( MEvent & event );
	void doReleaseCommon( MEvent & event );
	void makeCurve();
	// Temporary vector abstractions
	std::vector<MPoint> origins;
	std::vector<MVector> directions;
	std::vector<float> tValues;
	//Temporary screen locations
	short lastx, lasty;
	int startLevel;
	// screen space object
	M3dView view;
};

paintContext::paintContext()
{
	setTitleString ( "Easyl" );
	startLevel = 0;
	// Tell the context which XPM to use so the tool can properly
	// be a candidate for the 6th position on the toolbar.
	setImage("Easyl.xpm", MPxContext::kImage1 );

}

void paintContext::toolOnSetup ( MEvent & )
{
	setHelpString( helpString );
}

void paintContext::doPressCommon( MEvent & event )
{
	//beginning new line; remove all previous temp data
	origins.clear();
	directions.clear();
	tValues.clear();

	// Extract the event information
	short x, y;
	event.getPosition(x, y);
	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);
	lastx = x; lasty = y;

	origins.push_back(newOrg);
	directions.push_back(newDir);
	tValues.push_back(0);

	MGlobal::displayInfo("Started");
}

void paintContext::makeCurve() {

	MStatus s;

	//get the mesh
	MItDag itr(MItDag::kDepthFirst, MFn::kMesh);
	MObject meshObj = itr.item();

	//currently operates ONLY on the first mesh it finds
	MFnMesh mesh(meshObj, &s);

	if (!isFeather) {
		//determine t values for every i
		for (int i = 0; i < origins.size(); i++) {
			float error = 10000;
			float lastError = 10000;

			//check if we can rely on a converging error
			if (mesh.intersect(origins[i], directions[i], MPointArray(), &s)) {
				while (true) {
					MPoint closestPoint;
					MPoint thisPoint = origins[i] + tValues[i] * directions[i];
					mesh.getClosestPoint(thisPoint, closestPoint);
					error = thisPoint.distanceTo(closestPoint);
					if (error < kMaxError) break;
					tValues[i] += error;
				}
				//if not get near the inflection - REALLY raw right now
			}
			else {
				while (true) {
					MPoint closestPoint;
					MPoint thisPoint = origins[i] + tValues[i] * directions[i];
					mesh.getClosestPoint(thisPoint, closestPoint);
					error = thisPoint.distanceTo(closestPoint);
					if (error > lastError) {
						break;
					}
					tValues[i] += error;
					lastError = error;

				}
			}
		}

	//temporary feather demo - will be replaced by optimization method and custom tool options
	} else {
		//determine t values for first and last i
		for (int i = 0; i < origins.size(); i+=origins.size() - 1) {
			float error = 10000;
			float lastError = 10000;
			while (true) {
				MPoint closestPoint;
				MPoint thisPoint = origins[i] + tValues[i] * directions[i];
				mesh.getClosestPoint(thisPoint, closestPoint);
				error = thisPoint.distanceTo(closestPoint);
				if (i > 0) error -= 0.5;
				MGlobal::displayInfo(MString() + error + ", " + lastError);
				if (error > lastError || error < 0.05) {
					break;
				}
				tValues[i] += error;
				lastError = error;

			}
		}

		for (int i = 1; i < origins.size() - 1; i++) {
			tValues[i] = ((float)i / (float)origins.size()) * (tValues[origins.size()-1] - tValues[0]) + tValues[0];
		}
	}

	//call curve creation
	MString base = "curve -d 1";
	for (int i = 0; i < origins.size(); i++) {
		MPoint m = origins[i] + directions[i] * tValues[i];
		base += "-p";
		base += MString(" ") + m[0] + " " + m[1] + " " + m[2];
		MGlobal::displayInfo(MString() + tValues[i]);
	}
	base += ";";

	MGlobal::executeCommand(base);
}

void paintContext::doReleaseCommon( MEvent & event )
{
	// Extract the event information
	short x, y;
	event.getPosition(x, y);
	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	origins.push_back(newOrg);
	directions.push_back(newDir);
	tValues.push_back(0);

	makeCurve();

	MGlobal::displayInfo("Ended with " + origins.size());
}

MStatus paintContext::doPress( MEvent & event )
{
	view = M3dView::active3dView();
	doPressCommon(event);
	return MS::kSuccess;
}

MStatus paintContext::doDrag(MEvent & event)
{
	// Extract the event information
	short x, y;
	event.getPosition(x, y);
	if (sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 3) return MS::kSuccess;

	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	origins.push_back(newOrg);
	directions.push_back(newDir);
	tValues.push_back(0);

	return MS::kSuccess;	
}

MStatus paintContext::doRelease( MEvent & event )
//
// Selects objects within the marquee box.
{
	doReleaseCommon( event );
	return MS::kSuccess;
}

MStatus	paintContext::doPress (MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
	view = M3dView::active3dView();
	doPressCommon( event );
	return MS::kSuccess;
}

MStatus	paintContext::doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
	doReleaseCommon( event );
	return MS::kSuccess;
}

MStatus	paintContext::doDrag (MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
	// Extract the event information
	short x, y;
	event.getPosition(x, y);
	if (sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 3) return MS::kSuccess;

	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	origins.push_back(newOrg);
	directions.push_back(newDir);
	tValues.push_back(0);

	return MS::kSuccess;
}

void paintContext::setStartLevel(int theLevel) {
	startLevel = theLevel;
}

//////////////////////////////////////////////
// Command to create contexts
//////////////////////////////////////////////

class paintContextCmd : public MPxContextCommand
{
public:	
						paintContextCmd();
	virtual MPxContext*	makeObj();
	static	void*		creator();
	//temporary, until the tool menu mentioned below is implemented
	static  void*		creator2();
	bool				isFeather = false;

	/* Will eventually be used for tool menu *paired with commented methods below*
	virtual	MStatus		doEditFlags();
	virtual MStatus		doQueryFlags();
	virtual MStatus		appendSyntax();
	*/
protected:
	paintContext*		fPaintContext;
	//temporary, until the tool menu mentioned above is implemented
};

paintContextCmd::paintContextCmd() {}

MPxContext* paintContextCmd::makeObj()
{
	fPaintContext = new paintContext();
	if (isFeather) fPaintContext->isFeather = true;
	return fPaintContext;
}

void* paintContextCmd::creator()
{
	return new paintContextCmd;
}

void* paintContextCmd::creator2() {
	paintContextCmd* output = new paintContextCmd();
	output->isFeather = true;
	return output;
}

//Will Eventually be used for setting up tool menu *paired with commented methods in header*
/*
MStatus paintContextCmd::doEditFlags()
{
	MStatus status = MS::kSuccess;

	MArgParser argData = parser();

	if (argData.isFlagSet(kStartLevelSetFlag)) {
		int level;
		status = argData.getFlagArgument(kStartLevelSetFlag, 0, level);
		if (!status) {
			status.perror("numCVs flag parsing failed.");
			return status;
		}
		fPaintContext->setStartLevel(level);
	}

	return MS::kSuccess;
}

MStatus paintContextCmd::doQueryFlags()
{
	MArgParser argData = parser();

	if (argData.isFlagSet(kStartLevelSetFlag)) {
		setResult((int)fPaintContext->getStartLevel());
	}

	return MS::kSuccess;
}

MStatus paintContextCmd::appendSyntax()
{
	MSyntax mySyntax = syntax();

	if (MS::kSuccess != mySyntax.addFlag(kStartLevelSetFlag, kStartLevelSetFlagLong,
		MSyntax::kLong)) {
		return MS::kFailure;
	}

	return MS::kSuccess;
}
*/

//////////////////////////////////////////////
// plugin initialization
//////////////////////////////////////////////
MStatus initializePlugin( MObject obj )
{
	MStatus		status;
	MFnPlugin	plugin( obj, PLUGIN_COMPANY, "12.0", "Any");

	//auto-load the UI path and add it to MAYA_SCRIPT_PATH
	MString pathPrefix = plugin.loadPath();
	MGlobal::executeCommand("string $paat = \"" + pathPrefix + "\";");
	MGlobal::executeCommand("string $compat = $paat + \";\" + getenv(\"MAYA_SCRIPT_PATH\");");
	MGlobal::executeCommand("putenv \"MAYA_SCRIPT_PATH\" $compat;");

	status = plugin.registerContextCommand( "paintContext",
										    paintContextCmd::creator );

	status = plugin.registerContextCommand("featherPaintContext",
		paintContextCmd::creator2);

	return status;
}

MStatus uninitializePlugin( MObject obj )
{
	MStatus		status;
	MFnPlugin	plugin( obj );

	status = plugin.deregisterContextCommand( "paintContext");

	return status;
}
