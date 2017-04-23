#include "paintContext.h"
#include <maya\M3dView.h>
#include <maya\MItDag.h>
#include <maya\MPointArray.h>
#include <maya\MGlobal.h>

const char helpString[] = "Drag with the left mouse button to paint";
const float kMaxError = 0.1;
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

//Components of objective function
float paintContext::angleTerm() {
	//cycle through each pair of adjacent segments
	float output = 0;
	float dot;
	MPoint p1,p2,p3;
	MVector v1, v2;
	
	for (int i = 0; i < rays.size() - 2; i++) {
		//get unit vectors representing consecutive stroke segments
		p1 = rays[i].point(); p2 = rays[i + 1].point(); p3 = rays[i + 2].point();
		v1 = p2 - p1; v2 = p3 - p2;
		dot = v1.normal() * v2.normal();
		if (dot < 0) dot = 0;
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
			output += pow(actual.distanceTo(closest) - startLevel - 0.1, 2);
		}

	//in fur/feather, only the accuracy of the first and last points are weighted
	} else {
		MGlobal::displayError("Didn't Expect to be using this yet...");
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
		return angle * 0 + error;
	case ModeType::FeatherMode:
	case ModeType::FurMode:
		return angle + error + lengthTerm() * 0.05;
	default:
		MGlobal::displayError("Stroke type error");
		return 0;
	}
}
std::vector<float> paintContext::assessGradient(float h) {
	std::vector<float> output = std::vector<float>(); //The amount by which each t value will be adjusted next iteration
	float orig = assessObj(); //The current objective function value
	//float angle = angleTerm();
	//float error = errorTerm();

	//For every t value, compute the change in obj() that results from a small change in t
	for (int i = 0; i < rays.size(); i++) {
		rays[i].t -= h; //t = t - dt
		output.push_back(assessObj() - orig); //gradient[i] = obj(t[i]-dt) - obj(t[i])
		rays[i].t += h; //return to original t
	}

	//adjust t values according to gradient
	for (int i = 0; i < rays.size(); i++) {
		rays[i].t += output[i];
	}

	return output;
}

//return the magnitude of the gradient to decide when we've reached a minimum (grad ~= 0)
float gradMag(std::vector<float> grad) {
	float output = 0;
	for (int i = 0; i < grad.size(); i++) {
		output += pow(grad[i], 2);
	}

	return sqrt(output);
}


void paintContext::initializeT(MFnMesh& mesh, PaintRay& r, bool end) {
	float error = 10000;
	float lastError = 10000;

	//check if we can rely on a converging error
	if (mesh.intersect(r.origin, r.direction, MPointArray())) {
		while (true) {
			MPoint closestPoint;
			MPoint thisPoint = r.point();
			mesh.getClosestPoint(thisPoint, closestPoint);
			error = thisPoint.distanceTo(closestPoint);
			if (end) error -= endLevel;
			else error -= startLevel;
			if (error < kMaxError) break;
			r.t += error;
		}
	//if not get near the inflection - REALLY raw right now
	} else {
		while (true) {
			MPoint closestPoint;
			MPoint thisPoint = r.point();
			mesh.getClosestPoint(thisPoint, closestPoint);
			error = thisPoint.distanceTo(closestPoint);
			if (end) error -= endLevel;
			else error -= startLevel;
			if (error > lastError) break;
			r.t += error;
			lastError = error;
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
	float h = 0.3;
	float mag = 100;
	//compute an (hopefully) decreasing gradient until convergence or give-up
	MGlobal::displayInfo(MString("Initial Angle Term: ") + angleTerm());
	MGlobal::displayInfo(MString("Initial Error Term: ") + errorTerm());
	MGlobal::displayInfo("ITERATIVELY OPTIMIZING..................................");
	while (h > 0.01 && mag > 0.01) {
		mag = gradMag(assessGradient(h)) / h;
		h -= 0.001;
	}
	MGlobal::displayInfo(MString("Final Angle Term: ") + angleTerm());
	MGlobal::displayInfo(MString("Initial Error Term: ") + errorTerm());
	//final curve
	sendToMaya();
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

