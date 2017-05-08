#pragma once
#include <maya\MPxContext.h>
#include <vector>
#include <maya\MFnMesh.h>
#include <maya\MStatus.h>
#include <maya\MPoint.h>
#include <maya\MVector.h>
#include <maya\M3dView.h>

class PaintRay {
public:
	MPoint origin;
	MVector direction;
	float t;

	PaintRay() { } //shouldn't be called
	PaintRay(MPoint o, MVector d) {
		origin = o; direction = d; t = 0;
	}
	MPoint point() { return origin + t*direction; }
};

enum ModeType {ErrorMode,LevelMode,FurMode,FeatherMode};

class paintContext : public MPxContext
{
public:
	paintContext();
	virtual void	toolOnSetup(MEvent & event);
	void getClassName(MString &name) const;

	// Catch-all methods for use in any viewport renderer - each will be routed to their 'common' method
	virtual MStatus	doPress(MEvent & event);
	virtual MStatus	doDrag(MEvent & event);
	virtual MStatus	doRelease(MEvent & event);
	virtual MStatus	doPress(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doRelease(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);
	virtual MStatus	doDrag(MEvent & event, MHWRender::MUIDrawManager& drawMgr, const MHWRender::MFrameContext& context);

	//tool Settings methods - set
	void setStartLevel(float level);
	void setEndLevel(float level);
	void setMode(int modeInt);
	//get
	float getStartLevel() { return startLevel; };
	float getEndLevel() { return endLevel; };
	int getMode() { return (int)mode; };


private:
	// Press and Release shared operations (agnostic of viewport)
	void doPressCommon(MEvent & event);
	void doReleaseCommon(MEvent & event);
	void initializeCurve();
	void initializeT(MFnMesh& mesh, PaintRay& r, bool end = false);
	float angleTerm(int i);
	float lengthTerm(int i );
	float errorTerm(int i);
	void shapeCurve();
	void sendToMaya();

	// Temporary vector abstractions
	std::vector<PaintRay> rays;

	//Screen locations to detect movement threshold
	short lastx, lasty, threshold;
	float startLevel, endLevel;
	ModeType mode;
	float weight_a, weight_l, weight_e;
	float assessObj(int i);
	void refinePoint(int i);

	// screen space object
	M3dView view;
};