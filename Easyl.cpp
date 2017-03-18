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
	// screen space object
	M3dView view;
};

paintContext::paintContext()
{
	setTitleString ( "Easyl" );

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
		} else {
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

//////////////////////////////////////////////
// Command to create contexts
//////////////////////////////////////////////

class paintContextCmd : public MPxContextCommand
{
public:	
						paintContextCmd();
	virtual MPxContext*	makeObj();
	static	void*		creator();
};

paintContextCmd::paintContextCmd() {}

MPxContext* paintContextCmd::makeObj()
{
	return new paintContext();
}

void* paintContextCmd::creator()
{
	return new paintContextCmd;
}

//////////////////////////////////////////////
// plugin initialization
//////////////////////////////////////////////
MStatus initializePlugin( MObject obj )
{
	MStatus		status;
	MFnPlugin	plugin( obj, PLUGIN_COMPANY, "12.0", "Any");

	status = plugin.registerContextCommand( "paintContext",
										    paintContextCmd::creator );
	return status;
}

MStatus uninitializePlugin( MObject obj )
{
	MStatus		status;
	MFnPlugin	plugin( obj );

	status = plugin.deregisterContextCommand( "paintContext");

	return status;
}
