#include "paintContext.h"
#include <maya\M3dView.h>
#include <maya\MItDag.h>
#include <maya\MFnMesh.h>
#include <maya\MPointArray.h>
#include <maya\MGlobal.h>

const char helpString[] = "Drag with the left mouse button to paint";
const float kMaxError = 0.1;
const int thresholdDefault = 3;

paintContext::paintContext()
{
	setTitleString("Easyl");
	startLevel = 0;
	endLevel = 0;
	mode = LevelMode;

	// Tell the context which XPM to use, currently uses MarqueeTool's xmp
	setImage("Easyl.xpm", MPxContext::kImage1);

}

void paintContext::toolOnSetup(MEvent &)
{
	setHelpString(helpString);
}

void paintContext::getClassName(MString &name) const
{
	name.set("paintTool");
}

void paintContext::doPressCommon(MEvent & event)
{
	//beginning new line; remove all previous temp data
	rays.clear();

	// Extract the event information
	short x, y;
	event.getPosition(x, y);
	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);
	lastx = x; lasty = y;

	rays.push_back(PaintRay(newOrg,newDir));

	MGlobal::displayInfo("Started");
}

//TODO: ensure curve is high enough resolution for later optim.
void paintContext::makeCurve() {

	MStatus s;
	PaintRay* r;

	//get the mesh
	MItDag itr(MItDag::kDepthFirst, MFn::kMesh);
	MObject meshObj = itr.item();

	//currently operates ONLY on the first mesh it finds
	MFnMesh mesh(meshObj, &s);

	if (mode == LevelMode) {
		//determine t values for every i
		for (int i = 0; i < rays.size(); i++) {
			r = &rays[i];
			float error = 10000;
			float lastError = 10000;

			//check if we can rely on a converging error
			if (mesh.intersect(r->origin, r->direction, MPointArray(), &s)) {
				while (true) {
					MPoint closestPoint;
					MPoint thisPoint = r->origin + r->direction * r->t;
					mesh.getClosestPoint(thisPoint, closestPoint);
					error = thisPoint.distanceTo(closestPoint);
					error -= startLevel;
					if (error < kMaxError) break;
					r->t += error;
				}
				//if not get near the inflection - REALLY raw right now
			}
			else {
				while (true) {
					MPoint closestPoint;
					MPoint thisPoint = r->origin + r->direction * r->t;
					mesh.getClosestPoint(thisPoint, closestPoint);
					error = thisPoint.distanceTo(closestPoint);
					error -= startLevel;
					if (error > lastError) {
						break;
					}
					r->t += error;
					lastError = error;

				}
			}
		}

		//temporary feather demo - will be replaced by optimization method and custom tool options
	}
	else {
		//determine t values for first and last i
		for (int i = 0; i < rays.size(); i += rays.size() - 1) {
			r = &rays[i];
			float error = 10000;
			float lastError = 10000;
			while (true) {
				MPoint closestPoint;
				MPoint thisPoint = r->origin + r->direction * r->t;
				mesh.getClosestPoint(thisPoint, closestPoint);
				error = thisPoint.distanceTo(closestPoint);
				if (i > 0) error -= endLevel;
				else error -= startLevel;
				if (error > lastError || error < 0.05) {
					break;
				}
				r->t += error;
				lastError = error;

			}
		}


		for (int i = 1; i < rays.size() - 1; i++) {
			r = &rays[i];
			r->t = ((float)i / (float)rays.size()) * (rays.back().t - rays.front().t) + rays.front().t;
		}
	}

	//call curve creation
	MString base = "curve -d 1";
	for (int i = 0; i < rays.size(); i++) {
		r = &rays[i];
		MPoint m = r->origin + r->direction * r->t;
		base += "-p";
		base += MString(" ") + m[0] + " " + m[1] + " " + m[2];
	}
	base += ";";

	MGlobal::executeCommand(base);
}

void paintContext::doReleaseCommon(MEvent & event)
{
	// Extract the event information
	short x, y;
	event.getPosition(x, y);
	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	rays.push_back(PaintRay(newOrg, newDir));

	makeCurve();

	MGlobal::displayInfo("Ended with " + rays.size() + MString(" rays!"));
}

MStatus paintContext::doPress(MEvent & event)
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

	rays.push_back(PaintRay(newOrg, newDir));

	return MS::kSuccess;
}
MStatus paintContext::doRelease(MEvent & event)
//
// Selects objects within the marquee box.
{
	doReleaseCommon(event);
	return MS::kSuccess;
}
MStatus	paintContext::doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
	view = M3dView::active3dView();
	doPressCommon(event);
	return MS::kSuccess;
}
MStatus	paintContext::doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
	doReleaseCommon(event);
	return MS::kSuccess;
}
MStatus	paintContext::doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context)
{
	// Extract the event information
	short x, y;
	event.getPosition(x, y);
	if (sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 3) return MS::kSuccess;

	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	rays.push_back(PaintRay(newOrg, newDir));

	return MS::kSuccess;
}

void paintContext::setStartLevel(float theLevel) {
	startLevel = theLevel;
}

void paintContext::setEndLevel(float level) {
	endLevel = level;
}

void paintContext::setMode(int modeInt) {
	mode = static_cast<ModeType>(modeInt);
}