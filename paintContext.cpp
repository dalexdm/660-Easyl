#include "paintContext.h"
#include <maya\M3dView.h>
#include <maya\MItDag.h>
#include <maya\MPointArray.h>
#include <maya\MGlobal.h>

const char helpString[] = "Drag with the left mouse button to paint";
const float kMaxError = 0.05;
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

//Converging (FINALLY!!!!!) but unintended mutation of the very last point (seems to bend 90deg)
//also 1/10 lines will have a random kink?
float paintContext::angleTerm() {
	//cycle through each pair of adjacent segments
	float output = 0;
	float dot;
	MPoint p1,p2,p3;
	MVector v1, v2;

	if (mode == ModeType::FurMode) {
		//get that mesh
		MItDag itr(MItDag::kDepthFirst, MFn::kMesh);
		MObject meshObj = itr.item();
		MFnMesh mesh(meshObj);
		//prepend a control point (p1) directly toward the mesh from where we are
		p2 = rays[0].point(); p3 = rays[1].point();
		mesh.getClosestPoint(p2, p1);
		v1 = p2 - p1; v2 = p3 - p2;
		dot = v1.normal() * v2.normal();
		//assess cost based on the existence of that point
		output += pow(1 - dot, 2);
	}
	
	for (int i = 0; i < rays.size() - 2; i++) {
		//get unit vectors representing consecutive stroke segments
		p1 = rays[i].point(); p2 = rays[i + 1].point(); p3 = rays[i + 2].point();
		v1 = p2 - p1; v2 = p3 - p2;
		dot = v1.normal() * v2.normal();
		//output the 'cost': 0 = parallel... 1 = orthogonal
		output += pow(1 - dot,2);
	}
	return output;
}
float paintContext::lengthTerm() {
	float output = 0;
	for (int i = 0; i < rays.size()-1; i++) {
		output += pow(rays[i + 1].point().distanceTo(rays[i].point()),2);
	}
	return output;
}
float paintContext::errorTerm() {
	//find the mesh in the scene
	MItDag itr(MItDag::kDepthFirst, MFn::kMesh);
	MObject meshObj = itr.item();
	MFnMesh mesh(meshObj);

	MPoint actual, closest;
	float output = 0;

	//for level mode, the error is counted for every ray
	if (mode == ModeType::LevelMode) {
		for (int i = 0; i < rays.size(); i++) {
			actual = rays[i].point();
			mesh.getClosestPoint(actual, closest);
			output += pow(actual.distanceTo(closest) - startLevel - 0.001, 2);
		}

	//in fur/feather, only the accuracy of the first and last points are weighted
	} else {
		//first
		actual = rays[0].point();
		mesh.getClosestPoint(actual, closest);
		output += pow(actual.distanceTo(closest) - startLevel, 2);
		//last
		actual = rays[rays.size() - 1].point();
		mesh.getClosestPoint(actual, closest);
		output += pow(actual.distanceTo(closest) - endLevel, 2);
	}
	return output;
}

//Assess all three objective functions and weight each as prescribed in paper
float paintContext::assessObj() {
	float angle = angleTerm();
	float error = errorTerm();

	switch (mode) {
	case ModeType::LevelMode:
		return error + angle *0.1;
	case ModeType::FeatherMode:
	case ModeType::FurMode:
		return angle + error + lengthTerm() * 0.05; //error will not change for internal fur/feather points, will consider caching for lighter iteration
	default:
		MGlobal::displayError("Stroke type error");
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
		if (i == 0) currentObj = errorTerm(); // first point should not be angle-weighted
		else currentObj = assessObj();
		rays[i].t += h;
		if (i == 0) grad = errorTerm() - currentObj;
		else grad = (assessObj() - currentObj); //moving t by h changes f(t) by grad
		rays[i].t -= h; //return h to original value (for clarity)

		//descent step
		rays[i].t += gamma * -grad;
		gamma -= 0.01;
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
			stepSize = oldDistance;
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

		//linearly interpolate t values for internal points
		for (int i = 1; i < rays.size() - 1; i++) {
			PaintRay* r = &rays[i];
			r->t = ((float)i / (float)rays.size()) * (rays.back().t - rays.front().t) + rays.front().t;
		}
	}
}

void paintContext::shapeCurve() {


	//initial curve
	sendToMaya();
	MGlobal::displayInfo(MString("Initial Angle Term: ") + angleTerm());
	MGlobal::displayInfo(MString("Initial Error Term: ") + errorTerm());
	MGlobal::displayInfo("ITERATIVELY OPTIMIZING...........................");
	//go through each ray, starting at the 'root' point, and optimize piecemeal
	for (int i = 0; i < rays.size(); i++) {
		refinePoint(i);
	}
	MGlobal::displayInfo("DONE OPTIMIZING..................................");
	MGlobal::displayInfo(MString("Final Angle Term: ") + angleTerm());
	MGlobal::displayInfo(MString("Final Error Term: ") + errorTerm());

	//final curve
	sendToMaya();
}

void paintContext::doReleaseCommon(MEvent & event)
{
	short x, y;
	event.getPosition(x, y);
	//see if the release was far enough away from the last point to warrant a ray
	if (!sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 10) {

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
	if (sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 10) return MS::kSuccess;

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
	if (sqrt(pow(lastx - x, 2) + pow(lasty - y, 2)) < 10) return MS::kSuccess;

	MPoint newOrg = MPoint();
	MVector newDir = MVector();
	view.viewToWorld(x, y, newOrg, newDir);

	rays.push_back(PaintRay(newOrg, newDir));
	lastx = x; lasty = y;

	return MS::kSuccess;
}

void paintContext::sendToMaya() {
	PaintRay r;
	MString base = "curve -d 1";
	for (int i = 0; i < rays.size(); i++) {
		r = rays[i];
		MPoint m = r.point();
		base += "-p";
		base += MString(" ") + m[0] + " " + m[1] + " " + m[2];
	}
	base += ";";
	MGlobal::executeCommand(base);
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

