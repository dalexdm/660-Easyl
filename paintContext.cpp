#include "paintContext.h"
#include <maya\M3dView.h>
#include <maya\MItDag.h>
#include <maya\MPointArray.h>
#include <maya\MGlobal.h>

const char helpString[] = "Drag with the left mouse button to paint";
const float kMaxError = 0.05;
const float DRAW_RESOLUTION = 0.2; //between 1 (very very fine) and 0.1 (pretty coarse) 
const int thresholdDefault = 3;

void print(MString s) {
	MGlobal::displayInfo(s);
}

paintContext::paintContext()
{
	setTitleString("Easyl");
	startLevel = 0;
	endLevel = 0;
	mode = LevelMode;

	// Tell the context which XPM (menu icon) to use, currently uses MarqueeTool's xmp
	setImage("Easyl.xpm", MPxContext::kImage1);

}

//determine the tooltip
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
}

//Converging (FINALLY!!!!!)
float paintContext::angleTerm(int index) {
	float output = 0;
	float dot;
	MPoint p1,p2,p3;
	MVector v1, v2;

	//for the first point of a non-level-set stroke, we want to use a control point
	if (mode != ModeType::LevelMode && index == 1) {
		//get that mesh
		MItDag itr(MItDag::kDepthFirst, MFn::kMesh);
		MObject meshObj = itr.item();
		MFnMesh mesh(meshObj);

		p2 = rays[0].point(); p3 = rays[1].point();
		//p1 is now on the mesh surface
		mesh.getClosestPoint(p2, p1);

		if (mode == ModeType::FurMode) {
			//prepend a control point (p1) directly toward the mesh from where we are
			v1 = p2 - p1; v2 = p3 - p2;
			dot = v1.normal() * v2.normal();
			//assess cost of first angle based on the existence of that point
			output += pow(1 - dot, 2);
		}
		else if (mode == ModeType::FeatherMode) {
			//we want the cross of the point-to-surface and (the point-to-surface and the point-to-last-point)
			v1 = (p1 - p2) ^ ((p1 - p2) ^ (rays[rays.size() - 1].point() - p2)); v2 = p3 - p2;
			dot = -(v1.normal()) * v2.normal();
			//again, always calculate that first dot
			output += pow(1 - dot, 2) * 2;
		}
	} 
	
	//for the interior points of every stroke, we optimize for straightness
	else if (index > 1 && index < rays.size()) {
		//get unit vectors representing consecutive stroke segments
		p1 = rays[index-2].point(); p2 = rays[index-1].point(); p3 = rays[index].point();
		v1 = p2 - p1; v2 = p3 - p2;
		dot = v1.normal() * v2.normal();

		//output the 'cost': 0 = parallel... 1 = orthogonal
		output += pow(1 - dot, 2);
	}
	return output;
}
float paintContext::lengthTerm(int index) {
	float output = 0;
	if (index > 0) output += pow(rays[index - 1].point().distanceTo(rays[index].point()),2);
	if (index < rays.size() - 1) output += pow(rays[index + 1].point().distanceTo(rays[index].point()), 2);
	return output;
}
float paintContext::errorTerm(int index) {
	//find the mesh in the scene
	MItDag itr(MItDag::kDepthFirst, MFn::kMesh);
	MObject meshObj = itr.item();
	MFnMesh mesh(meshObj);

	MPoint actual, closest;
	float output = 0;

	//check the error for all level points, or the first point of fur/feather
	if (mode == ModeType::LevelMode || index == 0) {
		actual = rays[index].point();
		mesh.getClosestPoint(actual, closest);
		return pow(actual.distanceTo(closest) - startLevel - 0.001, 2);

	//check error for last point of fur/feather (uses end level)
	} else if (index == rays.size() - 1) {
		actual = rays[index].point();
		mesh.getClosestPoint(actual, closest);
		output += pow(actual.distanceTo(closest) - endLevel, 2);
	}
	return output;
}

//Assess all three objective functions and weight each as prescribed in paper
float paintContext::assessObj(int i) {

	switch (mode) {
	case ModeType::LevelMode:
		return errorTerm(i) + angleTerm(i) * 0.1;
	case ModeType::FeatherMode:
	case ModeType::FurMode:
		if (i == 0) return errorTerm(i); //root, must lie on desired level set for intelligibility
		else return angleTerm(i) + lengthTerm(i) * 0.1; //interior fur/feather wont affect error, dont compute
	default:
		MGlobal::displayError("Unrecognized stroke type error");
		return 0;
	}
}

void paintContext::refinePoint(int i) {
	float h = 0.001;
	float gamma = 5;
	float grad = 100;
	float currentObj = 0;
	//gradually decrease descent step size - recently changed from constant h and variable gamma coefficient
	while (gamma > 0.01 && pow(grad,2) > 0.000000001) {
		//finite difference to find the gradient
		currentObj = assessObj(i);
		rays[i].t += h;
		grad = (assessObj(i) - currentObj); //moving t by h changes f(t) by grad
		rays[i].t -= h; //return h to original value (for clarity)

		//descent step
		rays[i].t += gamma * -grad;
		gamma -= 0.03;
	}

}

void paintContext::initializeT(MFnMesh& mesh, PaintRay& r, bool end) {
	float error = 10000;
	float lastError = 10000;
	float stepSize = 0;
	MPoint closestPoint, thisPoint;
	float oldDistance = 100;
	float newDistance = 0;

	//check if we can rely on a converging error
	if (mesh.intersect(r.origin, r.direction, MPointArray())) {
		while (true) {
			thisPoint = r.point();
			mesh.getClosestPoint(thisPoint, closestPoint);
			error = thisPoint.distanceTo(closestPoint);
			if (end) error -= endLevel;
			else error -= startLevel;
			if (error < kMaxError) break;
			r.t += error;
		}

	} else {
		//loops until closest point on ray is found (~linesearch)
		while (oldDistance - newDistance > kMaxError) {
			//get the oldDistance from the point
			thisPoint = r.point();
			mesh.getClosestPoint(thisPoint, closestPoint);
			oldDistance = thisPoint.distanceTo(closestPoint);
			stepSize = oldDistance / 3;
			while (true) {
				//create a newDistance by adding a step
				thisPoint = r.point() + r.direction*stepSize;
				mesh.getClosestPoint(thisPoint, closestPoint);
				newDistance = thisPoint.distanceTo(closestPoint);
				//if newDistance is better, repeat outer
				if (newDistance - oldDistance < 0.005) {
					r.t += stepSize;
					break;
				}
				//otherwise reduce until newDistance is better or error is too small
				stepSize /= 2;
			}
		}
	}
}

void paintContext::initializeCurve() {

	MStatus s;

	//get the mesh: works reliably only after freezing transforms
	MItDag itr(MItDag::kDepthFirst, MFn::kMesh);
	MObject meshObj = itr.item();
	MFnMesh mesh(meshObj, &s);

	if (s != MStatus::kSuccess) {
		MGlobal::displayError("No mesh!");
		return;
	}

	if (mode == LevelMode) {
		//determine t values for every i
		for (int i = 0; i < rays.size(); i++) {
			initializeT(mesh, rays[i]);
		}

	//initialize hair or feathers
	} else {
		//determine t values for first and last i
		initializeT(mesh, rays[0]);
		initializeT(mesh, rays[rays.size() - 1], true);

		//EXPERIMENTAL
		//create an intersection plane on which to project the linearly initialize points
		MPoint P = rays[rays.size() - 1].point();
		MVector R = rays[rays.size() - 1].direction; //~eye to last
		MVector D = rays[0].point() - P; //last to first
		MVector planeNormal = D ^ (R^D); // Borrowing the 'minimum skew plane' from secondSkin: D x (R x D)

		for (int i = 1; i < rays.size() - 1; i++) {
			//typical plane intersection to linearly position internals
			rays[i].t = ((P - rays[i].origin) * planeNormal)
						/ (rays[i].direction * planeNormal);
		}
	}
}

void paintContext::shapeCurve() {


	//initial curve
	//sendToMaya();
	MGlobal::displayInfo("ITERATIVELY OPTIMIZING...........................");
	//go through each ray, starting at the 'root' point, and optimize piecemeal
	for (int i = 0; i < rays.size(); i++) {
		refinePoint(i);
	}
	MGlobal::displayInfo("DONE OPTIMIZING..................................");

	//final curve
	sendToMaya();
}

void paintContext::doReleaseCommon(MEvent & event)
{
	short x, y;
	event.getPosition(x, y);
	//see if the release was far enough away from the last point to warrant a ray
	if (!sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 1.0 / DRAW_RESOLUTION) {

		MPoint newOrg = MPoint();
		MVector newDir = MVector();
		view.viewToWorld(x, y, newOrg, newDir);

		rays.push_back(PaintRay(newOrg, newDir));

	}

	//begin creation of new curve
	initializeCurve();
	shapeCurve();
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
	if (sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 1.0 / DRAW_RESOLUTION) return MS::kSuccess;

	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	rays.push_back(PaintRay(newOrg, newDir));
	lastx = x; lasty = y;

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
	if (sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 1.0/DRAW_RESOLUTION) return MS::kSuccess;

	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	rays.push_back(PaintRay(newOrg, newDir));
	lastx = x; lasty = y;

	return MS::kSuccess;
}

void paintContext::sendToMaya() {
	PaintRay r;
	MString base = "string $theCurve = `curve -d 1";
	for (int i = 0; i < rays.size()-1; i++) {
		r = rays[i];
		MPoint m = r.point();
		base += " -p";
		base += MString(" ") + m[0] + " " + m[1] + " " + m[2];
	}
	base += "`;";
	MGlobal::executeCommand(base);
	MGlobal::executeCommand("AttachBrushToCurves;convertCurvesToStrokes;manipMoveValues Move;toolPropertyShow;autoUpdateAttrEd;");
	MGlobal::executeCommand("delete $theCurve;");
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

